//
// Copyright (c) 2015-2017, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#ifndef VRAY_FOR_HOUDINI_SOP_NODE_PHOENIX_CACHE_H
#define VRAY_FOR_HOUDINI_SOP_NODE_PHOENIX_CACHE_H
#ifdef  CGR_HAS_AUR

#include "sop_node_base.h"
#include "gu_volumegridref.h"

namespace VRayForHoudini {
namespace SOP {

class PhxShaderCache:
		public SOP::NodeBase
{
public:
	static PRM_Template      *GetPrmTemplate();

public:
	PhxShaderCache(OP_Network *parent, const char *name, OP_Operator *entry): NodeBase(parent, name, entry) {}
	virtual                  ~PhxShaderCache() {}

	virtual OP_ERROR          cookMySop(OP_Context &context) VRAY_OVERRIDE;
	virtual PluginResult      asPluginDesc(Attrs::PluginDesc &pluginDesc, VRayExporter &exporter, ExportContext *parentContext=nullptr) VRAY_OVERRIDE;

protected:
	virtual void              setPluginType() VRAY_OVERRIDE;

private:
	/// Updates gridRefPtr with the options taken from this primitive
	/// @param gridRefPtr Pointer to existing VRayVolumeGridRef instance. If there is no existing instance is nullptr
	/// @param pack Pointer to the packed primitive of type VRayVolumegridRef
	/// @param context
	/// @param time
	void                      updateVRayVolumeGridRefPrim(VRayVolumeGridRef *gridRefPtr, GU_PrimPacked *pack, OP_Context &context, const float t);

private:
	UT_StringArray            m_serializedChannels;
};

} // namespace SOP
} // namespace VRayForHoudini

#endif // CGR_HAS_AUR
#endif // VRAY_FOR_HOUDINI_SOP_NODE_PHOENIX_CACHE_H
