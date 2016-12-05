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
	PrimitiveExporter(obj, ctx, exp)
{ }


bool HairPrimitiveExporter::isHairPrimitive(const GEO_Primitive *prim)
{
	if (!prim) {
		return false;
	}
	return (   prim->getTypeId() == GEO_PRIMNURBCURVE
			|| prim->getTypeId() == GEO_PRIMBEZCURVE
			|| (prim->getTypeId() == GEO_PRIMPOLY && !(UTverify_cast< const GEO_PrimPoly* >(prim)->isClosed())) );
}


bool HairPrimitiveExporter::containsHairPrimitives(const GU_Detail &gdp)
{
	return (   gdp.containsPrimitiveType(GEO_PRIMNURBCURVE)
			|| gdp.containsPrimitiveType(GEO_PRIMBEZCURVE)
			|| gdp.containsPrimitiveType(GEO_PRIMPOLY) );
}


bool HairPrimitiveExporter::asPluginDesc(const GU_Detail &gdp, Attrs::PluginDesc &pluginDesc)
{
	if (!containsHairPrimitives(gdp)) {
		// no hair primitives
		return false;
	}

	// filter primitives
	GEO::GEOPrimList primList(    gdp.countPrimitiveType(GEO_PRIMNURBCURVE)
								+ gdp.countPrimitiveType(GEO_PRIMBEZCURVE)
								+ gdp.countPrimitiveType(GEO_PRIMPOLY));

	for (GA_Iterator jt(gdp.getPrimitiveRange()); !jt.atEnd(); jt.advance()) {
		const GEO_Primitive *prim = gdp.getGEOPrimitive(*jt);
		if (isHairPrimitive(prim)) {
			primList.append(prim);
		}
	}

	if (primList.size() <= 0) {
		// no valid hair primitives
		return false;
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

	pluginDesc.pluginID = "GeomMayaHair";
	pluginDesc.pluginName = VRayExporter::getPluginName(&m_object, "Hair");
	pluginDesc.addAttribute(Attrs::PluginAttr("num_hair_vertices", strands));
	pluginDesc.addAttribute(Attrs::PluginAttr("hair_vertices", verts));
	pluginDesc.addAttribute(Attrs::PluginAttr("widths", widths));
	pluginDesc.addAttribute(Attrs::PluginAttr("geom_splines", true));

	return true;
}


void HairPrimitiveExporter::exportPrimitives(const GU_Detail &gdp, PluginDescList &plugins)
{
	if (!containsHairPrimitives(gdp)) {
		// no hair primitives
		return;
	}

	// export
	Attrs::PluginDesc hairDesc;
	if (!asPluginDesc(gdp, hairDesc)) {
		// no valid hair primitives
		return;
	}

	plugins.push_back(Attrs::PluginDesc("", "Node"));
	Attrs::PluginDesc &nodeDesc = plugins.back();

	nodeDesc.addAttribute(Attrs::PluginAttr("geometry", m_exporter.exportPlugin(hairDesc)));
}
