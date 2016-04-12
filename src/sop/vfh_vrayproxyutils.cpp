//
// Copyright (c) 2015, Chaos Software Ltd
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
	UT_AutoLock lock(theLock);

	UT_StringHolder utfilepath = options.getFilepath();
	if (NOT(utfilepath.isstring())) {
		return GU_ConstDetailHandle();
	}

	const std::string filepath(utfilepath);
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
	return cache.getFrame(options);
}

const UT_StringRef VRayProxyParms::thePathToken         = "path";
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


UT_StringHolder VRayProxyParms::getPath(const UT_Options &options)
{
	return ((options.hasOption(thePathToken))? options.getOptionS(thePathToken) : UT_StringHolder());
}

UT_StringHolder VRayProxyParms::getFilepath(const UT_Options &options)
{
	return ((options.hasOption(theFileToken))? options.getOptionS(theFileToken) : UT_StringHolder());
}

exint VRayProxyParms::getLOD(const UT_Options &options)
{
	return ((options.hasOption(theLODToken))? options.getOptionI(theLODToken) : LOD_PREVIEW);
}

fpreal64 VRayProxyParms::getFloatFrame(const UT_Options &options)
{
	return ((options.hasOption(theFrameToken))? options.getOptionF(theFrameToken) : 0.f);
}

exint VRayProxyParms::getAnimType(const UT_Options &options)
{
	return ((options.hasOption(theAnimTypeToken))? options.getOptionI(theAnimTypeToken) : VUtils::MeshFileAnimType::Loop);
}

fpreal64 VRayProxyParms::getAnimOffset(const UT_Options &options)
{
	return ((options.hasOption(theAnimOffsetToken))? options.getOptionF(theAnimOffsetToken) : 0.f);
}

fpreal64 VRayProxyParms::getAnimSpeed(const UT_Options &options)
{
	return ((options.hasOption(theAnimSpeedToken))? options.getOptionF(theAnimSpeedToken) : 1.f);
}

bool VRayProxyParms::getAnimOverride(const UT_Options &options)
{
	return ((options.hasOption(theAnimOverrideToken))? options.getOptionB(theAnimOverrideToken) : 0);
}

exint VRayProxyParms::getAnimStart(const UT_Options &options)
{
	return ((options.hasOption(theAnimStartToken))? options.getOptionI(theAnimStartToken) : 0);
}

exint VRayProxyParms::getAnimLength(const UT_Options &options)
{
	return ((options.hasOption(theAnimLengthToken))? options.getOptionI(theAnimLengthToken) : 0);
}

fpreal64 VRayProxyParms::getScale(const UT_Options &options)
{
	return ((options.hasOption(theScaleToken))? options.getOptionF(theScaleToken) : 1.f);
}

exint VRayProxyParms::getFlipAxis(const UT_Options &options)
{
	return ((options.hasOption(theFlipAxisToken))? options.getOptionI(theFlipAxisToken) : 0);
}

VRayProxyParms::VRayProxyParms():
	m_lod(LOD_PREVIEW),
	m_floatFrame(0),
	m_animType(VUtils::MeshFileAnimType::Loop),
	m_animOffset(0),
	m_animSpeed(1),
	m_animOverride(0),
	m_animStart(0),
	m_animLength(0),
	m_scale(1),
	m_flipAxis(0)
{ }

VRayProxyParms::VRayProxyParms(const UT_Options &options):
	m_path(getPath(options)),
	m_filepath(getFilepath(options)),
	m_lod(getLOD(options)),
	m_floatFrame(getFloatFrame(options)),
	m_animType(getAnimType(options)),
	m_animOffset(getAnimOffset(options)),
	m_animSpeed(getAnimSpeed(options)),
	m_animOverride(getAnimOverride(options)),
	m_animStart(getAnimStart(options)),
	m_animLength(getAnimLength(options)),
	m_scale(getScale(options)),
	m_flipAxis(getFlipAxis(options))
{ }


VRayProxyParms& VRayProxyParms::operator =(const UT_Options &options)
{
	m_path = getPath(options);
	m_filepath = getFilepath(options);
	m_lod = getLOD(options);
	m_floatFrame = getFloatFrame(options);
	m_animType = getAnimType(options);
	m_animOffset = getAnimOffset(options);
	m_animSpeed = getAnimSpeed(options);
	m_animOverride = getAnimOverride(options);
	m_animStart = getAnimStart(options);
	m_animLength = getAnimLength(options);
	m_scale = getScale(options);
	m_flipAxis = getFlipAxis(options);
}


bool VRayProxyParms::operator ==(const UT_Options &options) const
{
	return (
			   m_path == getPath(options)
			&& m_filepath == getFilepath(options)
			&& m_lod == getLOD(options)
			&& m_floatFrame == getFloatFrame(options)
			&& m_animType == getAnimType(options)
			&& m_animOffset == getAnimOffset(options)
			&& m_animSpeed == getAnimSpeed(options)
			&& m_animOverride == getAnimOverride(options)
			&& m_animStart == getAnimStart(options)
			&& m_animLength == getAnimLength(options)
			&& m_scale == getScale(options)
			&& m_flipAxis == getFlipAxis(options)
			);
}

