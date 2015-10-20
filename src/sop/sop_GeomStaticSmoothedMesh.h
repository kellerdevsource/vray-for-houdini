//
// Copyright (c) 2015, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#ifndef VRAY_FOR_HOUDINI_SOP_NODE_GEOMSTATICSMOOTHEDMESH_H
#define VRAY_FOR_HOUDINI_SOP_NODE_GEOMSTATICSMOOTHEDMESH_H

#include "sop_node_base.h"


namespace VRayForHoudini {
namespace SOP {

class GeomStaticSmoothedMesh:
		public SOP::NodeBase
{
public:
	static void                 addPrmTemplate(Parm::PRMTmplList &prmTemplate);

public:
	GeomStaticSmoothedMesh(OP_Network *parent, const char *name, OP_Operator *entry):NodeBase(parent, name, entry) {}
	virtual                    ~GeomStaticSmoothedMesh() {}

	virtual OP_NodeFlags       &flags() VRAY_OVERRIDE;
	virtual OP_ERROR            cookMySop(OP_Context &context) VRAY_OVERRIDE;
	virtual PluginResult        asPluginDesc(Attrs::PluginDesc &pluginDesc, VRayExporter *exporter, OP_Node *parent=nullptr) VRAY_OVERRIDE;

protected:
	virtual void                setPluginType() VRAY_OVERRIDE;

};

} // namespace SOP
} // namespace VRayForHoudini

#endif // VRAY_FOR_HOUDINI_SOP_NODE_GEOMSTATICSMOOTHEDMESH_H
