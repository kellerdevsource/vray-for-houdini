//
// Copyright (c) 2015-2017, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#include <SYS/SYS_Types.h>

#include "vfh_defines.h"
#include "vfh_log.h"
#include "gu_vraysceneref.h"
#include "vfh_hashes.h"

#include <vrscene_preview.h>

#include <GU/GU_PackedFactory.h>
#include <GU/GU_PrimPacked.h>
#include <GU/GU_PrimPoly.h>
#include <GU/GU_PackedContext.h>
#include <GU/GU_PackedGeometry.h>
#include <FS/UT_DSO.h>

using namespace VRayForHoudini;
using namespace VUtils::Vrscene::Preview;

enum class FlipAxisMode {
	none = 0,  ///< No flipping
	automatic, ///< Gets the flipping from the vrscene description
	flipZY     ///< Force the scene to flip the Z and Y axis
};

/// *.vrscene preview data manager.
static VrsceneDescManager vrsceneMan(NULL);

/// Cache for GU_Detail for different frames, VRay scenes and VRay scene settings
static class DetailCache {
public:
	DetailCache() {}

	/// Register that an instance has a reference to a specific vrscene file
	/// @param filepath [in] - Path to VRay scene file
	/// @param settings [in] - Custom settings for the vrscene, if not provided default settings are used
	void registerInCache(const VUtils::CharString &filepath, const VrsceneSettings *settings = nullptr) {
		if (filepath.empty()) {
			return;
		}

		VrsceneSettings vrsceneSettings = getDefaultSettings();

		vrsceneCache[filepath.ptr()][vrsceneSettings].references++;
	}

	/// Provide cached detail, if it is outdated returns nullptr
	/// @param filepath[in] - Path to VRay scene file, used as ID for map
	/// @param frame[in] - Frame at which provided detail is required
	/// @param settings[in] - Settings for the cached data, used as seconday ID
	GU_Detail* getDetail(const VUtils::CharString &filepath,
							const fpreal frame,
							const VrsceneSettings *settings = nullptr) {
		VrsceneSettings vrsceneSettings = getCorrectSettings(settings);
		
		VrsceneDesc *tempDesc = vrsceneMan.getVrsceneDesc(filepath, &vrsceneSettings);

		if (tempDesc) {
			int key = frame * 1000;
			GU_Detail *tempDetail = vrsceneCache[filepath.ptr()][vrsceneSettings].frameDetailMap[key];
			if (tempDetail) {
				return tempDetail;
			}
		}
		
		return nullptr;
	}

	/// Checks if filepath and Vrscene Settings pair is cached, 
	/// if it is not removes cached vrscene description in vrscene manager asociated with the filepath
	void deleteUncachedResources(const VUtils::CharString &filepath, const VrsceneSettings *settings = nullptr) {
		VrsceneSettings vrsceneSettings = getCorrectSettings(settings);
		
		if (!filepath.empty()) {
			VUtils::StringHashMap<CacheElementMap>::iterator iterator = vrsceneCache.find(filepath.ptr());
			if ( iterator == vrsceneCache.end() ||
				iterator.data()[vrsceneSettings].references < 1) {
				Log::getLog().debug("deleting: %s", filepath.ptr());
				vrsceneMan.delVrsceneDesc(filepath);
			}
		}
	}

	/// Unregister a specific filepath and Vrscene settings pair from cache
	/// deletes cached data upon reference count reaching 0
	void unregister(const VUtils::CharString &filepath, const VrsceneSettings *settings = nullptr) {
		if (filepath.empty()) {
			return;
		}
		VrsceneSettings vrsceneSettings = getCorrectSettings(settings);

		CacheElementMap &tempVal = vrsceneCache[filepath.ptr()];
		if (--tempVal[vrsceneSettings].references < 1) {
			deleteFrameData(filepath, vrsceneSettings);
			Log::getLog().debug("deleting: %s", filepath.ptr());
			vrsceneMan.delVrsceneDesc(filepath);
			tempVal.erase(vrsceneSettings);
			if (tempVal.empty())
				vrsceneCache.erase(filepath.ptr());
		}

	}

	/// Save GU_Detail for specific filepath and vrscene settings pair
	/// @param filepath[in] - Path to VRay scene file, used as ID for map
	/// @param frame[in] - Frame at which provided detail is required
	/// @param detail [in] - GU_Detail pointer to be cached
	/// @param settings[in] - Settings for the cached data, used as seconday ID
	void setDetail(const VUtils::CharString &filepath, const fpreal &frame, GU_Detail &detail, const VrsceneSettings *settings = nullptr) {
		vassert(!filepath.empty());

		VrsceneSettings vrsceneSettings = getCorrectSettings(settings);
		int key = frame * 1000;
		vrsceneCache[filepath.ptr()][vrsceneSettings].frameDetailMap[key] = &detail;
	}

