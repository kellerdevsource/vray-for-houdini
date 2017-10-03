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
		// insert entry in cache
		VRayProxyCache &cache = theCacheMan[filepath];
		// try init
		VUtils::ErrorCode errCode = cache.init(filepath.c_str());
		if (errCode.error()) {
			// if we've failed to init delete the entry we've just created
			theCacheMan.erase(filepath);
			return GU_ConstDetailHandle();
		}
	}

	VRayProxyCache &cache = theCacheMan[filepath];
	return cache.getDetail(options);
}


bool VRayForHoudini::ClearVRayProxyCache(const UT_String &filepath)
{
	const std::string sfilepath = filepath.toStdString();

	UT_AutoLock lock(theLock);

	VRayProxyCacheMan &theCacheMan = GetVRayProxyCacheManager();
	if (NOT(theCacheMan.contains(sfilepath))) {
		return false;
	}

	VRayProxyCache &fileCache = theCacheMan[sfilepath];
	fileCache.clearCache();
	theCacheMan.erase(sfilepath);
	return true;
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
		// if we don't have cache for this file just return false
		return false;
	}

	VRayProxyCache &cache = theCacheMan[filepath];
	// NOTE: this will only work correcly if we've queried the detail first
	return cache.getBounds(options, box);
}


const UT_StringRef VRayProxyParms::theFileToken         = "file";
const UT_StringRef VRayProxyParms::theLODToken          = "lod";
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

		// TODO: add particles/instances

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
	m_detailCache(new DetailCache()),
	m_voxelToDetail(new VoxelToDetailMap())

{
	m_frameCache->setEvictCallback(FrameCache::CbEvict(boost::bind(&VRayProxyCache::evictFrame, this, _1, _2)));
}


VRayProxyCache::VRayProxyCache(VRayProxyCache&& other)
{
	m_proxy = other.m_proxy;
	m_filepath = other.m_filepath;
	m_frameCache = other.m_frameCache;
	m_detailCache = other.m_detailCache;
	m_voxelToDetail = other.m_voxelToDetail;
	m_frameCache->setEvictCallback(FrameCache::CbEvict(boost::bind(&VRayProxyCache::evictFrame, this, _1, _2)));

	other.m_proxy = nullptr;
	other.m_filepath = "";
	other.m_frameCache = std::shared_ptr<FrameCache>();
	other.m_detailCache = std::shared_ptr<DetailCache>();
	other.m_voxelToDetail = std::shared_ptr<VoxelToDetailMap>();
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
	m_voxelToDetail = other.m_voxelToDetail;
	m_frameCache->setEvictCallback(FrameCache::CbEvict(boost::bind(&VRayProxyCache::evictFrame, this, _1, _2)));

	other.m_proxy = nullptr;
	other.m_filepath = "";
	other.m_frameCache = std::shared_ptr<FrameCache>();
	other.m_detailCache = std::shared_ptr<DetailCache>();
	other.m_voxelToDetail = std::shared_ptr<VoxelToDetailMap>();
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
		// error occured while initilizing the mesh file
		return res;
	}

	UT_ASSERT( m_proxy );

	// init cache capacity
	HOM_Module &hou = HOM();
	HOM_Vector2 *animRange = hou.playbar().playbackRange();
	int nFrames = 1;
	if (animRange) {
		HOM_Vector2 &range = *animRange;
		nFrames = range[1] - range[0];
	}
	nFrames = std::min(std::max(m_proxy->getNumFrames(), 1), nFrames);

	m_frameCache->setCapacity(nFrames);
	// TODO: need to set item cache capacity to a reasonable value
	//       when switching bewtween preview or full geometry
	//       item cache might be too big for preview geometry
	//       or too small for full geometry to hold all voxels
	m_detailCache->setCapacity(nFrames * (m_proxy->getNumVoxels() + 1));

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
	if (m_voxelToDetail) {
		m_voxelToDetail->clear();
	}
}


