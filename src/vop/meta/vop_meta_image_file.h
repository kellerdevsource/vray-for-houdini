//
// Copyright (c) 2015-2018, Chaos Software Ltd
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

class MetaImageFile
	: public NodeBase
{
public:
	/// UVW generator types.
	enum UVWGenType {
		UVWGenMayaPlace2dTexture = 0,
		UVWGenEnvironment        = 1,
		UVWGenExplicit           = 2,
		UVWGenChannel            = 3,
		UVWGenObject             = 4,
		UVWGenObjectBBox         = 5,
		UVWGenPlanarWorld        = 6,
		UVWGenProjection         = 7,
	};

	struct UVWGenSocket {
		UVWGenSocket(const char *label, const VOP_TypeInfo typeInfo)
			: label(label)
			, typeInfo(typeInfo)
		{}

		const char *label;
		const VOP_TypeInfo typeInfo;
	};

	typedef std::vector<UVWGenSocket> UVWGenSocketsTable;

	static PRM_Template *GetPrmTemplate();

	MetaImageFile(OP_Network *parent, const char *name, OP_Operator *entry):NodeBase(parent, name, entry) {}
	virtual ~MetaImageFile() {}

	PluginResult  asPluginDesc(Attrs::PluginDesc &pluginDesc, VRayExporter &exporter, ExportContext *parentContext=nullptr) VRAY_OVERRIDE;

	const char *inputLabel(unsigned idx) const VRAY_OVERRIDE;

	unsigned orderedInputs() const VRAY_OVERRIDE;
	unsigned getNumVisibleInputs() const VRAY_OVERRIDE;

protected:
	void setPluginType() VRAY_OVERRIDE;

	void getInputNameSubclass(UT_String &in, int idx) const VRAY_OVERRIDE;
	int getInputFromNameSubclass(const UT_String &in) const VRAY_OVERRIDE;
	void getInputTypeInfoSubclass(VOP_TypeInfo &type_info, int idx) VRAY_OVERRIDE;

private:
	/// Get currently chosed UV generator type.
	UVWGenType getUVWGenType() const;

	/// Get currently chosed UV generator input sockets array.
	const UVWGenSocketsTable &getUVWGenInputs() const;
};

} // namespace VOP
} // namespace VRayForHoudini

#endif // VRAY_FOR_HOUDINI_VOP_NODE_META_IMAGE_FILE_H
