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
#include "vfh_sys_utils.h"
#include "vfh_prm_templates.h"

#include <OP/OP_Input.h>
#include <OP/OP_Options.h>
#include <VOP/VOP_Node.h>
#include <SHOP/SHOP_Node.h>

using namespace VRayForHoudini;

/// The length of VOP_SUBNET_VARIABLE_PREFIX.
static const int vopSubnetSuffixLength = int(strlen(VOP_SUBNET_VARIABLE_PREFIX));

/// Default Y-up matrix.
static const VRay::Transform envMatrix(VRay::Matrix(VRay::Vector(1.0f, 0.0f, 0.0f),
                                                    VRay::Vector(0.0f, 0.0f, 1.0f),
                                                    VRay::Vector(0.0f, -1.0f, 0.0f)),
                                       VRay::Vector(0.0f));

/// X-Y flip matrix.
static const UT_DMatrix4 yAxisUpRotationMatrix(1.0, 0.0, 0.0, 0.0,
                                               0.0, 0.0, 1.0, 0.0,
                                               0.0, -1.0, 0.0, 0.0,
                                               0.0, 0.0, 0.0, 0.0);

/// Get input socket VOP_Type.
/// @param vopNode VOP_Node instance.
/// @param socketIndex Socket index.
static VOP_Type getVopInputSocketType(const VOP_Node &vopNode, int socketIndex)
{
	return vopNode.getInputType(socketIndex);
}

/// Get output socket VOP_Type.
/// @param vopNode VOP_Node instance.
/// @param socketIndex Socket index.
static VOP_Type getVopOutputSocketType(const VOP_Node &vopNode, int socketIndex)
{
	return const_cast<VOP_Node&>(vopNode).getOutputType(socketIndex);
}

/// Set some defaults for render channel (e.g. if channel name is missing).
/// @param chanNode Render channel node.
/// @param pluginDesc Render channel plugin description.
static void setDefaultsRenderChannel(const VOP_Node &chanNode, Attrs::PluginDesc &pluginDesc)
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
static OP_Node *getOpNodeByType(const OP_Network &opNetwork, const char *opType)
{
	if (!opNetwork.isNetwork())
		return nullptr;

	OP_NodeList opList;
	if (const_cast<OP_Network&>(opNetwork).getOpsByName(opType, opList)) {
		return opList(0);
	}

	return nullptr;
}

/// Find output index node socket is connected to.
/// @param socketIndex Socket index.
/// @param opNode Socket owner.
/// @returns Output socket index of the node @a socketIndex is connected to.
static int getConnectedOutputIndex(int socketIndex, const OP_Node &opNode)
{
	OP_Input *conOutput = opNode.getInputReferenceConst(socketIndex);
	if (!conOutput)
		return -1;
	return conOutput->getNodeOutputIndex();
}

/// Check if node is connected to TexTriplanar.
/// @param vopNode VOP_Node instance.
static int isConnectedToTexTriplanar(const VOP_Node &vopNode)
{
	OP_NodeList outputs;
	vopNode.getOutputNodes(outputs);

	for (const OP_Node *opNode : outputs) {
		if (isOpType(*opNode, "VRayNodeTexTriPlanar")) {
			return true;
		}
	}

	return false;
}

/// Check if we need to export default UVW generator.
static int needDefaultUvwGen(const OP::VRayNode &vrayNode, const Parm::AttrDesc &attrDesc, const QString &attName)
{
	return !isBitSet(attrDesc.flags, Parm::attrFlagLinkedOnly) &&
	       vrayNode.getVRayPluginType() == VRayPluginType::TEXTURE &&
	       attName == SL("uvwgen");
}

/// Check if material plugin has "scene_name" attribute.
/// @param plugin Material plugin.
static int hasMaterialSceneName(const VRay::PluginRef &plugin)
{
	return plugin.getType() == SL("MtlSingleBRDF") ||
	       plugin.getType() == SL("MtlMulti");
}

ShaderExporter::ShaderExporter(VRayExporter &pluginExporter)
	: pluginExporter(pluginExporter)
{}

void ShaderExporter::reset()
{
	cacheMan.clear();
}

VOP_Node *ShaderExporter::getConnectedNode(const VOP_Node &vopNode, const QString &socketName)
{
	const int socketIndex = vopNode.getInputFromName(qPrintable(socketName));
	if (socketIndex < 0)
		return nullptr;
	return const_cast<VOP_Node&>(vopNode).findSimpleInput(socketIndex);
}

