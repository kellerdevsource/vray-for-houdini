//
// Copyright (c) 2015-2017, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#ifndef VRAY_FOR_HOUDINI_SOP_NODE_PHOENIX_CACHE_H
#define VRAY_FOR_HOUDINI_SOP_NODE_PHOENIX_CACHE_H
#ifdef  CGR_HAS_AUR

#include "sop_node_base.h"

namespace VRayForHoudini {
namespace SOP {

class PhxShaderCache
	: public NodePackedBase
{
public:
	static void buildMenuPrmNames(void *data, PRM_Name *choicenames, int listsize, const PRM_SpareData *spare, const PRM_Parm *parm);
	static PRM_Template* getPrmTemplate();

public:
	PhxShaderCache(OP_Network *parent, const char *name, OP_Operator *entry);

	UT_StringArray& getPhxChannels() const;

protected:
	// From VRayNode.
	void setPluginType() VRAY_OVERRIDE;

	// From NodePackedBase.
	void setTimeDependent() VRAY_OVERRIDE;
	void updatePrimitive(const OP_Context &context) VRAY_OVERRIDE;

private:
	mutable UT_StringArray m_phxChannels;
};

} // namespace SOP
} // namespace VRayForHoudini

#endif // CGR_HAS_AUR
#endif // VRAY_FOR_HOUDINI_SOP_NODE_PHOENIX_CACHE_H
