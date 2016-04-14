//
// Copyright (c) 2015-2016, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#include "vfh_vrayproxyutils.h"

#include <GU/GU_Detail.h>
#include <GU/GU_DetailHandle.h>
#include <GU/GU_PackedGeometry.h>
#include <GU/GU_PrimPoly.h>
#include <HOM/HOM_Vector2.h>
#include <HOM/HOM_BaseKeyframe.h>
#include <HOM/HOM_playbar.h>
#include <HOM/HOM_Module.h>
#include <UT/UT_Version.h>


using namespace VRayForHoudini;


namespace {


enum DataError {
	DE_INVALID_GEOM = 1,
	DE_NO_GEOM,
	DE_INVALID_FILE
};


static UT_Lock           theLock;
static VRayProxyCacheMan theCacheMan;

}


VRayProxyCacheMan& VRayForHoudini::GetVRayProxyCacheManager()
{
	return theCacheMan;
}


GU_ConstDetailHandle VRayForHoudini::GetVRayProxyDetail(const VRayProxyParms &options)
{
	UT_StringHolder utfilepath = options.getFilepath();
	if (NOT(utfilepath.isstring())) {
		return GU_ConstDetailHandle();
	}

	const std::string filepath(utfilepath);

	UT_AutoLock lock(theLock);

	if (NOT(theCacheMan.contains(filepath))) {
		// NOTE: insert entry in the cache
		VRayProxyCache &cache = theCacheMan[filepath];
		VUtils::ErrorCode errCode = cache.init(filepath.c_str());
		if (errCode.error()) {
			theCacheMan.erase(filepath);
			return GU_ConstDetailHandle();
		}
	}

	VRayProxyCache &cache = theCacheMan[filepath];
	return cache.getDetail(options);
}


bool VRayForHoudini::GetVRayProxyBounds(const VRayProxyParms &options, UT_BoundingBox &box)
{
	UT_StringHolder utfilepath = options.getFilepath();
	if (NOT(utfilepath.isstring())) {
		return false;
	}

	const std::string filepath(utfilepath);

	UT_AutoLock lock(theLock);

	if (NOT(theCacheMan.contains(filepath))) {
		return false;
	}

	VRayProxyCache &cache = theCacheMan[filepath];
	return cache.getBounds(options, box);
}


const UT_StringRef VRayProxyParms::theFileToken         = "file";
const UT_StringRef VRayProxyParms::theLODToken          = "lod";
const UT_StringRef VRayProxyParms::theFrameToken        = "frame";
const UT_StringRef VRayProxyParms::theAnimTypeToken     = "anim_type";
const UT_StringRef VRayProxyParms::theAnimOffsetToken   = "anim_offset";
const UT_StringRef VRayProxyParms::theAnimSpeedToken    = "anim_speed";
const UT_StringRef VRayProxyParms::theAnimOverrideToken = "anim_override";
const UT_StringRef VRayProxyParms::theAnimStartToken    = "anim_start";
const UT_StringRef VRayProxyParms::theAnimLengthToken   = "anim_length";
const UT_StringRef VRayProxyParms::theScaleToken        = "scale";
const UT_StringRef VRayProxyParms::theFlipAxisToken     = "flip_axis";


VRayProxyCache::GeometryHash::result_type VRayProxyCache::GeometryHash::operator()(const argument_type &val) const
{
	VoxelType voxelType = val.first;
	VUtils::MeshVoxel *voxel = val.second;
	Hash::MHash hashKey = 0;

	if (voxel) {
		VUtils::MeshChannel *channel = nullptr;

		if ( voxelType & MVF_GEOMETRY_VOXEL ) {
			channel = voxel->getChannel(VERT_GEOM_CHANNEL);
		} else if ( voxelType & MVF_HAIR_GEOMETRY_VOXEL ) {
			channel = voxel->getChannel(HAIR_VERT_CHANNEL);
		}

		if (channel && channel->data) {
			const int len = channel->elementSize * channel->numElements;
			const uint32 seed = (len < sizeof(uint32))? 0 : *(reinterpret_cast<uint32 *>(channel->data));
			Hash::MurmurHash3_x86_32(channel->data, len, seed, &hashKey);
		}
	}

	return hashKey;
}


