//
// Copyright (c) 2015-2016, Chaos Software Ltd
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

class BRDFVRayMtl: 
	public VRayForHoudini::VOP::NodeBase {
public: 
	BRDFVRayMtl(OP_Network *parent, const char *name, OP_Operator *entry):NodeBase(parent, name, entry) {}
	virtual ~BRDFVRayMtl() {} 

	virtual PluginResult asPluginDesc(VRayForHoudini::Attrs::PluginDesc &pluginDesc, VRayForHoudini::VRayExporter &exporter, VRayForHoudini::ExportContext *parentContext=nullptr) VRAY_OVERRIDE;
protected: 
	virtual void setPluginType() VRAY_OVERRIDE { 
		pluginType = VRayForHoudini::VRayPluginType::BRDF;
		pluginID   = "BRDFVRayMtl"; 
	} 


};

#endif