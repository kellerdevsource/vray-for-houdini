//
// Copyright (c) 2015, Chaos Software Ltd
//
// V-Ray For Houdini
//
// Andrei Izrantcev <andrei.izrantcev@chaosgroup.com>
//
// All rights reserved. These coded instructions, statements and
// computer programs contain unpublished information proprietary to
// Chaos Software Ltd, which is protected by the appropriate copyright
// laws and may not be disclosed to third parties or copied or
// duplicated, in whole or in part, without prior written consent of
// Chaos Software Ltd.
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

	VRayNode():
		pluginType(""),
		pluginID(""),
		pluginInfo(nullptr)
	{}

	/// Extra initialization
	void                      init();

	std::string               getVRayPluginType() const { return pluginType; }
	std::string               getVRayPluginID() const   { return pluginID;   }
	Parm::VRayPluginInfo     *getVRayPluginInfo() const { return pluginInfo; }

	/// Export as plugin description
	virtual PluginResult      asPluginDesc(Attrs::PluginDesc &pluginDesc, VRayExporter *exporter, OP_Node *parent=nullptr) { return PluginResult::PluginResultNA; }

protected:
	/// Defines V-Ray plugin type
	virtual void              setPluginType()=0;
	std::string               pluginType;
	std::string               pluginID;
	Parm::VRayPluginInfo     *pluginInfo;

};

} // namespace OP
} // namespace VRayForHoudini

#endif // VRAY_FOR_HOUDINI_OP_NODE_BASE_H
