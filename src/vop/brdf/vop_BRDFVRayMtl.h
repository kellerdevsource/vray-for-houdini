

#include "vop_node_base.h"

class BRDFVRayMtl: 
	public VRayForHoudini::VOP::NodeBase {
public: 
	BRDFVRayMtl(OP_Network *parent, const char *name, OP_Operator *entry):NodeBase(parent, name, entry) {} \
	virtual ~BRDFVRayMtl() {} 

	virtual PluginResult asPluginDesc(VRayForHoudini::Attrs::PluginDesc &pluginDesc, VRayForHoudini::VRayExporter &exporter, VRayForHoudini::ExportContext *parentContext=nullptr) VRAY_OVERRIDE;
protected: 
	virtual void setPluginType() VRAY_OVERRIDE { 
		pluginType = "BRDF"; 
		pluginID   = "BRDFVRayMtl"; 
	} 


};