VRayProxyCache::VRayProxyCache():
	m_proxy(nullptr),
	m_frameCache(new FrameCache()),
	m_detailCache(new DetailCache())
{
	m_frameCache->setEvictCallback(FrameCache::CbEvict(boost::bind(&VRayProxyCache::evictFrame, this, _1, _2)));
}


VRayProxyCache::VRayProxyCache(VRayProxyCache&& other)
{
	m_proxy = other.m_proxy;
	m_filepath = other.m_filepath;
	m_frameCache = other.m_frameCache;
	m_detailCache = other.m_detailCache;
	m_frameCache->setEvictCallback(FrameCache::CbEvict(boost::bind(&VRayProxyCache::evictFrame, this, _1, _2)));

	other.m_proxy = nullptr;
	other.m_filepath = "";
	other.m_frameCache = std::shared_ptr<FrameCache>();
	other.m_detailCache = std::shared_ptr<DetailCache>();
}


VRayProxyCache& VRayProxyCache::operator=(VRayProxyCache&& other)
{
	if (this == &other) {
		return *this;
	}

	m_proxy = other.m_proxy;
	m_filepath = other.m_filepath;
	m_frameCache = other.m_frameCache;
	m_detailCache = other.m_detailCache;
	m_frameCache->setEvictCallback(FrameCache::CbEvict(boost::bind(&VRayProxyCache::evictFrame, this, _1, _2)));

	other.m_proxy = nullptr;
	other.m_filepath = "";
	other.m_frameCache = std::shared_ptr<FrameCache>();
	other.m_detailCache = std::shared_ptr<DetailCache>();
	return *this;
}


VRayProxyCache::~VRayProxyCache()
{
	reset();
}


VUtils::ErrorCode VRayProxyCache::init(const VUtils::CharString &filepath)
{
	VUtils::ErrorCode res;

	// cleanup
	reset();

	// init file
	VUtils::CharString path(filepath);
	if (VUtils::bmpCheckAssetPath(path, NULL, NULL, false)) {
		VUtils::MeshFile *proxy = VUtils::newDefaultMeshFile(filepath.ptr());
		if (proxy) {
			if (proxy->init(filepath.ptr())) {
				m_proxy = proxy;
				m_filepath = filepath;
			} else {
				VUtils::deleteDefaultMeshFile(proxy);
				res.setError(__FUNCTION__, DE_INVALID_FILE, "File initialization error.");
			}
		} else {
			res.setError(__FUNCTION__, DE_INVALID_FILE, "File instantiation error.");
		}
	} else {
		res.setError(__FUNCTION__, DE_INVALID_FILE, "Invalid file path.");
	}

	if (res.error()) {
		return res;
	}

	UT_ASSERT( m_proxy );

	// init cache
	HOM_Module &hou = HOM();
	HOM_Vector2 *animRange = hou.playbar().playbackRange();
	int nFrames = 1;
	if (animRange) {
		HOM_Vector2 &range = *animRange;
		nFrames = range[1] - range[0];
	} else {
		nFrames = std::max(m_proxy->getNumFrames(), 1);
	}

	m_frameCache->setCapacity(nFrames);
	m_detailCache->setCapacity(nFrames);

	return res;
}


void VRayProxyCache::reset()
{
	clearCache();
	if (m_proxy) {
		VUtils::deleteDefaultMeshFile(m_proxy);
	}
	m_proxy = nullptr;
	m_filepath = "";
}


void VRayProxyCache::clearCache()
{
	if (m_frameCache) {
		m_frameCache->clear();
	}
	if (m_detailCache) {
		m_detailCache->clear();
	}
}


