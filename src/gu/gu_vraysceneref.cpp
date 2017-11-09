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

#include <GU/GU_PackedFactory.h>
#include <GU/GU_PrimPacked.h>
#include <GU/GU_PrimPoly.h>
#include <GU/GU_PackedContext.h>
#include <GU/GU_PackedGeometry.h>
#include <UT/UT_MemoryCounter.h>
#include <FS/UT_DSO.h>

#include <vrscene_preview.h>

using namespace VRayForHoudini;
using namespace VUtils;
using namespace Vrscene::Preview;

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

static const char mw_shadow_tint_color_param_name[] = "mw_shadow_tint_color";
static VrsceneDescManager vrsceneMan;
static GA_PrimitiveTypeId theTypeId(-1);

class VRaySceneFactory
	: public GU_PackedFactory
{
public:
	static VRaySceneFactory &getInstance() {
		static VRaySceneFactory theFactory;
		return theFactory;
	}

	GU_PackedImpl* create() const VRAY_OVERRIDE {
		return new VRaySceneRef();
	}

private:
	VRaySceneFactory();

	VUTILS_DISABLE_COPY(VRaySceneFactory);
};

VRaySceneFactory::VRaySceneFactory() :
	GU_PackedFactory("VRaySceneRef", "VRaySceneRef")
{
	VFH_MAKE_REGISTERS(VFH_VRAY_SCENE_PARAMS, VFH_VRAY_SCENE_PARAMS_COUNT, VRaySceneRef)
	VFH_MAKE_REGISTERS_TUPLE(VFH_VRAY_SCENE_PARAMS_TUPLES, VFH_VRAY_SCENE_PARAMS_TUPLES_COUNT, VRaySceneRef)

	registerTupleIntrinsic(
		mw_shadow_tint_color_param_name,
		IntGetterCast(&VRaySceneRef::get_mw_shadow_tint_color_size),
		F64VectorGetterCast(&VRaySceneRef::_get_mw_shadow_tint_color),
		F64VectorSetterCast(&VRaySceneRef::_set_mw_shadow_tint_color)
	);
}

UT_Vector3D VRaySceneRef::get_mw_shadow_tint_color(GET_SET_ARG_PRIM_SINGLE) const {
	fpreal64 v[3];
	_get_mw_shadow_tint_color(GET_SET_ARG_PRIM_FWD GET_SET_ARG_SEP v, 3);
	return UT_Vector3D(v[0], v[1], v[2]);
}

void VRaySceneRef::_get_mw_shadow_tint_color(GET_SET_ARG_PRIM fpreal64 * v, exint size) const {
	UT_Vector3D val = m_options.hasOption(mw_shadow_tint_color_param_name) ?
		m_options.getOptionV3(mw_shadow_tint_color_param_name) : UT_Vector3D();
	v[0] = val[0]; v[1] = val[1]; v[2] = val[2];
}

exint VRaySceneRef::get_mw_shadow_tint_color_size(GET_SET_ARG_PRIM_SINGLE) const {
	return m_options.hasOption(mw_shadow_tint_color_param_name) ?
		m_options.getOptionV3(mw_shadow_tint_color_param_name).theSize : 0;
}

void VRaySceneRef::set_mw_shadow_tint_color(SET_ARG_PRIM const UT_Vector3D & val) {
	m_options.setOptionV3(mw_shadow_tint_color_param_name, val);
}

void VRaySceneRef::_set_mw_shadow_tint_color(SET_ARG_PRIM const fpreal64 * v, exint size) {
	m_options.setOptionV3(mw_shadow_tint_color_param_name, v[0], v[1], v[2]);
}

void VRayForHoudini::VRaySceneRef::install(GA_PrimitiveFactory *gafactory)
{
	VRaySceneFactory &theFactory = VRaySceneFactory::getInstance();
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
	, m_options()
{}

VRaySceneRef::VRaySceneRef(const VRaySceneRef &src)
	: GU_PackedImpl(src)
	, m_detail(src.m_detail)
	, m_options(src.m_options)
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
	return &VRaySceneFactory::getInstance();
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
	const int useAnimOverrides = get_anim_override();
	if (useAnimOverrides) {
		t = calcFrameIndex(t,
			static_cast<MeshFileAnimType::Enum>(get_anim_type()),
								   get_anim_start(),
								   get_anim_length(),
								   get_anim_offset(),
								   get_anim_speed());
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
	const fpreal t = getFrame(get_current_frame());

	// Detail for the mesh
	GU_Detail *meshDetail = new GU_Detail();

	int meshVertexOffset = 0;

	for (VrsceneObjects::iterator obIt = vrsceneDesc->m_objects.begin(); obIt != vrsceneDesc->m_objects.end(); ++obIt) {
		VrsceneObjectBase *ob = obIt.data();
		if (ob && ob->getType() == ObjectTypeNode) {
			const TraceTransform &tm = ob->getTransform(t);

			VrsceneObjectNode     *node = static_cast<VrsceneObjectNode*>(ob);
			VrsceneObjectDataBase *nodeData = vrsceneDesc->getObjectData(node->getDataName().ptr());
			if (nodeData && nodeData->getDataType() == ObjectDataTypeMesh) {
				VrsceneObjectDataMesh *mesh = static_cast<VrsceneObjectDataMesh*>(nodeData);

				const VectorRefList &vertices = mesh->getVertices(t);
				const IntRefList    &faces = mesh->getFaces(t);

				// Allocate the points, this is the offset of the first one
				GA_Offset pointOffset = meshDetail->appendPointBlock(vertices.count());

				// Iterate through points by their offsets
				for (int v = 0; v < vertices.count(); ++v, ++pointOffset) {
					Vector vert = tm * vertices[v];
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

	VrsceneDesc *vrsceneDesc = vrsceneMan.getVrsceneDesc(get_filepath(), &vrsceneSettings);
	if (!vrsceneDesc) {
		detailClear();
	}
	else {
		// Update flip axis intrinsic.
		const FlipAxisMode flipAxis = parseFlipAxisMode(get_flip_axis());
		const bool shouldFlip = flipAxis == flipZY ||
			                    flipAxis == automatic && vrsceneDesc->getUpAxis() == vrsceneUpAxisZ;
		set_should_flip(shouldFlip);

		if (!get_add_nodes()) {
			detailClear();
		}
		else {
			detailBuild(vrsceneDesc, shouldFlip);
		}
	}

	topologyDirty();

	return true;
}
