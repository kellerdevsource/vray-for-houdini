//
// Copyright (c) 2015-2017, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#include "vfh_defines.h"
#include "vfh_log.h"

#include "gu_geomplaneref.h"

#include <GU/GU_PackedFactory.h>
#include <GU/GU_PrimPacked.h>
#include <GU/GU_PrimPoly.h>
#include <GU/GU_PackedGeometry.h>

using namespace VRayForHoudini;

static GA_PrimitiveTypeId theTypeId(-1);
static VRayBaseRefFactory<GeomPlaneRef> theFactory("GeomPlaneRef");

void GeomPlaneRef::install(GA_PrimitiveFactory *primFactory)
{
	theTypeId = theFactory.install(*primFactory, theFactory);
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
	return true;
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
