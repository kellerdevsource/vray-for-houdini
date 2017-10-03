//
// Copyright (c) 2015-2017, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#ifndef VRAY_FOR_HOUDINI_ROP_NODE_VRAYPROXYROP_H
#define VRAY_FOR_HOUDINI_ROP_NODE_VRAYPROXYROP_H

#include "vfh_vray.h"
#include "vfh_export_vrayproxy.h"

#include <ROP/ROP_Node.h>

namespace VRayForHoudini {

class VRayProxyROP
	: public ROP_Node
{
public:
	/// Provide access to VRayProxyROP custom parameter templates
	/// @retval pointer to a terminated PRM_Template list
	///         might be nullptr (no parameters)
	static PRM_Template * getMyPrmTemplate();

	/// Provide access to the ROP parameter templates (custom + generic ROP ones)
	/// @retval pointer to a OP_TemplatePair instance
	///         might be nullptr (no parameters)
	static OP_TemplatePair * getTemplatePair();

	/// Provide access to the ROP variables
	/// @retval pointer to a OP_VariablePair instance
	///         might be nullptr (no variables)
	static OP_VariablePair * getVariablePair();

	/// Register this node type as ROP operator
	/// @param table[out] - ponter to the operator table for ROP contex
	static void register_ropoperator(OP_OperatorTable *table);

	/// Register this node type as SOP operator
	/// @param table[out] - ponter to the operator table for SOP contex
	static void register_sopoperator(OP_OperatorTable *table);

	/// Create new instances of VRayProxyROP
	/// @param parent[in] - parent network for the node
	/// @param name[in] - node name
	/// @param op[in] - operator type for the node
	/// @retval pointer to the newly created node
	static OP_Node * creator(OP_Network *parent, const char *name, OP_Operator *op);

protected:
	/// Hide constructor for this class
	/// instances should only be created through creator() method
	/// @param parent[in] - parent network for the node
	/// @param name[in] - node name
	/// @param op[in] - operator type for the node
	VRayProxyROP(OP_Network *parent, const char *name, OP_Operator *op);

	/// Hide destructor for this class
	virtual ~VRayProxyROP();

	/// Called before the rendering begins to verify and initialize export options
	/// and geometry to export, create parent directories if necessary
	/// and execute prerender scripts
	/// @param nframes[in] - number of frames to be exported
	/// @param tstart[in] - start time of export
	/// @param tend[in] - end time of export
	/// @retval return false if the rendering process is aborted, true otherwise
	int startRender(int nframes, fpreal tstart, fpreal tend) VRAY_OVERRIDE;

	/// Called for each frame, executes per frame scripts
	/// @param time[in] - current export time
	/// @param boss[in] - used to interrrupt the export process for long operations
	///                   (if the user presses ESC for example)
	/// @retval one of three return codes
	///         ROP_ABORT_RENDER, ROP_CONTINUE_RENDER, ROP_RETRY_RENDER
	///         which tell Houdini whether to stop rendering, continue, or retry the frame
	ROP_RENDER_CODE renderFrame(fpreal time, UT_Interrupt *boss = 0) VRAY_OVERRIDE;

	/// Called after the rendering of all frames is done
	/// @retval one of three return codes to indicate the result of its operations
	///         ROP_ABORT_RENDER, ROP_CONTINUE_RENDER, ROP_RETRY_RENDER
	ROP_RENDER_CODE endRender() VRAY_OVERRIDE;

private:
	/// Return geometry sop nodes that should be exported
	/// @note called from startRender() only i.e. geometry can not vary per frame
	/// @param time[in] - current export time
	/// @param sopList[out] - return geometry sop nodes that should be exported
	/// @retval number of sops found
	int getSOPList(fpreal time, SOPList &sopList);

	/// Return export options at given time
	/// @note called from renderFrame() as some of the options might vary per frame
	/// @param time[in] - current export time
	/// @param options[out] - export options
	/// @retval true if no errors occur
	int getExportOptions(fpreal time, VRayProxyExportOptions &options) const;

	VUtils::ErrorCode doExport(const SOPList &sopList);

	VRayProxyExportOptions  m_options; ///< export options to configure .vrmesh exporter
	SOPList m_sopList; ///< list with geometry that should be exported

	VUTILS_DISABLE_COPY(VRayProxyROP)
};

} // namespace VRayForHoudini

#endif // VRAY_FOR_HOUDINI_ROP_NODE_VRAYPROXYROP_H
