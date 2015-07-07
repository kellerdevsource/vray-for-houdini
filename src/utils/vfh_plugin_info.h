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

#ifndef VRAY_FOR_HOUDINI_VRAY_PLUGIN_INFO_H
#define VRAY_FOR_HOUDINI_VRAY_PLUGIN_INFO_H

#include "vfh_prm_def.h"


namespace VRayForHoudini {
namespace Parm {


struct VRayPluginInfo {
	VRayPluginInfo() {}

	AttributeDescs          attributes;
	SocketsDesc             inputs;
	SocketsDesc             outputs;
	PRMTmplList             prm_template;

	std::string             pluginType;

	const AttrDesc         *getAttributeDesc(const std::string &attrName) const;

private:
	VRayPluginInfo(const VRayPluginInfo&);

};
typedef std::map<std::string, VRayPluginInfo*> VRayPluginsInfo;

VRayPluginInfo* GetVRayPluginInfo(const std::string &pluginID);
VRayPluginInfo* NewVRayPluginInfo(const std::string &pluginID);

} // namespace Parm
} // namespace VRayForHoudini

#endif // VRAY_FOR_HOUDINI_VRAY_PLUGIN_INFO_H
