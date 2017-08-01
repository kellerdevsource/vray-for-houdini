//
// Copyright (c) 2015-2016, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#ifndef VRAY_FOR_HOUDINI_VOP_NODE_META_IMAGE_FILE_H
#define VRAY_FOR_HOUDINI_VOP_NODE_META_IMAGE_FILE_H

#include "vop_node_base.h"

namespace VRayForHoudini {
namespace VOP {

class MetaImageFile
	: public NodeBase
{
public:
	MetaImageFile(OP_Network *parent, const char *name, OP_Operator *entry)
		: NodeBase(parent, name, entry)
	{}
	virtual ~MetaImageFile() {}

	// From OP::VRayNode
	void init() VRAY_OVERRIDE;
	PluginResult asPluginDesc(Attrs::PluginDesc &pluginDesc, VRayExporter &exporter, ExportContext *parentContext=nullptr) VRAY_OVERRIDE;

protected:
	// From OP::VRayNode
	void setPluginType() VRAY_OVERRIDE;
};

} // namespace VOP
} // namespace VRayForHoudini

#endif // VRAY_FOR_HOUDINI_VOP_NODE_META_IMAGE_FILE_H
