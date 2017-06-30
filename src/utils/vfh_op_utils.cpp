//
// Copyright (c) 2015-2017, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text:
//   https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#include "vfh_op_utils.h"
#include "vop_node_base.h"

#include <SHOP/SHOP_Node.h>

using namespace VRayForHoudini;

OP_Node* VRayForHoudini::getVRayNodeFromOp(OP_Node &matNode, const char *socketName, const char *pluginID)
{
	OP_Node *res = &matNode;

	SHOP_Node *shopNode = CAST_SHOPNODE(res);
	if (shopNode) {
		UT_ValArray<OP_Node*> opsByName;
		if (shopNode->getOpsByName("vray_material_output", opsByName)) {
			OP_Node *node = opsByName(0);
			if (node) {
				const int socketIdx = node->getInputFromName(socketName);
				res = node->getInput(socketIdx);
			}
		}
	}

	VOP_Node *vopNode = CAST_VOPNODE(res);
	if (vopNode && vopNode->getOperator()->getName().startsWith("VRayNode")) {
		VOP::NodeBase *vrayVopNode = static_cast<VOP::NodeBase*>(vopNode);
		if (vrayVopNode->getVRayPluginID() != pluginID) {
			return nullptr;
		}
	}

	return res;
}
