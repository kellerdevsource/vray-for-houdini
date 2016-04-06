//
// Copyright (c) 2015-2016, Chaos Software Ltd
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

class MaterialOutput:
		public VOP_Node
{
public:
	static OP_Node           *creator(OP_Network *parent, const char *name, OP_Operator *entry) { return new MaterialOutput(parent, name, entry); }
	static void               register_operator(OP_OperatorTable *table);

public:
	virtual void              getCode(UT_String &codestr, const VOP_CodeGenContext &context) VRAY_OVERRIDE;

protected:
	MaterialOutput(OP_Network *parent, const char *name, OP_Operator *entry) : VOP_Node(parent, name, entry) { }
	virtual                  ~MaterialOutput() { }

	virtual bool              willAutoconvertInputType(int idx) VRAY_OVERRIDE { return false; }
	virtual bool              generateErrorsSubclass() VRAY_OVERRIDE;
	virtual void              getInputNameSubclass(UT_String &in, int idx) const VRAY_OVERRIDE;
	virtual int               getInputFromNameSubclass(const UT_String &in) const VRAY_OVERRIDE;
	virtual void              getInputTypeInfoSubclass(VOP_TypeInfo &type_info, int idx) VRAY_OVERRIDE;
	virtual void              getAllowedInputTypeInfosSubclass(unsigned idx, VOP_VopTypeInfoArray &type_infos) VRAY_OVERRIDE;
};

} // namespace VOP
} // namespace VRayForHoudini

#endif // VRAY_FOR_HOUDINI_VOP_MATERIALOUTPUT_H
