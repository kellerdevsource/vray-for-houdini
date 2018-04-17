//
// Copyright (c) 2015-2017, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#include "vfh_vrayproxyutils.h"
#include "vfh_vray.h"
#include "vfh_log.h"
#include "vfh_hashes.h"

#include <GU/GU_Detail.h>
#include <GU/GU_DetailHandle.h>
#include <GU/GU_PrimPoly.h>
#include <GU/GU_PackedGeometry.h>

using namespace VRayForHoudini;

enum DataError {
	DE_INVALID_GEOM = 1,
	DE_NO_GEOM,
	DE_INVALID_FILE
};

struct MeshVoxelRAII
{
	MeshVoxelRAII(VUtils::MeshInterface *mi, int index)
		: mi(mi)
	{
		vassert(mi && "MeshInterface is NULL");

		if (index < 0 || index >= mi->getNumVoxels()) {
			vassert(false && "Invalid voxel index");
		}

		mv = mi->getVoxel(index);

		vassert(mv && "MeshVoxel is NULL");
	}

	~MeshVoxelRAII() {
		mi->releaseVoxel(mv);
	}

	bool isValid() const { return mv; }

	VUtils::MeshVoxel &getVoxel() const { return *mv; }

private:
	VUtils::MeshInterface *mi = nullptr;
	VUtils::MeshVoxel *mv = nullptr;
};

/// Appends mesh data to detail.
/// @param gdp Detail.
/// @param voxel Voxel of type MVF_GEOMETRY_VOXEL.
static int addMeshVoxelData(GU_Detail &gdp, VUtils::MeshVoxel &voxel, int maxFaces, bool useMaxFaces)
{
	const VUtils::MeshChannel *vertChan = voxel.getChannel(VERT_GEOM_CHANNEL);
	const VUtils::MeshChannel *faceChan = voxel.getChannel(FACE_TOPO_CHANNEL);
	if (!vertChan || !faceChan) {
		gdp.addWarning(GU_WARNING_NO_METAOBJECTS, "Found invalid mesh voxel!");
		return false;
	}

	const VUtils::VertGeomData *verts = reinterpret_cast<VUtils::VertGeomData*>(vertChan->data);
	const VUtils::FaceTopoData *faces = reinterpret_cast<VUtils::FaceTopoData*>(faceChan->data);
	if (!verts || !faces) {
		gdp.addWarning(GU_WARNING_NO_METAOBJECTS, "Found invalid mesh voxel data!");
		return false;
	}

	const int numVerts = vertChan->numElements;
	const int numFaces = faceChan->numElements;

	if (!useMaxFaces || numFaces <= maxFaces) {
		const GA_Offset pointOffset = gdp.appendPointBlock(numVerts);
		for (int vertexIndex = 0; vertexIndex < numVerts; ++vertexIndex) {
			const VUtils::Vector &vert = verts[vertexIndex];
			gdp.setPos3(pointOffset + vertexIndex, UT_Vector3F(vert.x, vert.y, vert.z));
		}

		for (int faceIndex = 0; faceIndex < numFaces; ++faceIndex) {
			const VUtils::FaceTopoData &face = faces[faceIndex];

			GU_PrimPoly *poly = GU_PrimPoly::build(&gdp, 3, GU_POLY_CLOSED, 0);
			poly->setVertexPoint(0, pointOffset + face.v[0]);
			poly->setVertexPoint(1, pointOffset + face.v[1]);
			poly->setVertexPoint(2, pointOffset + face.v[2]);
			poly->reverse();
		}
	}
	else {
		VUtils::VectorRefList previewVertsRaw(maxFaces * 3);
		VUtils::IntRefList    previewFacesRaw(maxFaces * 3);

		int numPreviewFaces = 0;
		int numPreviewVerts = 0;
		
		const double ratio = double(maxFaces)/ double(numFaces);

		for (int i = 0; i < numFaces; ++i) {
			const int p0 = i * ratio;
			const int p1 = (i + 1) * ratio;

			if (p0 != p1 && numPreviewFaces < maxFaces) {
				for (int k = 0; k < 3; ++k) {
					previewFacesRaw[numPreviewFaces * 3 + k] = numPreviewVerts;
					previewVertsRaw[numPreviewVerts++] = verts[faces->v[i * 3 + k]];
				}
				numPreviewFaces++;
			}
		}

		const GA_Offset pointOffset = gdp.appendPointBlock(numPreviewVerts);
		for (int vertexIndex = 0; vertexIndex < numPreviewVerts; ++vertexIndex) {
			const VUtils::Vector &vert = previewVertsRaw[vertexIndex];
			gdp.setPos3(pointOffset+vertexIndex, UT_Vector3F(vert.x, vert.y, vert.z));
		}

		for (int faceIndex = 0; faceIndex < numPreviewFaces; ++faceIndex) {
			GU_PrimPoly *poly = GU_PrimPoly::build(&gdp, 3, GU_POLY_CLOSED, 0);
			poly->setVertexPoint(0, pointOffset + previewFacesRaw[faceIndex * 3 + 0]);
			poly->setVertexPoint(1, pointOffset + previewFacesRaw[faceIndex * 3 + 1]);
			poly->setVertexPoint(2, pointOffset + previewFacesRaw[faceIndex * 3 + 2]);
			poly->reverse();
		}
	}

	return true;
}

