//
// Copyright (c) 2015, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#ifdef CGR_HAS_VRAYSCENE

#ifndef VRAY_FOR_HOUDINI_SOP_NODE_VRAYSCENE_H
#define VRAY_FOR_HOUDINI_SOP_NODE_VRAYSCENE_H

#include "sop_node_base.h"

#include <vrayscene_preview_mesh.h>


namespace VRayForHoudini {
namespace SOP {

class VRayScene:
		public SOP::NodeBase
{
public:
	VRayScene(OP_Network *parent, const char *name, OP_Operator *entry):NodeBase(parent, name, entry) {}
	virtual                    ~VRayScene() {}

	virtual OP_NodeFlags       &flags() VRAY_OVERRIDE;
	virtual OP_ERROR            cookMySop(OP_Context &context) VRAY_OVERRIDE;
	virtual PluginResult        asPluginDesc(Attrs::PluginDesc &pluginDesc, VRayExporter *exporter, OP_Node *parent=nullptr) VRAY_OVERRIDE;

private:
	static VRayScenePreviewMan  m_previewMan;

protected:
	virtual void                setPluginType() VRAY_OVERRIDE;

}; // VRayScene

} // namespace SOP
} // namespace VRayForHoudini

#endif // VRAY_FOR_HOUDINI_SOP_NODE_VRAYSCENE_H

#endif // CGR_HAS_VRAYSCENE
