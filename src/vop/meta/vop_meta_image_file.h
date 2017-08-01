//
// Copyright (c) 2015-2016, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#ifndef VRAY_FOR_HOUDINI_VOP_NODE_META_IMAGE_FILE_H
#define VRAY_FOR_HOUDINI_VOP_NODE_META_IMAGE_FILE_H

#include "vop_node_base.h"


namespace VRayForHoudini {
namespace VOP {

class MetaImageFile:
		public NodeBase
{
public:
	static PRM_Template  *GetPrmTemplate();

public:
	MetaImageFile(OP_Network *parent, const char *name, OP_Operator *entry):NodeBase(parent, name, entry) {}
	virtual              ~MetaImageFile() {}

	virtual PluginResult  asPluginDesc(Attrs::PluginDesc &pluginDesc, VRayExporter &exporter, ExportContext *parentContext=nullptr) VRAY_OVERRIDE;

	virtual const char *outputLabel(unsigned idx) const VRAY_OVERRIDE;
	virtual const char *inputLabel(unsigned idx) const VRAY_OVERRIDE;

	virtual unsigned          getNumVisibleOutputs() const VRAY_OVERRIDE;
	virtual unsigned          orderedInputs() const VRAY_OVERRIDE;
	virtual unsigned          maxOutputs() const VRAY_OVERRIDE;
	virtual unsigned          getNumVisibleInputs() const VRAY_OVERRIDE;
protected:
	virtual void          setPluginType() VRAY_OVERRIDE;

	virtual void              getOutputNameSubclass(UT_String &out, int idx) const VRAY_OVERRIDE;
	virtual void              getInputNameSubclass(UT_String &in, int idx) const VRAY_OVERRIDE;

	virtual int               getInputFromNameSubclass(const UT_String &in) const VRAY_OVERRIDE;
	virtual int               getOutputFromName(const UT_String &out) const VRAY_OVERRIDE;

	virtual void              getOutputTypeInfoSubclass(VOP_TypeInfo &type_info, int idx) VRAY_OVERRIDE;
};

} // namespace VOP
} // namespace VRayForHoudini

#endif // VRAY_FOR_HOUDINI_VOP_NODE_META_IMAGE_FILE_H
