//
// Copyright (c) 2015, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#include "vfh_log.h"
#include "vfh_hashes.h" // For MurmurHash3_x86_32
#include "vfh_lru_cache.hpp"

#include "sop_vrayproxy.h"

#include <GEO/GEO_Point.h>
#include <GU/GU_PrimPoly.h>
#include <HOM/HOM_Vector2.h>
#include <HOM/HOM_BaseKeyframe.h>
#include <HOM/HOM_playbar.h>
#include <HOM/HOM_Module.h>
#include <EXPR/EXPR_Lock.h>

using namespace VRayForHoudini;


enum DataError {
	DE_INVALID_GEOM = 1,
	DE_NO_GEOM,
	DE_INVALID_FILE
};


enum LoadType {
	LT_BBOX = 0,
	LT_PREVIEWGEO,
	LT_FULLGEO
};


typedef std::shared_ptr<GU_Detail>                 GU_DetailPtr;
typedef uint32                                     VoxelType;
typedef std::pair<VoxelType, VUtils::MeshVoxel* >  Geometry;


struct GeometryHash
{
	typedef Hash::MHash result_type;
	typedef Geometry argument_type;

	result_type operator()(const argument_type &val) const
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
};

class VRayProxyCache
{
/// VRayProxyCache caches .vrmesh preview geometry in memory for faster playback
/// preview geometry for a frame is decomposed into mesh and hair geometry and
/// cached individually as GU_Detail objects(houdini native geometry container)
/// VRayProxyCache uses 2 LRUCaches - one for cached frames and second for cached geometry
/// to allow storing same geometry only once, if used in different frames
/// NOTE:
///      1) currently geometry is hashed using vertex positions only(might change in future)
///      2) currently cache capacity is defined by playback range at the time of initialization(might change in future)

private:
	typedef unsigned FrameKey;
	typedef Hash::MHash ItemKey;
	typedef GU_DetailPtr Item;

	struct CachedFrame
	{
		std::vector<ItemKey> m_itemKeys;
	};

	struct CachedItem
	{
		Item m_item;
		int m_refCnt;
	};

	typedef Caches::LRUCache< FrameKey, CachedFrame, std::hash<FrameKey>, std::equal_to<FrameKey> > FrameCache;
	typedef Caches::LRUCache< ItemKey, CachedItem, std::hash<ItemKey>, std::equal_to<ItemKey> > ItemCache;

public:
	typedef FrameCache::size_type size_type;

	VRayProxyCache() :
		m_proxy(nullptr),
		m_frameCache(new FrameCache()),
		m_itemCache(new ItemCache())
	{ m_frameCache->setEvictCallback(FrameCache::CbEvict(boost::bind(&VRayProxyCache::evictFrame, this, _1, _2))); }

	VRayProxyCache(VRayProxyCache&& other)
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

	VRayProxyCache& operator=(VRayProxyCache&& other)
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

	~VRayProxyCache()
	{ reset(); }


/// @brief Clears previous cache, if any, and attempts to initialize the new .vrmesh file
///        cache capacity is defined by playback range at the time of initialization
/// @param filepath - path to the .vrmesh file
/// @return VUtils::ErrorCode - no error if initialized successfully
///                           - DE_INVALID_FILE if file initialization fails
	VUtils::ErrorCode init(const VUtils::CharString &filepath)
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

		vassert( m_proxy );

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
		m_itemCache->setCapacity(nFrames);

		return res;
	}

/// @brief Clears cache and deletes current .vrmesh file, if any
	void reset()
	{
		clearCache();
		if (m_proxy) {
			VUtils::deleteDefaultMeshFile(m_proxy);
		}
		m_proxy = nullptr;
		m_filepath = "";
	}

/// @brief Clears cache
	void clearCache()
	{
		if (m_frameCache) {
			m_frameCache->clear();
		}
		if (m_itemCache) {
			m_itemCache->clear();
		}
	}

	size_type capacity() const { return m_frameCache->capacity(); }
	size_type size() const { return m_frameCache->size(); }
	int empty() const { return (size() == 0); }

/// @brief Checks if a frame is cached
/// @param context - contains evaluation time information i.e. the frame
/// @param opParams - contains node parameters necessary to map the frame to a .vrmesh frame index
///	@return true - frame is cached(all geometry for that frame is present in the geometry cache)
///         false - otherwise
///         NOTE: if frame is cached but a geometry for that frame is missing
///               removes the cached frame and returns false
///               (could happen if the geometry was evicted from the geometry cache)
	int checkFrameCached(const OP_Context &context, const OP_Parameters &opParams)
	{
		if (NOT(m_proxy)) {
			return false;
		}

		FrameKey frameIdx = getFrameIdx(context, opParams);
		return checkCached(frameIdx);
	}

