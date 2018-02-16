//
// Copyright (c) 2015-2017, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#include "vop_meta_image_file.h"
#include "vfh_prm_templates.h"
#include "vfh_tex_utils.h"
#include "vfh_attr_utils.h"

#include <COP2/COP2_Node.h>

#include <map>
#include <vector>

using namespace VRayForHoudini;
using namespace VOP;

typedef std::map<MetaImageFile::UVWGenType, MetaImageFile::UVWGenSocketsTable> UVWGenSocketsMap;

/// A map of sockets per UVG generator type.
static UVWGenSocketsMap uvwGenInputsMap;

// NOTE: Keep in sync with MetaImageFile::UVWGenType.
static const UT_String uvwGenPluginIDs[] = {
	"UVWGenMayaPlace2dTexture",
	"UVWGenEnvironment",
	"UVWGenExplicit",
	"UVWGenChannel",
	"UVWGenObject",
	"UVWGenObjectBBox",
	"UVWGenPlanarWorld",
	"UVWGenProjection",
};

static void initInputsMap()
{
	if (!uvwGenInputsMap.empty())
		return;

	MetaImageFile::UVWGenSocketsTable &mayeSockets = uvwGenInputsMap[MetaImageFile::UVWGenMayaPlace2dTexture];
	mayeSockets.push_back(MetaImageFile::UVWGenSocket("uvwgen", VOP_TypeInfo(VOP_TYPE_VECTOR)));
	mayeSockets.push_back(MetaImageFile::UVWGenSocket("coverage_u_tex", VOP_TypeInfo(VOP_TYPE_FLOAT)));
	mayeSockets.push_back(MetaImageFile::UVWGenSocket("coverage_v_tex", VOP_TypeInfo(VOP_TYPE_FLOAT)));
	mayeSockets.push_back(MetaImageFile::UVWGenSocket("translate_frame_u_tex", VOP_TypeInfo(VOP_TYPE_FLOAT)));
	mayeSockets.push_back(MetaImageFile::UVWGenSocket("translate_frame_v_tex", VOP_TypeInfo(VOP_TYPE_FLOAT)));
	mayeSockets.push_back(MetaImageFile::UVWGenSocket("rotate_frame_tex", VOP_TypeInfo(VOP_TYPE_FLOAT)));
	mayeSockets.push_back(MetaImageFile::UVWGenSocket("repeat_u_tex", VOP_TypeInfo(VOP_TYPE_FLOAT)));
	mayeSockets.push_back(MetaImageFile::UVWGenSocket("repeat_v_tex", VOP_TypeInfo(VOP_TYPE_FLOAT)));
	mayeSockets.push_back(MetaImageFile::UVWGenSocket("offset_u_tex", VOP_TypeInfo(VOP_TYPE_FLOAT)));
	mayeSockets.push_back(MetaImageFile::UVWGenSocket("offset_v_tex", VOP_TypeInfo(VOP_TYPE_FLOAT)));
	mayeSockets.push_back(MetaImageFile::UVWGenSocket("rotate_uv_tex", VOP_TypeInfo(VOP_TYPE_FLOAT)));
	mayeSockets.push_back(MetaImageFile::UVWGenSocket("noise_u_tex", VOP_TypeInfo(VOP_TYPE_FLOAT)));
	mayeSockets.push_back(MetaImageFile::UVWGenSocket("noise_v_tex", VOP_TypeInfo(VOP_TYPE_FLOAT)));
	mayeSockets.push_back(MetaImageFile::UVWGenSocket("uvw_channel_tex", VOP_TypeInfo(VOP_TYPE_INTEGER)));

	MetaImageFile::UVWGenSocketsTable &environmentSockets = uvwGenInputsMap[MetaImageFile::UVWGenEnvironment];
	environmentSockets.push_back(MetaImageFile::UVWGenSocket("uvw_matrix", VOP_TypeInfo(VOP_TYPE_MATRIX3)));
	environmentSockets.push_back(MetaImageFile::UVWGenSocket("uvw_transform", VOP_TypeInfo(VOP_TYPE_MATRIX4)));
	environmentSockets.push_back(MetaImageFile::UVWGenSocket("ground_position", VOP_TypeInfo(VOP_TYPE_VECTOR)));

	MetaImageFile::UVWGenSocketsTable &explicitSockets = uvwGenInputsMap[MetaImageFile::UVWGenExplicit];
	explicitSockets.push_back(MetaImageFile::UVWGenSocket("u", VOP_TypeInfo(VOP_TYPE_FLOAT)));
	explicitSockets.push_back(MetaImageFile::UVWGenSocket("v", VOP_TypeInfo(VOP_TYPE_FLOAT)));
	explicitSockets.push_back(MetaImageFile::UVWGenSocket("w", VOP_TypeInfo(VOP_TYPE_FLOAT)));
	explicitSockets.push_back(MetaImageFile::UVWGenSocket("uvw", VOP_TypeInfo(VOP_TYPE_COLOR)));

	MetaImageFile::UVWGenSocketsTable &channelSockets = uvwGenInputsMap[MetaImageFile::UVWGenChannel];
	channelSockets.push_back(MetaImageFile::UVWGenSocket("uvw_transform", VOP_TypeInfo(VOP_TYPE_MATRIX4)));
	channelSockets.push_back(MetaImageFile::UVWGenSocket("uvw_transform tex", VOP_TypeInfo()));
	channelSockets.push_back(MetaImageFile::UVWGenSocket("tex_transfrom", VOP_TypeInfo(VOP_TYPE_MATRIX4)));
	channelSockets.push_back(MetaImageFile::UVWGenSocket("coverage", VOP_TypeInfo(VOP_TYPE_VECTOR)));
	channelSockets.push_back(MetaImageFile::UVWGenSocket("uvwgen", VOP_TypeInfo(VOP_TYPE_VECTOR)));

	MetaImageFile::UVWGenSocketsTable &objectSockets= uvwGenInputsMap[MetaImageFile::UVWGenObject];
	objectSockets.push_back(MetaImageFile::UVWGenSocket("uvw_transform", VOP_TypeInfo(VOP_TYPE_MATRIX4)));

	MetaImageFile::UVWGenSocketsTable &bboxSockets = uvwGenInputsMap[MetaImageFile::UVWGenObjectBBox];
	bboxSockets.push_back(MetaImageFile::UVWGenSocket("bbox_min", VOP_TypeInfo(VOP_TYPE_VECTOR)));
	bboxSockets.push_back(MetaImageFile::UVWGenSocket("bbox_max", VOP_TypeInfo(VOP_TYPE_VECTOR)));
	bboxSockets.push_back(MetaImageFile::UVWGenSocket("basemtl", VOP_TypeInfo(VOP_TYPE_UNDEF)));

	MetaImageFile::UVWGenSocketsTable &worldSockets = uvwGenInputsMap[MetaImageFile::UVWGenPlanarWorld];
	worldSockets.push_back(MetaImageFile::UVWGenSocket("uvw_transform", VOP_TypeInfo(VOP_TYPE_MATRIX4)));
	worldSockets.push_back(MetaImageFile::UVWGenSocket("uvw_transform tex", VOP_TypeInfo()));
	worldSockets.push_back(MetaImageFile::UVWGenSocket("tex_transfrom", VOP_TypeInfo(VOP_TYPE_MATRIX4)));
	worldSockets.push_back(MetaImageFile::UVWGenSocket("coverage", VOP_TypeInfo(VOP_TYPE_VECTOR)));

	MetaImageFile::UVWGenSocketsTable &projectionSockets = uvwGenInputsMap[MetaImageFile::UVWGenProjection];
	projectionSockets.push_back(MetaImageFile::UVWGenSocket("uvw_transform", VOP_TypeInfo(VOP_TYPE_MATRIX4)));
	projectionSockets.push_back(MetaImageFile::UVWGenSocket("uvw_transform tex", VOP_TypeInfo(VOP_TYPE_UNDEF)));
	projectionSockets.push_back(MetaImageFile::UVWGenSocket("tex_transfrom", VOP_TypeInfo(VOP_TYPE_MATRIX4)));
	projectionSockets.push_back(MetaImageFile::UVWGenSocket("camera_settings", VOP_TypeInfo(VOP_TYPE_UNDEF)));
	projectionSockets.push_back(MetaImageFile::UVWGenSocket("camera_view", VOP_TypeInfo(VOP_TYPE_UNDEF)));
	projectionSockets.push_back(MetaImageFile::UVWGenSocket("bitmap", VOP_TypeInfo(VOP_TYPE_UNDEF)));
}

