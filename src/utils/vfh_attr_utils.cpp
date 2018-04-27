//
// Copyright (c) 2015-2018, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#include "vfh_defines.h"
#include "vfh_attr_utils.h"
#include "vfh_log.h"

#include <OP/OP_Expression.h>

using namespace VRayForHoudini;

/// Relative path resolver.
static OP_ExprFindOp findOp;

UT_String getOpPathFromAttr(const OP_Node &node, const char *attrName, fpreal t)
{
	UT_String value;

	if (!value.startsWith(OPREF_PREFIX)) {
		node.evalString(value, attrName, 0, t);
	}
	else {
		int pi = 0;
		int opId = 0;
		fpreal opTime = 0.0;
		const_cast<OP_Node&>(node).evalOpPathString(value, attrName, pi, 0, t, opId, opTime);
	}

	return value;
}

UT_String getOpPathFromAttr(const OP_Node &node, const QString &attrName, fpreal t)
{
	return getOpPathFromAttr(node, _toChar(attrName), t);
}

OP_Node* getOpNodeFromPath(const OP_Node &node, const char *path, fpreal t)
{
	if (!UTisstring(path))
		return nullptr;

	return findOp.getNode(path, const_cast<OP_Node*>(&node), false);
}

OP_Node* getOpNodeFromAttr(const OP_Node &node, const char *attrName, fpreal t)
{
	const UT_String &value = getOpPathFromAttr(node, attrName, t);
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
