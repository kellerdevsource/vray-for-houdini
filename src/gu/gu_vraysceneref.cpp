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
#include "vfh_hashes.h"
#include "vfh_gu_cache.h"

#include "gu_vraysceneref.h"

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

struct SettingsWrapper {
	SettingsWrapper()
		: flipAxis(0)
	{}

	SettingsWrapper(const VrsceneSettings &other)
		: settings(other)
		, flipAxis(0)
	{}

	SettingsWrapper(const VrsceneSettings &other, bool flip)
		: settings(other)
		, flipAxis(flip)
	{}

	bool operator == (const SettingsWrapper &other) const {
		return (settings == other.settings &&
			flipAxis == other.flipAxis);
	}

	bool operator !=(const SettingsWrapper &other) {
		return !(*this == other);
	}

	VrsceneSettings settings;
	int flipAxis;
};

struct VrsceneSettingsHasher {
	uint32 operator()(const SettingsWrapper &key) const {
#pragma pack(push, 1)
		struct SettingsKey {
			int usePreview;
			int previewFacesCount;
			int minPreviewFaces;
			int masPreviewFaces;
			int previewType;
			int previewFlags;
			int shouldFlip;
		} settingsKey = { key.settings.usePreview
			, key.settings.previewFacesCount
			, key.settings.minPreviewFaces
			, key.settings.maxPreviewFaces
			, key.settings.previewType
			, key.settings.previewFlags
			, key.flipAxis
		};
#pragma pack(pop)

		Hash::MHash data;
		Hash::MurmurHash3_x86_32(&settingsKey, sizeof(SettingsKey), 42, &data);
		return data;
	}
};

struct ReturnSettings {
	explicit ReturnSettings(UT_BoundingBox &box)
		: box(box)
		, shouldFlip(false)
		, flipAxis(FlipAxisMode::none)
		, clearDetail(false)
	{}

	UT_BoundingBox &box;
	bool shouldFlip;
	FlipAxisMode flipAxis;
	bool clearDetail;
};

class VrsceneDescBuilder
	: public DetailBuilder<SettingsWrapper, ReturnSettings>
{
public:
	GU_DetailHandle buildDetail(const VUtils::CharString &filepath, const SettingsWrapper &settings, fpreal frame, ReturnSettings &rvalue) override {
		VrsceneDesc *vrsceneDesc = vrsceneMan.getVrsceneDesc(filepath, &settings.settings);

		if (!vrsceneDesc) {
			rvalue.clearDetail = true;
			return GU_DetailHandle();
		}

		rvalue.shouldFlip = rvalue.flipAxis == FlipAxisMode::flipZY ||
			rvalue.flipAxis == FlipAxisMode::automatic && vrsceneDesc->getUpAxis() == vrsceneUpAxisZ;

		return build(vrsceneDesc, rvalue.shouldFlip, frame, rvalue);
	}

	void cleanResource(const VUtils::CharString &filepath) override {}

private:
	static GU_DetailHandle build(VrsceneDesc *vrsceneDesc, int shouldFlip, const fpreal &t, ReturnSettings &rvalue) {
		// Detail for the mesh
		GU_Detail *meshDetail = new GU_Detail();

		int meshVertexOffset = 0;
		rvalue.box.initBounds();

		FOR_IT(VrsceneObjects, obIt, vrsceneDesc->m_objects) {
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

						rvalue.box.enlargeBounds(utVert);

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

		GU_DetailHandle detail;
		detail.allocateAndSet(meshDetail);

		return detail;
	}
} builder;

static DetailCachePrototype<ReturnSettings, SettingsWrapper, VrsceneSettingsHasher> cache(builder);

static SettingsWrapper getDefaultSettings() {
	VrsceneSettings tempSettings;
	tempSettings.usePreview = true;
	tempSettings.previewFacesCount = 100000;
	tempSettings.cacheSettings.cacheType = VrsceneCacheSettings::VrsceneCacheType::VrsceneCacheTypeNone;

	return tempSettings;
}

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

typedef VRayBaseRefFactory<VRaySceneRef> VRaySceneRefFactory;

static GA_PrimitiveTypeId theTypeId(-1);
static VRaySceneRefFactory theFactory("VRaySceneRef");

void VRaySceneRef::install(GA_PrimitiveFactory *primFactory)
{
	theTypeId = VRaySceneRefFactory::install(*primFactory, theFactory);

	VRayBaseRefCollect::install(theTypeId);
}

VRaySceneRef::VRaySceneRef() 
	: vrsceneFile("")
	, vrsSettings(getDefaultSettings().settings)
	, shouldFlipAxis(false)
{}

VRaySceneRef::VRaySceneRef(const VRaySceneRef &src)
	: VRaySceneRefBase(src)
	, vrsceneFile(src.vrsceneFile)
	, vrsSettings(src.vrsSettings)
	, shouldFlipAxis(src.shouldFlipAxis)
{
	cache.registerInCache(vrsceneFile, SettingsWrapper(vrsSettings, shouldFlipAxis));
}

VRaySceneRef::~VRaySceneRef()
{
	cache.unregister(vrsceneFile, SettingsWrapper(vrsSettings, shouldFlipAxis));
	if (!cache.isCached(vrsceneFile)) {
		vrsceneMan.delVrsceneDesc(vrsceneFile);
	}
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

void VRaySceneRef::updateCacheRelatedVars() {
	if (getCache()) {
		if (vrsceneFile != getFilepath() ) {
			cache.registerInCache(getFilepath(), SettingsWrapper(vrsSettings, shouldFlipAxis));
			cache.unregister(vrsceneFile, SettingsWrapper(vrsSettings, shouldFlipAxis));
			vrsceneFile = getFilepath();
		}
	}
	else {
		if (!vrsceneFile.empty()) {
			cache.unregister(vrsceneFile, SettingsWrapper(vrsSettings, shouldFlipAxis));
			vrsceneFile.clear();
		}
	}
}

int VRaySceneRef::detailRebuild()
{
	int res = false;
	updateCacheRelatedVars();

	ReturnSettings update(m_bbox);
	update.flipAxis = parseFlipAxisMode(getFlipAxis());

	m_detail = cache.getDetail(getFilepath(), SettingsWrapper(vrsSettings, shouldFlipAxis), getFrame(getCurrentFrame()), update);

	if (!update.clearDetail && getAddNodes()) {
		// Update flip axis intrinsic.
		setShouldFlip(update.shouldFlip);
		if (getCache() && shouldFlipAxis != update.shouldFlip) {
			cache.registerInCache(vrsceneFile, SettingsWrapper(vrsSettings, update.shouldFlip));
			cache.unregister(vrsceneFile, SettingsWrapper(vrsSettings, shouldFlipAxis));
			shouldFlipAxis = update.shouldFlip;
		}

		res = m_detail.isValid();
	}
	else {
		detailClear();
		res = true;
	}

	return res;
}
