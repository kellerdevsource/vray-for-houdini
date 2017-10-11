//
// Copyright (c) 2015-2017, Chaos Software Ltd
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

#include "vfh_vray.h"

const char VFH_ATTR_MATERIAL_ID[] = "switchmtl";
const char VFH_ATTR_MATERIAL_STYLESHEET[] = "material_stylesheet";
const char VFH_ATTR_MATERIAL_OVERRIDE[] = "material_override";
const char VFH_ATTRIB_TRANSFORM[] = "transform";
const char VFH_ATTRIB_OBJECTID[] = "vray_objectID";
const char VFH_ATTRIB_ANIM_OFFSET[] = "anim_offset";
const char VFH_ATTR_SHOP_MATERIAL_STYLESHEET[] = "shop_materialstylesheet";

/// Expands OP_Node path.
/// @param path Path. May be changed if path has "op:/" syntax.
/// @param t Time.
FORCEINLINE void expandOpNodePath(UT_String &path, fpreal t=0.0)
{
	if (path.startsWith(OPREF_PREFIX)) {
#if 0
		int op_id = 0;
		fpreal op_time = 0.0;
		OPgetDirector()->evalOpPathString(path, 0, 0, t, op_id, op_time);
#endif
	}
}

/// Returns OP_Node from path.
/// @param path Path. May be changed if path has "op:/" syntax.
/// @param t Time.
/// @returns OP_Node instance or NULL.
FORCEINLINE OP_Node *getOpNodeFromPath(UT_String &path, fpreal t=0.0)
{
	expandOpNodePath(path, t);
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

#define getCastOpNodeFromPath(PREFIX) \
/*! Returns PREFIX##_Node from path. \
 * @param path Path. \
 * @param t Time. \
 * @returns PREFIX##_Node instance or NULL. \
 */ \
FORCEINLINE PREFIX##_Node *get##PREFIX##NodeFromPath(const UT_String &path, fpreal t=0.0) \
{ \
	return CAST_##PREFIX##NODE(getOpNodeFromPath(path, t)); \
}

getCastOpNodeFromPath(OBJ)
getCastOpNodeFromPath(SOP)
getCastOpNodeFromPath(VOP)
getCastOpNodeFromPath(COP2)

/// Converts M3 to M4 leaving the offset 0.0.
/// @param m3 UT_Matrix3T matrix.
template <typename S>
FORCEINLINE UT_Matrix4T<S> toM4(const UT_Matrix3T<S> &m3)
{
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
FORCEINLINE VRay::Transform utMatrixToVRayTransform(const UT_MatrixType &m, bool flip=false)
{
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

/// Convert from Houdini vector to VRay::Vector
/// @param v Houdini vector.
template <typename UT_VectorType>
FORCEINLINE VRay::Vector utVectorVRayVector(const UT_VectorType &v)
{
	VRay::Vector vec;
	vec.set(static_cast<float>(v(0)), static_cast<float>(v(1)), static_cast<float>(v(2)));
	return vec;
}

#endif // VRAY_FOR_HOUDINI_VFH_ATTR_UTILS_H