/// @brief Merges the geometry for a frame into the GU_Detail passed
///        if the frame is not in cache loads the preview geometry for that frame
///        and caches it
/// @param context - contains evaluation time information i.e. the frame
/// @param opParams - contains node parameters necessary to map the frame to a .vrmesh frame index
/// @return VUtils::ErrorCode - no error if successful
///                           - DE_INVALID_FILE if cache is not initialized
///                           - DE_NO_GEOM if no preview geometry is found for that frame
///                           - DE_INVALID_GEOM if a cached geometry for that frame is invalid
	VUtils::ErrorCode getFrame(const OP_Context &context, const OP_Parameters &opParams, GU_Detail &gdp)
	{
		VUtils::ErrorCode res;

		if (NOT(m_proxy)) {
			res.setError(__FUNCTION__, DE_INVALID_FILE, "Invalid file path!");
			return res;
		}

		fpreal t = context.getTime();
		FrameKey frameIdx = getFrameIdx(context, opParams);
		LoadType loadType = static_cast<LoadType>(opParams.evalInt("loadtype", 0, t));

		switch (loadType) {
			case LT_BBOX:
			{
				createBBoxGeometry(frameIdx, gdp);
				return res;
			}
			case LT_FULLGEO:
			{
				std::vector<Geometry> geometryList;
				// last voxel in file is reserved for preview geometry
				for (int i = 0; i < m_proxy->getNumVoxels() - 1; ++i) {
					VUtils::MeshVoxel *voxel = getVoxel(frameIdx, i);
					vassert( voxel );
					getPreviewGeometry(*voxel, geometryList);
				}

				for (int i = 0; i < geometryList.size(); ++i) {
					const Geometry &geom = geometryList[i];
					GU_DetailPtr gdpPtr = createProxyGeometry(geom);
					if (gdpPtr) {
						gdp.merge(*gdpPtr);
					}
				}

				return res;
			}
			default:
				break;
		}

//		if not in cache load preview voxel and cache corresponding GU_Detail(s)
		if (NOT(checkCached(frameIdx))) {
			// last voxel in file is reserved for preview geometry
			VUtils::MeshVoxel *previewVoxel = getVoxel(frameIdx, m_proxy->getNumVoxels() - 1);
			if (NOT(previewVoxel)) {
				res.setError(__FUNCTION__, DE_NO_GEOM, "No preview geometry found for context #%0.3f.", context.getFloatFrame());
				return res;
			}

			std::vector<Geometry> previewGeometry;
			getPreviewGeometry(*previewVoxel, previewGeometry);
			if (NOT(previewGeometry.size())) {
				res.setError(__FUNCTION__, DE_NO_GEOM, "No preview geometry found for context #%0.3f.", context.getFloatFrame());
				return res;
			}

			insert(frameIdx, previewGeometry);
			m_proxy->releaseVoxel(previewVoxel);
		}

		CachedFrame& frameData = (*m_frameCache)[frameIdx];
		for (auto const &itemKey : frameData.m_itemKeys) {
			ItemCache::iterator itemIt = m_itemCache->find(itemKey);
			vassert( itemIt != m_itemCache->end() );

			CachedItem &itemData = *itemIt;
			GU_DetailPtr item = itemData.m_item;
			if (item) {
				gdp.merge(*item);
			} else {
				res.setError(__FUNCTION__, DE_INVALID_GEOM, "Invalid geometry found for context #%0.3f.", context.getFloatFrame());
			}
		}

		return res;
	}

