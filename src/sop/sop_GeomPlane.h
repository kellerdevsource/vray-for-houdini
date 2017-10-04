//
// Copyright (c) 2015-2017, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#ifndef VRAY_FOR_HOUDINI_SOP_NODE_GEOMPLANE_H
#define VRAY_FOR_HOUDINI_SOP_NODE_GEOMPLANE_H

#include "sop_node_base.h"
#include "vfh_prm_templates.h"


namespace VRayForHoudini {
namespace SOP {

/// SOP node that creates V-Ray infinite plane geometry
/// TODO: this has to generate custom primitive for the plane geo
class GeomPlane
	: public SOP::NodeBase
{
public:
	GeomPlane(OP_Network *parent, const char *name, OP_Operator *entry)
		: NodeBase(parent, name, entry)
	{}

	/// Houdini callback to cook custom geometry for this node
	/// @param context[in] - cook time
	OP_ERROR cookMySop(OP_Context &context) VRAY_OVERRIDE;

	/// Callback called by vfh exporter when exporting the node
	/// @param pluginDesc[out] - dynamic map containing plugin (property, value) pairs
	/// @param exporter - reference to main vfh exporter
	/// @param parentContext - context in which this node is exported
	///                        used to pass some data that will be needed during the export
	/// @retval result of the export for this node
	///         PluginResultSuccess - on success
	///         PluginResultContinue - on success, but singnals that not all relevant properties have been added
	///         PluginResultError- there has been an error
	PluginResult asPluginDesc(Attrs::PluginDesc &pluginDesc, VRayExporter &exporter, ExportContext *parentContext=nullptr) VRAY_OVERRIDE;

protected:
	/// Set custom plugin id and type for this node
	void setPluginType() VRAY_OVERRIDE;
};

} // namespace SOP
} // namespace VRayForHoudini

#endif // VRAY_FOR_HOUDINI_SOP_NODE_GEOMPLANE_H
