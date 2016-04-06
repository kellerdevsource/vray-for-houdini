//
// Copyright (c) 2015, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#include "vfh_vrayproxycache.h"

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
	m_itemCache(new ItemCache())
{ m_frameCache->setEvictCallback(FrameCache::CbEvict(boost::bind(&VRayProxyCache::evictFrame, this, _1, _2))); }


VRayProxyCache::VRayProxyCache(VRayProxyCache&& other)
{
	m_proxy = other.m_proxy;
	m_filepath = other.m_filepath;
	m_frameCache = other.m_frameCache;
	m_itemCache = other.m_itemCache;
	m_frameCache->setEvictCallback(FrameCache::CbEvict(boost::bind(&VRayProxyCache::evictFrame, this, _1, _2)));

	other.m_proxy = nullptr;
	other.m_filepath = "";
	other.m_frameCache = std::shared_ptr<FrameCache>();
	other.m_itemCache = std::shared_ptr<ItemCache>();
}


VRayProxyCache& VRayProxyCache::operator=(VRayProxyCache&& other)
{
	if (this == &other) {
		return *this;
	}

	m_proxy = other.m_proxy;
	m_filepath = other.m_filepath;
	m_frameCache = other.m_frameCache;
	m_itemCache = other.m_itemCache;
	m_frameCache->setEvictCallback(FrameCache::CbEvict(boost::bind(&VRayProxyCache::evictFrame, this, _1, _2)));

	other.m_proxy = nullptr;
	other.m_filepath = "";
	other.m_frameCache = std::shared_ptr<FrameCache>();
	other.m_itemCache = std::shared_ptr<ItemCache>();
	return *this;
}


VRayProxyCache::~VRayProxyCache()
{ reset(); }


VUtils::ErrorCode VRayProxyCache::init(const VUtils::CharString &filepath)
{
	VUtils::ErrorCode res;

//		cleanup
	reset();

//		init file
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

//		init cache
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
	// TODO: need to set item cache capacity to a reasonable value
	//       when switching bewtween preview or full geometry
	//       item cache might be too big for preview geometry
	//       or too small for full geometry to hold all voxels
	m_itemCache->setCapacity(nFrames * m_proxy->getNumVoxels());

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
	if (m_itemCache) {
		m_itemCache->clear();
	}
}


int VRayProxyCache::checkFrameCached(const UT_Options &options) const
{
	if (NOT(m_proxy)) {
		return false;
	}

	FrameKey frameIdx = getFrameIdx(options);
	LOD lod = static_cast<LOD>(VRayProxyUtils::getLOD(options));

	return checkCached(frameIdx, lod);
}


VUtils::ErrorCode VRayProxyCache::getFrame(const UT_Options &options, GU_Detail &gdp)
{
	VUtils::ErrorCode res;

	if (NOT(m_proxy)) {
		res.setError(__FUNCTION__, DE_INVALID_FILE, "Invalid file path!");
		return res;
	}

	FrameKey frameIdx = getFrameIdx(options);
	LOD lod = static_cast<LOD>(VRayProxyUtils::getLOD(options));

//	if (lod == LOD_BBOX) {
//		createBBoxGeometry(frameIdx, gdp);
//		return res;
//	}

	const int numVoxels = m_proxy->getNumVoxels();
//		if not in cache load preview voxel and cache corresponding GU_Detail(s)
	if (NOT(checkCached(frameIdx, lod))) {
		std::vector< VUtils::MeshVoxel* > voxels;
		std::vector< Geometry > geometry;
		switch (lod) {
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
			insert(frameIdx, lod, geometry);
		}
		else {
			res.setError(__FUNCTION__, DE_NO_GEOM, "No geometry found for context #%0.3f.", VRayProxyUtils::getFloatFrame(options));
		}

		for (VUtils::MeshVoxel *voxel : voxels) {
			m_proxy->releaseVoxel(voxel);
		}
	}

	if (m_frameCache->contains(frameIdx)) {
		CachedFrame &frameData = (*m_frameCache)[frameIdx];
		if ( frameData.hasItemKeys(lod) ) {
			for (auto const &itemKey : frameData.getItemKeys(lod)) {
				ItemCache::iterator itemIt = m_itemCache->find(itemKey);
				UT_ASSERT( itemIt != m_itemCache->end() );

				CachedItem &itemData = *itemIt;
				GU_DetailHandle &gdpHndl = itemData.m_item;
				if (gdpHndl.isValid()) {
//						gdp.merge(*gdpHndl.peekDetail());
					GU_PackedGeometry::packGeometry(gdp, gdpHndl);
				} else {
					res.setError(__FUNCTION__, DE_INVALID_GEOM, "Invalid geometry found for context #%0.3f.", VRayProxyUtils::getFloatFrame(options));
				}
			}
		}
	}

	return res;
}


