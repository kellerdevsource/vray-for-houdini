//
// Copyright (c) 2015-2018, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#ifndef VRAY_FOR_HOUDINI_VRAY_PLUGIN_INFO_H
#define VRAY_FOR_HOUDINI_VRAY_PLUGIN_INFO_H

#include "vfh_vray.h"
#include "vfh_prm_def.h"

namespace VRayForHoudini {
namespace Parm {

/// Export description for a single V-Ray plugin.
struct VRayPluginInfo {
	VRayPluginInfo() {}

	/// Checks if plugins has an attribute.
	/// @param attrName Attribute name.
	VRAY_DEPRECATED("Use hasAttribute(QString&)")
	bool hasAttribute(const tchar *attrName) const;

	/// Checks if plugins has an attribute.
	/// @param attrName Attribute name.
	bool hasAttribute(const QString &attrName) const;

	/// Returns attribute description.
	/// @param attrName Attribute name.
	VRAY_DEPRECATED("Use getAttribute(QString&)")
	const AttrDesc& getAttribute(const tchar *attrName) const;

	/// Returns attribute description.
	/// @param attrName Attribute name.
	const AttrDesc& getAttribute(const QString &attrName) const;

	/// Attribute description list.
	AttributeDescs attributes;

	/// Input sockets.
	VRayNodeSockets inputs;

	/// Output sockets.
	VRayNodeSockets outputs;

	VUTILS_DISABLE_COPY(VRayPluginInfo)
};

/// Get plugin info for a given plugin ID.
/// @param pluginID V-Ray plugin ID.
/// @returns VRayPluginInfo instance.
const VRayPluginInfo* getVRayPluginInfo(const QString &pluginID);

} // namespace Parm
} // namespace VRayForHoudini

#endif // VRAY_FOR_HOUDINI_VRAY_PLUGIN_INFO_H