private:
	int checkCached(const FrameKey &frameIdx)
	{
		if (NOT(m_frameCache->contains(frameIdx))) {
			return false;
		}

//		if in cache check if all items from the collection are cached
		int inCache = true;
		CachedFrame &frameData = (*m_frameCache)[frameIdx];
		for (const auto &itemKey : frameData.m_itemKeys) {
			if (NOT(m_itemCache->contains(itemKey))) {
				erase(frameIdx);
				inCache = false;
				break;
			}
		}
		return inCache;
	}

	int insert(const FrameKey &frameIdx, const std::vector<Geometry> &geometry)
	{
		if (checkCached(frameIdx)) {
			return false;
		}

//		insert new item in frameCache
		CachedFrame &frameData = (*m_frameCache)[frameIdx];
		frameData.m_itemKeys.resize(geometry.size());

//		cache each geometry type individually
		GeometryHash hasher;

		for (int i = 0; i < geometry.size(); ++i) {
			const Geometry &geom = geometry[i];
			ItemKey itemKey = hasher(geom);
			frameData.m_itemKeys[i] = itemKey;

			if (m_itemCache->contains(itemKey)) {
//				in itemCache only increase ref count
				CachedItem &itemData = (*m_itemCache)[itemKey];
				++itemData.m_refCnt;
			} else {
//				not in itemCache insert as new item and init ref count to 1
				CachedItem &itemData = (*m_itemCache)[itemKey];
				itemData.m_item = createProxyGeometry(geom);
				itemData.m_refCnt = 1;
			}
		}

		return true;
	}

	int erase(const FrameKey &frameIdx)
	{
		if (NOT(m_frameCache->contains(frameIdx))) {
			return false;
		}

		CachedFrame &frameData = (*m_frameCache)[frameIdx];
		evictFrame(frameIdx, frameData);

		return m_frameCache->erase(frameIdx);
	}

	void evictFrame(const FrameKey &frameIdx, CachedFrame &frameData)
	{
		for (const auto &itemKey : frameData.m_itemKeys) {
			if (m_itemCache->contains(itemKey)) {
				CachedItem& itemData = (*m_itemCache)[itemKey];
				--itemData.m_refCnt;
				if (itemData.m_refCnt <= 0) {
					m_itemCache->erase(itemKey);
				}
			}
		}
	}

	FrameKey getFrameIdx(const OP_Context &context, const OP_Parameters &opParams) const
	{
		vassert( m_proxy );

		const fpreal frame = context.getFloatFrame();
		const int animType = opParams.evalInt("anim_type", 0, 0.0f);
		const int animStart = opParams.evalInt("anim_start", 0, 0.0f);
		int animLength = opParams.evalInt("anim_length", 0, 0.0f);
		if (animLength <= 0) {
			animLength = std::max(m_proxy->getNumFrames(), 1);
		}

		const double animOffset = opParams.evalFloat("anim_offset", 0, 0.0f);
		const double animSpeed = opParams.evalFloat("anim_speed", 0, 0.0f);

		return static_cast<FrameKey>(VUtils::fast_round((VUtils::calcFrameIndex(frame,
															static_cast<VUtils::MeshFileAnimType::Enum>(animType),
															animStart,
															animLength,
															animOffset,
															animSpeed))));
	}

	VUtils::MeshVoxel *getVoxel(const FrameKey &frameKey, int voxelIdx) const
	{
		vassert( m_proxy );

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

	void getPreviewGeometry(VUtils::MeshVoxel &voxel, std::vector<Geometry> &geometry) const
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

	GU_DetailPtr createProxyGeometry(const Geometry &geom) const
	{
		VoxelType voxelType = geom.first;
		VUtils::MeshVoxel *voxel = geom.second;
		if (voxel) {
			if (voxelType & MVF_GEOMETRY_VOXEL) {
				return createMeshProxyGeometry(*voxel);

			} else if (voxelType & MVF_HAIR_GEOMETRY_VOXEL) {
				return createHairProxyGeometry(*voxel);
			}
		}

		return GU_DetailPtr();
	}

	GU_DetailPtr createMeshProxyGeometry(VUtils::MeshVoxel &voxel) const
	{
		VUtils::MeshChannel *verts_ch = voxel.getChannel(VERT_GEOM_CHANNEL);
		VUtils::MeshChannel *faces_ch = voxel.getChannel(FACE_TOPO_CHANNEL);
		if ( NOT(verts_ch) || NOT(faces_ch) ) {
			return GU_DetailPtr();
		}

		VUtils::VertGeomData *verts = static_cast<VUtils::VertGeomData *>(verts_ch->data);
		VUtils::FaceTopoData *faces = static_cast<VUtils::FaceTopoData *>(faces_ch->data);
		if (NOT(verts) || NOT(faces)) {
			return GU_DetailPtr();
		}

		GU_DetailPtr gdp = std::make_shared<GU_Detail>();
		int numVerts = verts_ch->numElements;
		int numFaces = faces_ch->numElements;
		GA_Offset voffset = gdp->getNumVertexOffsets();

		// Points
		for (int v = 0; v < numVerts; ++v) {
			VUtils::Vector vert = verts[v];

#if UT_MAJOR_VERSION_INT < 14
			GEO_Point *point = gdp->appendPointElement();
			point->setPos(UT_Vector4F(vert.x, vert.y, vert.z));
#else
			GA_Offset pointOffs = gdp->appendPoint();
			gdp->setPos3(pointOffs, UT_Vector4F(vert.x, vert.y, vert.z));
#endif
		}

		// Faces
		for (int f = 0; f < numFaces; ++f) {
			const VUtils::FaceTopoData &face = faces[f];

			GU_PrimPoly *poly = GU_PrimPoly::build(gdp.get(), 3, GU_POLY_CLOSED, 0);
			for (int c = 0; c < 3; ++c) {
				poly->setVertexPoint(c, voffset + face.v[c]);
			}

			poly->reverse();
		}

		return gdp;
	}

	GU_DetailPtr createHairProxyGeometry(VUtils::MeshVoxel &voxel) const
	{
		VUtils::MeshChannel *verts_ch = voxel.getChannel(HAIR_VERT_CHANNEL);
		VUtils::MeshChannel *strands_ch = voxel.getChannel(HAIR_NUM_VERT_CHANNEL);
		if ( NOT(verts_ch) || NOT(strands_ch) ) {
			return GU_DetailPtr();
		}

		VUtils::VertGeomData *verts = static_cast<VUtils::VertGeomData *>(verts_ch->data);
		int *strands = static_cast<int *>(strands_ch->data);
		if (NOT(verts) || NOT(strands)) {
			return GU_DetailPtr();
		}

		GU_DetailPtr gdp = std::make_shared<GU_Detail>();
		int numVerts = verts_ch->numElements;
		int numStrands = strands_ch->numElements;
		GA_Offset voffset = gdp->getNumVertexOffsets();

		// Points
		for (int i = 0; i < numVerts; ++i) {
			VUtils::Vector vert = verts[i];

#if UT_MAJOR_VERSION_INT < 14
			GEO_Point *point = gdp->appendPointElement();
			point->setPos(UT_Vector4F(vert.x, vert.y, vert.z));
#else
			GA_Offset pointOffs = gdp->appendPoint();
			gdp->setPos3(pointOffs, UT_Vector4F(vert.x, vert.y, vert.z));
#endif
		}

		// Strands
		for (int i = 0; i < numStrands; ++i) {
			int &vertsPerStrand = strands[i];

			GU_PrimPoly *poly = GU_PrimPoly::build(gdp.get(), vertsPerStrand, GU_POLY_OPEN, 0);
			for (int j = 0; j < vertsPerStrand; ++j) {
				poly->setVertexPoint(j, voffset + j);
			}

			voffset += vertsPerStrand;
		}

		return gdp;
	}

	void createBBoxGeometry(const FrameKey &frameKey, GU_Detail &gdp) const
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

private:
	VRayProxyCache(const VRayProxyCache &other);
	VRayProxyCache & operator =(const VRayProxyCache &other);

private:
	VUtils::MeshFile *m_proxy;
	VUtils::CharString m_filepath;
	std::shared_ptr<FrameCache> m_frameCache;
	std::shared_ptr<ItemCache> m_itemCache;
};