int VRayProxyCache::checkCached(const FrameKey &frameIdx, const LOD &lod) const
{
	if (NOT(m_frameCache->contains(frameIdx))) {
		return false;
	}

	CachedFrame &frameData = (*m_frameCache)[frameIdx];
	if (NOT(frameData.hasItemKeys(lod))) {
		return false;
	}

//		if in cache check if all items from the collection are cached
	int inCache = true;
	for (const auto &itemKey : frameData.getItemKeys(lod)) {
		if (NOT(m_itemCache->contains(itemKey))) {
			inCache = false;
			break;
		}
	}
	return inCache;
}


int VRayProxyCache::insert(const FrameKey &frameIdx, const LOD &lod, const std::vector<Geometry> &geometry)
{
	if (checkCached(frameIdx, lod)) {
		return false;
	}

//		insert new item in frameCache
	CachedFrame &frameData = (*m_frameCache)[frameIdx];
	ItemKeys &itemKeys = frameData.getItemKeys(lod);
	itemKeys.resize(geometry.size());

//		cache each geometry type individually
	GeometryHash hasher;

	for (int i = 0; i < geometry.size(); ++i) {
		const Geometry &geom = geometry[i];
		ItemKey itemKey = hasher(geom);
		itemKeys[i] = itemKey;

		if (m_itemCache->contains(itemKey)) {
//				in itemCache only increase ref count
			CachedItem &itemData = (*m_itemCache)[itemKey];
			++itemData.m_refCnt;
		} else {
//				not in itemCache insert as new item and init ref count to 1
			CachedItem &itemData = (*m_itemCache)[itemKey];
			createProxyGeometry(geom, itemData.m_item);
			itemData.m_refCnt = 1;
		}
	}

	return true;
}


int VRayProxyCache::erase(const FrameKey &frameIdx)
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
	for (const auto &keyPair : frameData.m_keys) {
		const LOD &lod = keyPair.first;
		const ItemKeys &itemKeys = keyPair.second;

		for (const auto &itemKey : itemKeys) {
			if (m_itemCache->contains(itemKey)) {
				CachedItem& itemData = (*m_itemCache)[itemKey];
				--itemData.m_refCnt;
				if (itemData.m_refCnt <= 0) {
					m_itemCache->erase(itemKey);
				}
			}
		}
	}
}


