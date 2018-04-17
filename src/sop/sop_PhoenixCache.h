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

namespace VRayForHoudini {
namespace SOP {

class PhxShaderCache
	: public NodePackedBase
{
public:
	/// Fills @param choicenames with the Phoenix channels names
	static void channelsMenuGenerator(void *data, PRM_Name *choicenames, int listsize, const PRM_SpareData *spare, const PRM_Parm *parm);
	/// Returns the parms of this SOP
	static PRM_Template* getPrmTemplate();

	PhxShaderCache(OP_Network *parent, const char *name, OP_Operator *entry);

protected:
	// From VRayNode.
	void setPluginType() VRAY_OVERRIDE;

	// From NodePackedBase.
	void setTimeDependent() VRAY_OVERRIDE;
	void updatePrimitive(const OP_Context &context) VRAY_OVERRIDE;

private:
	/// Get the channels names for the file "cache_path" in moment @param t
	/// @param t Time
	const UT_StringArray& getChannelsNames(fpreal t) const;

	/// Get the channels mapping for the file "cache_path" in moment @param t
	/// @param t Time
	UT_String getChannelsMapping(fpreal t);

	/// Compares if the value of "cache_path" is the same in m_primOptions as in @param options
	bool isSamePath(const OP_Options& options) const;

	/// Evaluates the "cache_path" parm in the specified time
	/// @param t Time
	/// @param sequencePath Removes $F substring from  "cache_path". If true replaces it with Phoenix sequence pattern("###"), else with the current frame.
	/// @retval UT_StringHolder with the cache_path value
	UT_StringHolder evalCachePath(fpreal t, bool sequencePath) const;

	/// Get current cache frame based on current frame + cache play settings
	int evalCacheFrame(fpreal t) const;

	mutable bool m_pathChanged; ///< True if the cache_path is changed
	mutable UT_StringArray m_phxChannels; ///< The names of the channels in the cache file
};

} // namespace SOP
} // namespace VRayForHoudini

#endif // CGR_HAS_AUR
#endif // VRAY_FOR_HOUDINI_SOP_NODE_PHOENIX_CACHE_H
