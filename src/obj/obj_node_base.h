//
// Copyright (c) 2015-2018, Chaos Software Ltd
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

#include "vfh_prm_templates.h"

#include <OBJ/OBJ_Light.h>
#include <OBJ/OBJ_Geometry.h>

namespace VRayForHoudini{
namespace OBJ{

class VRayClipper
	: public OP::VRayNode
	, public OBJ_Geometry
{
public:
	static PRM_Template *GetPrmTemplate();

	VRayClipper(OP_Network *parent, const char *name, OP_Operator *entry)
		: OBJ_Geometry(parent, name, entry)
	{}

	~VRayClipper() VRAY_DTOR_OVERRIDE
	{}

	PluginResult asPluginDesc(Attrs::PluginDesc &pluginDesc, VRayExporter &exporter,
	                                  ExportContext *parentContext = nullptr) VRAY_OVERRIDE;

protected:
	void setPluginType() VRAY_OVERRIDE;
};

template <VRayPluginID PluginID>
class LightNodeBase
	: public OP::VRayNode
	, public OBJ_Light
{
public:
	static PRM_Template *GetPrmTemplate();
	static int GetMyPrmTemplate(Parm::PRMList &myPrmList);

	LightNodeBase(OP_Network *parent, const char *name, OP_Operator *entry)
		: OBJ_Light(parent, name, entry)
	{}

	~LightNodeBase() VRAY_DTOR_OVERRIDE
	{}

	PluginResult asPluginDesc(Attrs::PluginDesc &pluginDesc, VRayExporter &exporter,
	                                  ExportContext *parentContext = nullptr) VRAY_OVERRIDE;

protected:
	void setPluginType() VRAY_OVERRIDE;
};

} // namespace OBJ
} // namespace VRayForHoudini

#endif // VRAY_FOR_HOUDINI_SOP_NODE_BASE_H
