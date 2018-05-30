//
// Copyright (c) 2015-2018, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#ifndef VRAY_FOR_HOUDINI_EXPORT_VOP_H
#define VRAY_FOR_HOUDINI_EXPORT_VOP_H

#include "vfh_vray.h"

#include <UT/UT_String.h>

class OP_Node;
class VOP_Node;
class SHOP_Node;

namespace VRayForHoudini {

class VRayExporter;

/// Currently processed VOP_Node socket info.
struct SocketInfo
{
	SocketInfo()
		: vopNode(nullptr)
		, index(-1)
	{}

	SocketInfo(VOP_Node *vopNode, const UT_String &name, int index)
		: vopNode(vopNode)
		, name(name)
		, index(index)
	{}

	/// Is socket info valid.
	int isValid() const { return vopNode && index >= 0; }

	/// VOP_Node this socket belongs to.
	VOP_Node *vopNode;

	/// Socket name.
	UT_String name;

	/// Socket index.
	int index;
};

/// V-Ray VOP shaders exporter.
struct ShaderExporter
{
	explicit ShaderExporter(VRayExporter &pluginExporter);

	/// Clear export caches.
	void reset();

	/// Export plugin from the supported VOP_Node type.
	/// This should be used when it's not possible to obtain
	/// the socket the connection is coming from. This could be the case
	/// when exporting top level nodes like "V-Ray Material Builder",
	/// "V-Ray Render Elements", etc, or some VRay VOP node is directly selected.
	/// @param opNode Node to export.
	/// @returns VRay::PluginRef. May be empty for unsupported types or any exporting error.
	VRay::PluginRef exportShaderNode(OP_Node &opNode);

	/// Export plugin from node connected to the socket.
	/// @param vopNode Currently processed node.
	/// @param socketName Currently processed socket name.
	/// @returns VRay::PluginRef. May be empty for unsupported types or any exporting error.
	VRay::PluginRef exportConnectedSocket(VOP_Node &vopNode, const UT_String &socketName);

	/// Export plugin from node connected to the socket.
	/// @param vopNode Currently processed node.
	/// @param socketIndex Currently processed socket index.
	/// @returns VRay::PluginRef. May be empty for unsupported types or any exporting error.
	VRay::PluginRef exportConnectedSocket(VOP_Node &vopNode, int socketIndex);

	/// Export plugin from node connected to the socket.
	/// @param vopNode Currently processed node.
	/// @param socketInfo Currently processed socket info.
	/// @returns VRay::PluginRef. May be empty for unsupported types or any exporting error.
	VRay::PluginRef exportConnectedSocket(VOP_Node &vopNode, const SocketInfo &socketInfo);

private:
	/// Export plugin from "switcher" node.
	/// @param switcher "switcher" node.
	/// @param fromSocket Where this node is connected to.
	/// @returns VRay::PluginRef. May be empty for unsupported types or any exporting error.
	VRay::PluginRef exportSwitcher(VOP_Node &switcher, const SocketInfo &fromSocket);

	/// Export plugin from "subnet" node.
	/// @param subnet "subnet" node.
	/// @param fromSocket Where this node is connected to.
	/// @returns VRay::PluginRef. May be empty for unsupported types or any exporting error.
	VRay::PluginRef exportSubnet(VOP_Node &subnet, const SocketInfo &fromSocket);

	/// Export plugin from "subinput" node.
	/// @param subnetInput "subinput" node.
	/// @param fromSocket Where this node is connected to.
	/// @returns VRay::PluginRef. May be empty for unsupported types or any exporting error.
	VRay::PluginRef exportSubnetInput(VOP_Node &subnetInput, const SocketInfo &fromSocket);

	/// Export "V-Ray Material Buider" SHOP node.
	/// @param shopNode Currently processed node.
	/// @returns VRay::PluginRef. May be empty for unsupported types or any exporting error.
	VRay::PluginRef exportVRayMaterialBuilder(SHOP_Node &shopNode);

	/// Export "V-Ray Material Output" node.
	/// @param vopNode Currently processed node.
	/// @returns VRay::PluginRef. May be empty for unsupported types or any exporting error.
	VRay::PluginRef exportVRayMaterialOutput(VOP_Node &vopNode);

	/// Export custom V-Ray VOP node.
	/// @param vopNode Currently processed node.
	/// @param fromSocket Where this node is connected to. 
	/// @returns VRay::PluginRef. May be empty for unsupported types or any exporting error.
	VRay::PluginRef exportVRayNode(VOP_Node &vopNode, const SocketInfo &fromSocket);

	/// Plugin exporter.
	VRayExporter &pluginExporter;

	/// Processed nodes cache.
	OpCacheMan cacheMan;
};

} // namespace VRayForHoudini

#endif // VRAY_FOR_HOUDINI_EXPORT_VOP_H
