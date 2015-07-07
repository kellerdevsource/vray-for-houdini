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

#ifndef VRAY_FOR_HOUDINI_VOP_NODE_BRDF_LAYERED_H
#define VRAY_FOR_HOUDINI_VOP_NODE_BRDF_LAYERED_H

#include "vop_node_base.h"


namespace VRayForHoudini {
namespace VOP {


class BRDFLayered:
		public VOP::NodeBase
{
public:
	static void           AddAttributes(Parm::VRayPluginInfo *pluginInfo);

public:
	BRDFLayered(OP_Network *parent, const char *name, OP_Operator *entry):NodeBase(parent, name, entry) {}
	virtual              ~BRDFLayered() {}

	// From OP::VRayNode
	virtual PluginResult  asPluginDesc(Attrs::PluginDesc &pluginDesc, VRayExporter *exporter, OP_Node *parent=nullptr) VRAY_OVERRIDE;

protected:
	virtual void          setPluginType() VRAY_OVERRIDE;

	// From VOP_Node
public:
	virtual void          getCode(UT_String &codestr, const VOP_CodeGenContext &context) VRAY_OVERRIDE;
	virtual const char   *inputLabel(unsigned idx) const VRAY_OVERRIDE;
	virtual const char   *outputLabel(unsigned idx) const VRAY_OVERRIDE;
	virtual unsigned      getNumVisibleInputs() const VRAY_OVERRIDE;
	virtual unsigned      orderedInputs() const VRAY_OVERRIDE;

protected:
	virtual void          getInputNameSubclass(UT_String &in, int idx) const VRAY_OVERRIDE;
	virtual int           getInputFromName(const UT_String &in) const VRAY_OVERRIDE;
	virtual int           getInputFromNameSubclass(const UT_String &in) const VRAY_OVERRIDE;
	virtual void          getInputTypeInfoSubclass(VOP_TypeInfo &type_info, int idx) VRAY_OVERRIDE;
	virtual void          getOutputNameSubclass(UT_String &out, int idx) const VRAY_OVERRIDE;
	virtual void          getOutputTypeInfoSubclass(VOP_TypeInfo &type_info,  int idx) VRAY_OVERRIDE;
	virtual void          getAllowedInputTypeInfosSubclass(unsigned idx, VOP_VopTypeInfoArray &type_infos) VRAY_OVERRIDE;

private:
	int                   customInputsCount() const;

};

} // namespace VOP
} // namespace VRayForHoudini

#endif // VRAY_FOR_HOUDINI_VOP_NODE_BRDF_LAYERED_H
