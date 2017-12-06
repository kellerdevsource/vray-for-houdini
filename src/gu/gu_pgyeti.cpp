//
// Copyright (c) 2015-2017, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#include <SYS/SYS_Types.h>

#include "vfh_defines.h"
#include "vfh_log.h"
#include "gu_pgyeti.h"

#include <GU/GU_PackedFactory.h>
#include <GU/GU_PrimPacked.h>
#include <GU/GU_PrimPoly.h>
#include <GU/GU_PackedContext.h>
#include <GU/GU_PackedGeometry.h>

#include <FS/UT_DSO.h>

#define hdf5_cpp_EXPORTS
#undef VERSION
#include <H5Cpp.h>

using namespace VRayForHoudini;

static GA_PrimitiveTypeId theTypeId(-1);

static class VRayPgYetiFactory
	: public GU_PackedFactory
{
public:
	VRayPgYetiFactory()
		: GU_PackedFactory("VRayPgYetiRef", "VRayPgYetiRef")
	{
		VRayPgYetiRef::registerIntrinsics<VRayPgYetiRef>(*this);
	}

	GU_PackedImpl* create() const VRAY_OVERRIDE {
		return new VRayPgYetiRef();
	}

	VUTILS_DISABLE_COPY(VRayPgYetiFactory)
} theFactory;


void VRayPgYetiRef::install(GA_PrimitiveFactory *gafactory)
{
	if (theFactory.isRegistered()) {
		Log::getLog().debug("Multiple attempts to install packed primitive %s from %s",
			static_cast<const char *>(theFactory.name()), UT_DSO::getRunningFile());
		return;
	}

	GU_PrimPacked::registerPacked(gafactory, &theFactory);
	if (NOT(theFactory.isRegistered())) {
		Log::getLog().error("Unable to register packed primitive %s from %s",
			static_cast<const char *>(theFactory.name()), UT_DSO::getRunningFile());
		return;
	}

	theTypeId = theFactory.typeDef().getId();
}

VRayPgYetiRef::VRayPgYetiRef()
	: VRayPgYetiRefOptions()
{}

VRayPgYetiRef::VRayPgYetiRef(const VRayPgYetiRef &src)
	: VRayPgYetiRefOptions(src)
{}

VRayPgYetiRef::~VRayPgYetiRef()
{}

GA_PrimitiveTypeId VRayPgYetiRef::typeId()
{
	return theTypeId;
}

GU_PackedFactory *VRayPgYetiRef::getFactory() const
{
	return &theFactory;
}

GU_PackedImpl *VRayPgYetiRef::copy() const
{
	return new VRayPgYetiRef(*this);
}

bool VRayPgYetiRef::unpack(GU_Detail&) const
{
	return true;
}

static int yetiIsFurGroup(const H5::Group &group)
{
	int hasVertex = false;
	int hasStrands = false;

	const int numObjects = group.getNumObjs();
	for (int i = 0; i < numObjects; ++i) {
		const H5G_obj_t itemType = group.getObjTypeByIdx(i);
		switch (itemType) {
			case H5G_GROUP: {
				const H5std_string &itemName = group.getObjnameByIdx(i);
				if (itemName == "P") {
					hasVertex = true;
				}
				else if (itemName == "numFaceVertices") {
					hasStrands = true;
				}
				break;
			}
			default:
				break;
		}

		if (hasVertex && hasStrands)
			return true;
	}

	return false;
}

static void buildHairDetailFromGroup(const H5::Group &furGroup, GU_Detail &gdp, UT_BoundingBox &bbox, int pointsOnly = true)
{
	using namespace H5;

	const Group &vertexGroup = furGroup.openGroup("P");
	const Group &vertexCount = furGroup.openGroup("numFaceVertices");

	const DataSet &vetrexCoordsDataSet = vertexGroup.openDataSet("smp0");
	const DataSet &vertexCountsDataSet = vertexCount.openDataSet("smp0");

	if (vetrexCoordsDataSet.getTypeClass() == H5T_FLOAT &&
		vertexCountsDataSet.getTypeClass() == H5T_INTEGER)
	{
		const DataSpace &vertexCoordsDataSpace = vetrexCoordsDataSet.getSpace();
		const DataSpace &vertexCountsDataSpace = vertexCountsDataSet.getSpace();

		hsize_t numVertexCoords = 0;
		hsize_t numVertexCounts = 0;
		if (vertexCoordsDataSpace.getSimpleExtentDims(&numVertexCoords) == 1 &&
			vertexCountsDataSpace.getSimpleExtentDims(&numVertexCounts) == 1)
		{
			const int numVertices = numVertexCoords / 3;

			float *vertexCoordsData = new float[numVertexCoords];
			vetrexCoordsDataSet.read(vertexCoordsData, PredType::NATIVE_FLOAT);

			const GA_Offset blockOffs = gdp.appendPointBlock(numVertices);

			for (int vertIndex = 0; vertIndex < numVertices; ++vertIndex) {
				const UT_Vector3 vert(vertexCoordsData[vertIndex * 3 + 0],
										vertexCoordsData[vertIndex * 3 + 1],
										vertexCoordsData[vertIndex * 3 + 2]);
				bbox.enlargeBounds(vert);
				gdp.setPos3(blockOffs + vertIndex, vert);
			}

			FreePtrArr(vertexCoordsData);

			if (!pointsOnly) {
				int *vertexCountsData = new int[numVertexCounts];
				vertexCountsDataSet.read(vertexCountsData, PredType::NATIVE_INT);

				int vertexIndex = 0;
				for (int strandIndex = 0; strandIndex < numVertexCounts; ++strandIndex) {
					if (vertexIndex >= numVertices)
						break;

					const int numStrandVertices = vertexCountsData[strandIndex];

					if (vertexIndex + numStrandVertices >= numVertices)
						break;

					GU_PrimPoly *poly = GU_PrimPoly::build(&gdp, numStrandVertices, GU_POLY_OPEN, 0);
					for (int polyVertIdx = 0; polyVertIdx < numStrandVertices; ++polyVertIdx) {
						poly->setVertexPoint(polyVertIdx, blockOffs + vertexIndex);
						vertexIndex++;
					}
				}

				FreePtrArr(vertexCountsData);
			}
		}
	}
}

void VRayPgYetiRef::detailRebuild()
{
	GU_Detail *gdp = new GU_Detail();

	const UT_String filePath(getFile());
	if (filePath.isstring()) {
		try {
			using namespace H5;

			Exception::dontPrint();

			H5File file(filePath.buffer(), H5F_ACC_RDONLY);

			Group geo = file.openGroup("/geo");
			const int numGroupObjects = geo.getNumObjs();

			for (int i = 0; i < numGroupObjects; ++i) {
				const H5G_obj_t itemType = geo.getObjTypeByIdx(i);
				if (itemType == H5G_GROUP) {
					const H5std_string &itemName = geo.getObjnameByIdx(i);

					const Group &furGroup = geo.openGroup(itemName);
					if (yetiIsFurGroup(furGroup)) {
						buildHairDetailFromGroup(furGroup, *gdp, m_bbox);
					}
				}
			}
		}
		catch (...) {
			Log::getLog().error("Error parsing Yeti cache file: \"%s\"", filePath.buffer());
		}
	}

	GU_DetailHandle gdpHndl;
	gdpHndl.allocateAndSet(gdp);

	m_detail = gdpHndl;
}
