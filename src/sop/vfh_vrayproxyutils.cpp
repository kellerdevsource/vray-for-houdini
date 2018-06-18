//
// Copyright (c) 2015-2018, Chaos Software Ltd
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

#include <vutils_bitset.h>

using namespace VRayForHoudini;

uint32 VRayProxyRefKey::hash() const
{
	if (keyHash)
		return keyHash;

#pragma pack(push, 1)
	const struct VRayProxyRefHashKey {
		uint32 filePath;
		uint32 objectPath;
		int objectType;
		int lod;
		int f;
		int animType;
		int animOffset;
		int animSpeed;
		int animOverride;
		int animStart;
		int animLength;
		int previewFaces;
	} vrayProxyRefHashKey = {
		Hash::hashLittle(filePath),
		Hash::hashLittle(objectPath),
		static_cast<int>(objectType),
		static_cast<int>(lod),
		VUtils::fast_floor(f * 1000.0),
		animType,
		VUtils::fast_floor(animOffset * 1000.0),
		VUtils::fast_floor(animSpeed * 1000.0),
		animOverride,
		animStart,
		animLength,
		previewFaces
	};
#pragma pack(pop)

	keyHash = Hash::hashLittle(vrayProxyRefHashKey);
	return keyHash;
}

VRayProxyObjectType VRayForHoudini::objectInfoIdToObjectType(int channelID)
{
	switch (channelID) {
		case OBJECT_INFO_CHANNEL:          return VRayProxyObjectType::geometry;
		case HAIR_OBJECT_INFO_CHANNEL:     return VRayProxyObjectType::hair;
		case PARTICLE_OBJECT_INFO_CHANNEL: return VRayProxyObjectType::particles;
		default: {
			vassert(false);
			return VRayProxyObjectType::none;
		}
	}
}

