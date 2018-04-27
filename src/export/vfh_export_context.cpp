//
// Copyright (c) 2015-2018, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#include "vfh_export_context.h"
#include "vfh_exporter.h"

using namespace VRayForHoudini;

ExportContext::ExportContext()
	: m_type(CT_NULL)
	, m_exporter(nullptr)
	, m_target(nullptr)
	, m_parentContext(nullptr)
{ }

ExportContext::ExportContext(ContextType type, VRayExporter &exporter, OP_Node &node)
	: m_type(type)
	, m_exporter(&exporter)
	, m_target(&node)
	, m_parentContext(nullptr)
{}

ExportContext::ExportContext(ContextType type, VRayExporter &exporter, OP_Node &node, ExportContext &parentContext)
	: m_type(type)
	, m_exporter(&exporter)
	, m_target(&node)
	, m_parentContext(&parentContext)
{}
