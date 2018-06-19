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
#include <GU/GU_PrimNURBCurve.h>
#include <GU/GU_PackedContext.h>
#include <GU/GU_PackedGeometry.h>

using namespace VRayForHoudini;
using namespace VUtils::Vrscene::Preview;

typedef VRayBaseRefFactory<VRaySceneRef> VRaySceneRefFactory;

/// *.vrscene preview data manager.
VrsceneDescManager VRayForHoudini::vrsceneMan(NULL);

static GA_PrimitiveTypeId theTypeId(-1);

struct ReturnSettings {
	explicit ReturnSettings(UT_BoundingBox &box)
		: box(box)
		, clearDetail(false)
	{}

	UT_BoundingBox &box;
	bool clearDetail;
};

/// A list of VrsceneObjectBase instances.
typedef QList<VrsceneObjectBase*> VrsceneObjectBaseList;

/// Recursively append VrsceneObjectBase from VrsceneSceneObjects children.
/// @param object Scene object.
/// @param[out] objectList VrsceneObjectBase list.
static void appendVrsceneSceneObjectPlugins(const VrsceneSceneObject &object, VrsceneObjectBaseList &objectList)
{
	const ObjectBaseTable &plugins = object.getObjectPlugins();
	const VrsceneSceneObjects &children = object.getChildren();

	if (!children.empty()) {
		FOR_CONST_IT(VrsceneSceneObjects, it, children) {
			appendVrsceneSceneObjectPlugins(*it.data(), objectList);
		}
	}

	for (const VrsceneObjectBase *ob : plugins) {
		objectList.append(const_cast<VrsceneObjectBase*>(ob));
	}
}

static void appendMesh(GU_Detail &gdp, VrsceneObjectBase &ob, int flipAxis, UT_BoundingBox &bbox, fpreal t)
{
	VrsceneObjectNode &node = static_cast<VrsceneObjectNode&>(ob);
	if (!node.getVisibility(t))
		return;

	VrsceneObjectDataBase *nodeData = node.getData();
	if (nodeData && nodeData->getDataType() == ObjectDataTypeMesh) {
		VrsceneObjectDataMesh *mesh = static_cast<VrsceneObjectDataMesh*>(nodeData);
		const VUtils::TraceTransform &tm = ob.getTransform(t);

		const VUtils::VectorRefList &vertices = mesh->getVertices(t);
		const VUtils::IntRefList &faces = mesh->getFaces(t);

		// Allocate the points, this is the offset of the first one
		const GA_Offset pointOffset = gdp.appendPointBlock(vertices.count());

		// Iterate through points by their offsets
		for (int v = 0; v < vertices.count(); ++v) {
			VUtils::Vector vert = tm * vertices[v];
			if (flipAxis) {
				vert = flipMatrixZY * vert;
			}

			const UT_Vector3 utVert(vert.x, vert.y, vert.z);

			bbox.enlargeBounds(utVert);

			gdp.setPos3(pointOffset + v, utVert);
		}

		for (int f = 0; f < faces.count(); f += 3) {
			GU_PrimPoly *poly = GU_PrimPoly::build(&gdp, 3, GU_POLY_CLOSED, false);
			poly->setVertexPoint(0, pointOffset + faces[f + 0]);
			poly->setVertexPoint(1, pointOffset + faces[f + 1]);
			poly->setVertexPoint(2, pointOffset + faces[f + 2]);
			poly->reverse();
		}
	}
}

static UT_Vector3 tmVector(const VUtils::Vector &value, const VUtils::TraceTransform &tm)
{
	const VUtils::Vector vec = tm * value;
	return UT_Vector3(vec.x, vec.y, vec.z);
}

static UT_Vector3 tmVector(float x, float y, float z, const VUtils::TraceTransform &tm)
{
	return tmVector(VUtils::Vector(x, y, z), tm);
}

static void appendShapeLine(GU_Detail &gdp, const VUtils::TraceTransform &tm, const VUtils::Vector &from, const VUtils::Vector &to)
{
	const GA_Offset pointOffset = gdp.appendPointBlock(2);
	gdp.setPos3(pointOffset+0, tmVector(from, tm));
	gdp.setPos3(pointOffset+1, tmVector(to, tm));

	GU_PrimNURBCurve *line = GU_PrimNURBCurve::build(&gdp, 2, 2, false, false, false);
	line->setVertexPoint(0, pointOffset+0);
	line->setVertexPoint(1, pointOffset+1);
}

