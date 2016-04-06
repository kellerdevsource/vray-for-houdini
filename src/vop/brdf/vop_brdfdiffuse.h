//
// Copyright (c) 2015-2016, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#ifndef VRAY_FOR_HOUDINI_VOP_NODE_BRDFDIFFUSE_H
#define VRAY_FOR_HOUDINI_VOP_NODE_BRDFDIFFUSE_H

#include "vop_node_base.h"


namespace VRayForHoudini {
namespace VOP {


class BRDFDiffuse:
		public NodeBase
{
public:
	BRDFDiffuse(OP_Network *parent, const char *name, OP_Operator *entry):NodeBase(parent, name, entry) {}
	virtual                  ~BRDFDiffuse() {}

	virtual void              getCode(UT_String &codestr, const VOP_CodeGenContext &context) VRAY_OVERRIDE;

protected:
	virtual void              setPluginType() VRAY_OVERRIDE;

}; // BRDFDiffuse

} // namespace VOP
} // namespace VRayForHoudini

#endif // VRAY_FOR_HOUDINI_VOP_NODE_BRDFDIFFUSE_H
