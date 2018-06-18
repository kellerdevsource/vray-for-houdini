//
// Copyright (c) 2015-2018, Chaos Software Ltd
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

namespace VRayForHoudini {
namespace SOP {

class VRayProxy
	: public NodePackedBase
{
public:
	static PRM_Template* getPrmTemplate();

	VRayProxy(OP_Network *parent, const char *name, OP_Operator *entry);
	~VRayProxy() VRAY_OVERRIDE;

protected:
	// From VRayNode.
	void setPluginType() VRAY_OVERRIDE;

	// From NodePackedBase.
	void setTimeDependent() VRAY_OVERRIDE;
	void getCreatePrimitive() VRAY_OVERRIDE;
	void updatePrimitiveFromOptions(const OP_Options &options) VRAY_OVERRIDE;
	void updatePrimitive(const OP_Context &context) VRAY_OVERRIDE;

private:
	PrimWithOptions &createPrimitive(const QString &name);

	void enumObjectInfo(const VUtils::ObjectInfoChannelData &chanData, int channelID);
	void enumMeshFile(const char *filePath);

	/// Packed primitives list.
	PrimWithOptionsList prims;
};

} // namespace SOP
} // namespace VRayForHoudini

#endif // VRAY_FOR_HOUDINI_SOP_NODE_VRAYPROXY_H
