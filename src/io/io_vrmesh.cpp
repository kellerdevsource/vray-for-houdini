//
// Copyright (c) 2015-2016, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#include "io_vrmesh.h"

#include "vfh_log.h"
#include "vfh_utils_vrmesh.h"
#include "uni.h"

#include <GU/GU_Detail.h>
#include <GU/GU_PrimVolume.h>
#include <GEO/GEO_AttributeHandle.h>
#include <SOP/SOP_Node.h>
#include <UT/UT_Assert.h>


using namespace VRayForHoudini;
using namespace VRayForHoudini::IO;


const char *Vrmesh::extension = "vrmesh";


Vrmesh::Vrmesh(const Vrmesh &other)
{
	// TODO: If needed
}


GEO_IOTranslator* Vrmesh::duplicate() const
{
	return new Vrmesh(*this);
}


const char *Vrmesh::formatName() const
{
	return "V-Ray Proxy Format";
}


int Vrmesh::checkExtension(const char *name)
{
	UT_String sname(name);
	// NOTE: +1 to skip dot
	if (sname.fileExtension() && !strcmp(sname.fileExtension()+1, Vrmesh::extension)) {
		return true;
	}
	return false;
}


int Vrmesh::checkMagicNumber(unsigned magic)
{
	return 0;
}


GA_Detail::IOStatus Vrmesh::fileLoad(GEO_Detail *geo, UT_IStream &stream, bool /*ate_magic*/)
{
	const char *filepath = stream.getFilename();
	GU_Detail  *gdp = dynamic_cast<GU_Detail*>(geo);

	if (gdp &&
		filepath && *filepath &&
		VUtils::uniPathOrFileExists(filepath))
	{
		Log::getLog().info("Vrmesh::fileLoad(%s)", filepath);

		VUtils::MeshFile *proxy = VUtils::newDefaultMeshFile(filepath);
		if (proxy) {
			int res = proxy->init(filepath);
			if (res && proxy->getNumVoxels()) {
				if (proxy->getNumFrames()) {
					// TODO: proxy->setCurrentFrame(t);
				}

				const int numVoxels = proxy->getNumVoxels();

				// Search for compatible preview data:
				//  [x] Mesh
				//  [ ] Preview mesh
				//  [ ] Hair
				//  [ ] Particles
				//
				int previewVoxelIdx = -1;
				int numHairVoxels   = 0;
				int numGeomVoxels   = 0;
				for (int voxelIdx = 0; voxelIdx < numVoxels; ++voxelIdx) {
					const uint32 meshVoxelFlags = proxy->getVoxelFlags(voxelIdx);
					if (meshVoxelFlags & MVF_GEOMETRY_VOXEL) {
						numGeomVoxels++;
					}
					else if (meshVoxelFlags & MVF_PREVIEW_VOXEL) {
						previewVoxelIdx = voxelIdx;
					}
					else if (meshVoxelFlags & MVF_HAIR_GEOMETRY_VOXEL) {
						numHairVoxels++;
					}
				}

				if (previewVoxelIdx >= 0 ||
					numGeomVoxels ||
					numHairVoxels)
				{
					GU_DetailHandle gdpHandle;
					gdpHandle.allocateAndSet(gdp, false);

					if (numGeomVoxels) {
						Mesh::createMeshProxyGeometry(*proxy, MVF_GEOMETRY_VOXEL, gdpHandle);
					}
				}
			}

			VUtils::deleteDefaultMeshFile(proxy);
		}
	}

	return GA_Detail::IOStatus(true);
}


GA_Detail::IOStatus Vrmesh::fileSave(const GEO_Detail *, std::ostream &)
{
	return GA_Detail::IOStatus(true);
}
