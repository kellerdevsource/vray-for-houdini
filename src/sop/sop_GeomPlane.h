//
// Copyright (c) 2015-2018, Chaos Software Ltd
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

namespace VRayForHoudini {
namespace SOP {

/// SOP node that creates V-Ray infinite plane geometry.
class GeomPlane
	: public NodePackedBase
{
public:
	GeomPlane(OP_Network *parent, const char *name, OP_Operator *entry);

protected:
	/// Set custom plugin id and type for this node
	void setPluginType() VRAY_OVERRIDE;
};

} // namespace SOP
} // namespace VRayForHoudini

#endif // VRAY_FOR_HOUDINI_SOP_NODE_GEOMPLANE_H