static void appendShapeRectangle(GU_Detail &gdp, const VUtils::TraceTransform &tm, float uSize, float vSize)
{
	const GA_Offset pointOffset = gdp.appendPointBlock(4);
	gdp.setPos3(pointOffset+0, tmVector(-uSize,  vSize, 0.0f, tm));
	gdp.setPos3(pointOffset+1, tmVector( uSize,  vSize, 0.0f, tm));
	gdp.setPos3(pointOffset+2, tmVector( uSize, -vSize, 0.0f, tm));
	gdp.setPos3(pointOffset+3, tmVector(-uSize, -vSize, 0.0f, tm));

	GU_PrimNURBCurve *rect = GU_PrimNURBCurve::build(&gdp, 5, 2, false, false, false);
	rect->setVertexPoint(0, pointOffset+0);
	rect->setVertexPoint(1, pointOffset+1);
	rect->setVertexPoint(2, pointOffset+2);
	rect->setVertexPoint(3, pointOffset+3);
	rect->setVertexPoint(4, pointOffset+0);
}

static void appendLightRectragle(GU_Detail &gdp, VrsceneObjectLight &light, const VUtils::TraceTransform &tm, UT_BoundingBox &bbox, fpreal t)
{
	vassert(light.getLightType() == LightTypeRectangle);

	VrsceneObjectLightRectangle &rl = static_cast<VrsceneObjectLightRectangle&>(light);

	const float uSize = rl.getSizeU(t);
	const float vSize = rl.getSizeV(t);

	const float arrowLength = sqrtf(uSize*uSize+vSize*vSize)*0.75f;
	const float arrowSpread = arrowLength*0.2f;
	const float arrowBack = arrowLength*0.8f;

	// Rectangle
	appendShapeRectangle(gdp, tm, uSize, vSize);

	// Cross
	appendShapeLine(gdp, tm, VUtils::Vector(uSize, vSize, 0.0f),VUtils::Vector(-uSize, -vSize, 0.0f));
	appendShapeLine(gdp, tm, VUtils::Vector(uSize, -vSize, 0.0f), VUtils::Vector(-uSize, vSize, 0.0f));

	// Arrow
	appendShapeLine(gdp, tm, VUtils::Vector(0.0f, 0.0f, 0.0f), VUtils::Vector(0.0f, 0.0f, -arrowLength));
	appendShapeLine(gdp, tm, VUtils::Vector(arrowSpread, 0.0f, -arrowBack), VUtils::Vector(0.0f, 0.0f, -arrowLength));
	appendShapeLine(gdp, tm, VUtils::Vector(-arrowSpread, 0.0f, -arrowBack), VUtils::Vector(0.0f, 0.0f, -arrowLength));
	appendShapeLine(gdp, tm, VUtils::Vector(0.0f, arrowSpread, -arrowBack), VUtils::Vector(0.0f, 0.0f, -arrowLength));
	appendShapeLine(gdp, tm, VUtils::Vector(0.0f, -arrowSpread, -arrowBack), VUtils::Vector(0.0f, 0.0f, -arrowLength));
}

static void appendLight(GU_Detail &gdp, VrsceneObjectBase &ob, int flipAxis, UT_BoundingBox &bbox, fpreal t)
{
	if (ob.getType() != ObjectTypeLight)
		return;

	VrsceneObjectLight &light = static_cast<VrsceneObjectLight&>(ob);

	VUtils::TraceTransform tm = light.getTransform(t);
	if (flipAxis) {
		tm.m    = flipMatrixZY * tm.m;
		tm.offs = flipMatrixZY * tm.offs;
	}

	switch (light.getLightType()) {
		case LightTypeOmni: break;
		case LightTypeRectangle: {
			appendLightRectragle(gdp, light, tm, bbox, t);
			break;
		}
		case LightTypeSphere: break;
		case LightTypeDirect: break;
		case LightTypeSpot: break;
		case LightTypeSun: break;
		case LightTypeIES: break;
		case LightTypeDome: break;
		case LightTypeMesh: break;
		default:
			break;
	}
}

