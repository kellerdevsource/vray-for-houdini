//
// Copyright (c) 2015-2017, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#ifndef VRAY_FOR_HOUDINI_SOP_NODE_VRAYPROXY_H
#define VRAY_FOR_HOUDINI_SOP_NODE_VRAYPROXY_H

#include "sop_node_base.h"

#include <OP/OP_Options.h>

namespace VRayForHoudini {
namespace SOP {

class VRayProxy
	: public NodePackedBase
{
public:
	static PRM_Template* getPrmTemplate();

	VRayProxy(OP_Network *parent, const char *name, OP_Operator *entry);
	virtual ~VRayProxy() {}

protected:
	/// Set custom plugin id and type for this node
	void setPluginType() VRAY_OVERRIDE;

	/// Set node time dependent flag based on UI settings.
	void setTimeDependent() VRAY_OVERRIDE;

	/// Setup / update primitive data based.
	/// @param context Cooking context.
	void updatePrimitive(const OP_Context &context) VRAY_OVERRIDE;
};

} // namespace SOP
} // namespace VRayForHoudini

#endif // VRAY_FOR_HOUDINI_SOP_NODE_VRAYPROXY_H
