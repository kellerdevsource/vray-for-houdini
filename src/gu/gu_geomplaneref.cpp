#include <GU/GU_PackedFactory.h>
#include <GU/GU_PrimPacked.h>
#include <GU/GU_PrimPoly.h>
#include <GU/GU_PackedGeometry.h>
#include <UT/UT_MemoryCounter.h>
#include <FS/UT_DSO.h>

#include "vfh_defines.h"
#include "vfh_log.h"

#include "gu_geomplaneref.h"

using namespace VRayForHoudini;

static GA_PrimitiveTypeId theTypeId(-1);

static class GeomPlaneFactory
	: public GU_PackedFactory
{
public:
	GeomPlaneFactory()
		: GU_PackedFactory("GeomPlaneRef", "GeomPlaneRef")
	{
		GeomPlaneRef::registerIntrinsics<GeomPlaneRef>(*this);
	}

	GU_PackedImpl* create() const VRAY_OVERRIDE {
		return new GeomPlaneRef();
	}

	VUTILS_DISABLE_COPY(GeomPlaneFactory)
} theFactory;

void GeomPlaneRef::install(GA_PrimitiveFactory *gafactory)
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

GeomPlaneRef::GeomPlaneRef()
	: GeomPlaneRefOptions()
{}

GeomPlaneRef::GeomPlaneRef(const GeomPlaneRef &src)
	: GeomPlaneRefOptions(src)
{}

GeomPlaneRef::~GeomPlaneRef()
{}

GU_PackedFactory *GeomPlaneRef::getFactory() const
{
	return &theFactory;
}

GA_PrimitiveTypeId GeomPlaneRef::typeId()
{
	return theTypeId;
}

GU_PackedImpl *GeomPlaneRef::copy() const
{
	return new GeomPlaneRef(*this);
}

bool GeomPlaneRef::unpack(GU_Detail&) const
{
	return false;
}

void GeomPlaneRef::detailRebuild()
{
	const float size = getPlaneSize();

	GU_Detail *meshDetail = new GU_Detail();

	GU_PrimPoly *poly = GU_PrimPoly::build(meshDetail, 4, GU_POLY_CLOSED, 0);

	GA_Offset pointOffs = meshDetail->appendPoint();
	meshDetail->setPos3(pointOffs, UT_Vector3(-size, 0.0f, -size));
	poly->setVertexPoint(0, pointOffs);

	pointOffs = meshDetail->appendPoint();
	meshDetail->setPos3(pointOffs, UT_Vector3(-size, 0.0f, size));
	poly->setVertexPoint(1, pointOffs);

	pointOffs = meshDetail->appendPoint();
	meshDetail->setPos3(pointOffs, UT_Vector3(size, 0.0f, size));
	poly->setVertexPoint(2, pointOffs);

	pointOffs = meshDetail->appendPoint();
	meshDetail->setPos3(pointOffs, UT_Vector3(size, 0.0f, -size));
	poly->setVertexPoint(3, pointOffs);

	poly->reverse();

	GU_DetailHandle meshDetailHandle;
	meshDetailHandle.allocateAndSet(meshDetail);

	m_detail = meshDetailHandle;
}
