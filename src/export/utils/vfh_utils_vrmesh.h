//
// Copyright (c) 2016, Chaos Software Ltd
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

int createMeshProxyGeometry(VUtils::MeshInterface &mi, int voxelType, GU_DetailHandle &gdpHandle);

} // namespace Mesh
} // namespace VRayForHoudini

#endif // VRAY_FOR_HOUDINI_UTILS_VRMESH_H