int VRayProxyCache::isCached(const VRayProxyParms &options)
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
				GU_DetailHandle &gdh = (*m_detailCache)[ key ];
				return GU_ConstDetailHandle(gdh);
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
			// only load and show bbox of the geometry
			CachedFrame &frameData = (*m_frameCache)[frameIdx];
			frameData.m_bbox = m_proxy->getBBox();

			GU_Detail *gdp = new GU_Detail();
			createBBoxGeometry(frameData.m_bbox, *gdp);

			DetailKey key = gdp->getUniqueId();
			UT_ASSERT( NOT(m_detailCache->contains(key)) );

			// cache the detail
			GU_DetailHandle &gdh = (*m_detailCache)[key];
			UT_ASSERT( NOT(gdh.isValid()) );

			gdh.allocateAndSet(gdp);
			frameData.setDetailKey(lod, key);

			res = true;
			break;
		}
		case LOD_PREVIEW:
		{
			// load geometry from the preview voxel
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
			// load geometry from all voxels except preview
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
		// we have some geometry to instert in cache
		res = insert(frameIdx, lod, geometry);
	}

	for (VUtils::MeshVoxel *voxel : voxels) {
		// release allocated voxels
		m_proxy->releaseVoxel(voxel);
	}

	return res;
}



bool VRayProxyCache::contains(const FrameKey &frameIdx, const LOD &lod)
{
	if (NOT(m_frameCache->contains(frameIdx))) {
		return false;
	}

	CachedFrame &frameData = (*m_frameCache)[frameIdx];
	if (NOT(frameData.hasDetailKey(lod))) {
		return false;
	}

	// geometry for this frame is considered cached if all details created
	// for it are in the geo cache i.e. details for each voxel if lod == LOD_FULL
	// and/or details for mesh/hair/particles
	int inCache = m_detailCache->contains(frameData.getDetailKey(lod));
	if (NOT(inCache)) {
		// detail is evicted from detail cache need to update the frame cache
		if (frameData.hasVoxelKeys(lod)) {
			updateDetailCacheForKeys(frameData.getVoxelKeys(lod));
		}
		frameData.eraseLOD(lod);
	}

	return inCache;
}


bool VRayProxyCache::insert(const FrameKey &frameIdx, const LOD &lod, const std::vector<Geometry> &geometry)
{
	UT_ASSERT( NOT(contains(frameIdx, lod)) );

//	CachedFrame &frameData = (*m_frameCache)[frameIdx];
//	frameData.m_bbox = m_proxy->getBBox();

//	GU_Detail *gdp = new GU_Detail();
//	DetailKey key = gdp->getUniqueId();
//	UT_ASSERT( NOT(m_detailCache->contains(key)) );

//	for (int i = 0; i < geometry.size(); ++i) {
//		const Geometry &geom = geometry[i];
//		createProxyGeometry(geom, *gdp);
//	}

//	GU_DetailHandle &gdh = (*m_detailCache)[key];
//	UT_ASSERT( NOT(gdh.isValid()) );

//	gdh.allocateAndSet(gdp);
//	frameData.setDetailKey(lod, key);


	// insert new item in frameCache and cache bbox
	CachedFrame &frameData = (*m_frameCache)[frameIdx];
	frameData.m_bbox = m_proxy->getBBox();

	HashKeys &voxelKeys = frameData.getVoxelKeys(lod);
	voxelKeys.resize(geometry.size());

	// create main detail for the frame
	GU_Detail *gdp = new GU_Detail();
	DetailKey key = gdp->getUniqueId();
	UT_ASSERT( NOT(m_detailCache->contains(key)) );

	// cache each geometry type as individual detail
	GeometryHash hasher;
	for (int i = 0; i < geometry.size(); ++i) {
		const Geometry &geom = geometry[i];
		HashKey geomHash = hasher(geom);
		voxelKeys[i] = geomHash;

		bool inCache = false;
		DetailKey detailKey = -1;
		if (m_voxelToDetail->count(geomHash)) {
			CachedDetail &detailData = m_voxelToDetail->at(geomHash);
			if (m_detailCache->contains(detailData.m_detailKey)) {
				// geometry is already in geo cache so increase ref count
				++detailData.m_refCnt;
				detailKey = detailData.m_detailKey;
				inCache = true;
			}
		}

		if (NOT(inCache)) {
			// geometry is NOT in geo cache so insert as new item and init ref count to 1
			GU_Detail *voxelgdp = new GU_Detail();
			detailKey = voxelgdp->getUniqueId();
			UT_ASSERT( NOT(m_detailCache->contains(detailKey)) );

			createProxyGeometry(geom, *voxelgdp);

			GU_DetailHandle &voxelGdh = (*m_detailCache)[detailKey];
			UT_ASSERT( NOT(voxelGdh.isValid()) );

			voxelGdh.allocateAndSet(voxelgdp);

			CachedDetail &detailData = (*m_voxelToDetail)[geomHash];
			detailData.m_detailKey = detailKey;
			detailData.m_refCnt = 1;
		}

		GU_DetailHandle &voxelGdh = (*m_detailCache)[detailKey];
		UT_ASSERT( voxelGdh.isValid() );

		// now pack the voxel geo in the main detail for this frame
		GU_PackedGeometry::packGeometry(*gdp, voxelGdh);
	}

	// cache main detail for this frame
	GU_DetailHandle &gdh = (*m_detailCache)[key];
	UT_ASSERT( NOT(gdh.isValid()) );

	gdh.allocateAndSet(gdp);
	frameData.setDetailKey(lod, key);

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
	// we need to cleanup data for this frame
	// first erase main details for each lod
	for (const auto &keyPair : frameData.m_lodToDetail) {
		const LOD &lod = keyPair.first;
		const DetailKey &detailKey = keyPair.second;

		if (m_detailCache->contains(detailKey)) {
			m_detailCache->erase(detailKey);
		}
	}

	// second update ref counts on voxel details created for this frame
	for (const auto &keyPair : frameData.m_voxelkeys) {
		const LOD &lod = keyPair.first;
		const HashKeys &voxelKeys = keyPair.second;

		updateDetailCacheForKeys(voxelKeys);
	}
}


