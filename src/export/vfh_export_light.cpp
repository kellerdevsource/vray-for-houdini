//
// Copyright (c) 2015-2017, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#include "vfh_exporter.h"
#include "vfh_prm_templates.h"
#include "obj/obj_node_base.h"
#include "rop/vfh_rop.h"

#include <OP/OP_Bundle.h>


using namespace VRayForHoudini;


void VRayExporter::RtCallbackLight(OP_Node *caller, void *callee, OP_EventType type, void* data)
{
	if (!csect.tryEnter())
		return;

	VRayExporter &exporter = *reinterpret_cast<VRayExporter*>(callee);

	OBJ_Node *objNode = caller->castToOBJNode();

	Log::getLog().debug("RtCallbackLight: %s from \"%s\"",
			   OPeventToString(type), caller->getName().buffer());

	switch (type) {
		case OP_PARM_CHANGED: {
			if (Parm::isParmSwitcher(*caller, reinterpret_cast<intptr_t>(data))) {
				break;
			}
		}
		case OP_INPUT_CHANGED:
		case OP_INPUT_REWIRED: {
			exporter.exportObject(objNode);
			break;
		}
		case OP_NODE_PREDELETE: {
			exporter.removePlugin(objNode);
			exporter.delOpCallbacks(objNode);
			break;
		}
		default:
			break;
	}

	csect.leave();
}

VRay::Plugin VRayExporter::exportDefaultHeadlight(bool update)
{
	static const UT_StringRef theHeadlightNameToken = "DefaultHeadlight";

	int headlight = 0;
	m_rop->evalParameterOrProperty("soho_autoheadlight",
								   0,
								   getContext().getTime(),
								   headlight
								   );
	if (!headlight) {
		return VRay::Plugin();
	}

	// create direcional light from camera
	OBJ_Node *cam = getCamera(m_rop);
	if (!cam) {
		return VRay::Plugin();
	}

	if (update) {
		VRay::Plugin light = m_renderer.getPlugin(theHeadlightNameToken);
		if (!light) {
			return VRay::Plugin();
		}
	}

	Attrs::PluginDesc pluginDesc(theHeadlightNameToken.buffer(), "LightDirect");
	pluginDesc.addAttribute(Attrs::PluginAttr("transform", m_viewParams.renderView.tm));

	return exportPlugin(pluginDesc);
}