/// Appends hair data to detail.
/// @param gdp Detail.
/// @param voxel Voxel of type MVF_HAIR_GEOMETRY_VOXEL.
static int addHairVoxelData(GU_Detail &gdp, VUtils::MeshVoxel &voxel)
{
	VUtils::MeshChannel *vertChan = voxel.getChannel(HAIR_VERT_CHANNEL);
	VUtils::MeshChannel *strandChan = voxel.getChannel(HAIR_NUM_VERT_CHANNEL);
	if (!vertChan || !strandChan) {
		gdp.addWarning(GU_WARNING_NO_METAOBJECTS, "Found invalid hair voxel!");
		return false;
	}

	const VUtils::VertGeomData *verts   = reinterpret_cast<VUtils::VertGeomData*>(vertChan->data);
	const int                  *strands = reinterpret_cast<int*>(strandChan->data);
	if (!verts || !strands) {
		gdp.addWarning(GU_WARNING_NO_METAOBJECTS, "Found invalid hair voxel data!");
		return false;
	}

	const int numVerts   = vertChan->numElements;
	const int numStrands = strandChan->numElements;

	GA_Offset pointOffset = gdp.appendPointBlock(numVerts);
	for (int v = 0; v < numVerts; ++v) {
		const VUtils::Vector &vert = verts[v];
		gdp.setPos3(pointOffset + v, UT_Vector3F(vert.x, vert.y, vert.z));
	}

	for (int strandIndex = 0; strandIndex < numStrands; ++strandIndex) {
		const int vertsPerStrand = strands[strandIndex];

		GU_PrimPoly *poly = GU_PrimPoly::build(&gdp, vertsPerStrand, GU_POLY_OPEN, 0);
		for (int strandVertexIndex = 0; strandVertexIndex < vertsPerStrand; ++strandVertexIndex) {
			poly->setVertexPoint(strandVertexIndex, pointOffset + strandVertexIndex);
		}

		pointOffset += vertsPerStrand;
	}

	return true;
}

/// Appends voxel data to detail.
/// @param gdp Detail.
/// @param voxel Voxel.
static int addVoxelData(GU_Detail &gdp, VUtils::MeshVoxel &voxel, uint32 voxelFlags, int maxFaces, bool useMaxFaces)
{
	if (voxelFlags & MVF_GEOMETRY_VOXEL ||
		voxelFlags & MVF_PREVIEW_VOXEL)
	{
		return addMeshVoxelData(gdp, voxel, maxFaces, useMaxFaces);
	}
	if (voxelFlags & MVF_HAIR_GEOMETRY_VOXEL) {
		return addHairVoxelData(gdp, voxel);
	}
	return false;
}

static int addVoxelData(GU_Detail &gdp, VUtils::MeshInterface &mi, int voxelIndex, int maxFaces = 100000, bool useMaxFaces = false)
{
	MeshVoxelRAII voxelRaii(&mi, voxelIndex);
	if (!voxelRaii.isValid())
		return false;

	const uint32 voxelFlags = mi.getVoxelFlags(voxelIndex);

	return addVoxelData(gdp, voxelRaii.getVoxel(), voxelFlags, maxFaces, useMaxFaces);
}

static VUtils::Box getBoundingBox(VUtils::MeshInterface &mi)
{
	VUtils::Box bbox = mi.getBBox();

	if (bbox.isEmpty() || bbox.isInfinite()) {
		bbox.init();
		bbox += VUtils::Vector( 1.0f,  1.0f,  1.0f);
		bbox += VUtils::Vector(-1.0f, -1.0f, -1.0f);
	}

	return bbox;
}

