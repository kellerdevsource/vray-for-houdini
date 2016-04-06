//
// Copyright (c) 2015-2016, Chaos Software Ltd
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

#include <vrscene_preview.h>


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
	virtual PluginResult        asPluginDesc(Attrs::PluginDesc &pluginDesc, VRayExporter &exporter, ExportContext *parentContext=nullptr) VRAY_OVERRIDE;

protected:
	virtual void                setPluginType() VRAY_OVERRIDE;

private:
	VUtils::Vrscene::Preview::VrsceneSettings            getVrsceneSettings() const;
	static VUtils::Vrscene::Preview::VrsceneDescManager  m_vrsceneMan;

}; // VRayScene

} // namespace SOP
} // namespace VRayForHoudini

#endif // VRAY_FOR_HOUDINI_SOP_NODE_VRAYSCENE_H

#endif // CGR_HAS_VRAYSCENE
