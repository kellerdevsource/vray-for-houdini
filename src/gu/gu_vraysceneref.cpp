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

static class VrsceneDescCachePrototype {
public:
	VrsceneDescCachePrototype() {}

	/// Register that an instance has a reference to a specific vrscene file
	/// @param filepath [in] - Path to VRay scene file
	/// @param settings [in] - Custom settings for the vrscene, if not provided default settings are used
	void registerInCache(const VUtils::CharString &filepath, const VrsceneSettings *settings = nullptr) {
		if (filepath.empty()) {
			return;
		}

		if (!settings) {
			VrsceneSettings vrsceneSettings;
			vrsceneSettings = getDefaultSettings();

			vrsceneCache[filepath.ptr()][vrsceneSettings].references++;
			Log::getLog().debug("references after increment: %d", vrsceneCache[filepath.ptr()][vrsceneSettings].references);
		}
		else {
			vrsceneCache[filepath.ptr()][*settings].references++;
			Log::getLog().debug("references after increment: %d", vrsceneCache[filepath.ptr()][*settings].references);
		}
	}

	GU_Detail* getDetail(const VUtils::CharString &filepath,
							const fpreal frame, 
							UT_BoundingBox &bbox,
							const VrsceneSettings *settings = nullptr, 
							const bool shouldFlip = false) {

		VrsceneSettings vrsceneSettings;
		if (settings) {
			vrsceneSettings = *settings;
		}
		else {
			vrsceneSettings = getDefaultSettings();
		}

		VrsceneDesc *tempDesc = vrsceneMan.getVrsceneDesc(filepath, &vrsceneSettings);

		if (!tempDesc) {
			return nullptr;
		}

		if (vrsceneCache[filepath.ptr()][vrsceneSettings].vrsceneDesc != tempDesc) {
			return rebuildDetail(filepath, vrsceneSettings, frame, tempDesc, bbox, shouldFlip);
		}
		else if (!vrsceneCache[filepath.ptr()][vrsceneSettings].frameDetailMap[frame]) {
			return buildDetailForFrame(filepath, vrsceneSettings, frame, tempDesc, bbox, shouldFlip);
		}

		return vrsceneCache[filepath.ptr()][vrsceneSettings].frameDetailMap[frame];
	}

	/// Checks if filepath and Vrscene Settings pair is cached, 
	/// if it is not removes cached vrscene description in vrscene manager asociated with the filepath
	void deleteUncachedResources(const VUtils::CharString &filepath, const VrsceneSettings *settings = nullptr) {
		VrsceneSettings vrsceneSettings;
		if (settings) {
			vrsceneSettings = *settings;
		}
		else {
			vrsceneSettings = getDefaultSettings();
		}
		if (!filepath.empty() && (
			vrsceneCache.find(filepath.ptr()) == vrsceneCache.end() ||
			vrsceneCache[filepath.ptr()][vrsceneSettings].references < 1)) {
			vrsceneMan.delVrsceneDesc(filepath);
			Log::getLog().debug("deleting uncached vrscene: %s", filepath.ptr());
		}
	}

	/// Unregister a specific filepath and Vrscene settings pair from cache
	/// deletes cached data upon reference count reaching 0
	void unregister(const VUtils::CharString &filepath, const VrsceneSettings *settings = nullptr) {
		if (filepath.empty()) {
			return;
		}
		VrsceneSettings vrsceneSettings;
		if (settings) {
			vrsceneSettings = *settings;
		}
		else {
			vrsceneSettings = getDefaultSettings();
		}
		int references = vrsceneCache[filepath.ptr()][vrsceneSettings].references;
		if (--vrsceneCache[filepath.ptr()][vrsceneSettings].references < 1) {
			Log::getLog().debug("deleting cached vrscene: %s", filepath.ptr());
			vrsceneMan.delVrsceneDesc(filepath);
		}

	}

private:

	/// Generate default Vrscene Settings
	static VrsceneSettings& getDefaultSettings() {
		VrsceneSettings tempSettings;
		tempSettings.usePreview = true;
		tempSettings.previewFacesCount = 100000;
		tempSettings.cacheSettings.cacheType = VrsceneCacheSettings::VrsceneCacheType::VrsceneCacheTypeRam;

		return tempSettings;
	}

	/// Reaquire vrscene Desctiption due to it being invalidated
	GU_Detail* rebuildDetail(const VUtils::CharString &filepath,
								const VrsceneSettings &settings, 
								const fpreal frame, 
								VrsceneDesc *desc, 
								UT_BoundingBox &bbox,
								const bool shouldFlip = false) {

		vrsceneCache[filepath.ptr()][settings].frameDetailMap.empty();
		vrsceneCache[filepath.ptr()][settings].vrsceneDesc = desc;

		//Build Detail for specific frame
		return buildDetailForFrame(filepath, settings, frame, desc, bbox, shouldFlip);
	}

	GU_Detail* buildDetailForFrame(const VUtils::CharString &filepath, 
							const VrsceneSettings &settings, 
							const fpreal frame, 
							VrsceneDesc *desc,
							UT_BoundingBox &bbox,
							const bool shouldFlip = false) {

		// Detail for the mesh
		GU_Detail *meshDetail = new GU_Detail();

		int meshVertexOffset = 0;
		bbox.initBounds();

		FOR_IT(VrsceneObjects, obIt, desc->m_objects) {
			VrsceneObjectBase *ob = obIt.data();
			if (ob && ob->getType() == ObjectTypeNode) {
				const VUtils::TraceTransform &tm = ob->getTransform(frame);

				VrsceneObjectNode     *node = static_cast<VrsceneObjectNode*>(ob);
				VrsceneObjectDataBase *nodeData = node->getData();
				if (nodeData && nodeData->getDataType() == ObjectDataTypeMesh) {
					VrsceneObjectDataMesh *mesh = static_cast<VrsceneObjectDataMesh*>(nodeData);

					const VUtils::VectorRefList &vertices = mesh->getVertices(frame);
					const VUtils::IntRefList    &faces = mesh->getFaces(frame);

					// Allocate the points, this is the offset of the first one
					GA_Offset pointOffset = meshDetail->appendPointBlock(vertices.count());

					// Iterate through points by their offsets
					for (int v = 0; v < vertices.count(); ++v, ++pointOffset) {
						VUtils::Vector vert = tm * vertices[v];
						if (shouldFlip) {
							vert = flipMatrixZY * vert;
						}

						const UT_Vector3 utVert(vert.x, vert.y, vert.z);

						bbox.enlargeBounds(utVert);

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

		vrsceneCache[filepath.ptr()][settings].frameDetailMap[frame] = meshDetail;
		//Log::getLog().debug("Caching: %s, at frame %d, with hash: %d", filepath.ptr(), frame, vrsceneCache[filepath.ptr()].hash(settings));
		//Log::getLog().debug("size of cache element map: %d", vrsceneCache[filepath.ptr()].size());
		return meshDetail;
	}

	struct CacheElement {
		CacheElement()
			: references(0)
			, vrsceneDesc(nullptr)
		{}

		bool operator == (const CacheElement &other) {
			return (references == other.references && 
					vrsceneDesc == other.vrsceneDesc);
		}

		int references;
		VUtils::HashMap<fpreal, GU_Detail*> frameDetailMap;
		VrsceneDesc *vrsceneDesc;
	};
	struct VrsceneSettingsHasher {
		uint32 operator()(const VrsceneSettings &key) const {
			uint32 data = -1;
			data += key.usePreview;
			data >> 3;
			data += key.previewFacesCount;
			data >> 3;
			data += key.minPreviewFaces;
			data >> 3;
			data += key.maxPreviewFaces;
			data >> 3;
			data += key.previewFacesCount;
			data >> 3;
			data += key.previewType;
			data >> 3;
			data += key.previewFlags;
			data >> 3;
			return data;
		}
	};
	VUtils::StringHashMap<VUtils::HashMap<VrsceneSettings, CacheElement, VrsceneSettingsHasher>> vrsceneCache;

	VUTILS_DISABLE_COPY(VrsceneDescCachePrototype)
}vrsCache;

static class VrsceneDescCache {
public:
	VrsceneDescCache() {}

	/// Register that an instance has a reference to a specific vrscene file
	/// @param filepath [in] - Path to VRay scene file
	/// @param settings [in] - Custom settings for the vrscene
	void registerInCache(const VUtils::CharString &filepath, const VrsceneSettings *settings = nullptr) {
		if (filepath.empty()) {
			return;
		}

		vrsceneDescCache[filepath.ptr()].references++;
		if (settings) {
			vrsceneDescCache[filepath.ptr()].vrsceneSettings = *settings;
		}
	}

	/// Get cached vrscene description with default vrscene settings
	/// @param filepath [in] - Path to VRay scene file
	VrsceneDesc *getCachedSceneDesc(const VUtils::CharString &filepath) {
		return vrsceneMan.getVrsceneDesc(filepath, &vrsceneDescCache[filepath.ptr()].vrsceneSettings);
	}
	
	/// Get cached vrscene description with custom vrscene settings
	/// @param filepath [in] - Path to VRay scene file
	/// @param settings [in] - Custom settings for the vrscene
	VrsceneDesc *getCachedSceneDesc(const VUtils::CharString &filepath, VrsceneSettings &settings) {
		return vrsceneMan.getVrsceneDesc(filepath, &settings);
	}

	/// Check if vrscene is cached, delete it if it isn't
	/// @param filepath [in] - Path to VRay scene file
	void deleteUncachedResources(const VUtils::CharString &filepath) {
		if (!filepath.empty() && (
			vrsceneDescCache.find(filepath.ptr()) == vrsceneDescCache.end() ||
			vrsceneDescCache[filepath.ptr()].references < 1)) {
			vrsceneMan.delVrsceneDesc(filepath);
		}
	}

	/// Log a reduction in the refference count, delete vrscene description if it reaches 0
	/// @param filepath [in] - Path to VRay scene file
	void unregister(const VUtils::CharString &filepath) {
		if (filepath.empty()) {
			return;
		}

		if (--vrsceneDescCache[filepath.ptr()].references < 1) {
			vrsceneMan.delVrsceneDesc(filepath);
		}

	}
private:
	struct CacheElement {
		CacheElement() : references(0)
		{
			vrsceneSettings.usePreview = true;
			vrsceneSettings.previewFacesCount = 100000;
			vrsceneSettings.cacheSettings.cacheType = VrsceneCacheSettings::VrsceneCacheType::VrsceneCacheTypeRam;
		}

		VrsceneSettings vrsceneSettings;
		int references;
	};

	VUtils::StringHashMap<CacheElement> vrsceneDescCache;

	VUTILS_DISABLE_COPY(VrsceneDescCache)
}vrsceneCache;

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
	: VRaySceneRefBase(src), 
	vrsceneFile("")
{}

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

	// TODO: Detail caching.
	m_detail.allocateAndSet(meshDetail);

	return true;
}

int VRaySceneRef::detailRebuild()
{
	int res;
	VrsceneDesc *vrsceneDesc = vrsceneMan.getVrsceneDesc(getFilepath());
	if (vrsceneFile != getFilepath()) {
		vrsCache.unregister(vrsceneFile);
		vrsceneFile = getFilepath();
		vrsCache.registerInCache(vrsceneFile);
	}

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
			// nullptr to simply generate the default vrsceneSettings
			m_detail.allocateAndSet(vrsCache.getDetail(vrsceneFile, getFrame(getCurrentFrame()), m_bbox, nullptr, shouldFlip), false);
			res = true;
			//res = detailRebuild(vrsceneDesc, shouldFlip);
			vrsCache.deleteUncachedResources(getFilepath());
		}
	}

	return res;
}