typedef Caches::LRUCache< std::string, VRayProxyCache > VRayProxyCacheMan;

static const int cacheCapacity = 10;
static VRayProxyCacheMan g_cacheMan(cacheCapacity);

/// VRayProxy node params
///
static PRM_Name prmCacheHeading("cacheheading", "VRayProxy Cache");
static PRM_Name prmClearCache("clear_cache", "Clear Cache");

static PRM_Name prmLoadType("loadtype", "Load");
static PRM_Name prmLoadTypeItems[] = {
	PRM_Name("Bounding Box"),
	PRM_Name("Preview Geometry"),
	PRM_Name("Full Geometry"),
	PRM_Name(),
};
static PRM_ChoiceList prmLoadTypeMenu(PRM_CHOICELIST_SINGLE, prmLoadTypeItems);

static PRM_Name prmProxyHeading("vrayproxyheading", "VRayProxy Settings");



void SOP::VRayProxy::addPrmTemplate(Parm::PRMTmplList &prmTemplate)
{
	prmTemplate.push_back(PRM_Template(PRM_HEADING, 1, &prmCacheHeading));
	prmTemplate.push_back(PRM_Template(PRM_ORD, 1, &prmLoadType, PRMoneDefaults, &prmLoadTypeMenu));
	prmTemplate.push_back(PRM_Template(PRM_CALLBACK, 1, &prmClearCache, 0, 0, 0, VRayProxy::cbClearCache));
	prmTemplate.push_back(PRM_Template(PRM_HEADING, 1, &prmProxyHeading));
}


