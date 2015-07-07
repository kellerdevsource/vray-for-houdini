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

#include "vop_brdfvraymtl.h"


using namespace VRayForHoudini;


void VOP::BRDFVRayMtl::setPluginType()
{
	pluginType = "BRDF";
	pluginID   = "BRDFVRayMtl";
}


void VOP::BRDFVRayMtl::getCode(UT_String &codestr, const VOP_CodeGenContext &)
{
	codestr.sprintf("Cf = {%.3f,%.3f,%.3f};",
					0.0f, 1.0f, 0.0f);
}


OP::VRayNode::PluginResult VOP::BRDFVRayMtl::asPluginDesc(Attrs::PluginDesc &pluginDesc, VRayExporter *exporter, OP_Node *parent)
{
	// TODO:
#if 0
	if (RNA_boolean_get(&brdfVRayMtl, "hilight_glossiness_lock")) {
		pluginAttrs["hilight_glossiness"] = pluginAttrs["reflect_glossiness"];
	}
#endif
	return PluginResult::PluginResultNA;
}
