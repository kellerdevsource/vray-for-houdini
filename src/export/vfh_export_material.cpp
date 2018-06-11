//
// Copyright (c) 2015-2018, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#include "vfh_exporter.h"
#include "vfh_op_utils.h"
#include "vfh_attr_utils.h"

#include "vop/material/vop_mtl_def.h"

#include <SHOP/SHOP_Node.h>
#include <VOP/VOP_ParmGenerator.h>
#include <OP/OP_Options.h>

using namespace VRayForHoudini;

static const QString clayMaterial("Mtl@Clay");

void VRayExporter::RtCallbackSurfaceShop(OP_Node *caller, void *callee, OP_EventType type, void *data)
{
	VRayExporter &exporter = *reinterpret_cast<VRayExporter*>(callee);
	if (exporter.inSceneExport)
		return;

	if (!csect.tryEnter())
		return;

	Log::getLog().debug("RtCallbackSurfaceShop: %s from \"%s\"",
					   OPeventToString(type), caller->getName().buffer());

	if (type == OP_INPUT_REWIRED) {
		ShaderExporter &shaderExporter = exporter.getShaderExporter();
		shaderExporter.reset();

		exporter.exportMaterial(caller);
	}
	else if (type == OP_NODE_PREDELETE) {
		exporter.delOpCallback(caller, RtCallbackSurfaceShop);
	}

	csect.leave();
}

VRay::Plugin VRayExporter::exportMaterial(OP_Node *matNode)
{
	VRay::PluginRef material;

	if (!matNode) {
		material = exportDefaultMaterial();
	}
	else if (!cacheMan.getShaderPlugin(*matNode, material)) {
		material = exportShaderNode(matNode);

		if (material.isEmpty()) {
			material = exportDefaultMaterial();
		}
		else {
			if (VOP_Node *vopNode = CAST_VOPNODE(matNode)) {
				const VOP_Type vopType = vopNode->getShaderType();
				if (vopType == VOP_TYPE_BSDF) {
					// Wrap BRDF into MtlSingleBRDF for RT GPU to work properly.
					Attrs::PluginDesc mtlPluginDesc(getPluginName(*vopNode, SL("MtlSingle")),
					                                SL("MtlSingleBRDF"));
					mtlPluginDesc.add(Attrs::PluginAttr(SL("brdf"), material));
					mtlPluginDesc.add(Attrs::PluginAttr(SL("scene_name"), getSceneName(*vopNode)));
					material = exportPlugin(mtlPluginDesc);
				}
			}

			if (isInteractive()) {
				// Wrap material into MtlRenderStats to always have the same material name.
				// Used when rewiring materials when running interactive RT session.
				Attrs::PluginDesc pluginDesc(getPluginName(*matNode, SL("MtlStats")),
				                             SL("MtlRenderStats"));
				pluginDesc.add(Attrs::PluginAttr(SL("base_mtl"), material));
				material = exportPlugin(pluginDesc);
			}

			addOpCallback(matNode, RtCallbackSurfaceShop);
		}

		vassert(material.isNotEmpty());

		cacheMan.addShaderPlugin(*matNode, material);
	}

	return VRay::Plugin(material);
}


VRay::Plugin VRayExporter::exportDefaultMaterial()
{
	VRay::Plugin material;

	if (!objectExporter.getPluginFromCache(clayMaterial, material)) {
		Attrs::PluginDesc brdfDesc(SL("BRDFDiffuse@Clay"), SL("BRDFDiffuse"));
		brdfDesc.add(Attrs::PluginAttr(SL("color"), 0.5f, 0.5f, 0.5f));

		Attrs::PluginDesc mtlDesc(clayMaterial, SL("MtlSingleBRDF"));
		mtlDesc.add(Attrs::PluginAttr(SL("brdf"), exportPlugin(brdfDesc)));
		mtlDesc.add(Attrs::PluginAttr(SL("scene_name"), getSceneName("DEFAULT_MATERIAL")));
		material = exportPlugin(mtlDesc);

		objectExporter.addPluginToCache(clayMaterial, material);
	}

	return material;
}


