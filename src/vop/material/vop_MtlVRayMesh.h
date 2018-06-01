//
// Copyright (c) 2015-2018, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#ifndef VRAY_FOR_HOUDINI_VOP_NODE_MTLVRAYMESH_H
#define VRAY_FOR_HOUDINI_VOP_NODE_MTLVRAYMESH_H

#include "vop_node_base.h"

namespace VRayForHoudini{
namespace VOP{

typedef QMap<int, UT_String> IndexToToken;
typedef QMap<QString, int> TokenToIndex;

struct MtlVRayMeshShaderSets {
	/// Init shader sets data from VRayProxy file.
	/// @param self MtlVRayMesh instance. Used for logging.
	/// @param filePath VRayProxy compatible file path.
	void init(const OP_Node &self, const UT_String &filePath);

	/// Add material token.
	/// @param index Index.
	/// @param setName Set name.
	void addSetName(int index, const char *setName);

	/// Get shaders sets count.
	int count() const;

	/// Clear shader sets data.
	void clear();

	IndexToToken indexToToken;
	TokenToIndex tokenToIndex;
};

class MtlVRayMesh
	: public NodeBase
{
public:
	static PRM_Template *getPrmTemplate();

	MtlVRayMesh(OP_Network *parent, const char *name, OP_Operator *entry)
		: NodeBase(parent, name, entry) {}
	~MtlVRayMesh() VRAY_DTOR_OVERRIDE {}

	// From OP::VRayNode
	PluginResult asPluginDesc(Attrs::PluginDesc &pluginDesc, VRayExporter &exporter) VRAY_OVERRIDE;

	/// Set update flag for rebuilding shader sets data.
	void setRebuildShaderSets(int value) const;

	/// Rebuild shader sets data.
	void rebuildShaderSets() const;

protected:
	void setPluginType() VRAY_OVERRIDE;

	// From VOP_Node
public:
	void opChanged(OP_EventType reason, void *data) VRAY_OVERRIDE;
	const char *inputLabel(unsigned idx) const VRAY_OVERRIDE;
	unsigned getNumVisibleInputs() const VRAY_OVERRIDE;
	unsigned orderedInputs() const VRAY_OVERRIDE;
	OP_ERROR saveIntrinsic(std::ostream &os, const OP_SaveFlags &sflags) VRAY_OVERRIDE;
	bool loadPacket(UT_IStream &is, const char *token, const char *path) VRAY_OVERRIDE;

protected:
	int getInputFromName(const UT_String &in) const VRAY_OVERRIDE;
	void getInputNameSubclass(UT_String &in, int idx) const VRAY_OVERRIDE;
	int getInputFromNameSubclass(const UT_String &in) const VRAY_OVERRIDE;
	void getInputTypeInfoSubclass(VOP_TypeInfo &type_info, int idx) VRAY_OVERRIDE;
	void getAllowedInputTypeInfosSubclass(unsigned idx, VOP_VopTypeInfoArray &type_infos) VRAY_OVERRIDE;
	void getAllowedInputTypesSubclass(unsigned idx, VOP_VopTypeArray &voptypes) VRAY_OVERRIDE;
	bool willAutoconvertInputType(int input_idx) VRAY_OVERRIDE;

private:
	int updateSopPath() const;
	int updateFilePath() const;

	/// Current VRayProxy SOP oppath.
	mutable UT_String sopPath;

	/// Current VRayProxy file oppath.
	mutable UT_String filePath;

	/// Shader sets data.
	mutable MtlVRayMeshShaderSets shaderSets;

	/// Flag indicating that we need to reload shader sets data.
	mutable int needRebuildShadersSets = false;
};

} // namespace VOP
} // namespace VRayForHoudini

#endif // VRAY_FOR_HOUDINI_VOP_NODE_MTLVRAYMESH_H