VOP_Node *ShaderExporter::getConnectedNode(const VOP_Node &vopNode, const UT_String &socketName)
{
	const int socketIndex = vopNode.getInputFromName(socketName);
	if (socketIndex < 0)
		return nullptr;
	return const_cast<VOP_Node&>(vopNode).findSimpleInput(socketIndex);
}

VOP_Node *ShaderExporter::getConnectedNode(const VOP_Node &vopNode, int socketIndex)
{
	return const_cast<VOP_Node&>(vopNode).findSimpleInput(socketIndex);
}

int ShaderExporter::isSocketLinked(const VOP_Node &vopNode, const UT_String &socketName)
{
	return getConnectedNode(vopNode, socketName) != nullptr;
}

int ShaderExporter::isSocketLinked(const VOP_Node &vopNode, const QString &socketName)
{
	return getConnectedNode(vopNode, socketName) != nullptr;
}

int ShaderExporter::isSocketLinked(const VOP_Node &vopNode, int socketIndex)
{
	return getConnectedNode(vopNode, socketIndex) != nullptr;
}

int ShaderExporter::hasPluginOutput(const VRay::Plugin &plugin, const QString &outputName)
{
	const Parm::VRayPluginInfo &vrayPlugInfo = *Parm::getVRayPluginInfo(plugin.getType());

	for (const Parm::SocketDesc &sock : vrayPlugInfo.outputs)
		if (sock.attrName == outputName)
			return true;

	return false;
}

VRay::PluginRef ShaderExporter::exportSubnet(const VOP_Node &subnet, const SocketInfo &fromSocket)
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

	VOP_Node *conNode = getConnectedNode(*subnetOutput, subnetOutputInputIndex);
	if (!conNode)
		return VRay::PluginRef();

	SocketInfo subnetOutputInfo;
	subnetOutputInfo.vopNode = subnetOutput;
	subnetOutputInfo.index = subnetOutputInputIndex;
	subnetOutputInfo.name = subnetOutputName;

	return exportConnectedSocket(*conNode, subnetOutputInfo);
}

VRay::PluginRef ShaderExporter::exportSubnetInput(const VOP_Node &subnetInput, const SocketInfo &fromSocket)
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

	UT_String subnetInputName;
	if (subinputOutputName.startsWith(VOP_SUBNET_VARIABLE_PREFIX)) {
		subinputOutputName.substr(subnetInputName, vopSubnetSuffixLength);
	}

	// Find correspondent input at the "subnet" node.
	const int subnetInputIndex = subnet->getInputFromName(subnetInputName);
	if (subnetInputIndex < 0) {
		Log::getLog().error("\"%s\": \"%s\" can't find correspondent input for \"%s\"!",
		                    subnet->getFullPath().nonNullBuffer(),
		                    subnetInput.getName().nonNullBuffer(),
		                    subinputOutputName.nonNullBuffer());
		return VRay::PluginRef();
	}

	VOP_Node *conNode = getConnectedNode(*subnet, subnetInputIndex);
	if (!conNode)
		return VRay::PluginRef();

	SocketInfo subnetInputInfo;
	subnetInputInfo.vopNode = subnet;
	subnetInputInfo.index = subnetInputIndex;
	subnetInputInfo.name = subinputOutputName;

	return exportConnectedSocket(*conNode, subnetInputInfo);
}

