//
// Copyright (c) 2015, Chaos Software Ltd
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

#include <OBJ/OBJ_Node.h>
#include <OBJ/OBJ_Light.h>

#include "vfh_vray.h"


namespace VRayForHoudini {
namespace OBJ {


struct VRayPluginType
{
	static constexpr const char Light[] = "LIGHT";
};


struct VRayPluginID
{
	static constexpr const char SunLight[] = "SunLight";
	static constexpr const char LightDirect[] = "LightDirect";
	static constexpr const char LightAmbient[] = "LightAmbient";
	static constexpr const char LightOmni[] = "LightOmni";
	static constexpr const char LightSphere[] = "LightSphere";
	static constexpr const char LightSpot[] = "LightSpot";
	static constexpr const char LightRectangle[] = "LightRectangle";
	static constexpr const char LightMesh[] = "LightMesh";
	static constexpr const char LightIES[] = "LightIES";
	static constexpr const char LightDome[] = "LightDome";
};


template< const char *PluginID >
class LightNodeBase:
		public OP::VRayNode,
		public OBJ_Light
{
public:
	static const char* getPluginType() { return VRayPluginType::Light; }
	static const char* getPluginID() { return PluginID; }

	static PRM_Template*       GetPrmTemplate();
	static int                 GetMyPrmTemplate(Parm::PRMTmplList &prmList, Parm::PRMDefList &prmFolders);

public:
	LightNodeBase(OP_Network *parent, const char *name, OP_Operator *entry):OBJ_Light(parent, name, entry) { }
	virtual                    ~LightNodeBase() { }

	virtual PluginResult        asPluginDesc(Attrs::PluginDesc &pluginDesc, VRayExporter &exporter, OP_Node *parent=nullptr) VRAY_OVERRIDE;

protected:
	virtual void                setPluginType() VRAY_OVERRIDE
	{
		pluginType = getPluginType();
		pluginID = getPluginID();
	}
};


} // namespace OBJ
} // namespace VRayForHoudini



#endif // VRAY_FOR_HOUDINI_SOP_NODE_BASE_H
