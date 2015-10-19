//
// Copyright (c) 2015, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#ifndef VRAY_FOR_HOUDINI_OBJ_NODE_LIGHTDOME_H
#define VRAY_FOR_HOUDINI_OBJ_NODE_LIGHTDOME_H

#include "obj_node_base.h"


namespace VRayForHoudini {
namespace OBJ {

class LightDome:
		public OBJ::LightNodeBase
{
public:
	static void                 AddAttributes(Parm::VRayPluginInfo *pluginInfo);

public:
	LightDome(OP_Network *parent, const char *name, OP_Operator *entry):LightNodeBase(parent, name, entry) {}
	virtual                    ~LightDome() {}

	virtual PluginResult        asPluginDesc(Attrs::PluginDesc &pluginDesc, VRayExporter *exporter, OP_Node *parent=nullptr) VRAY_OVERRIDE;

protected:
	virtual void                setPluginType() VRAY_OVERRIDE;

};

} // namespace OBJ
} // namespace VRayForHoudini

#endif // VRAY_FOR_HOUDINI_OBJ_NODE_LIGHTDOME_H
