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

#ifndef VRAY_FOR_HOUDINI_VOP_NODE_GEOMSTATICSMOOTHEDMESH_H
#define VRAY_FOR_HOUDINI_VOP_NODE_GEOMSTATICSMOOTHEDMESH_H

#include "vop_node_base.h"


namespace VRayForHoudini {
namespace VOP {

class GeomStaticSmoothedMesh:
		public VOP::NodeBase
{
public:
	GeomStaticSmoothedMesh(OP_Network *parent, const char *name, OP_Operator *entry):NodeBase(parent, name, entry) {}
	virtual                    ~GeomStaticSmoothedMesh() {}

//	From VOP_Node
	virtual void          getCode(UT_String &codestr, const VOP_CodeGenContext &context) VRAY_OVERRIDE;

	virtual PluginResult        asPluginDesc(Attrs::PluginDesc &pluginDesc, VRayExporter *exporter, OP_Node *parent=nullptr) VRAY_OVERRIDE;

protected:
	virtual void                setPluginType() VRAY_OVERRIDE;

};

} // namespace VOP
} // namespace VRayForHoudini

#endif // VRAY_FOR_HOUDINI_VOP_NODE_GEOMSTATICSMOOTHEDMESH_H