void ShaderExporter::exportConnectedSockets(const VOP_Node &vopNode, Attrs::PluginDesc &pluginDesc)
{
	const OP::VRayNode &vrayNode = static_cast<const OP::VRayNode&>(static_cast<const VOP::NodeBase&>(vopNode));

	const Parm::VRayPluginInfo &pluginInfo = *Parm::getVRayPluginInfo(pluginDesc.pluginID);

	for (int i = 0; i < pluginInfo.inputs.count(); ++i) {
		const Parm::SocketDesc &curSockInfo = pluginInfo.inputs[i];
		const QString &attrName = curSockInfo.attrName;

		if (pluginDesc.contains(attrName) || !pluginInfo.hasAttribute(attrName)) {
			continue;
		}

		const Parm::AttrDesc &attrDesc = pluginInfo.getAttribute(attrName);
		if (attrDesc.flags & Parm::attrFlagCustomHandling) {
			continue;
		}

		VRay::PluginRef conPlugin = exportConnectedSocket(vopNode, curSockInfo.socketLabel);
		if (conPlugin.isEmpty()) {
			if (needDefaultUvwGen(vrayNode, attrDesc, attrName)) {
				if (!isConnectedToTexTriplanar(vopNode)) {
					Attrs::PluginDesc defaultUVWGen(VRayExporter::getPluginName(vopNode, SL("DefaultUVWGen")),
					                                SL("UVWGenProjection"));
					defaultUVWGen.add(SL("type"), 6);
					defaultUVWGen.add(SL("object_space"), true);

					conPlugin = pluginExporter.exportPlugin(defaultUVWGen);
				}
			}
			else {
				VOP_Node *conNode = getConnectedNode(vopNode, attrName);
				if (conNode) {
					if (isOpType(*conNode, "makexform")) {
						switch (curSockInfo.attrType) {
							case Parm::eMatrix: {
								const bool flipAxis = pluginDesc.pluginID == SL("UVWGenPlanar") ||
								                      pluginDesc.pluginID == SL("UVWGenProjection") ||
								                      pluginDesc.pluginID == SL("UVWGenObject") ||
								                      pluginDesc.pluginID == SL("UVWGenEnvironment");

								const VRay::Transform &transform = getTransformFromXform(*conNode, flipAxis);
								pluginDesc.add(attrName, transform.matrix);
								break;
							}
							case Parm::eTransform: {
								pluginDesc.add(attrName, getTransformFromXform(*conNode));
								break;
							}
							default:
								break;
						}
					}
				}
			}
		}

		if (conPlugin.isNotEmpty()) {
			pluginDesc.add(attrName, conPlugin);
		}
	}
}

VRay::Transform ShaderExporter::getTransformFromXform(const VOP_Node &transform, bool flipAxis) const
{
	const fpreal t = pluginExporter.getContext().getTime();

	const int trs = transform.evalInt("trs", 0, t);
	const int xyz = transform.evalInt("xyz", 0, t);

	fpreal trans[3];
	getParmFloat3(transform, "trans", trans, t);

	fpreal rot[3];
	getParmFloat3(transform, "rot", rot, t);

	fpreal scale[3];
	getParmFloat3(transform, "scale", scale, t);

	fpreal pivot[3];
	getParmFloat3(transform, "pivot", pivot, t);

	UT_DMatrix4 m4;
	OP_Node::buildXform(trs, xyz,
	                    trans[0], trans[1], trans[2],
	                    rot[0],   rot[1],   rot[2],
	                    scale[0], scale[1], scale[2],
	                    pivot[0], pivot[1], pivot[2],
	                    m4);
	if (flipAxis) {
		m4 = m4 * yAxisUpRotationMatrix;
	}

	return VRayExporter::Matrix4ToTransform(m4);
}

