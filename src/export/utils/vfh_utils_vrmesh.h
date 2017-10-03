//
// Copyright (c) 2015-2017, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#ifndef VRAY_FOR_HOUDINI_UTILS_VRMESH_H
#define VRAY_FOR_HOUDINI_UTILS_VRMESH_H

#include "vfh_vray.h"

#include "GU/GU_DetailHandle.h"
#include "GEO/GEO_Detail.h"

namespace VRayForHoudini {
namespace Mesh {

/// Helper RAII structure to bind the life cycle of a MeshVoxel
/// to the lifetime of an instance of this class.
struct VoxelReleaseRAII {
	VoxelReleaseRAII(VUtils::MeshInterface &meshIface, int voxelIndex)
		: m_meshIface(meshIface)
		, m_meshVoxel(nullptr)
	{
		m_meshVoxel = m_meshIface.getVoxel(voxelIndex);
	}

	~VoxelReleaseRAII() {
		if (m_meshVoxel) {
			m_meshIface.releaseVoxel(m_meshVoxel);
		}
	}

	int                    isValid() const { return m_meshVoxel != nullptr; }
	VUtils::MeshVoxel     &getVoxel()      { return *m_meshVoxel; }

private:
	VUtils::MeshInterface &m_meshIface;
	VUtils::MeshVoxel     *m_meshVoxel;

	VUTILS_DISABLE_COPY(VoxelReleaseRAII);
};


/// Create mesh geometry from voxels of specified type from a V-Ray proxy file
/// @param mi[in] - reference V-Ray proxy file interface. Note that mi may be
///        modified if this is the first time to access its voxels.
/// @param voxelType[in] - geometry will be created only for voxels
///        matching this type
/// @param gdpHandle[out] - write lock handle, guarding the actual output gdp
int createMeshProxyGeometry(VUtils::MeshInterface &mi, int voxelType, GU_DetailHandle &gdpHandle);

} // namespace Mesh
} // namespace VRayForHoudini

#endif // VRAY_FOR_HOUDINI_UTILS_VRMESH_H