VRayProxyCache::FrameKey VRayProxyCache::getFrameIdx(const UT_Options &options) const
{
	UT_ASSERT( m_proxy );

	const fpreal64 frame      = VRayProxyUtils::getFloatFrame(options);
	const exint    animType   = VRayProxyUtils::getAnimType(options);
	const fpreal64 animOffset = VRayProxyUtils::getAnimOffset(options);
	const fpreal64 animSpeed  = VRayProxyUtils::getAnimSpeed(options);

	const bool  animOverride = VRayProxyUtils::getAnimOverride(options);
	const exint animStart  = (animOverride)? VRayProxyUtils::getAnimStart(options) : 0;
	exint       animLength = (animOverride)? VRayProxyUtils::getAnimLength(options) : 0;
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

	if ( m_proxy->getNumFrames() ) {
		m_proxy->setCurrentFrame(frameKey);
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


int VRayProxyCache::createProxyGeometry(const Geometry &geom, GU_DetailHandle &gdpHndl) const
{
	VoxelType voxelType = geom.first;
	VUtils::MeshVoxel *voxel = geom.second;
	if (voxel) {
		if (voxelType & MVF_GEOMETRY_VOXEL) {
			return createMeshProxyGeometry(*voxel, gdpHndl);

		} else if (voxelType & MVF_HAIR_GEOMETRY_VOXEL) {
			return createHairProxyGeometry(*voxel, gdpHndl);
		}
	}

	return false;
}


int VRayProxyCache::createMeshProxyGeometry(VUtils::MeshVoxel &voxel, GU_DetailHandle &gdpHndl) const
{
	VUtils::MeshChannel *verts_ch = voxel.getChannel(VERT_GEOM_CHANNEL);
	VUtils::MeshChannel *faces_ch = voxel.getChannel(FACE_TOPO_CHANNEL);
	if ( NOT(verts_ch) || NOT(faces_ch) ) {
		return false;
	}

	VUtils::VertGeomData *verts = static_cast<VUtils::VertGeomData *>(verts_ch->data);
	VUtils::FaceTopoData *faces = static_cast<VUtils::FaceTopoData *>(faces_ch->data);
	if (NOT(verts) || NOT(faces)) {
		return false;
	}

	GU_Detail *gdp = new GU_Detail();
	int numVerts = verts_ch->numElements;
	int numFaces = faces_ch->numElements;

	// Points
	GA_Offset voffset = gdp->appendPointBlock(numVerts);
	for (int v = 0; v < numVerts; ++v) {
		VUtils::Vector &vert = verts[v];
		GA_Offset pointOffs = voffset + v;

#if UT_MAJOR_VERSION_INT < 14
		GEO_Point *point = gdp->getGEOPoint(pointOffs);
		point->setPos(UT_Vector4F(vert.x, vert.y, vert.z));
#else
		gdp->setPos3(pointOffs, UT_Vector3F(vert.x, vert.y, vert.z));
#endif
	}

	// Faces
	for (int f = 0; f < numFaces; ++f) {
		const VUtils::FaceTopoData &face = faces[f];

		GU_PrimPoly *poly = GU_PrimPoly::build(gdp, 3, GU_POLY_CLOSED, 0);
		for (int c = 0; c < 3; ++c) {
			poly->setVertexPoint(c, voffset + face.v[c]);
		}

		poly->reverse();
	}

	gdpHndl.allocateAndSet(gdp);
	return true;
}


int VRayProxyCache::createHairProxyGeometry(VUtils::MeshVoxel &voxel, GU_DetailHandle &gdpHndl) const
{
	VUtils::MeshChannel *verts_ch = voxel.getChannel(HAIR_VERT_CHANNEL);
	VUtils::MeshChannel *strands_ch = voxel.getChannel(HAIR_NUM_VERT_CHANNEL);
	if ( NOT(verts_ch) || NOT(strands_ch) ) {
		return false;
	}

	VUtils::VertGeomData *verts = reinterpret_cast< VUtils::VertGeomData* >(verts_ch->data);
	int *strands = reinterpret_cast< int* >(strands_ch->data);
	if (NOT(verts) || NOT(strands)) {
		return false;
	}

	GU_Detail *gdp = new GU_Detail();
	int numVerts = verts_ch->numElements;
	int numStrands = strands_ch->numElements;

	// Points
	GA_Offset voffset = gdp->appendPointBlock(numVerts);
	for (int v = 0; v < numVerts; ++v) {
		VUtils::Vector &vert = verts[v];
		GA_Offset pointOffs = voffset + v;

#if UT_MAJOR_VERSION_INT < 14
		GEO_Point *point = gdp->getGEOPoint(pointOffs);
		point->setPos(UT_Vector4F(vert.x, vert.y, vert.z));
#else
		gdp->setPos3(pointOffs, UT_Vector3F(vert.x, vert.y, vert.z));
#endif
	}

	// Strands
	for (int i = 0; i < numStrands; ++i) {
		int &vertsPerStrand = strands[i];

		GU_PrimPoly *poly = GU_PrimPoly::build(gdp, vertsPerStrand, GU_POLY_OPEN, 0);
		for (int j = 0; j < vertsPerStrand; ++j) {
			poly->setVertexPoint(j, voffset + j);
		}

		voffset += vertsPerStrand;
	}

	gdpHndl.allocateAndSet(gdp);
	return true;
}


void VRayProxyCache::createBBoxGeometry(const FrameKey &frameKey, GU_Detail &gdp) const
{
	if ( m_proxy->getNumFrames() ) {
		m_proxy->setCurrentFrame(frameKey);
	}

	VUtils::Box bbox= m_proxy->getBBox();
	const VUtils::Vector &bboxMin = bbox.c(0);
	const VUtils::Vector &bboxMax = bbox.c(1);
	gdp.cube(bboxMin[0], bboxMax[0],
			bboxMin[1], bboxMax[1],
			bboxMin[2], bboxMax[2]);
}



VRayProxyCacheMan& VRayForHoudini::GetVRayProxyCacheManager()
{
	static VRayProxyCacheMan theCacheMan;
	return theCacheMan;
}


int VRayProxyUtils::getVRayProxyDetail(const UT_Options &options, GU_Detail &gdp)
{
	UT_StringHolder utfilepath = VRayProxyUtils::getFilepath(options);
	if (NOT(utfilepath.isstring())) {
		return false;
	}

	const std::string filepath(utfilepath);
	VRayProxyCacheMan& theCacheMan = GetVRayProxyCacheManager();
	if (NOT(theCacheMan.contains(filepath))) {
		// NOTE: insert entry in the cache
		VRayProxyCache &cache = theCacheMan[filepath];
		VUtils::ErrorCode errCode = cache.init(filepath.c_str());
		if (errCode.error()) {
			theCacheMan.erase(filepath);
			return false;
		}
	}

	VRayProxyCache &cache = theCacheMan[filepath];
	VUtils::ErrorCode errCode = cache.getFrame(options, gdp);

	return true;
}

const UT_StringRef VRayProxyUtils::theLODToken          = "lod";
const UT_StringRef VRayProxyUtils::theFileToken         = "file";
const UT_StringRef VRayProxyUtils::theFrameToken        = "frame";
const UT_StringRef VRayProxyUtils::theAnimTypeToken     = "anim_type";
const UT_StringRef VRayProxyUtils::theAnimOffsetToken   = "anim_offset";
const UT_StringRef VRayProxyUtils::theAnimSpeedToken    = "anim_speed";
const UT_StringRef VRayProxyUtils::theAnimOverrideToken = "anim_override";
const UT_StringRef VRayProxyUtils::theAnimStartToken    = "anim_start";
const UT_StringRef VRayProxyUtils::theAnimLengthToken   = "anim_length";