bool VRayProxyParms::operator ==(const VRayProxyParms &options) const
{
	return (
			   m_path == options.m_path
			&& m_filepath == options.m_filepath
			&& m_lod == options.m_lod
			&& m_floatFrame == options.m_floatFrame
			&& m_animType == options.m_animType
			&& m_animOffset == options.m_animOffset
			&& m_animSpeed == options.m_animSpeed
			&& m_animOverride == options.m_animOverride
			&& m_animStart == options.m_animStart
			&& m_animLength == options.m_animLength
			&& m_scale == options.m_scale
			&& m_flipAxis == options.m_flipAxis
			);
}


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
{
	m_frameCache->setEvictCallback(FrameCache::CbEvict(boost::bind(&VRayProxyCache::evictFrame, this, _1, _2)));
}


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


int VRayProxyCache::checkFrameCached(const VRayProxyParms &options) const
{
	if (NOT(m_proxy)) {
		return false;
	}

	FrameKey frameIdx = getFrameIdx(options);
	LOD lod = static_cast<LOD>(options.getLOD());

	return checkCached(frameIdx, lod);
}


GU_ConstDetailHandle VRayProxyCache::getFrame(const VRayProxyParms &options)
{
	if (NOT(m_proxy)) {
		return GU_ConstDetailHandle();
	}

	FrameKey frameIdx = getFrameIdx(options);
	LOD lod = static_cast<LOD>(options.getLOD());

//	if (lod == LOD_BBOX) {
//		createBBoxGeometry(frameIdx, gdp);
//		return res;
//	}

	const int numVoxels = m_proxy->getNumVoxels();
	// if not in cache load preview voxel and cache corresponding GU_Detail(s)
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

		for (VUtils::MeshVoxel *voxel : voxels) {
			m_proxy->releaseVoxel(voxel);
		}
	}

	if (m_frameCache->contains(frameIdx)) {
		CachedFrame &frameData = (*m_frameCache)[frameIdx];
		if ( frameData.hasDetail(lod) ) {
			return GU_ConstDetailHandle(frameData.getDetail(lod));
		}
	}

//	if (m_frameCache->contains(frameIdx)) {
//		CachedFrame &frameData = (*m_frameCache)[frameIdx];
//		if ( frameData.hasItemKeys(lod) ) {
//			for (auto const &itemKey : frameData.getItemKeys(lod)) {
//				ItemCache::iterator itemIt = m_itemCache->find(itemKey);
//				UT_ASSERT( itemIt != m_itemCache->end() );

//				CachedItem &itemData = *itemIt;
//				GU_DetailHandle &gdpHndl = itemData.m_item;
//				if (gdpHndl.isValid()) {
//					GU_PackedGeometry::packGeometry(gdp, gdpHndl);
//				} else {
//					res.setError(__FUNCTION__, DE_INVALID_GEOM, "Invalid geometry found for context #%0.3f.", options.getFloatFrame()));
//				}
//			}
//		}
//	}

	return GU_ConstDetailHandle();
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

	// if in cache check if all items from the collection are cached
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

	// insert new item in frameCache
	CachedFrame &frameData = (*m_frameCache)[frameIdx];
	ItemKeys &itemKeys = frameData.getItemKeys(lod);
	itemKeys.resize(geometry.size());

	// cache each geometry type individually
	GeometryHash hasher;

	for (int i = 0; i < geometry.size(); ++i) {
		const Geometry &geom = geometry[i];
		ItemKey itemKey = hasher(geom);
		itemKeys[i] = itemKey;

		if (m_itemCache->contains(itemKey)) {
			// in itemCache only increase ref count
			CachedItem &itemData = (*m_itemCache)[itemKey];
			++itemData.m_refCnt;
		} else {
			// not in itemCache insert as new item and init ref count to 1
			CachedItem &itemData = (*m_itemCache)[itemKey];
			createProxyGeometry(geom, itemData.m_item);
			itemData.m_refCnt = 1;
		}
	}

	Item &gdh = frameData.getDetail(lod);
	if (NOT(gdh.isValid())) {
		gdh.allocateAndSet(new GU_Detail());
	}

	GU_DetailHandleAutoWriteLock gdl(gdh);
	if (gdl.isValid()) {
		GU_Detail *gdp = gdl.getGdp();
		gdp->clearAndDestroy();

		if ( frameData.hasItemKeys(lod) ) {
			for (auto const &itemKey : frameData.getItemKeys(lod)) {
				ItemCache::iterator itemIt = m_itemCache->find(itemKey);
				UT_ASSERT( itemIt != m_itemCache->end() );

				CachedItem &itemData = *itemIt;
				GU_DetailHandle &gdpHndl = itemData.m_item;
				if (gdpHndl.isValid()) {
					gdp->merge(*gdpHndl.peekDetail());
				}
			}
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