#if 0
static Hash::MHash getBoundingBoxHash(VUtils::MeshInterface &mi)
{
	const VUtils::Box &bbox = getBoundingBox(mi);

	Hash::MHash bboxHash = 0;
	Hash::MurmurHash3_x86_32(&bbox, sizeof(VUtils::Box), 42, &bboxHash);

	return bboxHash;
}

static Hash::MHash getVoxelHash(VUtils::MeshVoxel &voxel, uint32 voxelFlags)
{
	Hash::MHash hashKey = 0;

	VUtils::MeshChannel *channel = nullptr;

	if (voxelFlags & MVF_GEOMETRY_VOXEL ||
		voxelFlags & MVF_PREVIEW_VOXEL)
	{
		channel = voxel.getChannel(VERT_GEOM_CHANNEL);
	}
	else if (voxelFlags & MVF_HAIR_GEOMETRY_VOXEL) {
		channel = voxel.getChannel(HAIR_VERT_CHANNEL);
	}

	if (channel && channel->data) {
		const int dataSize = channel->elementSize * channel->numElements;
		const uint32 seed = reinterpret_cast<uintptr_t>(channel->data);

		Hash::MurmurHash3_x86_32(channel->data, dataSize, seed, &hashKey);
	}

	return hashKey;
}

static Hash::MHash getVoxelHash(VUtils::MeshInterface &mi, int voxelIndex)
{
	MeshVoxelRAII voxelRaii(&mi, voxelIndex);
	if (!voxelRaii.isValid())
		return false;

	const uint32 voxelFlags = mi.getVoxelFlags(voxelIndex);

	return getVoxelHash(voxelRaii.getVoxel(), voxelFlags);
}
#endif

/// Creates a box in detail.
/// @param gdp Detail.
/// @param mi MeshInterface to get bounding box from.
static int addBoundingBoxData(GU_Detail &gdp, VUtils::MeshInterface &mi)
{
	const VUtils::Box &bbox = getBoundingBox(mi);

	const VUtils::Vector &boxMin = bbox.pmin;
	const VUtils::Vector &boxMax = bbox.pmax;

	gdp.cube(boxMin[0], boxMax[0],
			 boxMin[1], boxMax[1],
			 boxMin[2], boxMax[2],
			 0, 0, 0, 0,
			 true);

	return true;
}

class VRayProxyCache
{
public:
	/// Geometry hash type.
	typedef Hash::MHash DetailKey;

	VRayProxyCache() {}

	~VRayProxyCache() {
		freeMem();
	}

	/// Return the detail handle for a proxy packed primitive (based on frame, LOD)
	/// Caches the detail if not present in cache
	/// @param options[in] - proxy primitive options
	/// @return detail handle  if successful or emty one on error
	GU_DetailHandle getDetail(const VRayProxyRefKey &options);

	int open(const char *filepath);

	int getBBox(UT_BoundingBox &box);

private:
	/// Clears the caches and resets current .vrmesh file.
	void freeMem();

	/// Returns function to get the actual frame in the .vrmesh file based on
	/// proxy primitive options.
	int getFrame(const VRayProxyRefKey &options) const;
	
	GU_DetailHandle addDetail(const VRayProxyRefKey &options);

	/// MeshFile instance. 
	VUtils::MeshFile *meshFile = nullptr;
};

typedef QMap<QString, VRayProxyCache> VRayProxyCacheMan;

static UT_Lock theLock;
static VRayProxyCacheMan theCacheMan;

GU_DetailHandle VRayForHoudini::getVRayProxyDetail(const VRayProxyRefKey &options)
{
	UT_AutoLock lock(theLock);

	if (options.filePath.empty())
		return GU_DetailHandle();

	const char *filePath = options.filePath.ptr();

	VRayProxyCacheMan::iterator it = theCacheMan.find(filePath);
	if (it != theCacheMan.end()) {
		return it.value().getDetail(options);
	}

	VRayProxyCache &cache = theCacheMan[filePath];
	if (!cache.open(filePath)) {
		theCacheMan.remove(filePath);
		return GU_DetailHandle();
	}

	return cache.getDetail(options);
}

