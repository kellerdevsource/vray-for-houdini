//
// Copyright (c) 2015-2017, Chaos Software Ltd
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
	static PRM_Template *GetPrmTemplate();

public:
	MetaImageFile(OP_Network *parent, const char *name, OP_Operator *entry):NodeBase(parent, name, entry) {}
	virtual ~MetaImageFile() {}

	PluginResult  asPluginDesc(Attrs::PluginDesc &pluginDesc, VRayExporter &exporter, ExportContext *parentContext=nullptr) VRAY_OVERRIDE;

	const char *outputLabel(unsigned idx) const VRAY_OVERRIDE;
	const char *inputLabel(unsigned idx) const VRAY_OVERRIDE;

	unsigned getNumVisibleOutputs() const VRAY_OVERRIDE;
	unsigned orderedInputs() const VRAY_OVERRIDE;
	unsigned maxOutputs() const VRAY_OVERRIDE;
	unsigned getNumVisibleInputs() const VRAY_OVERRIDE;
protected:
	void setPluginType() VRAY_OVERRIDE;

	void getOutputNameSubclass(UT_String &out, int idx) const VRAY_OVERRIDE;
	void getInputNameSubclass(UT_String &in, int idx) const VRAY_OVERRIDE;

	int getInputFromNameSubclass(const UT_String &in) const VRAY_OVERRIDE;
	int getOutputFromName(const UT_String &out) const VRAY_OVERRIDE;

	void getOutputTypeInfoSubclass(VOP_TypeInfo &type_info, int idx) VRAY_OVERRIDE;
	void getInputTypeInfoSubclass(VOP_TypeInfo &type_info, int idx) VRAY_OVERRIDE;
};

} // namespace VOP
} // namespace VRayForHoudini

#endif // VRAY_FOR_HOUDINI_VOP_NODE_META_IMAGE_FILE_H