int SOP::VRayProxy::cbClearCache(void *data, int /*index*/, float t, const PRM_Template */*tplate*/)
{
	OP_Node *node = reinterpret_cast<OP_Node *>(data);
	UT_String filepath;
	{
		EXPR_GlobalStaticLock::Scope scopedLock;
		node->evalString(filepath, "file", 0, t);
	}

	if (g_cacheMan.contains(filepath.buffer())) {
		VRayProxyCache &fileCache = g_cacheMan[filepath.buffer()];
		fileCache.clearCache();
	}

	return 0;
}


void SOP::VRayProxy::setPluginType()
{
	pluginType = "GEOMETRY";
	pluginID   = "GeomMeshFile";
}


OP_NodeFlags &SOP::VRayProxy::flags()
{
	OP_NodeFlags &flags = SOP_Node::flags();

	const auto animType = static_cast<VUtils::MeshFileAnimType::Enum>(evalInt("anim_type", 0, 0.0f));
	const bool is_animated = (animType != VUtils::MeshFileAnimType::Still);
	flags.setTimeDep(is_animated);

	return flags;
}


OP_ERROR SOP::VRayProxy::cookMySop(OP_Context &context)
{
	Log::getLog().info("SOP::VRayProxy::cookMySop()");

	if (NOT(gdp)) {
		addError(SOP_MESSAGE, "Invalid geometry detail.");
		return error();
	}

	const float t = context.getTime();

	UT_String path;
	evalString(path, "file", 0, t);
	if (path.equal("")) {
		addError(SOP_ERR_FILEGEO, "Invalid file path!");
		return error();
	}

	std::string filepath(path.buffer());
	int inCache = g_cacheMan.contains(filepath);
	VRayProxyCache &fileCache = g_cacheMan[filepath];
	if (NOT(inCache)) {
		VUtils::ErrorCode errCode = fileCache.init(path.buffer());
		if (errCode.error()) {
			g_cacheMan.erase(filepath);
			addError(SOP_ERR_FILEGEO, errCode.getErrorString().ptr());
			return error();
		}
	}

	const bool flipAxis = (evalInt("flip_axis", 0, 0.0f) != 0);
	const float scale   = evalFloat("scale", 0, 0.0f);

	gdp->clearAndDestroy();

	if (error() < UT_ERROR_ABORT) {
		UT_Interrupt *boss = UTgetInterrupt();
		if (boss) {
			if(boss->opStart("Building V-Ray Scene Preview Mesh")) {
				VUtils::ErrorCode errCode = fileCache.getFrame(context, *this, *gdp);
				if (errCode.error()) {
					addWarning(SOP_MESSAGE, errCode.getErrorString().ptr());
				}

	//			scale & flip axis
				UT_Matrix4 mat(1.f);
				mat(0,0) = scale;
				mat(1,1) = scale;
				mat(2,2) = scale;
	//			houdini uses row major matrix
				if (flipAxis) {
					VUtils::swap(mat(1,0), mat(2,0));
					VUtils::swap(mat(1,1), mat(2,1));
					VUtils::swap(mat(1,2), mat(2,2));
					mat(2,0) = -mat(2,0);
					mat(2,1) = -mat(2,1);
					mat(2,2) = -mat(2,2);
				}
				gdp->transform(mat, 0, 0, true, true, true, true, false, 0, true);
			}

			boss->opEnd();
		}
	}

#if UT_MAJOR_VERSION_INT < 14
	gdp->notifyCache(GU_CACHE_ALL);
#endif

	return error();
}


OP::VRayNode::PluginResult SOP::VRayProxy::asPluginDesc(Attrs::PluginDesc &pluginDesc, VRayExporter &exporter, OP_Node *parent)
{
	vassert( exporter );

	UT_String path;
	evalString(path, "file", 0, 0.0f);
	if (NOT(path.isstring())) {
		Log::getLog().error("VRayProxy \"%s\": \"File\" is not set!",
					getName().buffer());
		return OP::VRayNode::PluginResultError;
	}

	pluginDesc.pluginID   = pluginID.c_str();
	pluginDesc.pluginName = VRayExporter::getPluginName(this);

	pluginDesc.pluginAttrs.push_back(Attrs::PluginAttr("file", path.buffer()));
	pluginDesc.pluginAttrs.push_back(Attrs::PluginAttr("flip_axis", evalInt("flip_axis", 0, 0.0f)));

	exporter.setAttrsFromOpNode(pluginDesc, this);

	return OP::VRayNode::PluginResultSuccess;
}
