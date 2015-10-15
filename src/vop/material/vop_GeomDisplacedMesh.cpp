//
// Copyright (c) 2015, Chaos Software Ltd
//
// V-Ray For Houdini
//
// Andrei Izrantcev <andrei.izrantcev@chaosgroup.com>
//
// All rights reserved. These coded instructions, statements and
// computer programs contain unpublished information proprietary to
// Chaos Software Ltd, which is protected by the appropriate copyright
// laws and may not be disclosed to third parties or copied or
// duplicated, in whole or in part, without prior written consent of
// Chaos Software Ltd.
//

#include "vop_GeomDisplacedMesh.h"


using namespace VRayForHoudini;


void VOP::GeomDisplacedMesh::setPluginType()
{
	pluginType = "GEOMETRY";
	pluginID   = "GeomDisplacedMesh";
}


OP::VRayNode::PluginResult VOP::GeomDisplacedMesh::asPluginDesc(Attrs::PluginDesc &pluginDesc, VRayExporter *exporter, OP_Node *parent)
{
	PRINT_WARN("OP::GeomDisplacedMesh::asPluginDesc()");

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
