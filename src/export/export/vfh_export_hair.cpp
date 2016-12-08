//
// Copyright (c) 2015-2016, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#include "vfh_export_hair.h"
#include "vfh_geoutils.h"

#include <GEO/GEO_PrimPoly.h>


using namespace VRayForHoudini;


namespace {

const char * const theHairParm = "geom_splines";

const char * const VFH_ATTRIB_INCANDESCENCE = "incandescence";
const char * const VFH_ATTRIB_TRANSPARENCY = "transparency";

}


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


HairPrimitiveExporter::HairPrimitiveExporter(OBJ_Node &obj, OP_Context &ctx, VRayExporter &exp):
	PrimitiveExporter(obj, ctx, exp)
{ }


OP_Node* HairPrimitiveExporter::findPramOwnerForHairParms() const
{
	if (   m_object.getParmList()
		&& m_object.getParmList()->getParmPtr(theHairParm))
	{
		 return &m_object;
	}

	OP_Node *obj = m_object.getParent();
	if (   obj
		&& obj->getOperator()->getName().contains("fur*")
		&& obj->getParmList()->getParmPtr(theHairParm) )
	{
		 return obj;
	}

	return nullptr;
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

	pluginDesc.pluginID = "GeomMayaHair";
	pluginDesc.pluginName = VRayExporter::getPluginName(&m_object, "Hair");
	pluginDesc.addAttribute(Attrs::PluginAttr("num_hair_vertices", strands));
	pluginDesc.addAttribute(Attrs::PluginAttr("hair_vertices", verts));

	OP_Node *prmOwner = findPramOwnerForHairParms();
	if (prmOwner) {
		m_exporter.setAttrsFromOpNodePrms(pluginDesc, prmOwner);
	}
	else {
		// no hair spare parameters - use defaults
		pluginDesc.addAttribute(Attrs::PluginAttr("geom_splines", true));
		pluginDesc.addAttribute(Attrs::PluginAttr("widths_in_pixels", false));
		pluginDesc.addAttribute(Attrs::PluginAttr("generate_w_coord", false));
		pluginDesc.addAttribute(Attrs::PluginAttr("use_global_hair_tree", true));
		pluginDesc.addAttribute(Attrs::PluginAttr("xgen_generated", false));
		pluginDesc.addAttribute(Attrs::PluginAttr("min_pixel_width", 0.f));
		pluginDesc.addAttribute(Attrs::PluginAttr("geom_tesselation_mult", 4.f));
	}

	// widths
	const GA_AttributeOwner vSearchOrder[] = {
		GA_ATTRIB_VERTEX,		// Unique vertex data
		GA_ATTRIB_POINT,		// Shared vertex data
	};

	GA_ROHandleF widthHdl = gdp.findAttribute(GEO_STD_ATTRIB_WIDTH,
											  vSearchOrder,
											  COUNT_OF(vSearchOrder));
	if (widthHdl.isInvalid()) {
		widthHdl = gdp.findAttribute(GEO_STD_ATTRIB_PSCALE,
									 vSearchOrder,
									 COUNT_OF(vSearchOrder));
	}

	if (widthHdl.isValid()) {
		VRay::VUtils::FloatRefList  widths(nVerts);
		GEO::getDataFromAttribute(widthHdl.getAttribute(), primList, widths);
		pluginDesc.addAttribute(Attrs::PluginAttr("widths", widths));
	}

	// colors
	GA_ROHandleV3 cdHdl = gdp.findAttribute(GEO_STD_ATTRIB_DIFFUSE,
											vSearchOrder,
											COUNT_OF(vSearchOrder));
	if (cdHdl.isValid()) {
		VRay::VUtils::ColorRefList colors(nVerts);
		GEO::getDataFromAttribute(cdHdl.getAttribute(), primList, colors);
		pluginDesc.addAttribute(Attrs::PluginAttr("colors", colors));
	}

	// transparency
	GA_ROHandleV3 transpHdl = gdp.findAttribute(VFH_ATTRIB_TRANSPARENCY,
												vSearchOrder,
												COUNT_OF(vSearchOrder));
	if (transpHdl.isValid()) {
		VRay::VUtils::ColorRefList transparency(nVerts);
		GEO::getDataFromAttribute(transpHdl.getAttribute(), primList, transparency);
		pluginDesc.addAttribute(Attrs::PluginAttr("transparency", transparency));
	}

	// incandescence
	GA_ROHandleV3 incdHdl = gdp.findAttribute(VFH_ATTRIB_INCANDESCENCE,
											  vSearchOrder,
											  COUNT_OF(vSearchOrder));
	if (incdHdl.isValid()) {
		VRay::VUtils::ColorRefList incandescence(nVerts);
		GEO::getDataFromAttribute(incdHdl.getAttribute(), primList, incandescence);
		pluginDesc.addAttribute(Attrs::PluginAttr("incandescence", incandescence));
	}

	// uvw
	VRay::VUtils::VectorRefList uvw(strands.size());
	GA_ROHandleV3 uvwHdl = gdp.findPrimitiveAttribute(GEO_STD_ATTRIB_TEXTURE);
	if (uvwHdl.isValid()) {
		GEO::getDataFromAttribute(uvwHdl.getAttribute(), primList, uvw);
	}
	else {
		std::memset(uvw.get(), 0, uvw.size() * sizeof(uvw[0]));
	}
	pluginDesc.addAttribute(Attrs::PluginAttr("strand_uvw", uvw));

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
