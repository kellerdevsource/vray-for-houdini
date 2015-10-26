//
// Copyright (c) 2015, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#ifndef VRAY_FOR_HOUDINI_OP_NODE_BASE_H
#define VRAY_FOR_HOUDINI_OP_NODE_BASE_H

#include "vfh_exporter.h"

#include <OP/OP_Node.h>


namespace VRayForHoudini {
namespace OP {

class VRayNode
{
public:
	enum PluginResult {
		PluginResultSuccess = 0,
		PluginResultContinue,
		PluginResultError,
		PluginResultNA,
	};

	VRayNode()
		: pluginInfo(nullptr)
	{}

	/// Extra initialization
	void                      init();

	std::string               getVRayPluginType() const { return pluginType; }
	std::string               getVRayPluginID() const   { return pluginID;   }
	Parm::VRayPluginInfo     *getVRayPluginInfo() const { return pluginInfo; }

	/// Export as plugin description
	virtual PluginResult      asPluginDesc(Attrs::PluginDesc &pluginDesc, VRayExporter &exporter, OP_Node *parent=nullptr) { return PluginResult::PluginResultNA; }

protected:
	/// Defines V-Ray plugin type
	virtual void              setPluginType()=0;
	std::string               pluginType;
	std::string               pluginID;
	Parm::VRayPluginInfo     *pluginInfo;

	VfhDisableCopy(VRayNode)
};

} // namespace OP
} // namespace VRayForHoudini

#endif // VRAY_FOR_HOUDINI_OP_NODE_BASE_H