int VRayProxyCache::isCached(const VRayProxyParms &options) const
{
	if (NOT(m_proxy)) {
		return false;
	}

	FrameKey frameIdx = getFrameIdx(options);
	LOD lod = UTverify_cast< LOD >(options.getLOD());

	return contains(frameIdx, lod);
}


bool VRayProxyCache::getBounds(const VRayProxyParms &options, UT_BoundingBox &box) const
{
	if (NOT(m_proxy)) {
		return false;
	}

	FrameKey frameIdx = getFrameIdx(options);
	if (NOT(m_frameCache->contains(frameIdx))) {
		return false;
	}

	CachedFrame &frameData = (*m_frameCache)[frameIdx];
	box.initBounds(frameData.m_bbox.pmin.x, frameData.m_bbox.pmin.y, frameData.m_bbox.pmin.z);
	box.enlargeBounds(frameData.m_bbox.pmax.x, frameData.m_bbox.pmax.y, frameData.m_bbox.pmax.z);
	return true;
}


GU_ConstDetailHandle VRayProxyCache::getDetail(const VRayProxyParms &options)
{
	if (NOT(m_proxy)) {
		return GU_ConstDetailHandle();
	}

	FrameKey frameIdx = getFrameIdx(options);
	LOD lod = UTverify_cast< LOD >(options.getLOD());

	if (NOT(contains(frameIdx, lod))) {
		// if not in cache load and cache corresponding geometry
		cache(frameIdx, lod);
	}

	if (m_frameCache->contains(frameIdx)) {
		CachedFrame &frameData = (*m_frameCache)[frameIdx];
		if ( frameData.hasDetailKey(lod)) {
			const DetailKey &key = frameData.getDetailKey(lod);
			if (m_detailCache->contains(key)) {
				CachedDetail &itemData = (*m_detailCache)[ key ];
				return GU_ConstDetailHandle(itemData.m_item);
			}
		}
	}

	return GU_ConstDetailHandle();
}


bool VRayProxyCache::cache(const FrameKey &frameIdx, const LOD &lod)
{
	bool res = false;

	if ( m_proxy->getNumFrames() ) {
		m_proxy->setCurrentFrame(frameIdx);
	}

	const int numVoxels = m_proxy->getNumVoxels();
	std::vector< VUtils::MeshVoxel* > voxels;
	std::vector< Geometry > geometry;
	switch (lod) {
		case LOD_BBOX:
		{
			CachedFrame &frameData = (*m_frameCache)[frameIdx];
			frameData.m_bbox = m_proxy->getBBox();

			GU_Detail *gdp = new GU_Detail();
			createBBoxGeometry(frameIdx, *gdp);

			DetailKey key = gdp->getUniqueId();
			UT_ASSERT( NOT(m_detailCache->contains(key)) );

			CachedDetail &itemData = (*m_detailCache)[key];
			UT_ASSERT( NOT(itemData.m_item.isValid()) );
			UT_ASSERT( itemData.m_refCnt == 0 );

			itemData.m_item.allocateAndSet(gdp);
			itemData.m_refCnt = 1;
			frameData.setDetailKey(lod, key);

			res = true;
			break;
		}
		case LOD_PREVIEW:
		{
			voxels.reserve(1);
			geometry.reserve(2);
			// last voxel in file is reserved for preview geometry
			VUtils::MeshVoxel *voxel = getVoxel(frameIdx, numVoxels - 1);
			if (voxel) {
				voxels.push_back(voxel);
				getPreviewGeometry(*voxel, geometry);
			}
			break;
		}
		case LOD_FULL:
		{
			voxels.reserve(numVoxels);
			geometry.reserve(2 * numVoxels);
			// last voxel in file is reserved for preview geometry, so skip it
			for (int i = 0; i < numVoxels - 1; ++i) {
				VUtils::MeshVoxel *voxel = getVoxel(frameIdx, i);
				if (voxel) {
					voxels.push_back(voxel);
					getPreviewGeometry(*voxel, geometry);
				}
			}
			break;
		}
		default:
			break;
	}

	if (geometry.size()) {
		res = insert(frameIdx, lod, geometry);
	}

	for (VUtils::MeshVoxel *voxel : voxels) {
		m_proxy->releaseVoxel(voxel);
	}

	return res;
}



