//
// Copyright (c) 2015-2017, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#ifndef VRAY_FOR_HOUDINI_OP_NODE_BASE_H
#define VRAY_FOR_HOUDINI_OP_NODE_BASE_H

#include "vfh_exporter.h"
#include "vfh_export_context.h"

namespace VRayForHoudini {
namespace OP {

/// Base class for all Houdini nodes that will represent concrete V-Ray plugin
class VRayNode
{
public:
	enum PluginResult {
		PluginResultSuccess = 0,
		PluginResultContinue,
		PluginResultError,
		PluginResultNA,
	};

	VRayNode()
		: pluginType(VRayPluginType::UNKNOWN)
		, pluginInfo(nullptr)
	{}

	virtual ~VRayNode() {}

	/// Extra initialization called by the creator
	virtual void init();

	/// Get the plugin type. Plugins are labeled with different categories:
	/// lights, geometry, textures, uvw generators, etc. The plugin category is
	/// used with custom VOPs for example to determine input/output plug type and color.
	VRayPluginType getVRayPluginType() const { return pluginType; }

	/// Get the V-Ray plugin name.
	std::string getVRayPluginID() const { return pluginID; }

	/// Get the static meta information about this specific plugin type
	const Parm::VRayPluginInfo* getVRayPluginInfo() const { return pluginInfo; }

	/// Export the plugin instance as plugin description
	/// @param pluginDesc[out] - accumulates attributes' changes
	/// @param exporter[in] - reference to the main vfh exporter
	/// @param parentContext[in] - not used
	virtual PluginResult asPluginDesc(Attrs::PluginDesc &pluginDesc, VRayExporter &exporter, ExportContext *parentContext=nullptr) {
		return PluginResultNA;
	}

protected:
	/// Override this to initilize the concrete plugin category and type
	virtual void setPluginType()=0;

	/// The plugin category.
	VRayPluginType pluginType;

	/// The plugin type.
	std::string pluginID;

	/// Provides static meta information about this specific plugin type
	/// like attributes, their types and default values, which attributes
	/// correspond to input/output plugs for VOPs, etc.
	/// TODO: better we should use or wrap around VRay::PluginMeta and
	/// VRay::PropertyMeta from AppSDK as these meta descriptions change
	/// often with the specific V-Ray version (which now makes our JSON
	/// descriptions outdated).
	const Parm::VRayPluginInfo *pluginInfo;

	VfhDisableCopy(VRayNode)
};

} // namespace OP
} // namespace VRayForHoudini

#endif // VRAY_FOR_HOUDINI_OP_NODE_BASE_H
