//
// Copyright (c) 2015, Chaos Software Ltd
//
// V-Ray For Houdini
//
// Andrei Izrantcev <andrei.izrantcev@chaosgroup.com>
//
// All rights reserved. These coded instructions, statements and
// computer programs contain unpublished information proprietary to
// Chaos Software Ltd, which is protected by the appropriate copyright
// laws and may not be disclosed to third parties or copied or
// duplicated, in whole or in part, without prior written consent of
// Chaos Software Ltd.
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
