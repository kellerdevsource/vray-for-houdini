//
// Copyright (c) 2015-2017, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#include "obj_node_base.h"

#include "vfh_export_geom.h"
#include "vfh_attr_utils.h"

using namespace VRayForHoudini;

PRM_Template* OBJ::VRayClipper::GetPrmTemplate()
{
	static Parm::PRMList myPrmList;
	if (myPrmList.empty()) {
		PRM_Template *objprmlist = OBJ_Geometry::getTemplateList(OBJ_PARMS_PLAIN);
		myPrmList.addFromPRMTemplate(objprmlist);

		if (myPrmList.empty()) {
			myPrmList.switcherBegin("stdswitcher");
		}

		// add plugin params
		myPrmList.addFolder("V-Ray Clipper");
		Parm::addPrmTemplateForPlugin( getVRayPluginIDName(VRayPluginID::VRayClipper), myPrmList);

		myPrmList.switcherEnd();
	}

	return myPrmList.getPRMTemplate();
}


OP::VRayNode::PluginResult OBJ::VRayClipper::asPluginDesc(Attrs::PluginDesc &pluginDesc, VRayExporter& /*exporter*/, ExportContext* /*parentContext*/)
{
	pluginDesc.pluginID   = pluginID.c_str();
	pluginDesc.pluginName = VRayExporter::getPluginName(this, "");

	return OP::VRayNode::PluginResultContinue;
}


void OBJ::VRayClipper::setPluginType()
{
	pluginType = VRayPluginType::GEOMETRY;
	pluginID = getVRayPluginIDName(VRayPluginID::VRayClipper);
}