void VRayExporter::setAttrsFromNetworkParameters(Attrs::PluginDesc &pluginDesc, const VOP_Node &vopNode)
{
	OP_Network *creator = vopNode.getCreator();
	if (!creator)
		return;

	const Parm::VRayPluginInfo *pluginInfo = Parm::getVRayPluginInfo(pluginDesc.pluginID);
	if (!pluginInfo)
		return;

	const fpreal t = m_context.getTime();

	VOP_ParmGeneratorList prmVOPs;
	const_cast<VOP_Node&>(vopNode).getParmInputs(prmVOPs);

	for (VOP_ParmGenerator *prmVOP : prmVOPs) {
		const int inpidx = vopNode.whichInputIs(prmVOP);
		if (inpidx < 0)
			continue;

		UT_String inpName;
		vopNode.getInputName(inpName, inpidx);

		const QString attrName(inpName.buffer());

		// Plugin doesn't have such attribute or it has already been exported.
		if (!pluginInfo->hasAttribute(attrName) || pluginDesc.contains(attrName))
			continue;

		const UT_String &prmToken = prmVOP->getParmNameCache();

		const PRM_Parm *prm = creator->getParmList()->getParmPtr(prmToken.buffer());
		if (!prm)
			continue;

		const int isAnimated = prm->isTimeDependent();

		const PRM_Type prmType = prm->getType();

		if (prmType.isStringType()) {
			UT_String path;
			creator->evalString(path, prm, 0, t);

			const VRay::Plugin opPlugin = exportNodeFromPath(path);
			if (opPlugin.isNotEmpty()) {
				pluginDesc.add(attrName, opPlugin);
			}
		}

		if (!prmType.isFloatType())
			continue;

		const Parm::AttrDesc &attrDesc = pluginInfo->getAttribute(attrName);
		switch (attrDesc.value.type) {
			case Parm::eBool:
			case Parm::eEnum:
			case Parm::eInt:
			case Parm::eTextureInt: {
				Attrs::PluginDesc texUserDesc(getPluginName(vopNode, attrName),
				                                  SL("TexUserInteger"));
				texUserDesc.add(SL("default_value"), creator->evalInt(prm, 0, t), isAnimated);
				texUserDesc.add(SL("user_attribute"), prm->getToken());

				const VRay::Plugin texUser = exportPlugin(texUserDesc);
				texUserDesc.add(attrName, texUser);

				break;
			}
			case Parm::eFloat:
			case Parm::eTextureFloat: {
				Attrs::PluginDesc texUserDesc(getPluginName(vopNode, attrName),
				                                  SL("TexUserScalar"));
				texUserDesc.add(SL("default_value"), creator->evalFloat(prm, 0, t), isAnimated);
				texUserDesc.add(SL("user_attribute"), prm->getToken());

				const VRay::Plugin texUser = exportPlugin(texUserDesc);
				texUserDesc.add(attrName, texUser);

				break;
			}
			case Parm::eColor:
			case Parm::eAColor:
			case Parm::eTextureColor: {
				Attrs::PluginDesc texUserDesc(getPluginName(vopNode, attrName),
				                                  SL("TexUserColor"));

				Attrs::PluginAttr defaultColor(SL("default_color"),
				                               Attrs::AttrTypeAColor);
				const int colorSize = prm->getVectorSize();
				for (int i = 0; i < std::min(colorSize, 4); ++i) {
					defaultColor.paramValue.valVector[i] = creator->evalFloat(prm, i, t);
				}
				if (colorSize == 3) {
					defaultColor.paramValue.valVector[3] = 1.0f;
				}
				defaultColor.setAnimated(isAnimated);

				texUserDesc.add(defaultColor);

				// Set priority to user attribute.
				texUserDesc.add(SL("attribute_priority"), 1);
				texUserDesc.add(SL("user_attribute"), prm->getToken());

				const VRay::Plugin texUser = exportPlugin(texUserDesc);
				pluginDesc.add(attrName, texUser);

				break;
			}
			case Parm::eVector: {
				VRay::Vector v;
				for (int i = 0; i < std::min(3, prm->getVectorSize()); ++i) {
					v[i] = creator->evalFloat(prm->getToken(), i, t);
				}

				pluginDesc.add(attrName, v, isAnimated);
				break;
			}
			case Parm::eMatrix: {
				VRay::Matrix m(1);

				for (int k = 0; k < std::min(9, prm->getVectorSize()); ++k) {
					const int i = k / 3;
					const int j = k % 3;
					m[i][j] = creator->evalFloat(prm->getToken(), k, t);
				}

				pluginDesc.add(attrName, m, isAnimated);
				break;
			}
			case Parm::eTransform: {
				VRay::Transform tm(1);

				for (int k = 0; k < std::min(16, prm->getVectorSize()); ++k) {
					const int i = k / 4;
					const int j = k % 4;
					if (i < 3) {
						if (j < 3) {
							tm.matrix[i][j] = creator->evalFloat(prm->getToken(), k, t);
						}
					}
					else {
						if (j < 3) {
							tm.offset[j] = creator->evalFloat(prm->getToken(), k, t);
						}
					}
				}

				pluginDesc.add(attrName, tm, isAnimated);
				break;
			}
			default:
				break;
		}
	}
}
