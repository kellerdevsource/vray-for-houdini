//
// Copyright (c) 2015-2017, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#ifndef VRAY_FOR_HOUDINI_VOP_MATERIALOUTPUT_H
#define VRAY_FOR_HOUDINI_VOP_MATERIALOUTPUT_H

#include "vfh_defines.h"
#include "vfh_vray.h" // For proper "systemstuff.h" inclusion

#include <VOP/VOP_Node.h>

namespace VRayForHoudini {
namespace VOP {

class MaterialOutput
	: public VOP_Node
{
public:
	static OP_Node *creator(OP_Network *parent, const char *name, OP_Operator *entry);
	static void register_operator(OP_OperatorTable *table);

protected:
	MaterialOutput(OP_Network *parent, const char *name, OP_Operator *entry);
	virtual ~MaterialOutput();

	const char *inputLabel(unsigned idx) const VRAY_OVERRIDE;
	unsigned getNumVisibleInputs() const VRAY_OVERRIDE;
	unsigned orderedInputs() const VRAY_OVERRIDE;
	bool willAutoconvertInputType(int idx) VRAY_OVERRIDE;
	void getInputNameSubclass(UT_String &in, int idx) const VRAY_OVERRIDE;
	int getInputFromNameSubclass(const UT_String &in) const VRAY_OVERRIDE;
	void getInputTypeInfoSubclass(VOP_TypeInfo &type_info, int idx) VRAY_OVERRIDE;
	void getAllowedInputTypeInfosSubclass(unsigned idx, VOP_VopTypeInfoArray &type_infos) VRAY_OVERRIDE;
};

} // namespace VOP
} // namespace VRayForHoudini

#endif // VRAY_FOR_HOUDINI_VOP_MATERIALOUTPUT_H
