//
// Copyright (c) 2015-2017, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#ifndef VRAY_FOR_HOUDINI_SOP_NODE_GEOMPLANE_H
#define VRAY_FOR_HOUDINI_SOP_NODE_GEOMPLANE_H

#include "sop_node_base.h"
#include "vfh_prm_templates.h"


namespace VRayForHoudini {
namespace SOP {

/// SOP node that creates V-Ray infinite plane geometry
class GeomPlane
	: public SOP::NodeBase
{
public:
	GeomPlane(OP_Network *parent, const char *name, OP_Operator *entry)
		: NodeBase(parent, name, entry)
	{}

	/// Houdini callback to cook custom geometry for this node
	/// @param context[in] - cook time
	OP_ERROR cookMySop(OP_Context &context) VRAY_OVERRIDE;

protected:
	/// Set custom plugin id and type for this node
	void setPluginType() VRAY_OVERRIDE;
};

} // namespace SOP
} // namespace VRayForHoudini

#endif // VRAY_FOR_HOUDINI_SOP_NODE_GEOMPLANE_H
