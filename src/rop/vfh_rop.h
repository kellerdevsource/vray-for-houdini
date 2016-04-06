//
// Copyright (c) 2015-2016, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#ifndef VRAY_FOR_HOUDINI_ROP_NODE_H
#define VRAY_FOR_HOUDINI_ROP_NODE_H

#include "vfh_defines.h"
#include "vfh_vray.h"
#include "vfh_exporter.h"

#include <ROP/ROP_Node.h>

class OP_TemplatePair;
class OP_VariablePair;

namespace VRayForHoudini {

class VRayRendererNode:
		public ROP_Node
{
public:
	static OP_TemplatePair      *getTemplatePair();
	static OP_Node              *myConstructor(OP_Network *net, const char*name, OP_Operator *op) { return new VRayRendererNode(net, name, op); }

	virtual bool                 updateParmsFlags() VRAY_OVERRIDE;

protected:
	VRayRendererNode(OP_Network *net, const char *name, OP_Operator *entry);
	virtual                     ~VRayRendererNode();

	virtual int                  startRender(int nframes, fpreal s, fpreal e) VRAY_OVERRIDE;
	virtual ROP_RENDER_CODE      renderFrame(fpreal time, UT_Interrupt *boss) VRAY_OVERRIDE;
	virtual ROP_RENDER_CODE      endRender() VRAY_OVERRIDE;

	int                          initSession(int interactive, int nframes, fpreal tstart, fpreal tend);
	void                         startIPR();

private:
	VRayExporter                 m_exporter;
	fpreal                       m_tend;

public:
	static void                  register_operator(OP_OperatorTable *table);
	static void                  RtCallbackRop(OP_Node *caller, void *callee, OP_EventType type, void *data);
	static int                   RtStartSession(void *data, int index, float t, const PRM_Template *tplate);
};

} // namespace VRayForHoudini

#endif // VRAY_FOR_HOUDINI_ROP_NODE_H
