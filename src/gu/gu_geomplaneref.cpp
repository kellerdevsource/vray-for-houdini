#include <GU/GU_PackedFactory.h>
#include <GU/GU_PrimPacked.h>
#include <GU/GU_PrimPoly.h>
#include <GU/GU_PackedGeometry.h>
#include <UT/UT_MemoryCounter.h>
#include <FS/UT_DSO.h>

#include "vfh_log.h"
#include "vfh_primitives.h"

#include "gu_geomplaneref.h"

using namespace VRayForHoudini;

class GeomPlaneFactory
	: public GU_PackedFactory
{
public:
	static GeomPlaneFactory &getInstance() {
		static GeomPlaneFactory theFactory;
		return theFactory;
	}

	GU_PackedImpl* create() const VRAY_OVERRIDE {
		return new GeomPlaneRef();
	}

private:
	GeomPlaneFactory()
		: GU_PackedFactory("GeomInfinitePlaneRef", "GeomInfinitePlaneRef")
	{
		registerIntrinsic(
			"plane_size",
			FloatGetterCast(&GeomPlaneRef::get_plane_size),
			FloatSetterCast(&GeomPlaneRef::set_plane_size)
		);
	}

	VUTILS_DISABLE_COPY(GeomPlaneFactory);
};

GA_PrimitiveTypeId GeomPlaneRef::theTypeId(-1);

void VRayForHoudini::GeomPlaneRef::install(GA_PrimitiveFactory *gafactory)
{
	GeomPlaneFactory &theFactory = GeomPlaneFactory::getInstance();
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


GU_PackedFactory *GeomPlaneRef::getFactory() const
{
	return &GeomPlaneFactory::getInstance();
}

GU_PackedImpl *GeomPlaneRef::copy() const
{
	return new GeomPlaneRef(*this);
}

bool GeomPlaneRef::isValid() const
{
	return m_detail.isValid();
}

void GeomPlaneRef::clearData()
{
	// This method is called when primitives are "stashed" during the cooking
	// process.  However, primitives are typically immediately "unstashed" or
	// they are deleted if the primitives aren't recreated after the fact.
	// We can just leave our data.
}

bool GeomPlaneRef::load(const UT_Options &options, const GA_LoadMap &map)
{
	return updateFrom(options);
}

void GeomPlaneRef::update(const UT_Options &options)
{
	updateFrom(options);
}

bool GeomPlaneRef::save(UT_Options &options, const GA_SaveMap &map) const
{
	options.merge(m_options);
	return true;
}

bool GeomPlaneRef::getBounds(UT_BoundingBox &box) const
{
	return getBounds(box);
}

bool GeomPlaneRef::getRenderingBounds(UT_BoundingBox &box) const
{
	return getBounds(box);
}

void GeomPlaneRef::getVelocityRange(UT_Vector3 &min, UT_Vector3 &max) const
{
	// No velocity attribute on geometry
	min = max = 0;
}

void GeomPlaneRef::getWidthRange(fpreal &wmin, fpreal &wmax) const
{
	// Width is only important for curves/points.
	wmin = wmax = 0;
}

bool GeomPlaneRef::unpack(GU_Detail &destgdp) const
{
	// This may allocate geometry for the primitive
	GU_DetailHandleAutoReadLock gdl(getPackedDetail());
	if (!gdl.isValid()) {
		return false;
	}

	return unpackToDetail(destgdp, gdl.getGdp());
}

GU_ConstDetailHandle GeomPlaneRef::getPackedDetail(GU_PackedContext *context) const
{
	if (!m_dirty) {
		return getDetail();
	}

	GU_Detail* gdmp = new GU_Detail();
	GU_PrimPoly *poly = GU_PrimPoly::build(gdmp, 4, GU_POLY_CLOSED, 0);

	const float size = get_plane_size();

	GA_Offset pOff = gdmp->appendPoint();
	gdmp->setPos3(pOff, UT_Vector3(-size, 0.0f, -size));
	poly->setVertexPoint(0, pOff);

	pOff = gdmp->appendPoint();
	gdmp->setPos3(pOff, UT_Vector3(-size, 0.0f, size));
	poly->setVertexPoint(1, pOff);

	pOff = gdmp->appendPoint();
	gdmp->setPos3(pOff, UT_Vector3(size, 0.0f, size));
	poly->setVertexPoint(2, pOff);

	pOff = gdmp->appendPoint();
	gdmp->setPos3(pOff, UT_Vector3(size, 0.0f, -size));
	poly->setVertexPoint(3, pOff);

	poly->reverse();

	// handle
	GU_DetailHandle gdmh;
	gdmh.allocateAndSet(gdmp);

	// pack the geometry in the scene detail
	GU_DetailHandle gdh;
	GU_Detail *gdp = new GU_Detail();
	GU_PackedGeometry::packGeometry(*gdp, gdmh);
	gdh.allocateAndSet(gdp);

	GeomPlaneRef* self = SYSconst_cast(this);

	if (GU_ConstDetailHandle(gdh) != getDetail()) {
		self->setDetail(gdh);
	}
	self->m_dirty = false;

	return getDetail();
}

int64 GeomPlaneRef::getMemoryUsage(bool inclusive) const
{
	int64 mem = inclusive ? sizeof(*this) : 0;
	// Don't count the (shared) GU_Detail, since that will greatly
	// over-estimate the overall memory usage.
	mem += m_detail.getMemoryUsage(false);
	return mem;
}

void GeomPlaneRef::countMemory(UT_MemoryCounter &counter, bool inclusive) const
{
	if (counter.mustCountUnshared()) {
		size_t mem = (inclusive) ? sizeof(*this) : 0;
		mem += m_detail.getMemoryUsage(false);
		UT_MEMORY_DEBUG_LOG(theFactory->name(), int64(mem));
		counter.countUnshared(mem);
	}
}

bool GeomPlaneRef::updateFrom(const UT_Options &options)
{
	if (m_options == options) {
		return false;
	}

	m_options.merge(options);
	m_dirty = true;

	// Notify base primitive that topology has changed
	topologyDirty();

	return true;
}