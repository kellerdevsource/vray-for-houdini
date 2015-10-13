//
// Copyright (c) 2015, Chaos Software Ltd
//
// V-Ray For Houdini
//
// Mila Grigorova <mila.grigorova@chaosgroup.com>
//
// All rights reserved. These coded instructions, statements and
// computer programs contain unpublished information proprietary to
// Chaos Software Ltd, which is protected by the appropriate copyright
// laws and may not be disclosed to third parties or copied or
// duplicated, in whole or in part, without prior written consent of
// Chaos Software Ltd.
//

#ifndef VRAY_FOR_HOUDINI_VOP_MATERIALOUTPUT_H
#define VRAY_FOR_HOUDINI_VOP_MATERIALOUTPUT_H

#include "vfh_defines.h"

#include <VOP/VOP_Node.h>

#include "systemstuff.h"


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
	virtual unsigned          getNumVisibleInputs() const VRAY_OVERRIDE { return nConnectedInputs() + 1; }

protected:
	MaterialOutput(OP_Network *parent, const char *name, OP_Operator *entry) : VOP_Node(parent, name, entry) { }
	virtual                  ~MaterialOutput() { }

	virtual void              getInputNameSubclass(UT_String &in, int idx) const VRAY_OVERRIDE;
	virtual int               getInputFromNameSubclass(const UT_String &in) const VRAY_OVERRIDE;
	virtual void              getInputTypeInfoSubclass(VOP_TypeInfo &type_info, int idx) VRAY_OVERRIDE;
	virtual void              getAllowedInputTypeInfosSubclass(unsigned idx, VOP_VopTypeInfoArray &type_infos) VRAY_OVERRIDE;
};

} // namespace VOP
} // namespace VRayForHoudini

#endif // VRAY_FOR_HOUDINI_VOP_MATERIALOUTPUT_H
