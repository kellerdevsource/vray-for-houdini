//
// Copyright (c) 2015-2016, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//


#ifndef VRAY_FOR_HOUDINI_VFH_ATTR_UTILS_H
#define VRAY_FOR_HOUDINI_VFH_ATTR_UTILS_H

#include <OP/OP_Node.h>
#include <OP/OP_Director.h>

#include <systemstuff.h>

/// Returns OP_Node from path.
/// @param path Path. May be changed if path has "op:/" syntax.
/// @param t Time.
/// @returns OP_Node instance or NULL.
FORCEINLINE OP_Node *getOpNodeFromPath(UT_String &path, fpreal t=0.0)
{
	if (path.startsWith(OPREF_PREFIX)) {
		int op_id = 0;
		fpreal op_time = 0.0;

		OPgetDirector()->evalOpPathString(path, 0, 0, t, op_id, op_time);
	}

	return OPgetDirector()->findNode(path.buffer());
}

/// Returns OP_Node from path.
/// @param path Path.
/// @param t Time.
/// @returns OP_Node instance or NULL.
FORCEINLINE OP_Node *getOpNodeFromPath(const UT_String &path, fpreal t=0.0)
{
	UT_String newPath(path);
	return getOpNodeFromPath(newPath, t);
}

#endif // VRAY_FOR_HOUDINI_VFH_ATTR_UTILS_H
