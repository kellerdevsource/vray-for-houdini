//
// Copyright (c) 2015-2018, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#include "vop_node_base.h"

#include "vfh_exporter.h"
#include "vfh_export_vop.h"
#include "vfh_op_utils.h"

#include <OP/OP_Input.h>
#include <VOP/VOP_Node.h>
#include <SHOP/SHOP_Node.h>

using namespace VRayForHoudini;

static const VRay::Transform envMatrix(VRay::Matrix(VRay::Vector(1.f, 0.f, 0.f),
                                                    VRay::Vector(0.f, 0.f, 1.f),
                                                    VRay::Vector(0.f, -1.f, 0.f)),
                                       VRay::Vector(0.f));

/// Get socket VOP_Type.
/// @param vopNode VOP_Node instance.
/// @param socketIndex Socket index.
/// @param isInput Is soket input or output.
static VOP_Type getSocketVopType(VOP_Node &vopNode, int socketIndex, int isInput=true)
{
	return isInput ? vopNode.getInputType(socketIndex) : vopNode.getOutputType(socketIndex);
}

/// Set some defaults for render channel (e.g. if channel name is missing).
/// @param chanNode Render channel node.
/// @param pluginDesc Render channel plugin description.
static void setDefaultsRenderChannel(VOP_Node &chanNode, Attrs::PluginDesc &pluginDesc)
{
	Attrs::PluginAttr *attrChanName = pluginDesc.get(SL("name"));
	if (!attrChanName || attrChanName->paramValue.valString.isEmpty()) {
		const QString channelName(chanNode.getName().buffer());
		if (!attrChanName) {
			pluginDesc.add(SL("name"), channelName);
		}
		else {
			attrChanName->paramValue.valString = channelName;
		}
	}
}

/// Set some defaults for UVWGenEnvironment.
/// @param pluginDesc Render channel plugin description.
static void setDefaultsUVWGenEnvironment(Attrs::PluginDesc &pluginDesc)
{
	if (!pluginDesc.contains(SL("uvw_matrix"))) {
		pluginDesc.add(SL("uvw_matrix"), envMatrix);
	}
}

/// Find first node of type @a opType inside @a onNetwork.
/// @param opNetwork OP_Network node.
/// @param opType Node operator type to search for.
/// @returns OP_Node* instance if found, nullptr - otherwise.
static OP_Node *findOpNodeByType(OP_Network &opNetwork, const char *opType)
{
	if (!opNetwork.isNetwork())
		return nullptr;

	OP_NodeList opList;
	if (opNetwork.getOpsByName(opType, opList)) {
		return opList(0);
	}

	return nullptr;
}

/// Find output index node socket is connected to.
/// @param socketIndex Socket index.
/// @param opNode Socket owner.
/// @returns Output socket index of the node @a socketIndex is connected to.
static int getConnectedOutputIndex(int socketIndex, OP_Node &opNode)
{
	OP_Input *conOutput = opNode.getInputReferenceConst(socketIndex);
	if (!conOutput)
		return -1;
	return conOutput->getNodeOutputIndex();
}

ShaderExporter::ShaderExporter(VRayExporter &pluginExporter)
	: pluginExporter(pluginExporter)
{}

void ShaderExporter::reset()
{
	cacheMan.clear();
}