bool VRayForHoudini::clearVRayProxyCache(const char *filepath)
{
	UT_AutoLock lock(theLock);
	return theCacheMan.remove(filepath);
}

bool VRayForHoudini::getVRayProxyBoundingBox(const VRayProxyRefKey &options, UT_BoundingBox &box)
{
	UT_AutoLock lock(theLock);

	if (options.filePath.empty())
		return false;
	
	VRayProxyCacheMan::iterator it = theCacheMan.find(options.filePath.ptr());
	
	int res;
	if (it != theCacheMan.end()) {
		res = it.value().getBBox(box);
	}
	else {
		// build a mesh, get bbox from that
		VRayProxyCache &cache = theCacheMan[options.filePath.ptr()];
		if (!cache.open(options.filePath.ptr())) {
			theCacheMan.remove(options.filePath.ptr());
			res = false;
		}
		else {
			res = cache.getBBox(box);
		}
	}

	return res;
}

int VRayProxyCache::open(const char *filepath)
{
	freeMem();

	VUtils::CharString path(filepath);
	if (!bmpCheckAssetPath(path, NULL, NULL, false)) {
		Log::getLog().error("VRayProxy: Can't find file \"%s\"", filepath);
		return false;
	}

	meshFile = VUtils::newDefaultMeshFile(path.ptr());
	if (!meshFile) {
		Log::getLog().error("VRayProxy: Can't allocate MeshFile \"%s\"", filepath);
		freeMem();
		return false;
	}

	if (meshFile->init(path.ptr()).error()) {
		Log::getLog().error("VRayProxy: Can't open file \"%s\"", filepath);
		freeMem();
		return false;
	}

	return true;
}

void VRayProxyCache::freeMem()
{
	if (meshFile) {
		deleteDefaultMeshFile(meshFile);
		meshFile = nullptr;
	}
}

int VRayProxyCache::getFrame(const VRayProxyRefKey &options) const
{
	const int animStart  = options.animOverride ? options.animStart : 0;

	int animLength = options.animOverride ? options.animLength : 0;
	if (animLength <= 0) {
		animLength = VUtils::Max(meshFile->getNumFrames(), 1);
	}

	return VUtils::fast_ceil(calcFrameIndex(options.f,
	                                        static_cast<VUtils::MeshFileAnimType::Enum>(options.animType),
	                                        animStart,
	                                        animLength,
	                                        options.animOffset,
	                                        options.animSpeed));
}

GU_DetailHandle VRayProxyCache::addDetail(const VRayProxyRefKey &options)
{
	vassert(meshFile);

	GU_DetailHandle gdpHandle;

	const int frameIndex = getFrame(options);
	meshFile->setCurrentFrame(frameIndex);

	GU_Detail *gdp = new GU_Detail;

	if (options.lod == VRayProxyPreviewType::bbox) {
		addBoundingBoxData(*gdp, *meshFile);
	}
	else {
		for (int voxelIndex = 0; voxelIndex < meshFile->getNumVoxels(); ++voxelIndex) {
			const uint32 voxelFlags = meshFile->getVoxelFlags(voxelIndex);
			if (options.lod == VRayProxyPreviewType::preview) {
				if (voxelFlags & MVF_PREVIEW_VOXEL) {
					addVoxelData(*gdp, *meshFile, voxelIndex, options.previewFaces, true);
					break;
				}
			}
			else if (options.lod == VRayProxyPreviewType::full) {
				addVoxelData(*gdp, *meshFile, voxelIndex);
			}
		}
	}

	gdpHandle.allocateAndSet(gdp);

	GU_Detail *gdpPacked = new GU_Detail;
	GU_PackedGeometry::packGeometry(*gdpPacked, gdpHandle);

	gdpHandle.allocateAndSet(gdpPacked);

	return gdpHandle;
}


GU_DetailHandle VRayProxyCache::getDetail(const VRayProxyRefKey &options)
{
	if (!meshFile)
		return GU_DetailHandle();

	return addDetail(options);
}

int VRayProxyCache::getBBox(UT_BoundingBox &box) {
	if (!meshFile) {
		return false;
	}

	const VUtils::Box &bbox = getBoundingBox(*meshFile);

	const VUtils::Vector &boxMin = bbox.pmin;
	const VUtils::Vector &boxMax = bbox.pmax;

	box.setBounds(boxMin.x, boxMin.y, boxMin.z, boxMax.x, boxMax.y, boxMax.z);

	return true;
}
