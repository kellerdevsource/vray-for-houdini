//
// Copyright (c) 2015, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#include "vop_GeomStaticSmoothedMesh.h"


using namespace VRayForHoudini;


void VOP::GeomStaticSmoothedMesh::setPluginType()
{
	pluginType = "GEOMETRY";
	pluginID   = "GeomStaticSmoothedMesh";
}


OP::VRayNode::PluginResult VOP::GeomStaticSmoothedMesh::asPluginDesc(Attrs::PluginDesc &pluginDesc, VRayExporter &exporter, ExportContext *parentContext)
{
	Log::getLog().warning("OP::GeomStaticSmoothedMesh::asPluginDesc()");

	return OP::VRayNode::PluginResultContinue;
}


void VOP::GeomStaticSmoothedMesh::getCode(UT_String &codestr, const VOP_CodeGenContext &)
{
}
