//
// Copyright (c) 2015-2016, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#include "vfh_exporter.h"
#include "vfh_export_hair.h"
#include "vfh_geoutils.h"


#include <SOP/SOP_Node.h>
#include <GEO/GEO_PrimPoly.h>


using namespace VRayForHoudini;


/// Returns hair width at strand vertex.
/// @param gdp Detail.
/// @param strand Strand primitive.
/// @param v Strand vertex.
static float getHairWidthAtVertex(const GU_Detail &gdp, const GEO_Primitive &strand, int v)
{
	float width = 0.01f;

	const GA_Offset &pointOffs = strand.getPointOffset(v);

	const GA_ROHandleF &pscaleHndl = gdp.findAttribute(GA_ATTRIB_POINT, "pscale");
	const GA_ROHandleF &widthHnld  = gdp.findAttribute(GA_ATTRIB_POINT, "width");

	if (pscaleHndl.isValid()) {
		width = pscaleHndl.get(pointOffs);
	}
	else if (widthHnld.isValid()) {
		width = widthHnld.get(pointOffs);
	}

	return width;
}


void VRayExporter::exportGeomMayaHairGeom(SOP_Node *sop_node, const GU_Detail *gdp, Attrs::PluginDesc &pluginDesc)
{
	const int numStrands = gdp->getPrimitiveRange().getEntries();

	Log::getLog().info("  Fur: %i strands", numStrands);

	VRay::VUtils::IntRefList num_hair_vertices(numStrands);

	int hair_vertex_index = 0;
	int hair_vertex_total = 0;
	for (GA_Iterator jt(gdp->getPrimitiveRange()); !jt.atEnd(); jt.advance()) {
		const GEO_Primitive *face = gdp->getGEOPrimitive(*jt);

		const int numVertices = face->getVertexCount();

		num_hair_vertices[hair_vertex_index++] = numVertices;

		hair_vertex_total += numVertices;
	}

	VRay::VUtils::VectorRefList hair_vertices(hair_vertex_total);
	VRay::VUtils::FloatRefList  widths(hair_vertex_total);

	hair_vertex_index = 0;
	for (GA_Iterator jt(gdp->getPrimitiveRange()); !jt.atEnd(); jt.advance()) {
		const GEO_Primitive *face = gdp->getGEOPrimitive(*jt);

		const int numVertices = face->getVertexCount();
		for (int i = 0; i < numVertices; ++i, ++hair_vertex_index) {
			const GA_Offset  &off = face->getVertexOffset(i);
			const UT_Vector3 &p = gdp->getPos3(off);

			hair_vertices[hair_vertex_index].set(p[0],p[1],p[2]);

			widths[hair_vertex_index] = getHairWidthAtVertex(*gdp, *face, i);
		}
	}

	pluginDesc.addAttribute(Attrs::PluginAttr("num_hair_vertices", num_hair_vertices));
	pluginDesc.addAttribute(Attrs::PluginAttr("hair_vertices", hair_vertices));
	pluginDesc.addAttribute(Attrs::PluginAttr("widths", widths));
	pluginDesc.addAttribute(Attrs::PluginAttr("geom_splines", true));
}


VRay::Plugin VRayExporter::exportGeomMayaHair(SOP_Node *sop_node, const GU_Detail *gdp)
{
	Attrs::PluginDesc geomMayaHairDesc(VRayExporter::getPluginName(sop_node, "Hair"), "GeomMayaHair");
	exportGeomMayaHairGeom(sop_node, gdp, geomMayaHairDesc);
	return exportPlugin(geomMayaHairDesc);
}


HairPrimitiveExporter::HairPrimitiveExporter(OBJ_Node &obj, OP_Context &ctx, VRayExporter &exp):
	PrimitiveExporter(obj, ctx, exp),
	m_nurbcurveTypeId(GEO_PRIMNURBCURVE),
	m_bezcurveTypeId(GEO_PRIMBEZCURVE),
	m_polyTypeId(GEO_PRIMPOLY)
{ }


