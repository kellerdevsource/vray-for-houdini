//
// Copyright (c) 2015-2018, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#ifndef VRAY_FOR_HOUDINI_VOP_NODE_BRDF_TOONMTL_H
#define VRAY_FOR_HOUDINI_VOP_NODE_BRDF_TOONMTL_H

#include "vop_BRDFVRayMtl.h"

namespace VRayForHoudini {

class BRDFToonMtl
	: public BRDFVRayMtl
{
public: 
	BRDFToonMtl(OP_Network *parent, const char *name, OP_Operator *entry)
		: BRDFVRayMtl(parent, name, entry)
	{}
	virtual ~BRDFToonMtl() {} 

	PluginResult asPluginDesc(Attrs::PluginDesc &pluginDesc, VRayExporter &exporter) VRAY_OVERRIDE;

protected:
	void setPluginType() VRAY_OVERRIDE;
};

} // VRayForHoudini

#endif // VRAY_FOR_HOUDINI_VOP_NODE_BRDF_TOONMTL_H
