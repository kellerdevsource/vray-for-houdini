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

#ifdef  CGR_HAS_AUR
#ifndef VRAY_FOR_HOUDINI_SOP_NODE_PHOENIX_CACHE_H
#define VRAY_FOR_HOUDINI_SOP_NODE_PHOENIX_CACHE_H

#include "sop_node_base.h"
#include "vfh_data_cache.h"


namespace VRayForHoudini {
namespace SOP {


struct FluidFrame
{
	FluidFrame():
		data(nullptr)
	{
		size[0] = 0;
		size[1] = 0;
		size[2] = 0;
	}

	~FluidFrame() {
		FreePtrArr(data);
	}

	float             *data;
	int                size[3];
	VUtils::Transform  c2n;
};


class FluidCache
{
	typedef VUtils::HashMap<FluidFrame> FluidFrames;

public:
	FluidFrame  *getData(const char *filePath, const int fluidResample=32);

private:
	FluidFrames  m_frames;

};


class PhxShaderCache:
		public SOP::NodeBase
{
public:
	static PRM_Template      *GetPrmTemplate();

public:
	PhxShaderCache(OP_Network *parent, const char *name, OP_Operator *entry):NodeBase(parent, name, entry) {}
	virtual                  ~PhxShaderCache() {}

	virtual OP_NodeFlags     &flags() VRAY_OVERRIDE;
	virtual OP_ERROR          cookMySop(OP_Context &context) VRAY_OVERRIDE;
	virtual PluginResult      asPluginDesc(Attrs::PluginDesc &pluginDesc, VRayExporter *exporter, OP_Node *parent=nullptr) VRAY_OVERRIDE;

protected:
	virtual void              setPluginType() VRAY_OVERRIDE;

private:
	static FluidCache         FluidFiles;

};

} // namespace SOP
} // namespace VRayForHoudini

#endif // VRAY_FOR_HOUDINI_SOP_NODE_PHOENIX_CACHE_H
#endif // CGR_HAS_AUR
