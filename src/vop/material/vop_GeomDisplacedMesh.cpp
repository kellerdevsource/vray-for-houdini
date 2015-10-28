//
// Copyright (c) 2015, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#include "vop_GeomDisplacedMesh.h"


using namespace VRayForHoudini;


void VOP::GeomDisplacedMesh::setPluginType()
{
	pluginType = "GEOMETRY";
	pluginID   = "GeomDisplacedMesh";
}


OP::VRayNode::PluginResult VOP::GeomDisplacedMesh::asPluginDesc(Attrs::PluginDesc &pluginDesc, VRayExporter &exporter, OP_Node *parent)
{
	Log::getLog().warning("OP::GeomDisplacedMesh::asPluginDesc()");

	// Displacement type
	//
	const int &displace_type = evalInt("type", 0, 0.0);

	pluginDesc.pluginAttrs.push_back(Attrs::PluginAttr("displace_2d",         int(displace_type == 1)));
	pluginDesc.pluginAttrs.push_back(Attrs::PluginAttr("vector_displacement", int(displace_type == 2)));

	return OP::VRayNode::PluginResultContinue;
}


void VOP::GeomDisplacedMesh::getCode(UT_String &codestr, const VOP_CodeGenContext &)
{
}
