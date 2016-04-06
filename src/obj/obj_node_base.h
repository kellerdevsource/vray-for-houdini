//
// Copyright (c) 2015-2016, Chaos Software Ltd
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
#include <OBJ/OBJ_Geometry.h>

#include "vfh_vray.h"


namespace VRayForHoudini {
namespace OBJ {

enum class VRayPluginType
{
	Light = 0,
	Geometry,
	MAX_PLUGINTYPE
};


enum class VRayPluginID
{
	SunLight = 0,
	LightDirect,
	LightAmbient,
	LightOmni,
	LightSphere,
	LightSpot,
	LightRectangle,
	LightMesh,
	LightIES,
	LightDome,
	VRayClipper,
	MAX_PLUGINID
};

const char *getVRayPluginTypeName(VRayPluginType pluginType);
const char *getVRayPluginIDName(VRayPluginID pluginID);


template< VRayPluginID PluginID >
class LightNodeBase:
		public OP::VRayNode,
		public OBJ_Light
{
public:
	static PRM_Template*       GetPrmTemplate();
	static int                 GetMyPrmTemplate(Parm::PRMTmplList &prmList, Parm::PRMDefList &prmFolders);

public:
	LightNodeBase(OP_Network *parent, const char *name, OP_Operator *entry):OBJ_Light(parent, name, entry) { }
	virtual                    ~LightNodeBase() { }

	virtual PluginResult        asPluginDesc(Attrs::PluginDesc &pluginDesc, VRayExporter &exporter, ExportContext *parentContext=nullptr) VRAY_OVERRIDE;

protected:
	virtual void                setPluginType() VRAY_OVERRIDE
	{
		pluginType = getVRayPluginTypeName(VRayPluginType::Light);
		pluginID = getVRayPluginIDName(PluginID);;
	}
};


class VRayClipper:
		public OBJ_Geometry,
		public OP::VRayNode
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

} // namespace OBJ
} // namespace VRayForHoudini



#endif // VRAY_FOR_HOUDINI_SOP_NODE_BASE_H
