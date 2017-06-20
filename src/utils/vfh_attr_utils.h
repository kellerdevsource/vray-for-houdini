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

/// Converts M3 to M4 leaving the offset 0.0.
/// @param m3 UT_Matrix3T matrix.
template <typename S>
FORCEINLINE UT_Matrix4T<S> toM4(const UT_Matrix3T<S> &m3) {
	UT_Matrix4T<S> m4;
    m4[0][0]=m3(0,0); m4[0][1]=m3(0,1); m4[0][2]=m3(0,2); m4[0][3]=static_cast<S>(0.);
    m4[1][0]=m3(1,0); m4[1][1]=m3(1,1); m4[1][2]=m3(1,2); m4[1][3]=static_cast<S>(0.);
    m4[2][0]=m3(2,0); m4[2][1]=m3(2,1); m4[2][2]=m3(2,2); m4[2][3]=static_cast<S>(0.);
	m4[3][0]=static_cast<S>(0.);
	m4[3][1]=static_cast<S>(0.);
	m4[3][2]=static_cast<S>(0.);
	m4[3][3]=static_cast<S>(1.);
    return m4;
}

/// Convert from Houdini transform to VRay::Transform
/// @param m Houdini matrix.
/// @param flip Axis flip flag.
template <typename UT_MatrixType>
FORCEINLINE VRay::Transform utMatrixToVRayTransform(const UT_MatrixType &m, bool flip=false) {
	VRay::Transform tm;
	for (int i = 0; i < 3; ++i) {
		tm.matrix[i].set(m[i][0], m[i][1], m[i][2]);
	}
	if (m.tuple_size == 16) {
		tm.offset.set(m[3][0], m[3][1], m[3][2]);
	}
	if (flip) {
		VUtils::swap(tm.matrix[1], tm.matrix[2]);
	}
	return tm;
}

#endif // VRAY_FOR_HOUDINI_VFH_ATTR_UTILS_H