static void appendObject(GU_Detail &gdp, VrsceneObjectBase &ob, const SettingsWrapper &settings, UT_BoundingBox &bbox, fpreal t)
{
	switch (ob.getType()) {
		case ObjectTypeUnsupported: break;
		case ObjectTypeNode: {
			if (settings.addNodes) {
				appendMesh(gdp, ob, settings.flipAxis, bbox, t);
			}
			break;
		}
		case ObjectTypeNodeParticle: break;
		case ObjectTypeLight: {
			if (settings.addLights) {
				appendLight(gdp, ob, settings.flipAxis, bbox, t);
			}
			break;
		}
		case ObjectTypeInstancer: break;
		case ObjectTypeVolume: break;
		case ObjectTypeVRayScene: break;
		case ObjectTypeMaterial: break;
		default:
			break;
	}
}

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
	typedef QList<VrsceneObjectBase*> VrsceneObjectBaseList;

	static void appendVrsceneSceneObjectPlugins(const VrsceneSceneObject &object, VrsceneObjectBaseList &objectList) {
		const ObjectBaseTable &plugins = object.getObjectPlugins();
		const VrsceneSceneObjects &children = object.getChildren();

		if (!children.empty()) {
			FOR_CONST_IT(VrsceneSceneObjects, it, children) {
				appendVrsceneSceneObjectPlugins(*it.data(), objectList);
			}
		}

		for (const VrsceneObjectBase *ob : plugins) {
			objectList.append(const_cast<VrsceneObjectBase*>(ob));
		}
	}

	static GU_DetailHandle build(VrsceneDesc *vrsceneDesc, const SettingsWrapper &settings, const fpreal &t, ReturnSettings &retValue) {
		GU_Detail *gdp = new GU_Detail();

		retValue.box.initBounds();

		VrsceneObjectBaseList previewObjects;

		if (!settings.objectPath.empty()) {
			const VrsceneSceneObject *sceneObject = vrsceneDesc->getSceneObjectByPath(settings.objectPath);
			if (sceneObject) {
				const VrsceneSceneObjects &children = sceneObject->getChildren();
				if (children.empty()) {
					appendVrsceneSceneObjectPlugins(*sceneObject, previewObjects);
				}
				else {
					FOR_CONST_IT(VrsceneSceneObjects, it, children) {
						const VrsceneSceneObject &child = *it.data();

						GU_PrimPacked *prim = GU_PrimPacked::build(*gdp, theTypeId);
						vassert(prim);

						// Set the location of the packed primitive point.
						const UT_Vector3 pivot(0.0, 0.0, 0.0);
						prim->setPivot(pivot);
						gdp->setPos3(prim->getPointOffset(0), pivot);

						// Add to named group.
						const QStringList childPathChunks(QString(child.getPath().ptr()).split('/'));
						for (QString childPathChunk : childPathChunks) {
							// XXX: Check if more filtering must be done!
							childPathChunk = childPathChunk.remove(':');

							GA_PrimitiveGroup *primGroup = gdp->newPrimitiveGroup(qPrintable(childPathChunk));
							vassert(primGroup);

							primGroup->add(prim);
						}

						// Update primitive.
						VRaySceneRef *vraySceneRef = static_cast<VRaySceneRef*>(prim->implementation());
						vassert(vraySceneRef);

						UT_Options options;
						options.merge(settings.options);
						options.setOptionS("object_name", child.getName().ptr());
						options.setOptionS("object_path", child.getPath().ptr());

						vraySceneRef->update(prim, options);
					}
				}
			}
		}
		else {
			FOR_IT(VrsceneObjects, obIt, vrsceneDesc->m_objects) {
				previewObjects.append(obIt.data());
			}
		}

		for (VrsceneObjectBase *ob : previewObjects) {
			if (!ob)
				continue;

			appendObject(*gdp, *ob, settings, retValue.box, t);
		}

		GU_DetailHandle gdpHandle;
		gdpHandle.allocateAndSet(gdp);

		GU_Detail *gdpPacked = new GU_Detail;
		GU_PackedGeometry::packGeometry(*gdpPacked, gdpHandle);

		gdpHandle.allocateAndSet(gdpPacked);

		return gdpHandle;
	}
};