	/// Generate default Vrscene Settings
	static VrsceneSettings getDefaultSettings() {
		VrsceneSettings tempSettings;
		tempSettings.usePreview = true;
		tempSettings.previewFacesCount = 100000;
		tempSettings.cacheSettings.cacheType = VrsceneCacheSettings::VrsceneCacheType::VrsceneCacheTypeRam;

		return tempSettings;
	}

private:
	/// Delete all cached data associated with a given set of filepath + vrscene settings
	/// @param filepath[in] - Path to VRay scene file, used as ID for map
	/// @param settings[in] - Settings for the cached data, used as seconday ID
	void deleteFrameData(const VUtils::CharString &filepath, const VrsceneSettings &settings) {
		typedef VUtils::HashMap<int, GU_Detail*> DetailMap;
		FOR_IT(DetailMap, detailIt, vrsceneCache[filepath.ptr()][settings].frameDetailMap) {
			delete detailIt.data();
			detailIt.data() = nullptr;
		}
	}

	

	static VrsceneSettings getCorrectSettings(const VrsceneSettings *settings) {
		return settings ? *settings : getDefaultSettings();
	}

	struct CacheElement {
		CacheElement()
			: references(0)
		{}

		bool operator == (const CacheElement &other) {
			return references == other.references;
		}

		int references;
		VUtils::HashMap<int, GU_Detail*> frameDetailMap;
	};

	struct VrsceneSettingsHasher {
		uint32 operator()(const VrsceneSettings &key) const {
#pragma pack(push, 1)
			struct SettingsKey {
				int usePreview;
				int previewFacesCount;
				int minPreviewFaces;
				int masPreviewFaces;
				int previewType;
				int previewFlags;
			} settingsKey = { key.usePreview
				, key.previewFacesCount
				, key.minPreviewFaces
				, key.maxPreviewFaces
				, key.previewType
				, key.previewFlags 
			};
#pragma pack(pop)

			Hash::MHash data;
			Hash::MurmurHash3_x86_32(&settingsKey, sizeof(SettingsKey), 42, &data);
			return data;
		}
	};

	typedef VUtils::HashMap<VrsceneSettings, CacheElement, VrsceneSettingsHasher> CacheElementMap;
	VUtils::StringHashMap<CacheElementMap> vrsceneCache;

	VUTILS_DISABLE_COPY(DetailCache)
}vrsCache;

/// Converts "flip_axis" saved as a string parameter to its corresponding
/// FlipAxisMode enum value.
/// @flipAxisModeS The value of the flip_axis parameter
/// @returns The corresponding to flipAxisModeS enum value
static FlipAxisMode parseFlipAxisMode(const UT_String &flipAxisModeS)
{
	FlipAxisMode mode = FlipAxisMode::none;

	if (flipAxisModeS.isInteger()) {
		mode = static_cast<FlipAxisMode>(flipAxisModeS.toInt());
	}

	return mode;
}

static GA_PrimitiveTypeId theTypeId(-1);
static VRayBaseRefFactory<VRaySceneRef> theFactory("VRaySceneRef");

void VRaySceneRef::install(GA_PrimitiveFactory *primFactory)
{
	theTypeId = theFactory.install(*primFactory, theFactory);
}

VRaySceneRef::VRaySceneRef() 
	: vrsceneFile("")
{}

VRaySceneRef::VRaySceneRef(const VRaySceneRef &src)
	: VRaySceneRefBase(src)
	, vrsceneFile(src.vrsceneFile)
{
	vrsCache.registerInCache(vrsceneFile);
}

VRaySceneRef::~VRaySceneRef()
{
	vrsCache.unregister(vrsceneFile);
}

GA_PrimitiveTypeId VRaySceneRef::typeId()
{
	return theTypeId;
}

GU_PackedFactory *VRaySceneRef::getFactory() const
{
	return &theFactory;
}

GU_PackedImpl *VRaySceneRef::copy() const
{
	return new VRaySceneRef(*this);
}

bool VRaySceneRef::unpack(GU_Detail&) const
{
	// This will show error and indicate that we don't support unpacking.
	return false;
}

double VRaySceneRef::getFrame(fpreal t) const
{
	const int useAnimOverrides = getAnimOverride();
	if (useAnimOverrides) {
		int animLength = getAnimLength();
		if (animLength <= 0) {
			animLength = 100;
		}

		t = VUtils::calcFrameIndex(t,
			static_cast<VUtils::MeshFileAnimType::Enum>(
								   getAnimType()),
								   getAnimStart(),
								   animLength,
								   getAnimOffset(),
								   getAnimSpeed());
	}

	return t;
}