VRay::PluginRef ShaderExporter::exportSubnet(VOP_Node &subnet, const SocketInfo &fromSocket)
{
	vassert(fromSocket.vopNode);

	VOP_Node *subnetOutput = subnet.getSubnetOutputNode();
	if (!subnetOutput) {
		Log::getLog().error("\"%s\": \"subnet\" output is not found!",
		                    subnet.getFullPath().nonNullBuffer());
		return VRay::PluginRef();
	}

	// Find "subnet" output index we are connected to.
	const int subnetSocketIndex = getConnectedOutputIndex(fromSocket.index, *fromSocket.vopNode);
	vassert(subnetSocketIndex >= 0);

	// Get this output name.
	UT_String subnetOutputName;
	subnet.getOutputName(subnetOutputName, subnetSocketIndex);
	vassert(subnetOutputName.isstring());

	// Find correspondent input at the "suboutput" node.
	const int subnetOutputInputIndex = subnetOutput->getInputFromName(subnetOutputName);
	if (subnetOutputInputIndex < 0) {
		Log::getLog().error("\"%s\": \"%s\" can't find correspondent input for output \"%s\"!",
		                    subnet.getFullPath().nonNullBuffer(),
		                    subnetOutput->getName().nonNullBuffer(),
		                    subnetOutputName.nonNullBuffer());
		return VRay::PluginRef();
	}

	SocketInfo subnetOutputInfo;
	subnetOutputInfo.vopNode = &subnet;
	subnetOutputInfo.index = subnetOutputInputIndex;
	subnetOutputInfo.name = subnetOutputName;

	return exportConnectedSocket(*subnetOutput, subnetOutputInfo);
}

VRay::PluginRef ShaderExporter::exportSubnetInput(VOP_Node &subnetInput, const SocketInfo &fromSocket)
{
	// Get "subnet" node this "subinput" belongs to.
	VOP_Node *subnet = CAST_VOPNODE(subnetInput.getParentNetwork());
	vassert(subnet);

	// Find "subinput" output index we are connected to.
	const int subinputSocketIndex = getConnectedOutputIndex(fromSocket.index, *fromSocket.vopNode);
	vassert(subinputSocketIndex >= 0);

	// Get this "subinput" output name.
	UT_String subinputOutputName;
	subnetInput.getOutputName(subinputOutputName, subinputSocketIndex);
	vassert(subinputOutputName.isstring());

	// Find correspondent input at the "subnet" node.
	const int subnetInputIndex = subnet->getInputFromName(subinputOutputName);
	if (subnetInputIndex < 0) {
		Log::getLog().error("\"%s\": \"%s\" can't find correspondent input for \"%s\"!",
		                    subnet->getFullPath().nonNullBuffer(),
		                    subnetInput.getName().nonNullBuffer(),
		                    subinputOutputName.nonNullBuffer());
		return VRay::PluginRef();
	}

	SocketInfo subnetInputInfo;
	subnetInputInfo.vopNode = subnet;
	subnetInputInfo.index = subnetInputIndex;
	subnetInputInfo.name = subinputOutputName;

	return exportConnectedSocket(*subnet, subnetInputInfo);
}

