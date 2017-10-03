//
// Copyright (c) 2015-2017, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
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
	virtual PluginResult  asPluginDesc(Attrs::PluginDesc &pluginDesc, VRayExporter &exporter, ExportContext *parentContext=nullptr) VRAY_OVERRIDE;

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