int VRayForHoudini::objectTypeToObjectInfoId(VRayProxyObjectType objectType)
{
	switch (objectType) {
		case VRayProxyObjectType::geometry:  return OBJECT_INFO_CHANNEL;
		case VRayProxyObjectType::hair:      return HAIR_OBJECT_INFO_CHANNEL;
		case VRayProxyObjectType::particles: return PARTICLE_OBJECT_INFO_CHANNEL;
		default: {
			vassert(false);
			return 0;
		}
	}
}

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
/// @param maxFaces Limit preview faces.
/// @param tm Additional transform.
static int addMeshVoxelData(GU_Detail &gdp, VUtils::MeshVoxel &voxel, const VUtils::Transform &tm, int maxFaces)
{
	const VUtils::MeshChannel *vertChan = voxel.getChannel(VERT_GEOM_CHANNEL);
	const VUtils::MeshChannel *faceChan = voxel.getChannel(FACE_TOPO_CHANNEL);
	if (!vertChan || !faceChan) {
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

	if (maxFaces == 0 || numFaces <= maxFaces) {
		const GA_Offset pointOffset = gdp.appendPointBlock(numVerts);
		for (int vertexIndex = 0; vertexIndex < numVerts; ++vertexIndex) {
			const VUtils::Vector &vert = tm * verts[vertexIndex];
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
	else if (maxFaces != 0) {
		VUtils::VectorRefList previewVertsRaw(maxFaces * 3);
		VUtils::IntRefList previewFacesRaw(maxFaces * 3);

		int numPreviewFaces = 0;
		int numPreviewVerts = 0;

		const double ratio = double(maxFaces) / double(numFaces);

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
			const VUtils::Vector &vert = tm * previewVertsRaw[vertexIndex];
			gdp.setPos3(pointOffset + vertexIndex, UT_Vector3F(vert.x, vert.y, vert.z));
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
/// @param tm Additional transform.
static int addHairVoxelData(GU_Detail &gdp, VUtils::MeshVoxel &voxel, const VUtils::Transform &tm)
{
	VUtils::MeshChannel *vertChan = voxel.getChannel(HAIR_VERT_CHANNEL);
	VUtils::MeshChannel *strandChan = voxel.getChannel(HAIR_NUM_VERT_CHANNEL);
	if (!vertChan || !strandChan) {
		return false;
	}

	const VUtils::VertGeomData *verts = reinterpret_cast<VUtils::VertGeomData*>(vertChan->data);
	const int *strands = reinterpret_cast<int*>(strandChan->data);
	if (!verts || !strands) {
		gdp.addWarning(GU_WARNING_NO_METAOBJECTS, "Found invalid hair voxel data!");
		return false;
	}

	const int numVerts = vertChan->numElements;
	const int numStrands = strandChan->numElements;

	GA_Offset pointOffset = gdp.appendPointBlock(numVerts);
	for (int v = 0; v < numVerts; ++v) {
		const VUtils::Vector &vert = tm * verts[v];
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
/// @param voxelFlags Voxel flags.
/// @param tm Additional transform.
/// @param maxFaces Limit preview faces.
static int addVoxelData(GU_Detail &gdp, VUtils::MeshVoxel &voxel, uint32 voxelFlags, const VUtils::Transform &tm, int maxFaces)
{
	if (voxelFlags & MVF_GEOMETRY_VOXEL ||
	    voxelFlags & MVF_PREVIEW_VOXEL)
	{
		return addMeshVoxelData(gdp, voxel, tm, maxFaces);
	}
	if (voxelFlags & MVF_HAIR_GEOMETRY_VOXEL ||
		voxelFlags & MVF_PREVIEW_VOXEL)
	{
		return addHairVoxelData(gdp, voxel, tm);
	}
	return false;
}

static VUtils::Box getBoundingBox(VUtils::MeshInterface &mi)
{
	VUtils::Box bbox = mi.getBBox();

	if (bbox.isEmpty() || bbox.isInfinite()) {
		bbox.init();
		bbox += VUtils::Vector(1.0f, 1.0f, 1.0f);
		bbox += VUtils::Vector(-1.0f, -1.0f, -1.0f);
	}

	return bbox;
}

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

static int getFrame(VUtils::MeshFile &meshFile, const VRayProxyRefKey &options)
{
	const int animStart = options.animOverride ? options.animStart : 0;

	int animLength = options.animOverride ? options.animLength : 0;
	if (animLength <= 0) {
		animLength = VUtils::Max(meshFile.getNumFrames(), 1);
	}

	return VUtils::fast_ceil(calcFrameIndex(options.f,
	                                        static_cast<VUtils::MeshFileAnimType::Enum>(options.animType),
	                                        animStart,
	                                        animLength,
	                                        options.animOffset,
	                                        options.animSpeed));
}

static VRayProxyRefItem buildDetail(const VRayProxyRefKey &options)
{
	VUtils::CharString path(options.filePath);
	if (!VUtils::bmpCheckAssetPath(path, NULL, NULL, false)) {
		Log::getLog().error("VRayProxy: Can't find file \"%s\"", path.ptr());
		return VRayProxyRefItem();
	}

	VUtils::MeshFile *meshFile = VUtils::newDefaultMeshFile(path.ptr());
	if (!meshFile) {
		Log::getLog().error("VRayProxy: Can't open \"%s\"", path.ptr());
		return VRayProxyRefItem();
	}

	const VUtils::ErrorCode res = meshFile->init(path.ptr());
	if (res.error()) {
		Log::getLog().error("VRayProxy: Can't initialize \"%s\" [%s]!",
		                    path.ptr(), res.getErrorString().ptrOrValue("UNKNOWN"));
		return VRayProxyRefItem();
	}

	const int frameIndex = getFrame(*meshFile, options);
	meshFile->setCurrentFrame(frameIndex);

	GU_Detail *gdp = new GU_Detail;

	if (options.lod == VRayProxyPreviewType::bbox) {
		addBoundingBoxData(*gdp, *meshFile);
	}
	else {
		static const int previewFlags = MVF_PREVIEW_VOXEL;
		static const int geomFlags = MVF_GEOMETRY_VOXEL|MVF_HAIR_GEOMETRY_VOXEL|MVF_PARTICLE_GEOMETRY_VOXEL;

		const int numVoxels = meshFile->getNumVoxels();
		const int singleObjectMode = !options.objectPath.empty() &&
		                             options.objectType != VRayProxyObjectType::none;

		uint32 meshVoxelFlags = previewFlags;
		int maxPreviewFaces = options.previewFaces;

		if (singleObjectMode) {
			meshVoxelFlags = geomFlags;
			maxPreviewFaces = numVoxels == 1
				              ? maxPreviewFaces
				              : VUtils::fast_ceil(float(options.previewFaces) / float(numVoxels - 1));
		}
		else if (options.lod == VRayProxyPreviewType::full) {
			meshVoxelFlags = geomFlags;
			maxPreviewFaces = 0;
		}

		if (singleObjectMode) {
			const int channelID =
				objectTypeToObjectInfoId(options.objectType);

			VUtils::ObjectInfoChannelData objectInfo;
			if (readObjectInfoChannelData(meshFile, objectInfo, channelID)) {
				enum class VisibilityListType {
					exclude = 0,
					include = 1,
				};

				VUtils::Table<int> objIDs;
				VUtils::Table<VUtils::CharString> objNames(1, options.objectPath);
#if 0
				VUtils::BitSet previewVisibility;
				objectInfo.getVisibility(previewVisibility, objNames, objIDs, int(VisibilityListType::include), false);

				switch (options.objectType) {
					case VRayProxyObjectType::geometry:
						removeInvisiblePreviewFaces(&previewVoxel, previewVisibility);
						break;
					case VRayProxyObjectType::hair:
						removeInvisiblePreviewHairs(&previewVoxel, previewVisibility);
						break;
					case VRayProxyObjectType::particles:
						removeInvisiblePreviewParticles(&previewVoxel, previewVisibility);
						break;
					default:
						break;
				}
#endif
				VUtils::BitSet voxelVisibility;
				objectInfo.getVisibility(voxelVisibility, objNames, objIDs, int(VisibilityListType::include), true);

				meshFile->setUseFullNames(false);
				meshFile->setVoxelVisibility(voxelVisibility);
			}
		}

		for (int i = numVoxels - 1; i >= 0; i--) {
			const uint32 flags = meshFile->getVoxelFlags(i);

			if (flags & meshVoxelFlags && !(flags & MVF_HIDDEN_VOXEL)) {
				VUtils::MeshVoxel *voxel = meshFile->getVoxel(i);

				VUtils::Transform tm(1);
				voxel->getTM(tm);

				// If this voxel is an instance, replace it with the voxel with the real geometry.
				if (!VUtils::replaceInstanceVoxelWithSource(meshFile, voxel))
					continue;

				addVoxelData(*gdp, *voxel, flags, tm, maxPreviewFaces);

				meshFile->releaseVoxel(voxel);
			}
		}
	}

	VUtils::deleteDefaultMeshFile(meshFile);

	VRayProxyRefItem item;

	gdp->computeQuickBounds(item.bbox);

	item.detail.allocateAndSet(gdp);

	GU_Detail *gdpPacked = new GU_Detail;
	GU_PackedGeometry::packGeometry(*gdpPacked, item.detail);

	item.detail.allocateAndSet(gdpPacked);

	return item;
}

VRayProxyRefItem VRayForHoudini::getVRayProxyDetail(const VRayProxyRefKey &options)
{
	return buildDetail(options);
}