void VRayProxyCache::updateDetailCacheForKeys(const HashKeys &voxelKeys)
{
	// decrease ref counts on voxel details for the given keys
	// and if necessary remove them from the geo cache
	for (const auto &voxelKey : voxelKeys) {
		if (m_voxelToDetail->count(voxelKey)) {
			CachedDetail& detailData = m_voxelToDetail->at(voxelKey);
			--detailData.m_refCnt;

			if (   detailData.m_refCnt <= 0
				|| NOT(m_detailCache->contains(detailData.m_detailKey)) )
			{
				m_detailCache->erase(detailData.m_detailKey);
				m_voxelToDetail->erase(voxelKey);
			}
		}
	}
}


VRayProxyCache::FrameKey VRayProxyCache::getFrameIdx(const VRayProxyParms &options) const
{
	UT_ASSERT( m_proxy );

	// calc true .vrmesh frame in range [0, animation length] from proxy options
	const exint    animType   = options.getAnimType();
	const fpreal64 animOffset = options.getAnimOffset();
	const fpreal64 animSpeed  = options.getAnimSpeed();

	const bool  animOverride = options.getAnimOverride();
	const exint animStart  = (animOverride)? options.getAnimStart() : 0;
	exint       animLength = (animOverride)? options.getAnimLength() : 0;
	if (animLength <= 0) {
		animLength = std::max(m_proxy->getNumFrames(), 1);
	}


	return static_cast<FrameKey>(VUtils::fast_round((VUtils::calcFrameIndex(0,
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
		//TODO: particles/instances
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

		gdp.setPos3(pointOffs, UT_Vector3F(vert.x, vert.y, vert.z));
	}

	// Faces
	// TODO: may be it will be better using polysoups
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

		gdp.setPos3(pointOffs, UT_Vector3F(vert.x, vert.y, vert.z));
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


bool VRayProxyCache::createBBoxGeometry(const VUtils::Box &bbox, GU_Detail &gdp) const
{
	const VUtils::Vector &bboxMin = bbox.c(0);
	const VUtils::Vector &bboxMax = bbox.c(1);
	gdp.cube(bboxMin[0], bboxMax[0],
			bboxMin[1], bboxMax[1],
			bboxMin[2], bboxMax[2]);

	return true;
}