VRay::PluginRef ShaderExporter::exportVRayNode(VOP_Node &vopNode, const SocketInfo &fromSocket)
{
	VRay::PluginRef plugin;

	VOP::NodeBase &vrayNode = static_cast<VOP::NodeBase&>(vopNode);

	if (!cacheMan.getMatPlugin(vopNode, plugin)) {
		Attrs::PluginDesc pluginDesc(VRayExporter::getPluginName(vopNode),
		                             vrayNode.getVRayPluginID());

		if (isOpType(vopNode, "VRayNodeTexSky")) {
			pluginExporter.fillNodeTexSky(vopNode, pluginDesc);
		}

		const OP::VRayNode::PluginResult res = vrayNode.asPluginDesc(pluginDesc, pluginExporter);
		if (res == OP::VRayNode::PluginResultError) {
			Log::getLog().error("Error creating plugin descripion for node \"%s\"!",
			                    vopNode.getFullPath().nonNullBuffer());
		}
		else if (res == OP::VRayNode::PluginResultNA ||
		         res == OP::VRayNode::PluginResultContinue)
		{
			pluginExporter.setAttrsFromOpNodeConnectedInputs(pluginDesc, vopNode);
#if 0 // Not sure we are still using those...
			pluginExporter.setAttrsFromSHOPOverrides(pluginDesc, vopNode);
#endif
			pluginExporter.setAttrsFromOpNodePrms(pluginDesc, &vopNode);

			// Setup default render channel name if it's missing.
			if (vrayNode.getVRayPluginType() == VRayPluginType::RENDERCHANNEL) {
				setDefaultsRenderChannel(vopNode, pluginDesc);
			}
			else if (vrayNode.getVRayPluginType() == VRayPluginType::UVWGEN) {
				if (pluginDesc.pluginID == SL("UVWGenEnvironment")) {
					setDefaultsUVWGenEnvironment(pluginDesc);
				}
			}

			plugin = pluginExporter.exportPlugin(pluginDesc);

			// Cache even invalid plugins to avoid multiple processing.
			cacheMan.addMatPlugin(vopNode, plugin);
		}
	}

	// Even if plugin is already cached check may be we are connected to another output.
	if (plugin.isNotEmpty()) {
		// Check if we are connected to a specific output.
		if (fromSocket.isValid()) {
			const int outputSocketIndex = getConnectedOutputIndex(fromSocket.index, *fromSocket.vopNode);

			UT_String outputSocketName;
			vopNode.getOutputName(outputSocketName, outputSocketIndex);
			vassert(outputSocketName.isstring());

			const Parm::VRayPluginInfo &vrayPlugInfo = *Parm::getVRayPluginInfo(plugin.getType());
			for (const Parm::SocketDesc &sock : vrayPlugInfo.outputs) {
				if (sock.socketLabel == outputSocketName.buffer()) {
					if (!sock.attrName.isEmpty()) {
						plugin = VRay::PluginRef(plugin, qPrintable(sock.attrName));
					}
					break;
				}
			}
		}
	}

	return plugin;
}

VRay::PluginRef ShaderExporter::exportSwitcher(VOP_Node &switcher, const SocketInfo &fromSocket)
{
	const fpreal t = pluginExporter.getContext().getTime();

	const int switcherIndex = switcher.evalInt("switcher", 0, t);

	VOP_Node *conNode = switcher.findSimpleInput(switcherIndex);
	if (!conNode)
		return VRay::PluginRef();

	return exportConnectedSocket(*conNode, fromSocket);
}

VRay::PluginRef ShaderExporter::exportConnectedSocket(VOP_Node &vopNode, const SocketInfo &socketInfo)
{
	VRay::PluginRef plugin;

	VOP_Node *conNode;
	if (!socketInfo.isValid()) {
		// Process current node.
		conNode = &vopNode;
	}
	else {
		// If socket info is valid then find the connected node.
		// This should pass through nodes like "switch", "null", etc.
		conNode = vopNode.findSimpleInput(socketInfo.index);
	}
	if (!conNode)
		return plugin;

	const UT_String &opType = conNode->getOperator()->getName();

	Log::getLog().debug("Processing node \"%s\" [%s]...",
	                    conNode->getFullPath().nonNullBuffer(),
	                    opType.buffer());

	if (isOpType(*conNode, "subnet")) {
		if (!socketInfo.isValid()) {
			Log::getLog().error("\"%s\": \"subnet\" node could not be directly selected! Yet...",
			                    conNode->getFullPath().nonNullBuffer());
		}
		else {
			plugin = exportSubnet(*conNode, socketInfo);
		}
	}
	else if (isOpType(*conNode, VOP_SUBNET_INPUT_NODE_NAME)) {
		if (!socketInfo.isValid()) {
			Log::getLog().error("\"%s\": \"" VOP_SUBNET_INPUT_NODE_NAME "\" node could not be directly selected!",
			                    conNode->getFullPath().nonNullBuffer());
		}
		else {
			plugin = exportSubnetInput(*conNode, socketInfo);
		}
	}
	else if (isOpType(*conNode, "switch")) {
		if (socketInfo.isValid()) {
			vassert(false && "VOP_Node::findSimpleInput() should have pass this through!");
		}
		else {
			plugin = exportSwitcher(*conNode, socketInfo);
		}
	}
	else if (isOpType(*conNode, "null")) {
		if (socketInfo.isValid()) {
			vassert(false && "VOP_Node::findSimpleInput() should have pass this through!");
		}
		else {
			plugin = pluginExporter.exportConnectedVop(conNode, 0);
		}
	}
	else if (isOpType(*conNode, "parameter")) {
		plugin = pluginExporter.exportConnectedVop(conNode, 0);
	}
	else if (opType.startsWith("principledshader")) {
		plugin = pluginExporter.exportPrincipledShader(*conNode);
	}
	else if (isOpType(*conNode, vfhNodeMaterialOutput)) {
		plugin = exportVRayMaterialOutput(*conNode);
	}
	else if (opType.startsWith("VRayNode")) {
		plugin = exportVRayNode(*conNode, socketInfo);
	}
	else {
		Log::getLog().error("Unsupported VOP node: %s",
		                    conNode->getFullPath().nonNullBuffer());
	}

	if (plugin.isEmpty()) {
		Log::getLog().error("Error exporting VOP node: %s",
		                    conNode->getFullPath().nonNullBuffer());
	}
	else {
		// Auto-convert socket types.
		// We could auto-convert types only if we knew where the connection if coming from.
		if (socketInfo.isValid()) {
			const int conOutputIndex = getConnectedOutputIndex(socketInfo.index, *socketInfo.vopNode);

			const VOP_Type inputType = getSocketVopType(*conNode, conOutputIndex, false);
			const VOP_Type fromType = getSocketVopType(*socketInfo.vopNode, socketInfo.index, false);

			if (inputType != fromType) {
				// TODO:
			}
		}

		// Attach IPR callback.
		pluginExporter.addOpCallback(conNode, VRayExporter::RtCallbackVop);
	}

	return plugin;
}

