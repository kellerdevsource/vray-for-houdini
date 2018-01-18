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
		long references;
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

VRaySceneRef::VRaySceneRef() :vrsceneFile("")
{}

VRaySceneRef::VRaySceneRef(const VRaySceneRef &src)
	: VRaySceneRefBase(src), vrsceneFile("")
{}

VRaySceneRef::~VRaySceneRef()
{
	vrsceneCache.unregister(vrsceneFile);
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
	VrsceneDesc *vrsceneDesc;
	if (m_options.getOptionI("cache")) {
		if (vrsceneFile != getFilepath()) {
			vrsceneCache.unregister(vrsceneFile);
			vrsceneFile = getFilepath();
			vrsceneCache.registerInCache(vrsceneFile);
		}
		vrsceneDesc = vrsceneCache.getCachedSceneDesc(vrsceneFile);
	}
	else {
		VrsceneSettings vrsceneSettings;
		vrsceneSettings.usePreview = true;
		vrsceneSettings.previewFacesCount = 100000;
		vrsceneSettings.cacheSettings.cacheType = VrsceneCacheSettings::VrsceneCacheType::VrsceneCacheTypeRam;
		vrsceneDesc = vrsceneMan.getVrsceneDesc(getFilepath(), &vrsceneSettings);
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
			res = detailRebuild(vrsceneDesc, shouldFlip);
			vrsceneCache.deleteUncachedResources(getFilepath());
		}
	}

	return res;
}