bool HairPrimitiveExporter::isHairPrimitive(const GEO_Primitive *prim) const
{
	if (!prim) {
		return false;
	}
	return (   prim->getTypeId() == m_nurbcurveTypeId
			|| prim->getTypeId() == m_bezcurveTypeId
			|| (prim->getTypeId() == m_polyTypeId && !(UTverify_cast< const GEO_PrimPoly* >(prim)->isClosed())) );
}


bool HairPrimitiveExporter::containsHairPrimitives(const GU_Detail &gdp) const
{
	return (   gdp.containsPrimitiveType(m_nurbcurveTypeId)
			|| gdp.containsPrimitiveType(m_bezcurveTypeId)
			|| gdp.containsPrimitiveType(m_polyTypeId) );
}


void HairPrimitiveExporter::exportPrimitives(const GU_Detail &gdp, PluginDescList &plugins)
{
	if (!containsHairPrimitives(gdp)) {
		return;
	}

	// filter primitives
	GEO::GEOPrimList primList(    gdp.countPrimitiveType(m_nurbcurveTypeId)
								+ gdp.countPrimitiveType(m_bezcurveTypeId)
								+ gdp.countPrimitiveType(m_polyTypeId));

	for (GA_Iterator jt(gdp.getPrimitiveRange()); !jt.atEnd(); jt.advance()) {
		const GEO_Primitive *prim = gdp.getGEOPrimitive(*jt);
		if (isHairPrimitive(prim)) {
			primList.append(prim);
		}
	}

	// collect strands
	VRay::VUtils::IntRefList strands( primList.size() );
	int nVerts = 0;
	int idx = 0;
	for (const GEO_Primitive *prim : primList) {
		const int nStarndVerts = prim->getVertexCount();
		strands[idx++] = nStarndVerts;
		nVerts += nStarndVerts;
	}

	// collect verts
	VRay::VUtils::VectorRefList verts(nVerts);
	GEO::getDataFromAttribute(gdp.getP(), primList, verts);

	// collect widths
	const GA_AttributeOwner searchOrder[] = {
		GA_ATTRIB_VERTEX,		// Unique vertex data
		GA_ATTRIB_POINT,		// Shared vertex data
		GA_ATTRIB_PRIMITIVE,	// Primitive attribute data
	};

	VRay::VUtils::FloatRefList  widths(nVerts);
	GA_ROHandleF widthHdl = gdp.findAttribute(GEO_STD_ATTRIB_WIDTH,
											  searchOrder,
											  COUNT_OF(searchOrder));
	if (widthHdl.isInvalid()) {
		widthHdl = gdp.findAttribute(GEO_STD_ATTRIB_PSCALE,
									 searchOrder,
									 COUNT_OF(searchOrder));
	}
	if (widthHdl.isValid()) {
		GEO::getDataFromAttribute(widthHdl.getAttribute(), primList, widths);
	}
	else {
		std::memset(widths.get(), 0, widths.size() * sizeof(widths[0]));
	}

	// export
	Attrs::PluginDesc hairDesc(VRayExporter::getPluginName(&m_object, "Hair"), "GeomMayaHair");
	hairDesc.addAttribute(Attrs::PluginAttr("num_hair_vertices", strands));
	hairDesc.addAttribute(Attrs::PluginAttr("hair_vertices", verts));
	hairDesc.addAttribute(Attrs::PluginAttr("widths", widths));
	hairDesc.addAttribute(Attrs::PluginAttr("geom_splines", true));

	plugins.push_back(Attrs::PluginDesc("", "Node"));
	Attrs::PluginDesc &nodeDesc = plugins.back();

	nodeDesc.addAttribute(Attrs::PluginAttr("geometry", m_exporter.exportPlugin(hairDesc)));
}
