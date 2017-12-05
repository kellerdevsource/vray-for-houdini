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
#include <UT/UT_MemoryCounter.h>
#include <FS/UT_DSO.h>

using namespace VRayForHoudini;
// using namespace VUtils; // XXX: VUtils::swap() causes issues.
using namespace VUtils::Vrscene::Preview;

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

static VrsceneDescManager vrsceneMan(NULL);
static GA_PrimitiveTypeId theTypeId(-1);

static class VRaySceneFactory
	: public GU_PackedFactory
{
public:
	VRaySceneFactory()
		: GU_PackedFactory("VRaySceneRef", "VRaySceneRef")
	{
		VRaySceneRef::registerIntrinsics<VRaySceneRef>(*this);
	}

	GU_PackedImpl* create() const VRAY_OVERRIDE {
		return new VRaySceneRef();
	}

	VUTILS_DISABLE_COPY(VRaySceneFactory)
} theFactory;

void VRaySceneRef::install(GA_PrimitiveFactory *gafactory)
{
	if (theFactory.isRegistered()) {
		Log::getLog().debug("Multiple attempts to install packed primitive %s from %s",
			static_cast<const char *>(theFactory.name()), UT_DSO::getRunningFile());
		return;
	}

	GU_PrimPacked::registerPacked(gafactory, &theFactory);
	if (NOT(theFactory.isRegistered())) {
		Log::getLog().error("Unable to register packed primitive %s from %s",
			static_cast<const char *>(theFactory.name()), UT_DSO::getRunningFile());
		return;
	}

	theTypeId = theFactory.typeDef().getId();
}

VRaySceneRef::VRaySceneRef()
	: GU_PackedImpl()
	, m_detail()
{}

VRaySceneRef::VRaySceneRef(const VRaySceneRef &src)
	: GU_PackedImpl(src)
	, VRaySceneRefOptions(src)
	, m_detail(src.m_detail)
{
}

VRaySceneRef::~VRaySceneRef()
{}

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

bool VRaySceneRef::isValid() const
{
	return m_detail.isValid();
}

void VRaySceneRef::clearData()
{
}

bool VRaySceneRef::save(UT_Options &options, const GA_SaveMap&) const
{
	options.merge(m_options);
	return true;
}

bool VRaySceneRef::getBounds(UT_BoundingBox &box) const
{
	box = m_bbox;
	return true;
}

bool VRaySceneRef::getRenderingBounds(UT_BoundingBox &box) const
{
	return getBounds(box);
}

void VRaySceneRef::getVelocityRange(UT_Vector3 &min, UT_Vector3 &max) const
{
	// No velocity attribute on geometry
	min = 0;
	max = 0;
}

void VRaySceneRef::getWidthRange(fpreal &wmin, fpreal &wmax) const
{
	// Width is only important for curves/points.
	wmin = wmax = 0;
}

bool VRaySceneRef::unpack(GU_Detail &destgdp) const
{
	GU_DetailHandleAutoReadLock gdl(getPackedDetail());
	if (!gdl.isValid())
		return false;

	return unpackToDetail(destgdp, gdl.getGdp());
}

GU_ConstDetailHandle VRaySceneRef::getPackedDetail(GU_PackedContext*) const
{
	return m_detail;
}

int64 VRaySceneRef::getMemoryUsage(bool inclusive) const
{
	int64 mem = inclusive ? sizeof(VRaySceneRef) : 0;
	mem += m_detail.getMemoryUsage(false);

	return mem;
}

void VRaySceneRef::countMemory(UT_MemoryCounter &counter, bool inclusive) const
{
	if (counter.mustCountUnshared()) {
		int64 mem = inclusive ? sizeof(VRaySceneRef) : 0;
		mem += m_detail.getMemoryUsage(false);
		UT_MEMORY_DEBUG_LOG(theFactory->name(), mem);
		counter.countUnshared(mem);
	}
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

void VRaySceneRef::detailClear()
{
	m_bbox.initBounds();
	m_detail = GU_ConstDetailHandle();
}

void VRaySceneRef::detailBuild(VrsceneDesc *vrsceneDesc, int shouldFlip)
{
	const fpreal t = getFrame(getCurrentFrame());

	// Detail for the mesh
	GU_Detail *meshDetail = new GU_Detail();

	int meshVertexOffset = 0;

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

	GU_DetailHandle meshDetailHandle;
	meshDetailHandle.allocateAndSet(meshDetail);

	m_detail = meshDetailHandle;
}

int VRaySceneRef::updateFrom(const UT_Options &options)
{
	if (m_options == options)
		return false;

	// Store new options
	m_options = options;

	VrsceneSettings vrsceneSettings;
	vrsceneSettings.usePreview = true;
	vrsceneSettings.previewFacesCount = 100000;
	vrsceneSettings.cacheSettings.cacheType = VrsceneCacheSettings::VrsceneCacheType::VrsceneCacheTypeRam;

	VrsceneDesc *vrsceneDesc = vrsceneMan.getVrsceneDesc(getFilepath(), &vrsceneSettings);
	if (!vrsceneDesc) {
		detailClear();
	}
	else {
		// Update flip axis intrinsic.
		const FlipAxisMode flipAxis = parseFlipAxisMode(getFlipAxis());
		const bool shouldFlip = flipAxis == flipZY ||
			                    flipAxis == automatic && vrsceneDesc->getUpAxis() == vrsceneUpAxisZ;
		setShouldFlip(shouldFlip);

		if (!getAddNodes()) {
			detailClear();
		}
		else {
			detailBuild(vrsceneDesc, shouldFlip);
		}
	}

	topologyDirty();

	return true;
}
