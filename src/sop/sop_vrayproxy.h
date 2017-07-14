//
// Copyright (c) 2015-2016, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#ifndef VRAY_FOR_HOUDINI_SOP_NODE_VRAYPROXY_H
#define VRAY_FOR_HOUDINI_SOP_NODE_VRAYPROXY_H

#include "sop_node_base.h"
#include "vfh_prm_templates.h"

namespace VRayForHoudini {
namespace SOP {

class VRayProxy
	: public SOP::NodeBase
{
public:
	static PRM_Template* getPrmTemplate();

	VRayProxy(OP_Network *parent, const char *name, OP_Operator *entry);
	virtual ~VRayProxy() {}

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

	/// Callback to clear cache for this node ("Reload Geometry" button in the GUI)
	/// @param data - pointer to the node it was called on
	/// @param index - he index of the menu entry
	/// @param t - current evaluation time
	/// @param tplate - pointer to the PRM_Template of the parameter it was triggered for.
	/// @return It should return 1 if you want the dialog to refresh
	///        (ie if you changed any values) and 0 otherwise.
	static int cbClearCache(void *data, int index, fpreal t, const PRM_Template* tplate);

protected:
	/// Set custom plugin id and type for this node
	void setPluginType() VRAY_OVERRIDE;
}; // VRayProxy

} // namespace SOP
} // namespace VRayForHoudini

#endif // VRAY_FOR_HOUDINI_SOP_NODE_VRAYPROXY_H