VRay::PluginRef ShaderExporter::exportConnectedSocket(VOP_Node &vopNode, const UT_String &socketName)
{
	const int socketIndex = vopNode.getInputFromName(socketName.buffer());
	if (socketIndex < 0)
		return VRay::PluginRef();

	SocketInfo socketIndo;
	socketIndo.name = socketName;
	socketIndo.index = socketIndex;

	return exportConnectedSocket(vopNode, socketIndo);
}

VRay::PluginRef ShaderExporter::exportConnectedSocket(VOP_Node &vopNode, int socketIndex)
{
	if (socketIndex < 0)
		return VRay::PluginRef();

	SocketInfo socketIndo;
	vopNode.getInputName(socketIndo.name, socketIndex);
	socketIndo.index = socketIndex;

	return exportConnectedSocket(vopNode, socketIndo);
}

VRay::PluginRef ShaderExporter::exportVRayMaterialBuilder(SHOP_Node &shopNode)
{
	OP_Node *opNode = findOpNodeByType(shopNode, vfhNodeMaterialOutput);
	if (opNode) {
		VOP_Node *vrayMaterialOutput = CAST_VOPNODE(opNode);
		vassert(vrayMaterialOutput);

		return exportVRayMaterialOutput(*vrayMaterialOutput);
	}

	return VRay::PluginRef();
}

VRay::PluginRef ShaderExporter::exportVRayMaterialOutput(VOP_Node &vopNode)
{
	SocketInfo materialSocket;
	materialSocket.vopNode = &vopNode;
	materialSocket.index = 0;
	materialSocket.name = vfhSocketMaterialOutputMaterial;

	return exportConnectedSocket(vopNode, materialSocket);
}

VRay::PluginRef ShaderExporter::exportShaderNode(OP_Node &opNode)
{
	if (isOpType(opNode, vfhNodeMaterialBuilder)) {
		SHOP_Node *shopNode = CAST_SHOPNODE(&opNode);
		vassert(shopNode);

		return exportVRayMaterialBuilder(*shopNode);
	}

	VOP_Node *vopNode = CAST_VOPNODE(&opNode);
	if (vopNode) {
		SocketInfo dummyInfo;
		dummyInfo.vopNode = vopNode;

		return exportConnectedSocket(*vopNode, dummyInfo);
	}

	return VRay::PluginRef();
}
