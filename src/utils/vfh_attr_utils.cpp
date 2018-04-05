//
// Copyright (c) 2015-2017, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#include "vfh_attr_utils.h"
#include "vfh_log.h"

#include <OP/OP_Director.h>
#include <OP/OP_Expression.h>

using namespace VRayForHoudini;

/// Relative path resolver.
static OP_ExprFindOp findOp;

OP_Node* getOpNodeFromPath(const OP_Node &node, const char *path, fpreal t)
{
	if (!UTisstring(path))
		return nullptr;

	UT_String value(path);

	int op_id = 0;
	fpreal op_time = 0.0;
	OPgetDirector()->evalOpPathString(value, 0, 0, t, op_id, op_time);

	return findOp.getNode(value.buffer(), const_cast<OP_Node*>(&node), false);
}

OP_Node* getOpNodeFromAttr(const OP_Node &node, const char *attrName, fpreal t)
{
	UT_String value;
	node.evalString(value, attrName, 0, t);

	if (!value.isstring())
		return nullptr;

	return getOpNodeFromPath(node, value.buffer(), t);
}

OBJ_Node *getOBJNodeFromAttr(const OP_Node &node, const char *attrName, fpreal t)
{
	return CAST_OBJNODE(getOpNodeFromAttr(node, attrName, t));
}

SOP_Node *getSOPNodeFromAttr(const OP_Node &node, const char *attrName, fpreal t)
{
	return CAST_SOPNODE(getOpNodeFromAttr(node, attrName, t));
}

VOP_Node *getVOPNodeFromAttr(const OP_Node &node, const char *attrName, fpreal t)
{
	return CAST_VOPNODE(getOpNodeFromAttr(node, attrName, t));
}

COP2_Node *getCOP2NodeFromAttr(const OP_Node &node, const char *attrName, fpreal t)
{
	return CAST_COP2NODE(getOpNodeFromAttr(node, attrName, t));
}

SHOP_Node *getSHOPNodeFromAttr(const OP_Node &node, const char *attrName, fpreal t)
{
	return CAST_SHOPNODE(getOpNodeFromAttr(node, attrName, t));
}
