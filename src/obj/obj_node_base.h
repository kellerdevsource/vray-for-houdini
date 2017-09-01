//
// Copyright (c) 2015-2017, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#ifndef VRAY_FOR_HOUDINI_OBJ_NODE_BASE_H
#define VRAY_FOR_HOUDINI_OBJ_NODE_BASE_H

#include "op/op_node_base.h"

#include <OBJ/OBJ_Light.h>
#include <OBJ/OBJ_Geometry.h>

#include "vfh_prm_templates.h"


namespace VRayForHoudini {
namespace OBJ {

class VRayClipper:
		public OP::VRayNode,
		public OBJ_Geometry
{
public:
	static PRM_Template*       GetPrmTemplate();

public:
	VRayClipper(OP_Network *parent, const char *name, OP_Operator *entry):OBJ_Geometry(parent, name, entry) { }
	virtual                    ~VRayClipper() { }

	virtual PluginResult        asPluginDesc(Attrs::PluginDesc &pluginDesc, VRayExporter &exporter, ExportContext *parentContext=nullptr) VRAY_OVERRIDE;

protected:
	virtual void                setPluginType() VRAY_OVERRIDE;
};


template< VRayPluginID PluginID >
class LightNodeBase:
		public OP::VRayNode,
		public OBJ_Light
{
public:
	static PRM_Template*       GetPrmTemplate();
	static int                 GetMyPrmTemplate(Parm::PRMList &myPrmList);

public:
	LightNodeBase(OP_Network *parent, const char *name, OP_Operator *entry):OBJ_Light(parent, name, entry) { }
	virtual                    ~LightNodeBase() { }

	virtual PluginResult        asPluginDesc(Attrs::PluginDesc &pluginDesc, VRayExporter &exporter, ExportContext *parentContext=nullptr) VRAY_OVERRIDE;

protected:
	void setPluginType() VRAY_OVERRIDE {
		pluginType = VRayPluginType::LIGHT;
		pluginID = getVRayPluginIDName(PluginID);
	}
};


} // namespace OBJ
} // namespace VRayForHoudini

#endif // VRAY_FOR_HOUDINI_SOP_NODE_BASE_H
