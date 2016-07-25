//
// Copyright (c) 2015-2016, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#include <vop_PhoenixSim.h>
#include <vfh_prm_templates.h>

using namespace VRayForHoudini;
using namespace VOP;

static PRM_Template * AttrItems = nullptr;

PRM_Template* PhxShaderSim::GetPrmTemplate()
{
	if (!AttrItems) {
		static Parm::PRMList paramList;
		paramList.addFromFile(Parm::PRMList::expandUiPath("CustomPhxShaderSim.ds"));
		AttrItems = paramList.getPRMTemplate();
	}

	return AttrItems;
}

void PhxShaderSim::setPluginType()
{
	pluginType = "MATERIAL";
	pluginID   = "PhxShaderSim";
}

OP::VRayNode::PluginResult PhxShaderSim::asPluginDesc(Attrs::PluginDesc &pluginDesc, VRayExporter &exporter, ExportContext *parentContext)
{
	const auto t = exporter.getContext().getTime();

	// cache plugin
	//if (phxShaderCache) {
	//	phxShaderSimDesc.addAttribute(Attrs::PluginAttr("cache", phxShaderCache));
	//}
	// tex plugins
	//if (customFluidData.size()) {
	//	if (customFluidData.count("heat")) {
	//		phxShaderSimDesc.addAttribute(Attrs::PluginAttr("darg", 4));
	//		phxShaderSimDesc.addAttribute(Attrs::PluginAttr("dtex", customFluidData["heat"]));
	//	}
	//	if (customFluidData.count("density")) {
	//		phxShaderSimDesc.addAttribute(Attrs::PluginAttr("targ", 4));
	//		phxShaderSimDesc.addAttribute(Attrs::PluginAttr("ttex", customFluidData["density"]));
	//	}
	//	if (customFluidData.count("temperature")) {
	//		phxShaderSimDesc.addAttribute(Attrs::PluginAttr("earg", 4));
	//		phxShaderSimDesc.addAttribute(Attrs::PluginAttr("etex", customFluidData["temperature"]));
	//	}
	//	if (customFluidData.count("velocity")) {
	//		phxShaderSimDesc.addAttribute(Attrs::PluginAttr("varg", 2));
	//		phxShaderSimDesc.addAttribute(Attrs::PluginAttr("vtex", customFluidData["velocity"]));
	//	}
	//}

	// node tm
	//nodeTm.offset = nodeTm.matrix * phxTm.offset + nodeTm.offset;
	//nodeTm.matrix = nodeTm.matrix * phxTm.matrix;
	//phxShaderSimDesc.addAttribute(Attrs::PluginAttr("node_transform", nodeTm));

	RenderMode rendMode;

	// renderMode
	rendMode = static_cast<RenderMode>(evalInt("renderMode", 0, t));
	pluginDesc.addAttribute(Attrs::PluginAttr("geommode", rendMode == Volumetric_Geometry || rendMode == Volumetric_Heat_Haze || rendMode == Isosurface));
	pluginDesc.addAttribute(Attrs::PluginAttr("mesher", rendMode == Mesh));
	pluginDesc.addAttribute(Attrs::PluginAttr("rendsolid", rendMode == Isosurface));
	pluginDesc.addAttribute(Attrs::PluginAttr("heathaze", rendMode == Volumetric_Heat_Haze));

	// TODO: find a better way to pass these
	// add these so we know later in what to wrap this sim
	Attrs::PluginAttr attrRendMode("_vray_render_mode", Attrs::PluginAttr::AttrTypeIgnore);
	attrRendMode.paramValue.valInt = static_cast<int>(rendMode);
	pluginDesc.add(attrRendMode);

	const bool dynamic_geometry = evalInt("dynamic_geometry", 0, t) == 1;
	Attrs::PluginAttr attrDynGeom("_vray_dynamic_geometry", Attrs::PluginAttr::AttrTypeIgnore);
	attrRendMode.paramValue.valInt = dynamic_geometry;
	pluginDesc.add(attrDynGeom);


	const auto primVal = evalInt("pmprimary", 0, t);
	const bool enableProb = (exporter.isIPR() && primVal) || primVal == 2;
	pluginDesc.addAttribute(Attrs::PluginAttr("pmprimary", enableProb));

	exporter.setAttrsFromOpNodePrms(pluginDesc, this, "", true);

	return OP::VRayNode::PluginResultContinue;
}