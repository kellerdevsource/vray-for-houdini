//
// Copyright (c) 2015, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#ifndef VRAY_FOR_HOUDINI_VOP_NODE_TEXTURE_OUTPUT_H
#define VRAY_FOR_HOUDINI_VOP_NODE_TEXTURE_OUTPUT_H

#include "vop_node_base.h"


namespace VRayForHoudini {
namespace VOP {


class TextureOutput:
		public NodeBase
{
public:
	static void           addPrmTemplate(Parm::PRMTmplList &prmTemplate);

public:
	TextureOutput(OP_Network *parent, const char *name, OP_Operator *entry):NodeBase(parent, name, entry) {}
	virtual              ~TextureOutput() {}

	virtual PluginResult  asPluginDesc(Attrs::PluginDesc &pluginDesc, VRayExporter &exporter, OP_Node *parent=nullptr) VRAY_OVERRIDE;

protected:
	virtual void          setPluginType() VRAY_OVERRIDE;

};

} // namespace VOP
} // namespace VRayForHoudini

#endif // VRAY_FOR_HOUDINI_VOP_NODE_TEXTURE_OUTPUT_H
