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

#include <SOP/SOP_Node.h>


using namespace VRayForHoudini;


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

	GA_ROAttributeRef ref_width(gdp->findAttribute(GA_ATTRIB_PRIMITIVE, "width"));
	const GA_ROHandleF hnd_width(ref_width.getAttribute());

	hair_vertex_index = 0;
	for (GA_Iterator jt(gdp->getPrimitiveRange()); !jt.atEnd(); jt.advance()) {
		const GEO_Primitive *face = gdp->getGEOPrimitive(*jt);

		const int numVertices = face->getVertexCount();
		for (int i = 0; i < numVertices; ++i, ++hair_vertex_index) {
			const GA_Offset  &off = face->getVertexOffset(i);
			const UT_Vector3 &p = gdp->getPos3(off);

			hair_vertices[hair_vertex_index].set(p[0],p[1],p[2]);

			widths[hair_vertex_index] = hnd_width.isValid()
										? hnd_width.get(off)
										: 0.01f;
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