bool VRayProxyCache::contains(const FrameKey &frameIdx, const LOD &lod) const
{
	if (NOT(m_frameCache->contains(frameIdx))) {
		return false;
	}

	CachedFrame &frameData = (*m_frameCache)[frameIdx];
	if (NOT(frameData.hasDetailKey(lod))) {
		return false;
	}

	int inCache = m_detailCache->contains(frameData.getDetailKey(lod));
	if (NOT(inCache)) {
		// detail is evicted from item cache need to update the frame cache
		frameData.eraseDetailKey(lod);
	}

	return inCache;
}


bool VRayProxyCache::insert(const FrameKey &frameIdx, const LOD &lod, const std::vector<Geometry> &geometry)
{
	UT_ASSERT( NOT(contains(frameIdx, lod)) );

	CachedFrame &frameData = (*m_frameCache)[frameIdx];
	frameData.m_bbox = m_proxy->getBBox();

	GU_Detail *gdp = new GU_Detail();
	DetailKey key = gdp->getUniqueId();
	UT_ASSERT( NOT(m_detailCache->contains(key)) );

	CachedDetail &itemData = (*m_detailCache)[key];
	UT_ASSERT( NOT(itemData.m_item.isValid()) );
	UT_ASSERT( itemData.m_refCnt == 0 );

	for (int i = 0; i < geometry.size(); ++i) {
		const Geometry &geom = geometry[i];
		createProxyGeometry(geom, *gdp);
	}

	itemData.m_item.allocateAndSet(gdp);
	itemData.m_refCnt = 1;

	frameData.setDetailKey(lod, key);


//	// insert new item in frameCache
//	CachedFrame &frameData = (*m_frameCache)[frameIdx];
//	frameData.m_bbox = m_proxy->getBBox();

//	HashKeys &itemKeys = frameData.getHashKeys(lod);
//	itemKeys.resize(geometry.size());

//	GU_Detail *gdp = new GU_Detail();
//	DetailKey key = gdp->getUniqueId();
//	UT_ASSERT( NOT(m_detailCache->contains(key)) );

//	CachedDetail &itemData = (*m_detailCache)[key];
//	UT_ASSERT( NOT(itemData.m_item.isValid()) );
//	UT_ASSERT( itemData.m_refCnt == 0 );

//	// cache each geometry type individually
//	GeometryHash hasher;
//	for (int i = 0; i < geometry.size(); ++i) {
//		const Geometry &geom = geometry[i];
//		HashKey itemKey = hasher(geom);
//		itemKeys[i] = itemKey;

//		bool inCache = m_detailCache->contains(itemKey);
//		CachedDetail &geomData = (*m_detailCache)[itemKey];
//		if (inCache) {
//			// in itemCache only increase ref count
//			++geomData.m_refCnt;
//		} else {
//			// not in itemCache insert as new item and init ref count to 1
//			GU_Detail *voxelgdp = new GU_Detail();
//			createProxyGeometry(geom, *voxelgdp);
//			geomData.m_item.allocateAndSet(voxelgdp);
//			geomData.m_refCnt = 1;
//		}

//		GU_PackedGeometry::packGeometry(*gdp, geomData.m_item);
//	}

//	itemData.m_item.allocateAndSet(gdp);
//	itemData.m_refCnt = 1;

//	frameData.setDetailKey(lod, key);

	return true;
}


bool VRayProxyCache::erase(const FrameKey &frameIdx)
{
	if (NOT(m_frameCache->contains(frameIdx))) {
		return false;
	}

	CachedFrame &frameData = (*m_frameCache)[frameIdx];
	evictFrame(frameIdx, frameData);

	return m_frameCache->erase(frameIdx);
}