PRM_Template* MetaImageFile::GetPrmTemplate()
{
	initInputsMap();

	static Parm::PRMList myPrmList;
	if (myPrmList.empty()) {
		myPrmList.addFromFile("MetaImageFile");
	}

	return myPrmList.getPRMTemplate();
}

void MetaImageFile::setPluginType()
{
	pluginType = VRayPluginType::TEXTURE;

	// Base plugin
	pluginID = "TexBitmap";
}

OP::VRayNode::PluginResult MetaImageFile::asPluginDesc(Attrs::PluginDesc &pluginDesc, VRayExporter &exporter, ExportContext *parentContext)
{
	const fpreal t = exporter.getContext().getTime();

	const UVWGenSocketsTable &selectedUVWGen = getUVWGenInputs();

	const UVWGenType current = getUVWGenType();
	const UT_String &selectedUVWGenName = uvwGenPluginIDs[current];

	Attrs::PluginDesc selectedUVPluginDesc(VRayExporter::getPluginName(*this, selectedUVWGenName.c_str()), selectedUVWGenName.c_str());

	for (const UVWGenSocket &it : selectedUVWGen) {
		const std::string &inputName = it.label;

		const int idx = getInputFromName(inputName.c_str());

		OP_Node *connectedInput = getInput(idx);
		if (connectedInput) {
			const VRay::Plugin connectedPlugin = exporter.exportVop(connectedInput, parentContext);
			if (connectedPlugin) {
				const Parm::SocketDesc *fromSocketInfo = VRayExporter::getConnectedOutputType(this, inputName);
				selectedUVPluginDesc.addAttribute(Attrs::PluginAttr(inputName, connectedPlugin, fromSocketInfo->attrName.ptr()));
			}
		}
	}

	exporter.setAttrsFromOpNodePrms(selectedUVPluginDesc, this, boost::str(Parm::FmtPrefix % selectedUVWGenName.buffer()));

	Attrs::PluginDesc bitmapBufferDesc;
	bitmapBufferDesc.pluginName = VRayExporter::getPluginName(*this, "BitmapBuffer");
	bitmapBufferDesc.pluginID = "BitmapBuffer";

	UT_String path;
	evalString(path, "BitmapBuffer_file", 0, t);
	if (path.startsWith(OPREF_PREFIX)) {
		OP_Node *opNode = getOpNodeFromPath(path, t);
		if (opNode) {
			COP2_Node *copNode = opNode->castToCOP2Node();
			if (copNode) {
				bitmapBufferDesc.pluginID = "RawBitmapBuffer";
				bitmapBufferDesc.addAttribute(Attrs::PluginAttr("file", Attrs::PluginAttr::AttrTypeIgnore));

				if (!exporter.fillCopNodeBitmapBuffer(*copNode, bitmapBufferDesc)) {
					Log::getLog().error("Failed to bake texture data from \"%s\"", copNode->getName().buffer());
				}
			}
		}
	}

	exporter.setAttrsFromOpNodePrms(bitmapBufferDesc, this, "BitmapBuffer_");

	pluginDesc.addAttribute(Attrs::PluginAttr("bitmap", exporter.exportPlugin(bitmapBufferDesc)));
	pluginDesc.addAttribute(Attrs::PluginAttr("uvwgen", exporter.exportPlugin(selectedUVPluginDesc)));
	exporter.setAttrsFromOpNodePrms(pluginDesc, this, "TexBitmap_");

	return OP::VRayNode::PluginResultContinue;
}

