//
// Copyright (c) 2015-2016, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#ifndef VRAY_FOR_HOUDINI_CMD_VRAYPROXY_H
#define VRAY_FOR_HOUDINI_CMD_VRAYPROXY_H

#include <CMD/CMD_Args.h>


namespace VRayForHoudini {
namespace CMD {

/// Option string containing command line arguments for vrayproxy hscript command
/// format is similar to the option parsing string of the standard C function getopt.
///
/// varyproxy usage:
/// vrayproxy -n filepath [-c] [-f] [-m] [-i] [-l] [t]
///           [-a animStart animEnd]
///           [-v velocityStart velocityEnd]
///           [-T previewType]
///           [-F maxPreviewFaces] [-H maxPreviewStrands]
///           [-X maxFacesPerVoxel]
///           [-P pointSize]
/// -n filepath (mandatory)
///     filepath for the .vrmesh file
/// -c (optional)
///     create non-existing dirs in filepath
///     default is off
/// -f (optional)
///     overwrite existing files
///     default is off
/// -m exportMode (optional)
///     exportMode = 0 - export each object in separate file
///     exportMode = 1 - export all objects in single file
///     default 1
/// -i (optional)
///     do not ignore hidden and templated geometry
///     default is off
/// -l (optional)
///     use last object only for preview geometry
///     default is off
/// -t (optional)
///     export geometry in world space coordinates
///     default is off
/// -a animStart animEnd (optional)
///     export frame range, animStart = strat frame, animEnd = end frame
///     default - current time only
/// -v velocityStart velocityEnd (optional)
///     export vertex velocity, velocityStart = start sample time in [0,1) range
///     velocityEnd = end sample time in (0,1] range
///     default - no velocity
/// -T previewType (optional)
///     specifies the simplification type for the preview geometry
///     previewType = 0 - SIMPLIFY_CLUSTERING
///     previewType = 1 - SIMPLIFY_COMBINED
///     previewType = 2 - SIMPLIFY_FACE_SAMPLING
///     previewType = 3 - SIMPLIFY_EDGE_COLLAPSE
///     default - SIMPLIFY_COMBINED
/// -F maxPreviewFaces (optional)
///     max number of faces for preview geometry voxel
///     default 100
/// -H maxPreviewStrands (optional)
///     max number of strands for preview geometry voxel
///     default 100
/// -X maxFacesPerVoxel (optional)
///     maxFacesPerVoxel - specifies max number of faces per voxel
///     default 0 - assume 1 voxel per mesh
/// -P pointSize (optional)
///     point cloud information will be computed and stored with each voxel in the file
///     pointSize -  average area covered by one point
///     default - no point cloud
///
const char* const vrayproxyFormat = "n:cfmia::ltv::T:F:H:X:P:";

/// Callback function for creating V-Ray proxy geometry - called when vrayproxy hscript command is called
/// @param[in] args - command arguments in the format above
/// @retval no return val
void vrayproxy(CMD_Args &args);

}
}

#endif // VRAY_FOR_HOUDINI_CMD_VRAYPROXY_H
