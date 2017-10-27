//
// Copyright (c) 2015-2017, Chaos Software Ltd
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

enum class VRaySessionType {
	vraySessionProduction = 0,
	vraySessionRT,
};

class VRayRendererNode
	: public ROP_Node
{
public:
	/// Provide access to the ROP parameter templates (custom + generic ROP ones)
	/// @returns pointer to a OP_TemplatePair instance
	/// might be nullptr (no parameters).
	static OP_TemplatePair* getTemplatePair();

	/// Provide access to the ROP variables
	/// @returns pointer to a OP_VariablePair instance
	/// might be nullptr (no variables)
	static OP_VariablePair* getVariablePair();

	/// Create new instance of V-Ray ROP
	/// @param parent[in] - parent network for the node
	/// @param name[in] - node name
	/// @param op[in] - operator type for the node
	/// @returns pointer to the newly created node
	static OP_Node *myConstructor(OP_Network *parent, const char *name, OP_Operator *entry);

	/// Starts RT rendering session.
	/// @param time Current time.
	void startRT(fpreal time);

protected:
	/// Hide constructor/destructor for this class
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
	/// the scene for the current time
	/// @param time[in] - current export time
	/// @param boss[in] - used to interrrupt the export process for long operations
	///        (if the user presses ESC for example)
	/// @returns one of three return codes
	///         ROP_ABORT_RENDER, ROP_CONTINUE_RENDER, ROP_RETRY_RENDER
	///         which tell Houdini whether to stop rendering, continue, or retry the frame
	ROP_RENDER_CODE renderFrame(fpreal time, UT_Interrupt *boss) VRAY_OVERRIDE;

	/// Called after the rendering of all frames is done. Finalizes the export and
	/// rendering and calls any post render scripts
	/// @returns one of three return codes to indicate the result of its operations
	///         ROP_ABORT_RENDER, ROP_CONTINUE_RENDER, ROP_RETRY_RENDER
	ROP_RENDER_CODE endRender() VRAY_OVERRIDE;

	/// Initialize a rendering session - intitlize the renderer, setup DR (if enabled)
	/// init the exporter and export global rendering settings.
	/// @param interactive[in] - start rendering in IPR mode (we always use RT GPU engine for IPR)
	/// @param nframes[in] - number of frames to be exported
	/// @param tstart[in] - start time of export
	/// @param tend[in] - end time of export
	/// @returns ROP_RENDER_CODE as int
	int initSession(int interactive, int nframes, fpreal tstart, fpreal tend);

private:
	/// Scene exporter.
	VRayExporter m_exporter;

	/// Start render time.
	fpreal m_tstart;

	/// End render time.
	fpreal m_tend;

public:
	/// Register this node type as ROP operator
	/// @param table[out] - ponter to the operator table for ROP contex
	static void register_operator(OP_OperatorTable *table);

	/// IPR callback to track changes on the ROP node.
	static void RtCallbackRop(OP_Node *caller, void *callee, OP_EventType type, void *data);

	/// Callback for the "Render RT" button on the ROP node.
	/// This will start the renderer in IPR mode.
	static int RtStartSession(void *data, int index, fpreal t, const PRM_Template *tplate);

	/// Callback for the "Show VFB" button on the ROP node
	/// Shows VFB window if there is one
	static int RendererShowVFB(void *data, int index, fpreal t, const PRM_Template *tplate);
};

} // namespace VRayForHoudini

#endif // VRAY_FOR_HOUDINI_ROP_NODE_H
