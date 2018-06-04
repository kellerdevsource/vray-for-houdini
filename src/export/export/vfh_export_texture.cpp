//
// Copyright (c) 2015-2018, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text:
//   https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#include "vfh_exporter.h"
#include "vfh_attr_utils.h"
#include "vfh_prm_templates.h"
#include "op_node_base.h"

using namespace VRayForHoudini;
using namespace Attrs;

void VRayExporter::fillNodeTexSky(const OP_Node &opNode, Attrs::PluginDesc &pluginDesc)
{
	OP_Node *sunOp = nullptr;

	if (!opNode.evalInt("auto_sun", 0, 0.0)) {
		sunOp = getOpNodeFromAttr(opNode, "sun");
	}
	else {
		// Take the first SunLight node found in OBJ.
		OP_Node *objContext = OPgetDirector()->findNode("/obj");
		vassert(objContext);

		OP_NodeList objNodes;
		objContext->getAllChildren(objNodes);

		for (int i = 0; i < objNodes.size(); ++i) {
			OP_Node *objOpNode = objNodes(i);
			if (objOpNode) {
				OBJ_Node *objNode = objOpNode->castToOBJNode();
				if (objNode && objNode->castToOBJLight()) {
					OP::VRayNode *vrayLightNode = dynamic_cast<OP::VRayNode*>(objNode);
					if (vrayLightNode && vrayLightNode->getPluginID() == static_cast<int>(VRayPluginID::SunLight)) {
						sunOp = objNode;
						break;
					}
				}
			}
		}

		if (!sunOp) {
			Log::getLog().error("TexSky \"%s\": VRaySun is not found!",
			                    opNode.getFullPath().buffer());
		}
	}

	if (sunOp) {
		DelayedExportItem item;
		item.type = DelayedExportItem::ItemType::typeLightPlugin;
		item.opNode = sunOp;
		item.pluginName = pluginDesc.pluginName;
		item.pluginID = pluginDesc.pluginID;
		item.parmName = SL("sun");

		delayedExport.append(item);
	}
}