int VRaySceneRef::detailRebuild(VrsceneDesc *vrsceneDesc, int shouldFlip)
{
	const fpreal t = getFrame(getCurrentFrame());

	// Detail for the mesh
	GU_Detail *meshDetail = new GU_Detail();

	int meshVertexOffset = 0;
	m_bbox.initBounds();

	FOR_IT (VrsceneObjects, obIt, vrsceneDesc->m_objects) {
		VrsceneObjectBase *ob = obIt.data();
		if (ob && ob->getType() == ObjectTypeNode) {
			const VUtils::TraceTransform &tm = ob->getTransform(t);

			VrsceneObjectNode     *node = static_cast<VrsceneObjectNode*>(ob);
			VrsceneObjectDataBase *nodeData = node->getData();
			if (nodeData && nodeData->getDataType() == ObjectDataTypeMesh) {
				VrsceneObjectDataMesh *mesh = static_cast<VrsceneObjectDataMesh*>(nodeData);

				const VUtils::VectorRefList &vertices = mesh->getVertices(t);
				const VUtils::IntRefList    &faces = mesh->getFaces(t);

				// Allocate the points, this is the offset of the first one
				GA_Offset pointOffset = meshDetail->appendPointBlock(vertices.count());

				// Iterate through points by their offsets
				for (int v = 0; v < vertices.count(); ++v, ++pointOffset) {
					VUtils::Vector vert = tm * vertices[v];
					if (shouldFlip) {
						vert = flipMatrixZY * vert;
					}

					const UT_Vector3 utVert(vert.x, vert.y, vert.z);

					m_bbox.enlargeBounds(utVert);

					meshDetail->setPos3(pointOffset, utVert);
				}

				for (int f = 0; f < faces.count(); f += 3) {
					GU_PrimPoly *poly = GU_PrimPoly::build(meshDetail, 3, GU_POLY_CLOSED, 0);
					for (int c = 0; c < 3; ++c) {
						poly->setVertexPoint(c, meshVertexOffset + faces[f + c]);
					}
					poly->reverse();
				}

				meshVertexOffset += vertices.count();
			}
		}
	}
	if (m_options.getOptionI("cache")) {
		VrsceneSettings settings = vrsCache.getDefaultSettings();
		if (shouldFlip) {
			settings.previewFlags |= (1<<7);// indicate that the detail is flipped axis wise
		}
		vrsCache.setDetail(vrsceneFile, t, *meshDetail, new VrsceneSettings(settings));

		m_detail.allocateAndSet(meshDetail, false);
	}
	else {
		m_detail.allocateAndSet(meshDetail);
	}

	return true;
}

int VRaySceneRef::detailRebuild()
{
	int res;
	if (getCache()) {
		if (vrsceneFile != getFilepath()) {
			vrsCache.unregister(vrsceneFile);
			vrsceneFile = getFilepath();
			vrsCache.registerInCache(vrsceneFile);
		}
	}
	else {
		if (!vrsceneFile.empty()) {
			vrsCache.unregister(vrsceneFile);
			vrsceneFile.clear();
		}
	}

	VrsceneDesc *vrsceneDesc = vrsceneMan.getVrsceneDesc(getFilepath());
	if (!vrsceneDesc) {
		detailClear();
		res = true;
	}
	else {
		// Update flip axis intrinsic.
		const FlipAxisMode flipAxis = parseFlipAxisMode(getFlipAxis());
		const bool shouldFlip = flipAxis == FlipAxisMode::flipZY ||
			                    flipAxis == FlipAxisMode::automatic && vrsceneDesc->getUpAxis() == vrsceneUpAxisZ;
		setShouldFlip(shouldFlip);

		if (!getAddNodes()) {
			detailClear();
			res = true;
		}
		else {
			VrsceneSettings settings = vrsCache.getDefaultSettings();
			if (shouldFlip) {
				settings.previewFlags |= (1<<7);
			}
			
			GU_Detail* temp = vrsCache.getDetail(vrsceneFile, getFrame(getCurrentFrame()), new VrsceneSettings(settings));
			if (!temp) {
				res = detailRebuild(vrsceneDesc, shouldFlip);
			}
			else {
				// false due to the fact it is cached
				m_detail.allocateAndSet(temp, false);
			}
			res = true;
			vrsCache.deleteUncachedResources(getFilepath());
		}
	}

	return res;
}
