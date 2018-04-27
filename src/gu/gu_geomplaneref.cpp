//
// Copyright (c) 2015-2018, Chaos Software Ltd
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
{}

GeomPlaneRef::GeomPlaneRef(const GeomPlaneRef &src)
	: GeomPlaneRefBase(src)
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
	// This will show error and indicate that we don't support unpacking.
	return false;
}

static void addPlainPoint(GU_Detail &gdp, UT_BoundingBox &bbox,
						  GU_PrimPoly &poly, GA_Size vertexOffs,
						   const UT_Vector3 &point)
{
	const GA_Offset pointOffs = gdp.appendPoint();
	gdp.setPos3(pointOffs, point);
	poly.setVertexPoint(vertexOffs, pointOffs);

	bbox.enlargeBounds(point);
}

int GeomPlaneRef::detailRebuild()
{
	const float size = getPlaneSize();

	GU_Detail *meshDetail = new GU_Detail();
	m_bbox.initBounds();

	GU_PrimPoly *poly = GU_PrimPoly::build(meshDetail, 4, GU_POLY_CLOSED, 0);

	addPlainPoint(*meshDetail, m_bbox, *poly, 0,  UT_Vector3(-size, 0.0f, -size));
	addPlainPoint(*meshDetail, m_bbox, *poly, 1,  UT_Vector3(-size, 0.0f,  size));
	addPlainPoint(*meshDetail, m_bbox, *poly, 2,  UT_Vector3( size, 0.0f,  size));
	addPlainPoint(*meshDetail, m_bbox, *poly, 3,  UT_Vector3( size, 0.0f, -size));

	poly->reverse();

	m_detail.allocateAndSet(meshDetail);

	return true;
}
