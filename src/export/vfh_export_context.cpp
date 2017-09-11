//
// Copyright (c) 2015-2017, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#include "vfh_export_context.h"
#include "vfh_exporter.h"

#include <PRM/PRM_ParmMicroNode.h>
#include <OBJ/OBJ_Node.h>
#include <SHOP/SHOP_Node.h>
#include <SHOP/SHOP_Util.h>


using namespace VRayForHoudini;


ExportContext::ExportContext():
	m_type(CT_NULL),
	m_exporter(nullptr),
	m_target(nullptr),
	m_parentContext(nullptr)
{ }


ExportContext::ExportContext(ContextType type, VRayExporter &exporter, OP_Node &node):
	m_type(type),
	m_exporter(&exporter),
	m_target(&node),
	m_parentContext(nullptr)
{ }


ExportContext::ExportContext(ContextType type, VRayExporter &exporter, OP_Node &node, ExportContext &parentContext):
	m_type(type),
	m_exporter(&exporter),
	m_target(&node),
	m_parentContext(&parentContext)
{ }


ECFnOBJNode::ECFnOBJNode(ExportContext *ctx):
	m_context(nullptr)
{
	if (   ctx
		&& ctx->hasType(CT_OBJ)
		&& ctx->getExporter()
		&& ctx->getTarget()
		&& ctx->getTarget()->castToOBJNode())
	{
		m_context = ctx;
	}
}


bool ECFnOBJNode::isValid() const
{
	return (m_context);
}


OBJ_Node* ECFnOBJNode::getTargetNode() const
{
	return m_context->getTarget()->castToOBJNode();
}
