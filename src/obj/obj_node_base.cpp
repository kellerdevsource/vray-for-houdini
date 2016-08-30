//
// Copyright (c) 2015-2016, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#include "obj_node_base.h"
#include "vfh_export_geom.h"


using namespace VRayForHoudini;


const char *OBJ::getVRayPluginTypeName(VRayPluginType pluginType)
{
	static const char* pluginTypeNames[static_cast<std::underlying_type<VRayPluginType>::type>( VRayPluginType::MAX_PLUGINTYPE )] =
	{
		"LIGHT",
		"GEOMETRY"
	};

	return (pluginType < VRayPluginType::MAX_PLUGINTYPE)? pluginTypeNames[static_cast<std::underlying_type<VRayPluginType>::type>( pluginType )] : nullptr;
}


const char *OBJ::getVRayPluginIDName(VRayPluginID pluginID)
{
	static const char* pluginIDNames[static_cast<std::underlying_type<VRayPluginID>::type>( VRayPluginID::MAX_PLUGINID )] =
	{
		"SunLight",
		"LightDirect",
		"LightAmbient",
		"LightOmni",
		"LightSphere",
		"LightSpot",
		"LightRectangle",
		"LightMesh",
		"LightIES",
		"LightDome",
		"VRayClipper"
	};

	return (pluginID < VRayPluginID::MAX_PLUGINID)? pluginIDNames[static_cast<std::underlying_type<VRayPluginID>::type>( pluginID )] : nullptr;
}


///////                           VRayClipper definition
///

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
		const std::string dsfullpath = Parm::PRMList::getUIPluginPath( getVRayPluginIDName(VRayPluginID::VRayClipper) );
		myPrmList.addFromFile(dsfullpath.c_str());

		myPrmList.switcherEnd();
	}

	return myPrmList.getPRMTemplate();
}


OP::VRayNode::PluginResult OBJ::VRayClipper::asPluginDesc(Attrs::PluginDesc &pluginDesc, VRayExporter &exporter, ExportContext *parentContext)
{
	pluginDesc.pluginID   = pluginID.c_str();
	pluginDesc.pluginName = VRayExporter::getPluginName(this, "");

	return OP::VRayNode::PluginResultContinue;
}


void OBJ::VRayClipper::setPluginType()
{
	pluginType = getVRayPluginTypeName(VRayPluginType::Geometry);
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
	const std::string dsfullpath = Parm::PRMList::getUIPluginPath( getVRayPluginIDName(PluginID) );
	myPrmList.addFromFile(dsfullpath.c_str());

	return myPrmList.size() - idx;
}


template< VRayPluginID PluginID >
OP::VRayNode::PluginResult LightNodeBase< PluginID >::asPluginDesc(Attrs::PluginDesc &pluginDesc, VRayExporter &exporter, ExportContext *parentContext)
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
	const std::string dsfullpath = Parm::PRMList::getUIPluginPath( getVRayPluginIDName(VRayPluginID::LightDome) ) ;
	myPrmList.addFromFile(dsfullpath.c_str());

	return myPrmList.size() - idx;
}


template<>
OP::VRayNode::PluginResult LightNodeBase< VRayPluginID::LightMesh >::asPluginDesc(Attrs::PluginDesc &pluginDesc, VRayExporter &exporter, ExportContext *parentContext)
{
	const fpreal t = exporter.getContext().getTime();

	pluginDesc.pluginID   = pluginID.c_str();
	pluginDesc.pluginName = VRayExporter::getPluginName(this);

	UT_String geometrypath;
	evalString(geometrypath, "geometry", 0, t);
	if (NOT(geometrypath.equal(""))) {
		OP_Node *op_node = OPgetDirector()->findNode(geometrypath.buffer());
		if (op_node) {
			OBJ_Node *obj_node = op_node->castToOBJNode();
			if (obj_node) {
				OBJ_Geometry * obj_geom = obj_node->castToOBJGeometry();
				if (obj_geom) {
					GeometryExporter geoExporter(*obj_geom, exporter);
					if (geoExporter.exportNodes() > 0) {
						Attrs::PluginAttr *attr = geoExporter.getPluginDescAt(0).get("geometry");
						if (attr) {
							pluginDesc.addAttribute(Attrs::PluginAttr("geometry", attr->paramValue.valPlugin));
						}
					}
				}
				else {
					Log::getLog().error("Geometry node export failed!");
				}
			}
			else {
				Log::getLog().error("Geometry node not found!");
			}
		}
		else {
			Log::getLog().error("Geometry node not found!");
		}
	}

	return OP::VRayNode::PluginResultContinue;
}


template<>
OP::VRayNode::PluginResult LightNodeBase< VRayPluginID::SunLight >::asPluginDesc(Attrs::PluginDesc &pluginDesc, VRayExporter &exporter, ExportContext *parentContext)
{
	pluginDesc.pluginID   = pluginID.c_str();
	pluginDesc.pluginName = VRayExporter::getPluginName(this);

	pluginDesc.addAttribute(Attrs::PluginAttr("up_vector", VRay::Vector(0.f,1.f,0.f)));

	UT_String targetpath;
	evalString(targetpath, "lookatpath", 0, exporter.getContext().getTime());
	OBJ_Node *targetNode = OPgetDirector()->findOBJNode(targetpath);
	if (targetNode) {
		VRay::Transform tm = exporter.getObjTransform(targetNode, exporter.getContext());
		pluginDesc.addAttribute(Attrs::PluginAttr("target_transform", tm));
	}

	return OP::VRayNode::PluginResultContinue;
}


// explicitly instantiate op node classes for light plugins
template class LightNodeBase< VRayPluginID::SunLight >;
template class LightNodeBase< VRayPluginID::LightDirect >;
template class LightNodeBase< VRayPluginID::LightAmbient >;
template class LightNodeBase< VRayPluginID::LightOmni >;
template class LightNodeBase< VRayPluginID::LightSphere >;
template class LightNodeBase< VRayPluginID::LightSpot >;
template class LightNodeBase< VRayPluginID::LightRectangle >;
template class LightNodeBase< VRayPluginID::LightMesh >;
template class LightNodeBase< VRayPluginID::LightIES >;
template class LightNodeBase< VRayPluginID::LightDome >;

}
}
