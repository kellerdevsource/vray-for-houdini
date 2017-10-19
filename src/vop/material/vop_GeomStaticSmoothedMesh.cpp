//
// Copyright (c) 2015-2017, Chaos Software Ltd
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
	pluginType = VRayPluginType::GEOMETRY;
	pluginID   = "GeomStaticSmoothedMesh";
}


OP::VRayNode::PluginResult VOP::GeomStaticSmoothedMesh::asPluginDesc(Attrs::PluginDesc &pluginDesc, VRayExporter &exporter, ExportContext *parentContext)
{
	ECFnOBJNode fnObjContext(parentContext);
	if (NOT(fnObjContext.isValid())) {
		return OP::VRayNode::PluginResultError;
	}

	pluginDesc.pluginName = VRayExporter::getPluginName(fnObjContext.getTargetNode(), boost::str(Parm::FmtPrefixManual % pluginID % "@"));
	pluginDesc.pluginID = pluginID;

	return OP::VRayNode::PluginResultContinue;
}


void VOP::GeomStaticSmoothedMesh::getCode(UT_String &codestr, const VOP_CodeGenContext &)
{
}