namespace VRayForHoudini {
namespace OBJ {

template< VRayPluginID PluginID >
PRM_Template* LightNodeBase< PluginID >::GetPrmTemplate()
{
	static Parm::PRMList myPrmList;
	if (myPrmList.empty()) {
		PRM_Template *objprmlist = OBJ_Light::getTemplateList(OBJ_PARMS_PLAIN);
		myPrmList.addFromPRMTemplate(objprmlist);

		if (myPrmList.empty()) {
			myPrmList.switcherBegin("stdswitcher");
		}

		for (int i = 0; i < myPrmList.size(); ++i) {
			PRM_Template *tmpl = myPrmList.getPRMTemplate(i);
			UT_ASSERT( tmpl );

			// skip switcher parameters
			if (tmpl->getType() == PRM_SWITCHER) {
				continue;
			}

			int switcher = -1;
			int folder = -1;
			PRM_Template::getEnclosingSwitcherFolder( myPrmList.getPRMTemplate(), i, switcher, folder);
			if (   switcher == 0
				&& folder == 0 ) {
				// skip parameters from Transform folder
				continue;
			}

			// adjust visibility
			UT_StringRef prmName = tmpl->getToken();
			if ( prmName == "dimmer" ) {
				tmpl->setInvisible(true);
			}
		}

		// add plugin params
		myPrmList.addFolder("V-Ray Light");
		GetMyPrmTemplate(myPrmList);

		myPrmList.switcherEnd();
	}

	return myPrmList.getPRMTemplate();
}


template< VRayPluginID PluginID >
int LightNodeBase< PluginID >::GetMyPrmTemplate(Parm::PRMList &myPrmList)
{
	int idx = myPrmList.size();
	Parm::addPrmTemplateForPlugin( getVRayPluginIDName(PluginID), myPrmList);

	return myPrmList.size() - idx;
}


template< VRayPluginID PluginID >
OP::VRayNode::PluginResult LightNodeBase< PluginID >::asPluginDesc(Attrs::PluginDesc &pluginDesc, VRayExporter& /*exporter*/, ExportContext* /*parentContext*/)
{
	pluginDesc.pluginID   = pluginID.c_str();
	pluginDesc.pluginName = VRayExporter::getPluginName(this);

	return OP::VRayNode::PluginResultContinue;
}


// explicitly instantiate CustomPrmTemplates for LightDome op node
template<>
int LightNodeBase< VRayPluginID::LightDome >::GetMyPrmTemplate(Parm::PRMList &myPrmList)
{
	// filter default params and hide unnecessery ones
	for (int i = 0; i < myPrmList.size(); ++i) {
		PRM_Template *tmpl = myPrmList.getPRMTemplate(i);
		UT_ASSERT( tmpl );

		int switcher = -1;
		int folder = -1;
		PRM_Template::getEnclosingSwitcherFolder(myPrmList.getPRMTemplate(), i, switcher, folder);
		if (   switcher == 0
			&& folder == 0 )
		{
			// hide some parameters from Transform folder
			UT_StringRef prmName = tmpl->getToken();
			if (   prmName != "xOrd"
				&& prmName != "rOrd"
				&& prmName != "r"
				&& prmName != "lookatpath"
				&& prmName != "lookup" )
			{
				tmpl->setInvisible(true);
			}
		}
	}

	int idx = myPrmList.size();
	Parm::addPrmTemplateForPlugin( getVRayPluginIDName(VRayPluginID::LightDome), myPrmList);

	return myPrmList.size() - idx;
}


template<>
OP::VRayNode::PluginResult LightNodeBase< VRayPluginID::LightMesh >::asPluginDesc(Attrs::PluginDesc &pluginDesc, VRayExporter &exporter, ExportContext* /*parentContext*/)
{
	const fpreal t = exporter.getContext().getTime();

	pluginDesc.pluginID   = pluginID.c_str();
	pluginDesc.pluginName = VRayExporter::getPluginName(this);

	UT_String geometrypath;
	evalString(geometrypath, "geometry", 0, t);
	if (!geometrypath.equal("")) {
		OBJ_Node *obj_node = getOBJNodeFromPath(geometrypath, t);
		if (!obj_node) {
			Log::getLog().error("Geometry node not found!");
		}
		else {
			OBJ_Geometry *obj_geom = obj_node->castToOBJGeometry();
			if (!obj_geom) {
				Log::getLog().error("Geometry node export failed!");
			}
			else {
				VRay::Plugin geometry = exporter.getObjectExporter().exportGeometry(*obj_node);
				if (geometry) {
					pluginDesc.addAttribute(Attrs::PluginAttr("geometry", geometry));
				}
			}
		}
	}

	return OP::VRayNode::PluginResultContinue;
}


template<>
OP::VRayNode::PluginResult LightNodeBase< VRayPluginID::SunLight >::asPluginDesc(Attrs::PluginDesc &pluginDesc, VRayExporter &exporter, ExportContext* /*parentContext*/)
{
	pluginDesc.pluginID   = pluginID.c_str();
	pluginDesc.pluginName = VRayExporter::getPluginName(this);

	pluginDesc.addAttribute(Attrs::PluginAttr("up_vector", VRay::Vector(0.f,1.f,0.f)));

	UT_String targetpath;
	evalString(targetpath, "lookatpath", 0, exporter.getContext().getTime());

	OP_Node *opNode = getOpNodeFromPath(targetpath, exporter.getContext().getTime());
	if (opNode) {
		OBJ_Node *targetNode = CAST_OBJNODE(opNode);
		if (targetNode) {
			const VRay::Transform &tm = exporter.getObjTransform(targetNode, exporter.getContext());
			pluginDesc.addAttribute(Attrs::PluginAttr("target_transform", tm));
		}
	}

	return OP::VRayNode::PluginResultContinue;
}

static VRay::Plugin exportAttributeFromPathAuto(VRayExporter &exporter,
                                                const OP_Node &node,
                                                const char *attrName,
                                                VRayExporter::DefaultMappingType mappingType,
                                                Attrs::PluginDesc &pluginDesc)
{
	static boost::format fmtToggle("use_%s_tex");
	static boost::format fmtTex("%s_tex");

	const std::string toggleAttrName(str(fmtToggle % attrName));
	const std::string texAttrName(str(fmtTex % attrName));

	const OP_Context &ctx = exporter.getContext();
	const fpreal t = ctx.getTime();

	if (!node.evalInt(toggleAttrName.c_str(), 0, t))
		return VRay::Plugin();

	UT_String texPath;
	node.evalString(texPath, texAttrName.c_str(), 0, t);

	const VRay::Plugin texPlugin = exporter.exportNodeFromPathWithDefaultMapping(texPath, mappingType);
	if (texPlugin) {
		pluginDesc.addAttribute(Attrs::PluginAttr(texAttrName, texPlugin));
	}

	return texPlugin;
}

template<>
OP::VRayNode::PluginResult LightNodeBase<VRayPluginID::LightDome>::asPluginDesc(Attrs::PluginDesc &pluginDesc, VRayExporter &exporter, ExportContext* /*parentContext*/)
{
	pluginDesc.pluginID = pluginID.c_str();
	pluginDesc.pluginName = VRayExporter::getPluginName(this);
	
	const VRayExporter::DefaultMappingType domeMapping = VRayExporter::defaultMappingSpherical;

	const VRay::Plugin domeTex = exportAttributeFromPathAuto(exporter, *this, "dome", domeMapping, pluginDesc);
	if (!domeTex) {
		pluginDesc.add(Attrs::PluginAttr("use_dome_tex", false));
	}

	exportAttributeFromPathAuto(exporter, *this, "color",       domeMapping, pluginDesc);
	exportAttributeFromPathAuto(exporter, *this, "intensity",   domeMapping, pluginDesc);
	exportAttributeFromPathAuto(exporter, *this, "shadowColor", domeMapping, pluginDesc);

	return PluginResultContinue;
}

template<>
OP::VRayNode::PluginResult LightNodeBase<VRayPluginID::LightRectangle>::asPluginDesc(Attrs::PluginDesc &pluginDesc, VRayExporter &exporter, ExportContext* /*parentContext*/)
{
	const fpreal t = exporter.getContext().getTime();

	pluginDesc.pluginID = pluginID.c_str();
	pluginDesc.pluginName = VRayExporter::getPluginName(this);

	const VRayExporter::DefaultMappingType rectMapping = VRayExporter::defaultMappingChannel;

	const VRay::Plugin rectTex = exportAttributeFromPathAuto(exporter, *this, "rect", rectMapping, pluginDesc);
	if (!rectTex) {
		pluginDesc.add(Attrs::PluginAttr("use_rect_tex", false));
	}

	exportAttributeFromPathAuto(exporter, *this, "color",       rectMapping, pluginDesc);
	exportAttributeFromPathAuto(exporter, *this, "intensity",   rectMapping, pluginDesc);
	exportAttributeFromPathAuto(exporter, *this, "shadowColor", rectMapping, pluginDesc);

	pluginDesc.add(Attrs::PluginAttr("u_size", evalFloat("u_size", 0, t) / 2.0f));
	pluginDesc.add(Attrs::PluginAttr("v_size", evalFloat("v_size", 0, t) / 2.0f));

	return PluginResultContinue;
}

template class LightNodeBase<VRayPluginID::SunLight>;
template class LightNodeBase<VRayPluginID::LightDirect>;
template class LightNodeBase<VRayPluginID::LightAmbient>;
template class LightNodeBase<VRayPluginID::LightOmni>;
template class LightNodeBase<VRayPluginID::LightSphere>;
template class LightNodeBase<VRayPluginID::LightSpot>;
template class LightNodeBase<VRayPluginID::LightRectangle>;
template class LightNodeBase<VRayPluginID::LightMesh>;
template class LightNodeBase<VRayPluginID::LightIES>;
template class LightNodeBase<VRayPluginID::LightDome>;

} // namespace OBJ

} // namespace VRayForHoudini
