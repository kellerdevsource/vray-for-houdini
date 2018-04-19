//
// Copyright (c) 2015-2018, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#include "vfh_vray.h"
#include "vfh_defines.h"
#include "vfh_gu_cache.h"

#include "gu_vraysceneref.h"

#include <GU/GU_PackedFactory.h>
#include <GU/GU_PrimPacked.h>
#include <GU/GU_PrimPoly.h>
#include <GU/GU_PackedContext.h>
#include <GU/GU_PackedGeometry.h>

using namespace VRayForHoudini;
using namespace VUtils::Vrscene::Preview;

/// *.vrscene preview data manager.
VrsceneDescManager VRayForHoudini::vrsceneMan(NULL);

struct ReturnSettings {
	explicit ReturnSettings(UT_BoundingBox &box)
		: box(box)
		, clearDetail(false)
	{}

	UT_BoundingBox &box;
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

		return build(vrsceneDesc, settings, frame, rvalue);
	}

	void cleanResource(const VUtils::CharString &filepath) override {
		vrsceneMan.delVrsceneDesc(filepath);
	}

private:
	static GU_DetailHandle build(VrsceneDesc *vrsceneDesc, const SettingsWrapper &settings, const fpreal &t, ReturnSettings &retValue) {
		GU_Detail *meshDetail = new GU_Detail();

		int meshVertexOffset = 0;
		retValue.box.initBounds();

		QList<VrsceneObjectBase*> previewObjects;

		if (settings.objectName.empty()) {
			FOR_IT(VrsceneObjects, obIt, vrsceneDesc->m_objects) {
				previewObjects.append(obIt.data());
			}
		}
		else {
			const VrsceneSceneObject *sceneObject = vrsceneDesc->getSceneObject(settings.objectName.ptr());
			if (sceneObject) {
				const ObjectBaseTable &nodesTable = sceneObject->getObjectNodes();
				for (const VrsceneObjectBase *ob : nodesTable) {
					previewObjects.append(const_cast<VrsceneObjectBase*>(ob));
				}
			}
		}

		for (VrsceneObjectBase *ob : previewObjects) {
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
						if (settings.flipAxis) {
							vert = flipMatrixZY * vert;
						}

						const UT_Vector3 utVert(vert.x, vert.y, vert.z);

						retValue.box.enlargeBounds(utVert);

						meshDetail->setPos3(pointOffset, utVert);
					}

					for (int f = 0; f < faces.count(); f += 3) {
						GU_PrimPoly *poly = GU_PrimPoly::build(meshDetail, 3, GU_POLY_CLOSED, 0);
						poly->setVertexPoint(0, meshVertexOffset + faces[f + 0]);
						poly->setVertexPoint(1, meshVertexOffset + faces[f + 1]);
						poly->setVertexPoint(2, meshVertexOffset + faces[f + 2]);
						poly->reverse();
					}

					meshVertexOffset += vertices.count();
				}
			}
		}

		GU_DetailHandle detail;
		detail.allocateAndSet(meshDetail);

		GU_Detail *gdpPacked = new GU_Detail;
		GU_PackedGeometry::packGeometry(*gdpPacked, detail);

		detail.allocateAndSet(gdpPacked);

		return detail;
	}
};

static VrsceneDescBuilder builder;
static DetailCachePrototype<ReturnSettings, SettingsWrapper> cache(builder);

typedef VRayBaseRefFactory<VRaySceneRef> VRaySceneRefFactory;

static GA_PrimitiveTypeId theTypeId(-1);
static VRaySceneRefFactory theFactory("VRaySceneRef");


bool SettingsWrapper::operator==(const SettingsWrapper &other) const
{
	return settings == other.settings &&
	       objectName == other.objectName &&
	       flipAxis == other.flipAxis;
}

bool SettingsWrapper::operator!=(const SettingsWrapper &other) const
{
	return !(*this == other);
}

bool SettingsWrapper::operator<(const SettingsWrapper &other) const
{
	return settings.previewFacesCount < other.settings.previewFacesCount;
}

