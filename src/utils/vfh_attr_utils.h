//
// Copyright (c) 2015-2018, Chaos Software Ltd
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

#include "vfh_vray.h"

const char VFH_ATTR_MATERIAL_ID[] = "switchmtl";
const char VFH_ATTR_MATERIAL_STYLESHEET[] = "material_stylesheet";
const char VFH_ATTR_MATERIAL_OVERRIDE[] = "material_override";
const char VFH_ATTRIB_TRANSFORM[] = "transform";
const char VFH_ATTR_SHOP_MATERIAL_STYLESHEET[] = "shop_materialstylesheet";

const char VFH_ATTRIB_VRAY_OBJECTID[] = "vray_objectID";
const char VFH_ATTRIB_VRAY_ANIM_OFFSET[] = "vray_anim_offset";

/// Resolves OP_Node path. 
/// @param node Attribute owner.
/// @param attrName Attribute name.
/// @param t Time.
/// @returns Resolved path.
UT_String getOpPathFromAttr(const OP_Node &node, const char *attrName, fpreal t=0.0);

/// Resolves OP_Node path. 
/// @param node Attribute owner.
/// @param attrName Attribute name.
/// @param t Time.
/// @returns Resolved path.
UT_String getOpPathFromAttr(const OP_Node &node, const QString &attrName, fpreal t=0.0);

/// Resolves OP_Node from path. 
/// @param node Attribute owner.
/// @param path Path.
/// @param t Time.
/// @returns OP_Node instance or NULL.
OP_Node* getOpNodeFromPath(const OP_Node &node, const char *path, fpreal t=0.0);

/// Resolves OP_Node from path attribute. 
/// @param node Attribute owner.
/// @param attrName Attribute name.
/// @param t Time.
/// @returns OP_Node instance or NULL.
OP_Node* getOpNodeFromAttr(const OP_Node &node, const char *attrName, fpreal t=0.0);

/// Resolves OBJ_Node from path attribute. 
/// @param node Attribute owner.
/// @param attrName Attribute name.
/// @param t Time.
/// @returns OBJ_Node instance or NULL.
OBJ_Node *getOBJNodeFromAttr(const OP_Node &node, const char *attrName, fpreal t = 0.0);

/// Resolves SOP_Node from path attribute. 
/// @param node Attribute owner.
/// @param attrName Attribute name.
/// @param t Time.
/// @returns SOP_Node instance or NULL.
SOP_Node *getSOPNodeFromAttr(const OP_Node &node, const char *attrName, fpreal t = 0.0);

/// Resolves VOP_Node from path attribute. 
/// @param node Attribute owner.
/// @param attrName Attribute name.
/// @param t Time.
/// @returns VOP_Node instance or NULL.
VOP_Node *getVOPNodeFromAttr(const OP_Node &node, const char *attrName, fpreal t = 0.0);

/// Resolves COP2_Node from path attribute. 
/// @param node Attribute owner.
/// @param attrName Attribute name.
/// @param t Time.
/// @returns COP2_Node instance or NULL.
COP2_Node *getCOP2NodeFromAttr(const OP_Node &node, const char *attrName, fpreal t = 0.0);

/// Resolves SHOP_Node from path attribute. 
/// @param node Attribute owner.
/// @param attrName Attribute name.
/// @param t Time.
/// @returns SHOP_Node instance or NULL.
SHOP_Node *getSHOPNodeFromAttr(const OP_Node &node, const char *attrName, fpreal t = 0.0);

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
