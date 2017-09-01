//
// Copyright (c) 2015-2017, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#ifdef CGR_HAS_VRAYSCENE

#ifndef VRAY_FOR_HOUDINI_SOP_NODE_VRAYSCENE_H
#define VRAY_FOR_HOUDINI_SOP_NODE_VRAYSCENE_H

#include "sop_node_base.h"

#include <vrscene_preview.h>


namespace VRayForHoudini {
namespace SOP {

/// SOP that creates geometry from
/// TODO: this has to be implemented as OBJ network with shop subnet for materials and
///       custom primitives to show geometry and proper export
class VRayScene:
		public SOP::NodeBase
{
public:
	VRayScene(OP_Network *parent, const char *name, OP_Operator *entry):
		NodeBase(parent, name, entry)
	{ }
	virtual ~VRayScene()
	{ }

	/// Houdini callback to cook custom geometry for this node
	/// @param context[in] - cook time
	virtual OP_ERROR cookMySop(OP_Context &context) VRAY_OVERRIDE;

	/// Callback called by vfh exporter when exporting the node
	/// @param pluginDesc[out] - dynamic map containing plugin (property, value) pairs
	/// @param exporter - reference to main vfh exporter
	/// @param parentContext - context in which this node is exported
	///                        used to pass some data that will be needed during the export
	/// @retval result of the export for this node
	///         PluginResultSuccess - on success
	///         PluginResultContinue - on success, but singnals that not all relevant properties have been added
	///         PluginResultError- there has been an error
	virtual PluginResult asPluginDesc(Attrs::PluginDesc &pluginDesc, VRayExporter &exporter, ExportContext *parentContext=nullptr) VRAY_OVERRIDE;

protected:
	/// Set custom plugin id and type for this node
	virtual void setPluginType() VRAY_OVERRIDE;

private:
	/// Helper function to generate vrscene settings
	VUtils::Vrscene::Preview::VrsceneSettings getVrsceneSettings() const;

	static VUtils::Vrscene::Preview::VrsceneDescManager  m_vrsceneMan; ///< vrscene manager

}; // VRayScene

} // namespace SOP
} // namespace VRayForHoudini

#endif // VRAY_FOR_HOUDINI_SOP_NODE_VRAYSCENE_H

#endif // CGR_HAS_VRAYSCENE