void VRayProxyCache::evictFrame(const FrameKey &frameIdx, CachedFrame &frameData)
{
	for (const auto &keyPair : frameData.m_lodToDetail) {
		const LOD &lod = keyPair.first;
		const DetailKey &detailKey = keyPair.second;

		if (m_detailCache->contains(detailKey)) {
			CachedDetail& itemData = (*m_detailCache)[detailKey];
			--itemData.m_refCnt;
			if (itemData.m_refCnt <= 0) {
				m_detailCache->erase(detailKey);
			}
		}
	}
}


VRayProxyCache::FrameKey VRayProxyCache::getFrameIdx(const VRayProxyParms &options) const
{
	UT_ASSERT( m_proxy );

	const fpreal64 frame      = options.getFloatFrame();
	const exint    animType   = options.getAnimType();
	const fpreal64 animOffset = options.getAnimOffset();
	const fpreal64 animSpeed  = options.getAnimSpeed();

	const bool  animOverride = options.getAnimOverride();
	const exint animStart  = (animOverride)? options.getAnimStart() : 0;
	exint       animLength = (animOverride)? options.getAnimLength() : 0;
	if (animLength <= 0) {
		animLength = std::max(m_proxy->getNumFrames(), 1);
	}


	return static_cast<FrameKey>(VUtils::fast_round((VUtils::calcFrameIndex(frame,
														static_cast<VUtils::MeshFileAnimType::Enum>(animType),
														animStart,
														animLength,
														animOffset,
														animSpeed))));
}


VUtils::MeshVoxel* VRayProxyCache::getVoxel(const FrameKey &frameKey, int voxelIdx) const
{
	UT_ASSERT( m_proxy );

	VUtils::MeshVoxel *voxel = nullptr;
	if (   voxelIdx < 0
		|| voxelIdx >= m_proxy->getNumVoxels() )
	{
		return voxel;
	}

	voxel = m_proxy->getVoxel(voxelIdx);
	return voxel;
}


void VRayProxyCache::getPreviewGeometry(VUtils::MeshVoxel &voxel, std::vector<Geometry> &geometry) const
{
	VUtils::MeshChannel *verts_ch = voxel.getChannel(VERT_GEOM_CHANNEL);
	if (verts_ch && verts_ch->data) {
		geometry.emplace_back(MVF_GEOMETRY_VOXEL, &voxel);
	}

	verts_ch = voxel.getChannel(HAIR_VERT_CHANNEL);
	if (verts_ch && verts_ch->data) {
		geometry.emplace_back(MVF_HAIR_GEOMETRY_VOXEL, &voxel);
	}
}


bool VRayProxyCache::createProxyGeometry(const Geometry &geom, GU_Detail &gdp) const
{
	VoxelType voxelType = geom.first;
	VUtils::MeshVoxel *voxel = geom.second;
	if (voxel) {
		if (voxelType & MVF_GEOMETRY_VOXEL) {
			return createMeshProxyGeometry(*voxel, gdp);

		} else if (voxelType & MVF_HAIR_GEOMETRY_VOXEL) {
			return createHairProxyGeometry(*voxel, gdp);
		}
	}

	return false;
}