Hash::MHash SettingsWrapper::getHash() const
{
	Hash::MHash nameHash = 0;
	if (!objectName.empty()) {
		Hash::MurmurHash3_x86_32(objectName.ptr(), objectName.length(), 42, &nameHash);
	}

#pragma pack(push, 1)
	struct SettingsKey {
		int usePreview;
		int previewFacesCount;
		int minPreviewFaces;
		int masPreviewFaces;
		int previewType;
		uint32 previewFlags;
		int shouldFlip;
		Hash::MHash nameHash;
	} settingsKey = {
		settings.usePreview,
		settings.previewFacesCount,
		settings.minPreviewFaces,
		settings.maxPreviewFaces,
		settings.previewType,
		settings.previewFlags,
		flipAxis,
		nameHash
	};
#pragma pack(pop)

	Hash::MHash keyHash;
	Hash::MurmurHash3_x86_32(&settingsKey, sizeof(SettingsKey), 42, &keyHash);

	return keyHash;
}

void VRaySceneRef::install(GA_PrimitiveFactory *primFactory)
{
	theTypeId = VRaySceneRefFactory::install(*primFactory, theFactory);

	VRayBaseRefCollect::install(theTypeId);
}

VRaySceneRef::VRaySceneRef() 
{}

VRaySceneRef::VRaySceneRef(const VRaySceneRef &src)
	: VRaySceneRefBase(src)
	, filePath(src.filePath)
{
	// TODO: Rework cache registration / deregistration.
}

VRaySceneRef::~VRaySceneRef()
{
	// TODO: Rework cache registration / deregistration.
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

VRay::VUtils::CharStringRefList VRaySceneRef::getObjectNames() const
{
	const SettingsWrapper currentSettings(getSettings());

	VrsceneDesc *vrsceneDesc = vrsceneMan.getVrsceneDesc(getFilepath(), &currentSettings.settings);
	if (!vrsceneDesc)
		return VRay::VUtils::CharStringRefList();

	VRay::VUtils::CharStringRefList namesList;

	const VrsceneSceneObject *sceneObject = vrsceneDesc->getSceneObject(currentSettings.objectName.ptr());
	if (sceneObject) {
		const ObjectBaseTable &nodesTable = sceneObject->getObjectNodes();
		if (nodesTable.count()) {
			namesList = VRay::VUtils::CharStringRefList(nodesTable.count());

			for (int i = 0; i < nodesTable.count(); ++i) {
				const VrsceneObjectBase *ob = nodesTable[i];
				namesList[i].set(ob->getPluginName());
			}
		}
	}

	return namesList;
}

double VRaySceneRef::getFrame(fpreal t) const
{
	const int useAnimOverrides = getAnimOverride();
	if (useAnimOverrides) {
		int animLength = getAnimLength();
		if (animLength <= 0) {
			animLength = 100;
		}

		t = calcFrameIndex(
			t,
			static_cast<VUtils::MeshFileAnimType::Enum>(getAnimType()),
			getAnimStart(),
			animLength,
			getAnimOffset(),
			getAnimSpeed()
		);
	}

	return t;
}

SettingsWrapper VRaySceneRef::getSettings() const
{
	SettingsWrapper settings;
	settings.objectName = getObjectName();
	settings.flipAxis = getShouldFlip();
	settings.settings.usePreview = true;
	settings.settings.previewFacesCount = 10000;
	settings.settings.cacheSettings.cacheType = VrsceneCacheSettings::VrsceneCacheType::VrsceneCacheTypeNone;
	return settings;
}

int VRaySceneRef::detailRebuild()
{
	const SettingsWrapper cacheKey(getSettings());

	ReturnSettings retValue(m_bbox);

	m_detail = cache.getDetail(getFilepath(), cacheKey, getFrame(getCurrentFrame()), retValue);

	// XXX: Rework cache registration / deregistration.

	int res;
	if (!retValue.clearDetail && getAddNodes()) {
		res = m_detail.isValid();
	}
	else {
		detailClear();
		res = true;
	}

	return res;
}
