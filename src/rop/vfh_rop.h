//
// Copyright (c) 2015-2018, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#ifndef VRAY_FOR_HOUDINI_ROP_NODE_H
#define VRAY_FOR_HOUDINI_ROP_NODE_H

#include "vfh_exporter.h"

#include <ROP/ROP_Node.h>

class OP_TemplatePair;
class OP_VariablePair;

namespace VRayForHoudini {

class VRayRendererNode
	: public ROP_Node
{
public:
	static OP_TemplatePair *getTemplatePair();
	static OP_VariablePair *getVariablePair();
	static OP_Node *myConstructor(OP_Network *parent, const char *name, OP_Operator *entry);

	/// Starts RT rendering session.
	/// @param time Current time.
	void startRenderRT(fpreal time);

	/// Shows V-Ray Frame Buffer window (if exists).
	void showVFB();

	/// Applies which take to use. Used for RT sessions.
	/// @param take Take name.
	/// @returns TAKE_Take instance or NULL.
	TAKE_Take *applyTake(const char *take);

	/// Restores "system" take. Used for RT sessions.
	/// @param take Take instance.
	void restoreTake(TAKE_Take *take);

	/// Called when ROP "take" is changed during the RT session.
	void onRtTakeChange();

protected:
	VRayRendererNode(OP_Network *net, const char *name, OP_Operator *entry);
	virtual ~VRayRendererNode();

	/// Called before the rendering begins to verify Houdini license,
	/// initialize a rendering session and call any prerender scripts.
	/// @param nframes Number of frames to be exported
	/// @param tstart Start time of export
	/// @param tend End time of export
	/// @returns False if the rendering process is aborted, true otherwise.
	int startRender(int nframes, fpreal tstart, fpreal tend) VRAY_OVERRIDE;

	/// Called for each frame, executes per frame scripts and calls the exporter to export
	/// the scene for the current time.
	/// @param time Frame time.
	/// @param boss Used to interrrupt the export process for long operations
	/// (if the user presses ESC for example).
	/// @returns ROP_RENDER_CODE.
	ROP_RENDER_CODE renderFrame(fpreal time, UT_Interrupt *boss) VRAY_OVERRIDE;

	/// Called after the rendering of all frames is done. Finalizes the export and
	/// rendering and calls any post render scripts
	/// @returns ROP_RENDER_CODE.
	ROP_RENDER_CODE endRender() VRAY_OVERRIDE;

	/// Initialize a rendering session: renderer, DR (if enabled),
	/// exporter and export global rendering settings, etc.
	/// @param sessionType Session type.
	/// @param nframes Number of frames to export.
	/// @param tstart Start export time.
	/// @param tend End export time.
	/// @returns ROP_RENDER_CODE as int
	int initSession(VfhSessionType sessionType, int nframes, fpreal tstart, fpreal tend);

private:
	/// Scene exporter.
	VRayExporter m_exporter;

	/// Start render time.
	fpreal m_tstart;

	/// End render time.
	fpreal m_tend;

public:
	/// Registers VRayRendererNode node type as a ROP operator.
	/// @param table A pointer to the operator table for the ROP context.
	static void register_operator(OP_OperatorTable *table);

	/// IPR callback to track changes on the ROP node.
	static void RtCallbackRop(OP_Node *caller, void *callee, OP_EventType type, void *data);
};

} // namespace VRayForHoudini

#endif // VRAY_FOR_HOUDINI_ROP_NODE_H
