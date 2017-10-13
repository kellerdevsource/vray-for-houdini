#include <SYS/SYS_Types.h>

#include "gu_vraysceneref.h"
#include <vrscene_preview.h>

#include "vfh_log.h"

#include <GU/GU_PackedFactory.h>
#include <GU/GU_PrimPacked.h>
#include <GU/GU_PrimPoly.h>
#include <GU/GU_PackedContext.h>
#include <GU/GU_PackedGeometry.h>
#include <UT/UT_MemoryCounter.h>
#include <FS/UT_DSO.h>

using namespace VRayForHoudini;

const char *VRaySceneRef::mw_shadow_tint_color_param_name = "mw_shadow_tint_color";
VUtils::Vrscene::Preview::VrsceneDescManager VRaySceneRef::vrsceneMan;
GA_PrimitiveTypeId VRaySceneRef::theTypeId(-1);

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
		VRaySceneRef::mw_shadow_tint_color_param_name,
		IntGetterCast(&VRaySceneRef::get_mw_shadow_tint_color_size),
		F64VectorGetterCast(&VRaySceneRef::_get_mw_shadow_tint_color),
		F64VectorSetterCast(&VRaySceneRef::_set_mw_shadow_tint_color)
	);
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

VRaySceneRef::VRaySceneRef():
	GU_PackedImpl(),
	m_detail(),
	m_options(),
	m_dirty(false)
{}

VRaySceneRef::VRaySceneRef(const VRaySceneRef &src):
	GU_PackedImpl(src),
	m_detail(),
	m_options(),
	m_dirty(false)
{
	updateFrom(src.m_options);
}

VRaySceneRef::~VRaySceneRef()
{
	clearDetail();
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
	// This method is called when primitives are "stashed" during the cooking
	// process.  However, primitives are typically immediately "unstashed" or
	// they are deleted if the primitives aren't recreated after the fact.
	// We can just leave our data.
}

bool VRaySceneRef::load(const UT_Options &options, const GA_LoadMap &map)
{
	return updateFrom(options);
}

void VRaySceneRef::update(const UT_Options &options)
{
	updateFrom(options);
}

bool VRaySceneRef::save(UT_Options &options, const GA_SaveMap &map) const
{
	options.merge(m_options);
	return true;
}

bool VRaySceneRef::getBounds(UT_BoundingBox &box) const
{
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
	// This may allocate geometry for the primitive
	GU_DetailHandleAutoReadLock gdl(getPackedDetail());
	if (NOT(gdl.isValid())) {
		return false;
	}

	return unpackToDetail(destgdp, gdl.getGdp());
}

GU_ConstDetailHandle VRaySceneRef::getPackedDetail(GU_PackedContext *context) const
{
	if (!m_dirty) {
		return getDetail();
	}

	VRaySceneRef *me = SYSconst_cast(this);

	using namespace VUtils;
	VUtils::Vrscene::Preview::VrsceneSettings vrsceneSettings;
	vrsceneSettings.usePreview = true;
	vrsceneSettings.previewFacesCount = 100000;
	vrsceneSettings.cacheSettings.cacheType = VUtils::Vrscene::Preview::VrsceneCacheSettings::VrsceneCacheType::VrsceneCacheTypeRam;

	GU_DetailHandle gdh;

	Vrscene::Preview::VrsceneDesc *vrsceneDesc = VRaySceneRef::vrsceneMan.getVrsceneDesc(get_filepath(), &vrsceneSettings);
	if (vrsceneDesc) {
		const bool flipAxis = get_flip_axis();
		unsigned meshVertexOffset = 0;

		for (Vrscene::Preview::VrsceneObjects::iterator obIt = vrsceneDesc->m_objects.begin(); obIt != vrsceneDesc->m_objects.end(); ++obIt) {
			Vrscene::Preview::VrsceneObjectBase *ob = obIt.data();
			if (ob && ob->getType() == Vrscene::Preview::ObjectTypeNode) {
				const TraceTransform &tm = ob->getTransform(get_current_frame());

				Vrscene::Preview::VrsceneObjectNode     *node = static_cast<Vrscene::Preview::VrsceneObjectNode*>(ob);
				Vrscene::Preview::VrsceneObjectDataBase *nodeData = vrsceneDesc->getObjectData(node->getDataName().ptr());
				if (nodeData && nodeData->getDataType() == Vrscene::Preview::ObjectDataTypeMesh) {
					// detail for the mesh
					GU_Detail *gdmp = new GU_Detail();

					// create preview mesh
					Vrscene::Preview::VrsceneObjectDataMesh *mesh = static_cast<Vrscene::Preview::VrsceneObjectDataMesh*>(nodeData);
					const VUtils::VectorRefList &vertices = mesh->getVertices(get_current_frame());
					const VUtils::IntRefList    &faces = mesh->getFaces(get_current_frame());

					// allocate the points, this is the offset of the first one
					GA_Offset pointOffset = gdmp->appendPointBlock(vertices.count());
					// iterate through points by their offsets
					for (int v = 0; v < vertices.count(); ++v, ++pointOffset) {

						Vector vert = tm * vertices[v];
						if (flipAxis) {
							vert = Vrscene::Preview::flipMatrix * vert;
						}

						gdmp->setPos3(pointOffset, UT_Vector3(vert.x, vert.y, vert.z));
					}

					for (int f = 0; f < faces.count(); f += 3) {
						GU_PrimPoly *poly = GU_PrimPoly::build(gdmp, 3, GU_POLY_CLOSED, 0);
						for (int c = 0; c < 3; ++c) {
							poly->setVertexPoint(c, meshVertexOffset + faces[f + c]);
						}
						poly->reverse();
					}

					meshVertexOffset += vertices.count();

					// handle
					GU_DetailHandle gdmh;
					gdmh.allocateAndSet(gdmp);

					// pack the geometry in the scene detail
					GU_Detail *gdp = new GU_Detail();
					GU_PackedGeometry::packGeometry(*gdp, gdmh);
					gdh.allocateAndSet(gdp);
				}
			}
		}
	}


	if (GU_ConstDetailHandle(gdh) != getDetail()) {
		me->setDetail(gdh);
		getPrim()->getParent()->getPrimitiveList().bumpDataId();
	}

	me->m_dirty = false;

	return getDetail();
}

int64 VRaySceneRef::getMemoryUsage(bool inclusive) const
{
	int64 mem = inclusive ? sizeof(*this) : 0;
	// Don't count the (shared) GU_Detail, since that will greatly
	// over-estimate the overall memory usage.
	mem += m_detail.getMemoryUsage(false);
	return mem;
}

void VRaySceneRef::countMemory(UT_MemoryCounter &counter, bool inclusive) const
{
	if (counter.mustCountUnshared()) {
		size_t mem = (inclusive) ? sizeof(*this) : 0;
		mem += m_detail.getMemoryUsage(false);
		UT_MEMORY_DEBUG_LOG(theFactory->name(), int64(mem));
		counter.countUnshared(mem);
	}
}

void VRaySceneRef::clearDetail()
{
	m_detail = GU_ConstDetailHandle();
}

bool VRaySceneRef::updateFrom(const UT_Options &options)
{
	if (m_options == options) {
		return false;
	}

	m_options = options;
	m_dirty = true;
	// Notify base primitive that topology has changed
	topologyDirty();

	return true;
}
