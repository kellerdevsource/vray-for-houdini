//
// Copyright (c) 2015-2017, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#include "vfh_log.h"
#include "vfh_utils_vrmesh.h"

#include <UT/UT_Version.h>

#include <GU/GU_PrimPoly.h>
#include <GU/GU_Detail.h>
#include <GU/GU_PackedGeometry.h>
#include <GU/GU_PrimPacked.h>

using namespace VRayForHoudini;
using namespace VRayForHoudini::Mesh;


int VRayForHoudini::Mesh::createMeshProxyGeometry(VUtils::MeshInterface &mi, int voxelType, GU_DetailHandle &gdpHandle)
{
	GU_Detail *gdp = gdpHandle.writeLock();
	if (gdp) {
		GU_Detail *primGdp = new GU_Detail;

		const int numVoxels = mi.getNumVoxels();
		for (int voxelIdx = 0; voxelIdx < numVoxels; ++voxelIdx) {
			const uint32 meshVoxelFlags = mi.getVoxelFlags(voxelIdx);
			if (meshVoxelFlags & voxelType) {
				VoxelReleaseRAII voxelUnload(mi, voxelIdx);
				if (voxelUnload.isValid()) {
					VUtils::MeshVoxel &voxel = voxelUnload.getVoxel();

					VUtils::MeshChannel *vertsChan = voxel.getChannel(VERT_GEOM_CHANNEL);
					VUtils::MeshChannel *facesChan = voxel.getChannel(FACE_TOPO_CHANNEL);
					if (vertsChan && facesChan) {
						VUtils::VertGeomData *verts = reinterpret_cast<VUtils::VertGeomData*>(vertsChan->data);
						VUtils::FaceTopoData *faces = reinterpret_cast<VUtils::FaceTopoData*>(facesChan->data);
						if (verts && faces) {
							const int numVerts = vertsChan->numElements;
							const int numFaces = facesChan->numElements;

							if (numVerts && numFaces) {
								// Points
								const GA_Offset voffset = primGdp->appendPointBlock(numVerts);
								for (int v = 0; v < numVerts; ++v) {
									const VUtils::Vector &vert = verts[v];
									const GA_Offset pointOffs = voffset + v;

									primGdp->setPos3(pointOffs, UT_Vector3F(vert.x, vert.y, vert.z));
								}

								// Faces
								for (int f = 0; f < numFaces; ++f) {
									const VUtils::FaceTopoData &face = faces[f];

									GU_PrimPoly *poly = GU_PrimPoly::build(primGdp, 3, GU_POLY_CLOSED, 0);
									for (int c = 0; c < 3; ++c) {
										poly->setVertexPoint(c, voffset + face.v[c]);
									}

									poly->reverse();
								}
							}
						}
					}
				}
			}
		}

		GU_DetailHandle primGdpHandle;
		primGdpHandle.allocateAndSet(primGdp);

		GU_PackedGeometry::packGeometry(*gdp, primGdpHandle);

		gdpHandle.unlock(gdp);
	}

	return true;
}