bool VRayProxyCache::createMeshProxyGeometry(VUtils::MeshVoxel &voxel, GU_Detail &gdp) const
{
	VUtils::MeshChannel *verts_ch = voxel.getChannel(VERT_GEOM_CHANNEL);
	VUtils::MeshChannel *faces_ch = voxel.getChannel(FACE_TOPO_CHANNEL);
	if ( NOT(verts_ch) || NOT(faces_ch) ) {
		gdp.addWarning(GU_WARNING_NO_METAOBJECTS, "Found invalid mesh voxel!");
		return false;
	}

	VUtils::VertGeomData *verts = reinterpret_cast<VUtils::VertGeomData *>(verts_ch->data);
	VUtils::FaceTopoData *faces = reinterpret_cast<VUtils::FaceTopoData *>(faces_ch->data);
	if (NOT(verts) || NOT(faces)) {
		gdp.addWarning(GU_WARNING_NO_METAOBJECTS, "Found invalid mesh voxel data!");
		return false;
	}

	int numVerts = verts_ch->numElements;
	int numFaces = faces_ch->numElements;

	// Points
	GA_Offset voffset = gdp.appendPointBlock(numVerts);
	for (int v = 0; v < numVerts; ++v) {
		VUtils::Vector &vert = verts[v];
		GA_Offset pointOffs = voffset + v;

#if UT_MAJOR_VERSION_INT < 14
		GEO_Point *point = gdp.getGEOPoint(pointOffs);
		point->setPos(UT_Vector4F(vert.x, vert.y, vert.z));
#else
		gdp.setPos3(pointOffs, UT_Vector3F(vert.x, vert.y, vert.z));
#endif
	}

	// Faces
	for (int f = 0; f < numFaces; ++f) {
		const VUtils::FaceTopoData &face = faces[f];

		GU_PrimPoly *poly = GU_PrimPoly::build(&gdp, 3, GU_POLY_CLOSED, 0);
		for (int c = 0; c < 3; ++c) {
			poly->setVertexPoint(c, voffset + face.v[c]);
		}

		poly->reverse();
	}

	return true;
}


bool VRayProxyCache::createHairProxyGeometry(VUtils::MeshVoxel &voxel, GU_Detail &gdp) const
{
	VUtils::MeshChannel *verts_ch = voxel.getChannel(HAIR_VERT_CHANNEL);
	VUtils::MeshChannel *strands_ch = voxel.getChannel(HAIR_NUM_VERT_CHANNEL);
	if ( NOT(verts_ch) || NOT(strands_ch) ) {
		gdp.addWarning(GU_WARNING_NO_METAOBJECTS, "Found invalid hair voxel!");
		return false;
	}

	VUtils::VertGeomData *verts = reinterpret_cast< VUtils::VertGeomData* >(verts_ch->data);
	int *strands = reinterpret_cast< int* >(strands_ch->data);
	if (NOT(verts) || NOT(strands)) {
		gdp.addWarning(GU_WARNING_NO_METAOBJECTS, "Found invalid hair voxel data!");
		return false;
	}

	int numVerts = verts_ch->numElements;
	int numStrands = strands_ch->numElements;

	// Points
	GA_Offset voffset = gdp.appendPointBlock(numVerts);
	for (int v = 0; v < numVerts; ++v) {
		VUtils::Vector &vert = verts[v];
		GA_Offset pointOffs = voffset + v;

#if UT_MAJOR_VERSION_INT < 14
		GEO_Point *point = gdp.getGEOPoint(pointOffs);
		point->setPos(UT_Vector4F(vert.x, vert.y, vert.z));
#else
		gdp.setPos3(pointOffs, UT_Vector3F(vert.x, vert.y, vert.z));
#endif
	}

	// Strands
	for (int i = 0; i < numStrands; ++i) {
		int &vertsPerStrand = strands[i];

		GU_PrimPoly *poly = GU_PrimPoly::build(&gdp, vertsPerStrand, GU_POLY_OPEN, 0);
		for (int j = 0; j < vertsPerStrand; ++j) {
			poly->setVertexPoint(j, voffset + j);
		}

		voffset += vertsPerStrand;
	}

	return true;
}


bool VRayProxyCache::createBBoxGeometry(const FrameKey &frameIdx, GU_Detail &gdp) const
{
	VUtils::Box bbox= m_proxy->getBBox();
	const VUtils::Vector &bboxMin = bbox.c(0);
	const VUtils::Vector &bboxMax = bbox.c(1);
	gdp.cube(bboxMin[0], bboxMax[0],
			bboxMin[1], bboxMax[1],
			bboxMin[2], bboxMax[2]);

	return true;
}