VRay::PluginRef ShaderExporter::exportVRayNode(const VOP_Node &vopNode, const SocketInfo &fromSocket)
{
	VRay::PluginRef plugin;

	const VOP::NodeBase &vrayNode = static_cast<const VOP::NodeBase&>(vopNode);

	if (!cacheMan.getShaderPlugin(vopNode, plugin)) {
		Attrs::PluginDesc pluginDesc(VRayExporter::getPluginName(vopNode),
		                             vrayNode.getVRayPluginID());

		if (isOpType(vopNode, "VRayNodeTexSky")) {
			pluginExporter.fillNodeTexSky(vopNode, pluginDesc);
		}

		const OP::VRayNode::PluginResult res =
			const_cast<VOP::NodeBase&>(vrayNode).asPluginDesc(pluginDesc, pluginExporter);

		if (res == OP::VRayNode::PluginResultError) {
			Log::getLog().error("Error creating plugin descripion for node \"%s\"!",
			                    vopNode.getFullPath().nonNullBuffer());
		}
		else if (res == OP::VRayNode::PluginResultNA ||
		         res == OP::VRayNode::PluginResultContinue)
		{
			exportConnectedSockets(vopNode, pluginDesc);

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
			cacheMan.addShaderPlugin(vopNode, plugin);
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

VRay::PluginRef ShaderExporter::exportNull(const VOP_Node &null, const SocketInfo &fromSocket)
{
	VOP_Node *conNode = getConnectedNode(null, 0);
	if (!conNode)
		return VRay::PluginRef();
	return exportConnectedSocket(*conNode, fromSocket);
}

VRay::PluginRef ShaderExporter::exportSwitcher(const VOP_Node &switcher, const SocketInfo &fromSocket)
{
	const fpreal t = pluginExporter.getContext().getTime();

	const int switcherIndex = switcher.evalInt("switcher", 0, t);

	VOP_Node *conNode = getConnectedNode(switcher, switcherIndex);
	if (!conNode)
		return VRay::PluginRef();

	return exportConnectedSocket(*conNode, fromSocket);
}

VRay::PluginRef ShaderExporter::exportConnectedSocket(const VOP_Node &vopNode, const SocketInfo &fromSocket)
{
	VRay::PluginRef plugin;

	const UT_String &opType = vopNode.getOperator()->getName();

	Log::getLog().debug("Processing node \"%s\" [%s]...",
	                    vopNode.getFullPath().nonNullBuffer(),
	                    opType.buffer());

	if (isOpType(vopNode, "subnet")) {
		if (!fromSocket.isValid()) {
			Log::getLog().error("\"%s\": \"subnet\" node could not be directly selected!",
			                    vopNode.getFullPath().nonNullBuffer());
		}
		else {
			plugin = exportSubnet(vopNode, fromSocket);
		}
	}
	else if (isOpType(vopNode, VOP_SUBNET_INPUT_NODE_NAME)) {
		if (!fromSocket.isValid()) {
			Log::getLog().error("\"%s\": \"" VOP_SUBNET_INPUT_NODE_NAME "\" node could not be directly selected!",
			                    vopNode.getFullPath().nonNullBuffer());
		}
		else {
			plugin = exportSubnetInput(vopNode, fromSocket);
		}
	}
	else if (isOpType(vopNode, "switch")) {
		if (fromSocket.isValid()) {
			vassert(false && "VOP_Node::findSimpleInput() should have pass through this!");
		}
		else {
			plugin = exportSwitcher(vopNode, fromSocket);
		}
	}
	else if (isOpType(vopNode, "null")) {
		if (fromSocket.isValid()) {
			vassert(false && "VOP_Node::findSimpleInput() should have pass through this!");
		}
		else {
			plugin = exportNull(vopNode, fromSocket);
		}
	}
	else if (opType.startsWith("principledshader")) {
		plugin = pluginExporter.exportPrincipledShader(vopNode);
	}
	else if (isOpType(vopNode, vfhNodeMaterialOutput)) {
		plugin = exportVRayMaterialOutput(vopNode);
	}
	else if (opType.startsWith("VRayNode")) {
		plugin = exportVRayNode(vopNode, fromSocket);
	}
	else if (isOpType(vopNode, "parameter")) {
		// This is handled by VRayExporter::setAttrsFromNetworkParameters().
		return VRay::PluginRef();
	}
	else {
		Log::getLog().warning("Unsupported node \"%s\" [%s]",
		                      vopNode.getFullPath().buffer(), opType.buffer());
		return VRay::PluginRef();
	}

	if (plugin.isEmpty()) {
		Log::getLog().error("Error exporting \"%s\"!",
		                    vopNode.getFullPath().buffer());
	}
	else {
		// Auto-convert socket types.
		// We could auto-convert types only if we knew where the connection if coming from.
		if (fromSocket.isValid()) {
			const int conOutputIndex = getConnectedOutputIndex(fromSocket.index, *fromSocket.vopNode);

			const VOP_Type outputType = getVopOutputSocketType(vopNode, conOutputIndex);
			const VOP_Type inputType  = getVopInputSocketType(*fromSocket.vopNode, fromSocket.index);

			if (outputType != inputType) {
				// If connected plugin type is BRDF, but we expect a Material, wrap it into "MtlSingleBRDF".
				if (outputType == VOP_TYPE_BSDF && inputType == VOP_SURFACE_SHADER) {
					Attrs::PluginDesc brdfToMtl(SL("%1|%2").arg(plugin.getName()).arg(fromSocket.name.buffer()),
					                            SL("MtlSingleBRDF"));
					brdfToMtl.add(SL("brdf"), plugin);

					plugin = pluginExporter.exportPlugin(brdfToMtl);
				}
				else {
					// Check if we need to auto-convert color / float.
					QString floatColorConverterType;

					if (outputType == VOP_TYPE_FLOAT && inputType == VOP_TYPE_COLOR) {
						if (hasPluginOutput(plugin, SL("color"))) {
							plugin = VRay::PluginRef(plugin, "color");
						}
						else {
							floatColorConverterType = SL("TexFloatToColor");
						}
					}
					else if (outputType == VOP_TYPE_COLOR && inputType == VOP_TYPE_FLOAT) {
						if (hasPluginOutput(plugin, SL("out_intensity"))) {
							plugin = VRay::PluginRef(plugin, "out_intensity");
						}
						else {
							floatColorConverterType = SL("TexColorToFloat");
						}
					}

					if (!floatColorConverterType.isEmpty()) {
						Attrs::PluginDesc floatColorConv(SL("%1|ValueConverter|%2").arg(plugin.getName()).arg(fromSocket.name.buffer()),
						                                 floatColorConverterType);
						floatColorConv.add(SL("input"), plugin);

						plugin = pluginExporter.exportPlugin(floatColorConv);
					}
				}
			}
		}

		// Set "scene_name" for Cryptomatte.
		if (hasMaterialSceneName(plugin)) {
			plugin.setValue("scene_name", VRayExporter::getSceneName(vopNode));
		}

		// Attach IPR callback.
		pluginExporter.addOpCallback(const_cast<VOP_Node*>(&vopNode), VRayExporter::RtCallbackVop);
	}

	return plugin;
}

VRay::PluginRef ShaderExporter::exportConnectedSocket(const VOP_Node &vopNode, const UT_String &socketName)
{
	const int socketIndex = vopNode.getInputFromName(socketName.buffer());
	if (socketIndex < 0)
		return VRay::PluginRef();

	VOP_Node *conNode = getConnectedNode(vopNode, socketIndex);
	if (!conNode)
		return VRay::PluginRef();

	SocketInfo fromSocket;
	fromSocket.vopNode = &vopNode;
	fromSocket.name = socketName;
	fromSocket.index = socketIndex;

	return exportConnectedSocket(*conNode, fromSocket);
}

VRay::PluginRef ShaderExporter::exportConnectedSocket(const VOP_Node &vopNode, const QString &socketName)
{
	const UT_String utSocketName(qPrintable(socketName), true);
	return exportConnectedSocket(vopNode, utSocketName);
}

VRay::PluginRef ShaderExporter::exportConnectedSocket(const VOP_Node &vopNode, int socketIndex)
{
	if (socketIndex < 0)
		return VRay::PluginRef();

	VOP_Node *conNode = getConnectedNode(vopNode, socketIndex);
	if (!conNode)
		return VRay::PluginRef();

	SocketInfo fromSocket;
	vopNode.getInputName(fromSocket.name, socketIndex);
	fromSocket.index = socketIndex;

	return exportConnectedSocket(*conNode, fromSocket);
}

VRay::PluginRef ShaderExporter::exportVRayMaterialBuilder(const SHOP_Node &shopNode)
{
	OP_Node *opNode = getOpNodeByType(shopNode, vfhNodeMaterialOutput);
	if (opNode) {
		VOP_Node *vrayMaterialOutput = CAST_VOPNODE(opNode);
		vassert(vrayMaterialOutput);

		return exportVRayMaterialOutput(*vrayMaterialOutput);
	}

	return VRay::PluginRef();
}

VRay::PluginRef ShaderExporter::exportVRayMaterialOutput(const VOP_Node &materailOutput)
{
	VOP_Node *conNode = getConnectedNode(materailOutput, vfhSocketMaterialOutputMaterialIndex);
	if (!conNode) {
		Log::getLog().error("\"%s\": Empty \"Material\" input!",
		                    materailOutput.getFullPath().nonNullBuffer());
		return VRay::PluginRef();
	}

	SocketInfo materialSocket;
	materialSocket.vopNode = &materailOutput;
	materialSocket.index = vfhSocketMaterialOutputMaterialIndex;
	materialSocket.name = vfhSocketMaterialOutputMaterial;

	return exportConnectedSocket(*conNode, materialSocket);
}

VRay::PluginRef ShaderExporter::exportShaderNode(const OP_Node &opNode)
{
	if (isOpType(opNode, vfhNodeMaterialBuilder)) {
		const SHOP_Node *shopNode = CAST_SHOPNODE(&opNode);
		vassert(shopNode);

		return exportVRayMaterialBuilder(*shopNode);
	}

	const VOP_Node *vopNode = CAST_VOPNODE(&opNode);
	if (vopNode) {
		const SocketInfo dummyInfo;
		return exportConnectedSocket(*vopNode, dummyInfo);
	}

	return VRay::PluginRef();
}
