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

#ifndef VRAY_FOR_HOUDINI_VOP_NODE_RENDERCHANNEL_CONTAINER_H
#define VRAY_FOR_HOUDINI_VOP_NODE_RENDERCHANNEL_CONTAINER_H

#include "vop_node_base.h"


namespace VRayForHoudini {
namespace VOP {


class RenderChannelsContainer:
		public VOP::NodeBase
{
public:
	RenderChannelsContainer(OP_Network *parent, const char *name, OP_Operator *entry):NodeBase(parent, name, entry) {}
	virtual              ~RenderChannelsContainer() {}

	// From OP::VRayNode
	virtual PluginResult  asPluginDesc(Attrs::PluginDesc &pluginDesc, VRayExporter *exporter, OP_Node *parent=nullptr) VRAY_OVERRIDE;

protected:
	virtual void          setPluginType() VRAY_OVERRIDE;

	// From VOP_Node
public:
	virtual const char   *inputLabel(unsigned idx) const VRAY_OVERRIDE;
	virtual unsigned      getNumVisibleInputs() const VRAY_OVERRIDE;
	virtual unsigned      orderedInputs() const VRAY_OVERRIDE;

protected:
	virtual int           getInputFromName(const UT_String &in) const VRAY_OVERRIDE;
	virtual void          getInputNameSubclass(UT_String &in, int idx) const VRAY_OVERRIDE;
	virtual int           getInputFromNameSubclass(const UT_String &in) const VRAY_OVERRIDE;
	virtual void          getInputTypeInfoSubclass(VOP_TypeInfo &type_info, int idx) VRAY_OVERRIDE;
	virtual void          getAllowedInputTypeInfosSubclass(unsigned idx, VOP_VopTypeInfoArray &type_infos) VRAY_OVERRIDE;

};

} // namespace VOP
} // namespace VRayForHoudini

#endif // VRAY_FOR_HOUDINI_VOP_NODE_RENDERCHANNEL_CONTAINER_H
