//
// Copyright (c) 2015-2017, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#ifndef VRAY_FOR_HOUDINI_VOP_PHOENIX_SIM_H
#define VRAY_FOR_HOUDINI_VOP_PHOENIX_SIM_H

#ifdef CGR_HAS_AUR

#include <vfh_vray.h>
#include "vop_node_base.h"

#include "ramps.h"
#include <vector>
#include <boost/unordered_map.hpp>

namespace VRayForHoudini {
namespace VOP {

struct RampContext;

class PhxShaderSim:
		public VOP::NodeBase
{
public:
	enum RenderMode {
		Volumetric  = 0,
		Volumetric_Geometry  = 1,
		Volumetric_Heat_Haze  = 2,
		Isosurface  = 3,
		Mesh  = 4,
	};

	static PRM_Template       *GetPrmTemplate();

	                           PhxShaderSim(OP_Network *parent, const char *name, OP_Operator *entry);
	virtual                   ~PhxShaderSim() {}

	/// Called by Houdini when all nodes are loaded, this parses UI and sets proper state on m_ramps
	virtual void               finishedLoadingNetwork(bool is_child_call=false) VRAY_OVERRIDE;

	/// Called by Houdini when scene is saved
	OP_ERROR                   saveIntrinsic(std::ostream &os, const OP_SaveFlags &sflags) VRAY_OVERRIDE;
	/// Called by Houdini on each packet in the saved scene, we parse the one we saved with saveIntrinsic
	bool                       loadPacket(UT_IStream &is, const char *token, const char *path) VRAY_OVERRIDE;

	/// Called by Houdini when it saves presets, so we save current ramps data
	bool                       savePresetContents(std::ostream &os) VRAY_OVERRIDE;
	/// Called by Houdini on each packet in preset and we only load the one saved with savePresetContents
	bool                       loadPresetContents(const char *tok, UT_IStream &is) VRAY_OVERRIDE;

	virtual PluginResult       asPluginDesc(Attrs::PluginDesc &pluginDesc, VRayExporter &exporter, ExportContext *parentContext=nullptr) VRAY_OVERRIDE;

	/// Load data ranges from the selectedSopPath's value
	void                       loadDataRanges();
protected:
	/// Clear all ramp's points
	void                       clearRampData();

	/// Set the *non* preset defaults for all ramps in sim
	void                       setRampDefaults();

	/// Used as callback for when the current preset field is set to update ramp data accordingly
	/// @param data - pointer to OP_Node that called the callback
	/// @param index - the index of the selected option [1, count)
	/// @param time - the time that the change was made
	/// @param tplate - the param template that this was triggered for
	/// @retval 1 if houdini should refresh the UI
	static int                 setPresetTypeCB(void *data, int index, fpreal64 time, const PRM_Template *tplate);

	/// Used as callback for when channel dropdown is changed. It sets the active channel for the appropriate ramp
	/// @param data - pointer to OP_Node that called the callback
	/// @param index - the index of the selected option [1, count)
	/// @param time - the time that the change was made
	/// @param tplate - the param template that this was triggered for
	/// @retval 1 if houdini should refresh the UI
	static int                 rampDropDownDependCB(void * data, int index, fpreal64 time, const PRM_Template *tplate);

	/// Called when user clicks on button for ramp, this should open the UI if it is not yet open
	/// @param data - pointer to OP_Node that called the callback
	/// @param index - the index of the selected option [1, count)
	/// @param time - the time that the change was made
	/// @param tplate - the param template that this was triggered for
	/// @retval 1 if houdini should refresh the UI
	static int                 rampButtonClickCB(void *data, int index, fpreal64 time, const PRM_Template *tplate);

	/// Called when 'selectedSopPath' is changed so we can update data ranges appropriatelly
	/// @param data - pointer to OP_Node that called the callback
	/// @param index - the index of the selected option [1, count)
	/// @param time - the time that the change was made
	/// @param tplate - the param template that this was triggered for
	/// @retval 1 if houdini should refresh the UI
	static int                 setVopPathCB(void *data, int index, fpreal64 time, const PRM_Template *tplate);

	/// Set the current active channels for all ramps
	/// @param fromUi - if true this takes the values from the current UI, otherwise uses the default from .ds file.
	///                 This is true when the scene is loaded from file and we need to parse the loaded channels
	void                       onLoadSetActiveChannels(bool fromUi);

	/// Write ramp data to ostream *wuthot* writing the packet name
	/// @retval - true on success
	bool                       saveRamps(std::ostream & os);
	/// Read ramp data from UT_IStream
	bool                       loadRamps(UT_IStream & is);

	/// Maps property name to ramp data, but since we can have a curve and color ramp in same window
	/// Some properties might map to one context
	boost::unordered_map<std::string, std::shared_ptr<RampContext>> m_ramps;
	/// Maps property name to ramp type, so we know what data to get from RampContext
	boost::unordered_map<std::string, AurRamps::RampType>           m_rampTypes;

	virtual void               setPluginType() VRAY_OVERRIDE;
};


} // namespace VOP
} // namespace VRayForHoudini

#endif // CGR_HAS_AUR

#endif // VRAY_FOR_HOUDINI_VOP_PHOENIX_SIM_H