const char *MetaImageFile::inputLabel(unsigned idx) const
{
	const UVWGenSocketsTable &uvwGenInput = getUVWGenInputs();
	vassert(idx >= 0 && idx < uvwGenInput.size());
	return uvwGenInput[idx].label;
}

void MetaImageFile::getInputNameSubclass(UT_String &in, int idx) const
{
	const UVWGenSocketsTable &uvwGenInput = getUVWGenInputs();
	if (idx >= 0 && idx < uvwGenInput.size()) {
		in = uvwGenInput[idx].label;
	}
	else {
		in = "unknown";
	}
}

int MetaImageFile::getInputFromNameSubclass(const UT_String &out) const
{
	const UVWGenSocketsTable &uvwGenInput = getUVWGenInputs();
	for (int i = 0; i < uvwGenInput.size(); i++) {
		if (out.equal(uvwGenInput[i].label))
			return i;
	}
	return -1;
}

unsigned MetaImageFile::getNumVisibleInputs() const
{
	return orderedInputs();
}

unsigned MetaImageFile::orderedInputs() const
{
	const UVWGenSocketsTable &uvwGenInput = getUVWGenInputs();
	return uvwGenInput.size();
}

void MetaImageFile::getInputTypeInfoSubclass(VOP_TypeInfo &type_info, int idx)
{
	const UVWGenSocketsTable &uvwGenInput = getUVWGenInputs();

	vassert(idx >= 0 && idx < uvwGenInput.size());

	type_info = uvwGenInput[idx].typeInfo;
}

MetaImageFile::UVWGenType MetaImageFile::getUVWGenType() const
{
	initInputsMap();
	return static_cast<UVWGenType>(evalInt("meta_image_uv_generator", 0, 0.0));
}

const MetaImageFile::UVWGenSocketsTable &MetaImageFile::getUVWGenInputs() const
{
	const UVWGenType currentUVWGen = getUVWGenType();

	const UVWGenSocketsMap::const_iterator it = uvwGenInputsMap.find(currentUVWGen);
	vassert(it != uvwGenInputsMap.end());

	return it->second;
}
