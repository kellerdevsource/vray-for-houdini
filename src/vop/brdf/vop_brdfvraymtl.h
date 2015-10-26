//
// Copyright (c) 2015, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#ifndef VRAY_FOR_HOUDINI_VOP_NODE_BRDFVRAYMTL_H
#define VRAY_FOR_HOUDINI_VOP_NODE_BRDFVRAYMTL_H

#include "vop_node_base.h"


namespace VRayForHoudini {
namespace VOP {


class BRDFVRayMtl:
		public NodeBase
{
public:
	BRDFVRayMtl(OP_Network *parent, const char *name, OP_Operator *entry):NodeBase(parent, name, entry) {}
	virtual                  ~BRDFVRayMtl() {}

	virtual void              getCode(UT_String &codestr, const VOP_CodeGenContext &context) VRAY_OVERRIDE;

	// From OP::VRayNode
	virtual PluginResult      asPluginDesc(Attrs::PluginDesc &pluginDesc, VRayExporter &exporter, OP_Node *parent=nullptr) VRAY_OVERRIDE;

protected:
	virtual void              setPluginType() VRAY_OVERRIDE;

}; // BRDFDiffuse

} // namespace VOP
} // namespace VRayForHoudini

#endif // VRAY_FOR_HOUDINI_VOP_NODE_BRDFVRAYMTL_H
