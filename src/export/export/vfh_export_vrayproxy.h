//
// Copyright (c) 2015-2017, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#ifndef VRAY_FOR_HOUDINI_EXPORT_VRAYPROXY_H
#define VRAY_FOR_HOUDINI_EXPORT_VRAYPROXY_H

#include "vfh_exporter.h"

#include <SOP/SOP_Node.h>

#include <simplifier.h>


namespace VRayForHoudini {

/// SOP node array type.
typedef UT_ValArray<SOP_Node*> SOPList;

/// VRayProxyExportOptions wraps all options necessary to export .vrmesh file(s).
/// "vrayproxy" hscript cmd and "V-Ray Proxy ROP" create and configure an instance
/// of this data structure and pass it to VRayProxyExporter
struct VRayProxyExportOptions {
	VRayProxyExportOptions();

	/// Return the .vrmesh filepath and adjust the filename if needed
	/// 1. append the sop path to the filename if separate objects
	///    should be exported into separate files (m_exportAsSingle == false)
	/// 2. append .vrmesh extension if missing
	/// @param sop[in] - the sop node being processed
	/// @retval filepath
	inline UT_String getFilepath(const SOP_Node &sop) const;

	/// Easy accessor to check if new content should be appended to the .vrmesh file
	/// @retval false if we are exporting single frame or the first frame of an animation
	///         true otherwise
	inline bool isAppendMode() const;

	/// filepath to the .vrmesh file
	/// cmd arg: -n "path/to/filename.vrmesh" (mandatory)
	UT_String m_filepath;

	/// (bool) if true, non-existing dirs in the filepath will be created
	/// default value: true
	/// cmd arg: -c (optional)
	int m_mkpath;

	/// (bool) if true, existing file(s) will be overwritten
	/// default value: false
	/// cmd arg: -f (optional)
	int m_overwrite;

	/// (bool) if true, export all geometry in a single .vrmesh file,
	/// otherwise export each object in separate file
	/// default value: true
	/// cmd arg: -m (optional, if given m_exportAsSingle will be set to false)
	int m_exportAsSingle;

	/// (bool) if true, export animation range in a single .vrmesh file
	/// otherwise only current frame will be exported
	/// default value: true
	/// cmd arg: -a animStart animEnd (optional)
	int m_exportAsAnimation;

	/// (fpreal) start time of the animation
	/// default value: 0
	/// cmd arg: -a animStart animEnd (mandatory, if m_exportAsAnimation is true)
	fpreal m_animStart;

	/// (fpreal) end time of the animation
	/// default value: 0
	/// cmd arg: -a animStart animEnd (mandatory, if m_exportAsAnimation is true)
	fpreal m_animEnd;

	/// (int) number of frames to be exported
	/// this is calculated based on m_animStart m_animEnd values
	/// default value: 0
	int m_animFrames;

	/// (OP_Context) current time at which to export geometry geometry
	/// default value: 0
	OP_Context m_context;

	/// (bool) if true, use last object in list as preview geometry
	/// this option is always false if exporting single object per file
	/// default value: false
	/// cmd arg: -l (optional)
	int m_lastAsPreview;

	/// (bool) if true, export geometry in world space
	/// default value: false
	/// cmd arg: -t (optional)
	int m_applyTransform;

	/// (bool) if true, export vertex velocity
	/// this option is always false if exportAnimation is false
	/// vertex velocity is calulated as: (vert_pos(velocityEndTime) - vert_pos(velocityStartTime)) / (velocityEndTime - velocityStartTime)
	/// default value: false
	/// cmd arg: -v velocityStart velocityEnd (optional)
	int m_exportVelocity;

	/// (float) start time for vertex position sample
	/// velocityStart should be in range [0,1)
	/// default value: 0.f
	/// cmd arg: -v velocityStart velocityEnd (mandatory, if exportVelocity is true)
	fpreal m_velocityStart;

	/// (float) end time for vertex position sample
	/// velocityEnd should be in range (0,1]
	/// default value: 0.05f
	/// cmd arg: -v velocityStart velocityEnd (mandatory, if exportVelocity is true)
	fpreal m_velocityEnd;

	/// (SimplificationType enum) specifies how to do the simplification
	/// default value: SIMPLIFY_COMBINED
	/// cmd arg: -T previewType (optional)
	VUtils::SimplificationType m_simplificationType;

	/// (int) max number of faces for preview geometry voxel
	/// default value: 100
	/// cmd arg: -F maxPreviewFaces (optional)
	int m_maxPreviewFaces;

	/// (int) max number of strands for preview geometry voxel
	/// default value: 100
	/// cmd arg: -H maxPreviewStrands (optional)
	int m_maxPreviewStrands;

	/// (int) max number of faces per voxel
	/// if 0, assume 1 voxel per mesh
	/// default value: 0
	/// cmd arg: -X maxFacesPerVoxel (optional)
	int m_maxFacesPerVoxel;

	/// (bool) if true, point cloud information will be computed and stored with each voxel in the file
	/// default value: false
	/// cmd arg: -P pointSize (optional)
	int m_exportPCLs;

	/// (float) specifies the desired density for the points in the point cloud -
	/// average area covered by one point
	/// default value: 0.5f
	/// cmd arg: -P pointSize (optional)
	fpreal m_pointSize;
};


/// VRayProxyExporter wraps the geometry export of a single frame into a single .vrmesh file
class VRayProxyExporter
{
public:
	/// Wrap the actual export of .vrmesh file(s) besed on given geometry and export options
	/// @param options[in] - options used to configure how geometry should be exported
	/// @param sopList[in] - list of geometry sop nodes. Note that options.m_context would be modified
	///                       if we are exporting animation
	/// @retval error code - use ErrorCode::error() to check for errors
	///                      and ErrorCode::getErrorString() to get the error message
	VUtils::ErrorCode doExport(VRayProxyExportOptions &options, const SOPList &sopList);

	/// Constructor
	/// @note at this point m_geomDescrList is only partially initilized and the number of voxels is determined
	/// @param options[in] - options used to configure how geometry should be exported
	/// @param sopList[in] - list of geometry sop nodes that need to be exported as proxy
	VRayProxyExporter(const VRayProxyExportOptions &options, const SOPList &sopList, ROP_Node *ropNode);

	/// Initilize the exporter for the current time (based on what is set in m_options.m_context)
	/// by caching out geometry data for all objects in m_geomDescrList
	/// @note this should be called for every animation frame before doExportFrame()
	/// @retval error code - use ErrorCode::error() to check for errors
	///                      and ErrorCode::getErrorString() to get the error message
	VUtils::ErrorCode init();

	/// Export data from renderer to .vrscene file and start ply2vrmesh on it
	/// @retval error code - use ErrorCode::error() to check for errors
	///                      and ErrorCode::getErrorString() to get the error message
	VUtils::ErrorCode convertData(float start, float end);

private:
	/// Input SOP list.
	const SOPList &sopList; ///< List of all SOP nodes that we need to export as proxies
	ROP_Node *m_rop; ///< Pointer to the Proxy ROP that called us
	const VRayProxyExportOptions &m_options; ///< Export options
	VRayExporter exporter; ///< The exporter used to generate the intermediate .vrscene

	VUTILS_DISABLE_COPY(VRayProxyExporter)
};

} // namespace VRayForHoudini

#endif // VRAY_FOR_HOUDINI_EXPORT_VRAYPROXY_H
