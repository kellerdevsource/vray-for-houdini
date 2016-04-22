//
// Copyright (c) 2015-2016, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#ifndef VRAY_FOR_HOUDINI_SOP_NODE_VRAYPROXYROP_H
#define VRAY_FOR_HOUDINI_SOP_NODE_VRAYPROXYROP_H

#include "vfh_vray.h"
#include <SOP/SOP_Node.h>


namespace VRayForHoudini {
namespace SOP {

class VRayProxyROP:
		public SOP_Node
{
public:
	static OP_Node *        creator(OP_Network *parent, const char *name, OP_Operator *entry);
	static PRM_Template *   getPrmTemplate();

public:
	virtual OP_NodeFlags &    flags() VRAY_OVERRIDE;
	virtual OP_ERROR          cookMySop(OP_Context &context) VRAY_OVERRIDE;

protected:
	VRayProxyROP(OP_Network *parent, const char *name, OP_Operator *entry):
		SOP_Node(parent, name, entry)
	{ }
	virtual	~VRayProxyROP()
	{ }
};

} // namespace SOP
} // namespace VRayForHoudini

#endif // VRAY_FOR_HOUDINI_SOP_NODE_VRAYPROXYROP_H
