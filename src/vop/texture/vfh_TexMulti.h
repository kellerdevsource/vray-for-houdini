//
// Copyright (c) 2015-2017, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#ifndef VRAY_FOR_HOUDINI_VFH_TEXMULTI_H
#define VRAY_FOR_HOUDINI_VFH_TEXMULTI_H

#include "vop_node_base.h"

namespace VRayForHoudini {
namespace VOP {

class TexMulti
	: public VOP::NodeBase
{
public:
	TexMulti(OP_Network *parent, const char *name, OP_Operator *entry)
		: NodeBase(parent, name, entry)
	{}
	~TexMulti() VRAY_OVERRIDE {}

	// From OP::VRayNode
	PluginResult asPluginDesc(Attrs::PluginDesc &pluginDesc,
	                          VRayExporter &exporter,
	                          ExportContext *parentContext = nullptr) VRAY_OVERRIDE;

protected:
	void setPluginType() VRAY_OVERRIDE;

	// From VOP_Node
public:
	void getCode(UT_String &codestr, const VOP_CodeGenContext &context)
	VRAY_OVERRIDE;
	const char *inputLabel(unsigned idx) const VRAY_OVERRIDE;
	unsigned getNumVisibleInputs() const VRAY_OVERRIDE;
	unsigned orderedInputs() const VRAY_OVERRIDE;

protected:
	int getInputFromName(const UT_String &in) const VRAY_OVERRIDE;
	void getInputNameSubclass(UT_String &in, int idx) const VRAY_OVERRIDE;
	int getInputFromNameSubclass(const UT_String &in) const VRAY_OVERRIDE;
	void getInputTypeInfoSubclass(VOP_TypeInfo &type_info, int idx) VRAY_OVERRIDE;
	void getAllowedInputTypeInfosSubclass(unsigned idx, VOP_VopTypeInfoArray &type_infos) VRAY_OVERRIDE;

private:
	int customInputsCount() const;
};

} // namespace VOP
} // namespace VRayForHoudini

#endif // VRAY_FOR_HOUDINI_VFH_TEXMULTI_H
