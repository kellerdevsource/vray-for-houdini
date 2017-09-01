//
// Copyright (c) 2015-2017, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#ifndef VRAY_FOR_HOUDINI_VOP_NODE_BRDF_SCANNED_H
#define VRAY_FOR_HOUDINI_VOP_NODE_BRDF_SCANNED_H

#include "vop_node_base.h"

namespace VRayForHoudini {
namespace VOP {

class BRDFScanned:
		public VOP::NodeBase
{
public:
	BRDFScanned(OP_Network *parent, const char *name, OP_Operator *entry):
		NodeBase(parent, name, entry)
	{ }
	virtual              ~BRDFScanned()
	{ }

	virtual bool          updateParmsFlags() VRAY_OVERRIDE;
	virtual PluginResult  asPluginDesc(Attrs::PluginDesc &pluginDesc,
									   VRayExporter &exporter,
									   ExportContext *parentContext=nullptr) VRAY_OVERRIDE;

protected:
	virtual void          getNodeSpecificInfoText (OP_Context &context,
												   OP_NodeInfoParms &iparms) VRAY_OVERRIDE;
	virtual void          setPluginType() VRAY_OVERRIDE;
};

} // namespace VOP
} // namespace VRayForHoudini

#endif // VRAY_FOR_HOUDINI_VOP_NODE_BRDF_SCANNED_H
