//
// Copyright (c) 2015-2017, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#ifndef VRAY_FOR_HOUDINI_VOP_NODE_BRDF_VRAYMTL_H
#define VRAY_FOR_HOUDINI_VOP_NODE_BRDF_VRAYMTL_H

#include "vop_node_base.h"

namespace VRayForHoudini {

class BRDFVRayMtl
	: public VOP::NodeBase
{
public: 
	BRDFVRayMtl(OP_Network *parent, const char *name, OP_Operator *entry)
		: NodeBase(parent, name, entry)
	{}
	virtual ~BRDFVRayMtl() {} 

	PluginResult asPluginDesc(Attrs::PluginDesc &pluginDesc, VRayExporter &exporter, ExportContext *parentContext=nullptr) VRAY_OVERRIDE;

protected:
	void setPluginType() VRAY_OVERRIDE { 
		pluginType = VRayPluginType::BRDF; 
		pluginID   = "BRDFVRayMtl"; 
	} 
};

} // VRayForHoudini

#endif // VRAY_FOR_HOUDINI_VOP_NODE_BRDF_VRAYMTL_H
