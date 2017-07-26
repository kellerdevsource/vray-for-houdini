//
// Copyright (c) 2015-2016, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#ifndef VRAY_FOR_HOUDINI_VRAY_PLUGIN_INFO_H
#define VRAY_FOR_HOUDINI_VRAY_PLUGIN_INFO_H

#include "vfh_defines.h"
#include "vfh_prm_def.h"

namespace VRayForHoudini {
namespace Parm {

/// Export description for a single V-Ray plugin.
struct VRayPluginInfo {
	VRayPluginInfo() {}

	/// Checks if plugins has an attribute.
	/// @param attrName Attribute name.
	bool hasAttribute(const tchar *attrName) const;

	/// Returns attribute description.
	/// @param attrName Attribute name.
	const AttrDesc &getAttribute(const tchar *attrName) const;

	/// Attribute description list.
	AttributeDescs attributes;

	/// Input sockets.
	VRayNodeSockets inputs;

	/// Output sockets.
	VRayNodeSockets outputs;

	VfhDisableCopy(VRayPluginInfo)
};

/// Get plugin info for a given plugin ID.
/// @param pluginID V-Ray plugin ID.
/// @returns VRayPluginInfo instance.
const VRayPluginInfo* getVRayPluginInfo(const char *pluginID);

} // namespace Parm
} // namespace VRayForHoudini

#endif // VRAY_FOR_HOUDINI_VRAY_PLUGIN_INFO_H
