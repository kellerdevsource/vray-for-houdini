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

#ifndef VRAY_FOR_HOUDINI_SOP_NODE_GEOMDISPLACEDMESH_H
#define VRAY_FOR_HOUDINI_SOP_NODE_GEOMDISPLACEDMESH_H

#include "sop_node_base.h"


namespace VRayForHoudini {
namespace SOP {

class GeomDisplacedMesh:
		public SOP::NodeBase
{
public:
	static void                 AddAttributes(Parm::VRayPluginInfo *pluginInfo);

public:
	GeomDisplacedMesh(OP_Network *parent, const char *name, OP_Operator *entry):NodeBase(parent, name, entry) {}
	virtual                    ~GeomDisplacedMesh() {}

	virtual OP_NodeFlags       &flags() VRAY_OVERRIDE;
	virtual OP_ERROR            cookMySop(OP_Context &context) VRAY_OVERRIDE;
	virtual PluginResult        asPluginDesc(Attrs::PluginDesc &pluginDesc, VRayExporter *exporter, OP_Node *parent=nullptr) VRAY_OVERRIDE;

protected:
	virtual void                setPluginType() VRAY_OVERRIDE;

};

} // namespace SOP
} // namespace VRayForHoudini

#endif // VRAY_FOR_HOUDINI_SOP_NODE_GEOMDISPLACEDMESH_H
