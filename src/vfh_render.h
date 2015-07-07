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
	// NOTE: Keep in sync with "render_export_mode"
	enum ExpWorkMode {
		ExpRender = 0,
		ExpExportRender,
		ExpExport,
	};

public:
	/// Provides access to our parm templates.
	static OP_TemplatePair      *getTemplatePair();
	/// Provides access to our variables.
	static OP_VariablePair      *getVariablePair();
	/// Creates an instance of this node.
	static OP_Node              *myConstructor(OP_Network *net, const char*name, OP_Operator *op) { return new VRayRendererNode(net, name, op); }

	virtual bool                 updateParmsFlags() VRAY_OVERRIDE;

protected:
	VRayRendererNode(OP_Network *net, const char *name, OP_Operator *entry);
	virtual                     ~VRayRendererNode();

	virtual int                  startRender(int nframes, fpreal s, fpreal e) VRAY_OVERRIDE;
	virtual ROP_RENDER_CODE      renderFrame(fpreal time, UT_Interrupt *boss) VRAY_OVERRIDE;
	virtual ROP_RENDER_CODE      endRender() VRAY_OVERRIDE;

private:
	VRayExporter                 m_exporter;

	int                          m_frames;
	fpreal                       m_time_end;
	ROP_RENDER_CODE              m_error;

public:
	static void                  register_operator(OP_OperatorTable *table);

public:
	static void                  RtCallbackRop(OP_Node *caller, void *callee, OP_EventType type, void *data);

};

} // namespace VRayForHoudini

#endif // VRAY_FOR_HOUDINI_ROP_NODE_H