static VrsceneDescBuilder builder;
static DetailCachePrototype<ReturnSettings, SettingsWrapper> cache(builder);
static VRaySceneRefFactory theFactory("VRaySceneRef");

Hash::MHash SettingsWrapper::getHash() const
{
	Hash::MHash objectPathHash = 0;
	if (!objectPath.empty()) {
		Hash::MurmurHash3_x86_32(objectPath.ptr(), objectPath.length(), 42, &objectPathHash);
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
		int addNodes;
		int addLights;
		Hash::MHash objectPathHash;
	} settingsKey = {
		settings.usePreview,
		settings.previewFacesCount,
		settings.minPreviewFaces,
		settings.maxPreviewFaces,
		settings.previewType,
		settings.previewFlags,
		flipAxis,
		addNodes,
		addLights,
		objectPathHash
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

bool VRaySceneRef::unpack(GU_Detail &destGdp) const
{
	GU_DetailHandleAutoReadLock gdl(m_detail);
	if (!gdl.isValid())
		return false;

	const GU_Detail &tempGdp = *gdl.getGdp();

	for (GA_Iterator jt(tempGdp.getPrimitiveRange()); !jt.atEnd(); jt.advance()) {
		const GEO_Primitive *prim = tempGdp.getGEOPrimitive(*jt);
		if (prim && GU_PrimPacked::isPackedPrimitive(*prim)) {
			GU_PrimPacked *primPacked = const_cast<GU_PrimPacked*>(static_cast<const GU_PrimPacked*>(prim));
			if (primPacked->getTypeId() == GU_PackedGeometry::typeId()) {
				GU_ConstDetailHandle packedGdp = primPacked->getPackedDetail();
				if (packedGdp.isValid()) {
					destGdp.copy(*packedGdp.gdp(), GEO_COPY_ADD);
				}
			}
		}
	}

	return true;
}

VRay::VUtils::CharStringRefList VRaySceneRef::getObjectNamesFromPath() const
{
	const SettingsWrapper currentSettings(getSettings());

	VrsceneDesc *vrsceneDesc = vrsceneMan.getVrsceneDesc(getFilepath(), &currentSettings.settings);
	if (!vrsceneDesc)
		return VRay::VUtils::CharStringRefList();

	VRay::VUtils::CharStringRefList namesList;

	const VrsceneSceneObject *sceneObject = vrsceneDesc->getSceneObjectByPath(currentSettings.objectPath);
	if (sceneObject) {
		VrsceneObjectBaseList objectList;
		objectList.reserve(1024);

		appendVrsceneSceneObjectPlugins(*sceneObject, objectList);

		const int numObjectPlugins = objectList.size();
		if (numObjectPlugins) {
			namesList = VRay::VUtils::CharStringRefList(numObjectPlugins);

			for (int i = 0; i < numObjectPlugins; ++i) {
				const VrsceneObjectBase *ob = objectList[i];
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
	settings.objectPath = getObjectPath();
	settings.flipAxis = getShouldFlip();
	settings.addNodes = getAddNodes();
	settings.addLights = getAddLights();

	// NOTE: Set cache type to VrsceneCacheTypeNone,
	// because we'll cache constructed GU_Detail.
	settings.settings.cacheSettings.cacheType =
		VrsceneCacheSettings::VrsceneCacheType::VrsceneCacheTypeNone;

	settings.settings.usePreview = getUsePreviewFaces();
	settings.settings.previewFacesCount = getPreviewFaces();

	settings.options.merge(m_options);

	return settings;
}

int VRaySceneRef::detailRebuild(GU_PrimPacked *prim)
{
	// TODO: Cached detail is not storing bbox so it'll be invalid!
	// TODO: Rework cache registration / deregistration.

	ReturnSettings retValue(m_bbox);

	const SettingsWrapper cacheKey(getSettings());
	const GU_DetailHandle detail = cache.getDetail(getFilepath(), cacheKey, getFrame(getCurrentFrame()), retValue);

	const int isDetailChanged = m_detail != detail;

	// Store new detail.
	m_detail = detail;

	return isDetailChanged;
}
