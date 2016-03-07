//
// Copyright (c) 2015, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#include "vfh_exporter.h"
#include "vfh_mesh_utils.h"
#include "vfh_ga_utils.h"
#include "sop_vrayproxy.h"

#include <OBJ/OBJ_Geometry.h>
#include <SHOP/SHOP_Node.h>
#include <SHOP/SHOP_GeoOverride.h>
#include <PRM/PRM_ParmMicroNode.h>
#include <CH/CH_Channel.h>

#include <SOP/SOP_Node.h>
#include <GU/GU_Detail.h>
#include <GU/GU_PrimPolySoup.h>
#include <GU/GU_PrimPacked.h>
#include <GA/GA_PageHandle.h>


#include <UT/UT_Version.h>
#include <UT/UT_Assert.h>

#include <unordered_set>
#include <unordered_map>


using namespace VRayForHoudini;


class PolyMeshExporter
{
public:
	PolyMeshExporter(const GU_Detail &gdp, VRayExporter &pluginExporter);
	~PolyMeshExporter() { }

	PolyMeshExporter& setSubdivApplied(bool val) { m_hasSubdivApplied = val; }
	int exportGeometry();

private:

private:
	const GU_Detail &m_gdp;
	VRayExporter    &m_pluginExporter;

	int numPoints;
	int numFaces;
	int numMtlIDs;
	VRay::VUtils::VectorRefList vertices;
	VRay::VUtils::VectorRefList normals;
	VRay::VUtils::IntRefList faces;
	VRay::VUtils::IntRefList face_mtlIDs;
	VRay::VUtils::IntRefList edge_visibility;
	Mesh::MapChannels map_channels_data;
	bool m_hasSubdivApplied;
};


PolyMeshExporter::PolyMeshExporter(const GU_Detail &gdp, VRayExporter &pluginExporter):
	m_gdp(gdp),
	m_pluginExporter(pluginExporter)
{ }


struct GeomExportData {
	VRay::VUtils::VectorRefList vertices;
	VRay::VUtils::VectorRefList normals;
	VRay::VUtils::IntRefList    faces;
	VRay::VUtils::IntRefList    edge_visibility;
	VRay::VUtils::IntRefList    face_mtlIDs;
	Mesh::MapChannels           map_channels_data;
	int numPoints;
	int numFaces;
	int numMtlIDs;
};


//
// MATERIAL OVERRIDE
// * Go through all "material" OBJ child nodes and collect override attribute names and types.
// * Find target VOP's to override attribute at.
// * Bake color and float attributes into mesh's map channles.
// * Check override type and descide whether to export a separate material or:
//   - Override attribute with mesh's map channel (using TexUserColor or TexUser)
//   - Override attribute with texture id map (using TexMultiID)
//
// Primitive color override example
//   "diffuser" : 1.0, "diffuseg" : 1.0, "diffuseb" : 1.0
//
// We don't care about the separate channels, we have to export attribute as "diffuse".
// We need to go through all the "material" nodes inside the network and collect actual
// parameter names. Then we'll bake float and color attributes as map channels.
//


struct SHOPHasher
{
	typedef uint32     result_type;

	static result_type getSHOPId(const UT_String &shopPath)
	{
		return (NOT(shopPath.isstring()))? 0 : shopPath.hash();
	}

	result_type operator()(const SHOP_Node *shopNode) const
	{
		return (NOT(shopNode))? 0 : getSHOPId(shopNode->getFullPath());
	}

	result_type operator()(const UT_String &shopPath) const
	{
		return getSHOPId(shopPath);
	}
};


typedef std::unordered_set< UT_String , SHOPHasher > SHOPList;



struct PrimOverride
{
	typedef std::unordered_map< std::string, VRay::Vector > MtlOverrides;

	PrimOverride(SHOP_Node *shopNode = nullptr):
		shopNode(shopNode)
	{}

	SHOP_Node *shopNode;
	MtlOverrides mtlOverrides;
};


int getPerPrimMtlOverrides(const OP_Context &context, const GU_Detail &gdp, std::unordered_set< std::string > &o_mapChannelOverrides, std::vector< PrimOverride > &o_primOverrides)
{
	const GA_ROHandleS materialPathHndl(gdp.findStringTuple(GA_ATTRIB_PRIMITIVE, "shop_materialpath"));
	const GA_ROHandleS materialOverrideHndl(gdp.findStringTuple(GA_ATTRIB_PRIMITIVE, "material_override"));
	if (   !materialPathHndl.isValid()
		|| !materialOverrideHndl.isValid())
	{
		return 0;
	}

	o_primOverrides.resize(gdp.getNumPrimitives());

	int k = 0;
	for (GA_Iterator jt(gdp.getPrimitiveRange()); !jt.atEnd(); jt.advance(), ++k) {
		const GA_Offset off = *jt;
		const GEO_Primitive *prim = gdp.getGEOPrimitive(off);
		if (   prim->getTypeId() != GEO_PRIMPOLY
			&& prim->getTypeId() != GEO_PRIMPOLYSOUP)
		{
			continue;
		}

		PrimOverride &primOverride = o_primOverrides[k];
		primOverride.shopNode = OPgetDirector()->findSHOPNode( materialPathHndl.get(off) );
		if ( NOT(primOverride.shopNode) ) {
			Log::getLog().error("Error for primitive #\"%d\": Shop node \"%s\" not found!",
								k, materialPathHndl.get(off));
			continue;
		}

		// material override is string representation of python dict
		// using HDK helper class SHOP_GeoOverride to parse that
		SHOP_GeoOverride mtlOverride;
		mtlOverride.load( materialOverrideHndl.get(off) );
		if (mtlOverride.entries() <= 0) {
			continue;
		}

		const PRM_ParmList *shopPrmList = primOverride.shopNode->getParmList();
		UT_StringArray mtlOverrideChs;
		mtlOverride.getKeys(mtlOverrideChs);
		for ( const UT_StringHolder &chName : mtlOverrideChs) {
			int chIdx = -1;
			PRM_Parm *prm = shopPrmList->getParmPtrFromChannel(chName, &chIdx);
			if (NOT(prm)) {
				continue;
			}

			std::string channelName = prm->getToken();
			// skip overrides of 4th component on 4-tuple params
			// we can not store the 4th channel in VRay::Vector which has only 3 components
			// TODO: need a way to export these
			if (   chIdx < 0
				|| chIdx >= 3)
			{
				continue;
			}
			// skip overrides on string and toggle params for now - they can't be exported as map channels
			// TODO: need a way to export these
			const PRM_Type &prmType = prm->getType();
			if (   mtlOverride.isString(chName.buffer())
				|| NOT(prmType.isFloatType()) )
			{
				continue;
			}

			if ( primOverride.mtlOverrides.count(channelName) == 0 ) {
				o_mapChannelOverrides.insert(channelName);
				// get default value from the shop parameter
				// so if the overrides are NOT on all param channels
				// we still get the default value for the channel from the shop param
				VRay::Vector &val = primOverride.mtlOverrides[ channelName ];
				for (int i = 0; i < prm->getVectorSize() && i < 3; ++i) {
					fpreal fval = 0;
					prm->getValue(context.getTime(), fval, i, context.getThread());
					val[i] = fval;
				}
			}
			// override the channel value
			VRay::Vector &val = primOverride.mtlOverrides[ channelName ];
			fpreal fval = 0;
			mtlOverride.import(chName, fval);
			val[ chIdx ] = fval;
		}
	}

	return k;
}


static void getMtlOverrides(const OP_Context &context, const GU_Detail &gdp, GeomExportData &expData)
{
	std::unordered_set< std::string > mapChannelOverrides;
	std::vector< PrimOverride > primOverrides;
	if ( getPerPrimMtlOverrides(context, gdp, mapChannelOverrides, primOverrides) > 0) {

		for (const std::string channelName : mapChannelOverrides ) {
			Mesh::MapChannel &map_channel = expData.map_channels_data[ channelName ];
			// max number of different vertices int the channel is bounded by number of primitives
			map_channel.vertices = VRay::VUtils::VectorRefList(gdp.getNumPrimitives());
			map_channel.faces = VRay::VUtils::IntRefList(expData.numFaces * 3);

			int k = 0;
			int faceVertIndex = 0;
			for (GA_Iterator jt(gdp.getPrimitiveRange()); !jt.atEnd(); jt.advance(), ++k) {
				const GEO_Primitive *prim = gdp.getGEOPrimitive(*jt);
				if (   prim->getTypeId() != GEO_PRIMPOLY
					&& prim->getTypeId() != GEO_PRIMPOLYSOUP)
				{
					continue;
				}

				PrimOverride &primOverride = primOverrides[k];
				if ( primOverride.shopNode ) {
					VRay::Vector &val = map_channel.vertices[k];
					// if the parameter is overriden by the primitive get the overriden value
					if ( primOverride.mtlOverrides.count(channelName) ) {
						val = primOverride.mtlOverrides[channelName];
					}
					// else if the parameter exists on the shop node get the default value from there
					else if ( primOverride.shopNode->hasParm(channelName.c_str()) ) {
						const PRM_Parm &prm = primOverride.shopNode->getParm(channelName.c_str());
						for (int i = 0; i < prm.getVectorSize() && i < 3; ++i) {
							fpreal fval = 0;
							prm.getValue(context.getTime(), fval, i, context.getThread());
							val[i] = fval;
						}
					}
					// finally there is no such param on the shop node so leave a default value of Vector(0,0,0)
				}

				// TODO: need to refactor this:
				// for all map channels "faces" will be same  array so no need to recalc it every time
				switch (prim->getTypeId().get()) {
					case GEO_PRIMPOLYSOUP:
					{
						const GU_PrimPolySoup *polySoup = static_cast<const GU_PrimPolySoup*>(prim);
						for (GA_Size i = 0; i < polySoup->getPolygonCount(); ++i) {
							GA_Size vCnt = std::max(polySoup->getPolygonSize(i) - 2, GA_Size(0));
							for (GA_Size j = 0; j < vCnt; ++j) {
								map_channel.faces[faceVertIndex++] = k;
								map_channel.faces[faceVertIndex++] = k;
								map_channel.faces[faceVertIndex++] = k;
							}
						}
						break;
					}
					case GEO_PRIMPOLY:
					{
						GA_Size vCnt = std::max(prim->getVertexCount() - 2, GA_Size(0));
						for (GA_Size j = 0; j < vCnt; ++j) {
							map_channel.faces[faceVertIndex++] = k;
							map_channel.faces[faceVertIndex++] = k;
							map_channel.faces[faceVertIndex++] = k;
						}
						break;
					}
					default:
						;
				}
			}
		}
	}
}


void vertexAttrAsMapChannel(const GU_Detail &gdp, const GA_Attribute &vertexAttr, int numFaces, GeomExportParams &expParams, Mesh::MapChannel &map_channel)
{
	GA_ROPageHandleV3 vaPageHndl(&vertexAttr);
	GA_ROHandleV3 vaHndl(&vertexAttr);

	UT_ASSERT(vaPageHndl.isValid());
	UT_ASSERT(vaHndl.isValid());

	map_channel.name = GA::getGaAttributeName(vertexAttr);
	Log::getLog().info("  Found map channel: %s",
					   map_channel.name.c_str());

	if (expParams.uvWeldThreshold > 0) {
		// weld vertex attribute values before populating the map channel
		GA_Offset start, end;
		for (GA_Iterator it(gdp.getVertexRange()); it.blockAdvance(start, end); ) {
			vaPageHndl.setPage(start);
			for (GA_Offset offset = start; offset < end; ++offset) {
				const UT_Vector3 &val = vaPageHndl.value(offset);
				map_channel.verticesSet.insert(Mesh::MapVertex(val));
			}
		}

		// Init map channel data
		map_channel.vertices = VRay::VUtils::VectorRefList(map_channel.verticesSet.size());
		map_channel.faces = VRay::VUtils::IntRefList(numFaces * 3);

		int i = 0;
		for (auto &mv: map_channel.verticesSet) {
			mv.index = i;
			map_channel.vertices[i++].set(mv.v[0], mv.v[1], mv.v[2]);
		}
		UT_ASSERT( i == map_channel.vertices.size() );

		// Process map channels (uv and other tuple(3) attributes)
		//
		int faceMapVertIndex = 0;
		for (GA_Iterator jt(gdp.getPrimitiveRange()); !jt.atEnd(); jt.advance()) {
			const GEO_Primitive *prim = gdp.getGEOPrimitive(*jt);
			GA_PrimitiveTypeId primType = prim->getTypeId();
			if (   primType != GEO_PRIMPOLY
				&& primType != GEO_PRIMPOLYSOUP)
			{
				continue;
			}

			const int &v0 = map_channel.verticesSet.find(Mesh::MapVertex(vaHndl.get(prim->getVertexOffset(0))))->index;
			const int &v1 = map_channel.verticesSet.find(Mesh::MapVertex(vaHndl.get(prim->getVertexOffset(1))))->index;
			const int &v2 = map_channel.verticesSet.find(Mesh::MapVertex(vaHndl.get(prim->getVertexOffset(2))))->index;

			map_channel.faces[faceMapVertIndex++] = v2;
			map_channel.faces[faceMapVertIndex++] = v1;
			map_channel.faces[faceMapVertIndex++] = v0;

			if (prim->getVertexCount() == 4) {
				const int &v3 = map_channel.verticesSet.find(Mesh::MapVertex(vaHndl.get(prim->getVertexOffset(3))))->index;

				map_channel.faces[faceMapVertIndex++] = v3;
				map_channel.faces[faceMapVertIndex++] = v2;
				map_channel.faces[faceMapVertIndex++] = v0;
			}
		}
		UT_ASSERT( faceMapVertIndex == map_channel.faces.size() );

		// Cleanup hash
		map_channel.verticesSet.clear();
	}
	else {
		// populate map channel with original values

		// Init map channel data
		map_channel.vertices = VRay::VUtils::VectorRefList(gdp.getNumVertices());
		map_channel.faces = VRay::VUtils::IntRefList(numFaces * 3);

		int i = 0;
		GA_Offset start, end;
		for (GA_Iterator it(gdp.getVertexRange()); it.blockAdvance(start, end); ) {
			vaPageHndl.setPage(start);
			for (GA_Offset offset = start; offset < end; ++offset) {
				const UT_Vector3 &val = vaPageHndl.value(offset);
				map_channel.vertices[i++].set(val[0], val[1], val[2]);
			}
		}
		UT_ASSERT(i == gdp.getNumVertices());

		i = 0;
		GA_Index vi = 0;
		for (GA_Iterator jt(gdp.getPrimitiveRange()); !jt.atEnd(); jt.advance()) {
			const GEO_Primitive *prim = gdp.getGEOPrimitive(*jt);
			GA_PrimitiveTypeId primType = prim->getTypeId();
			if (   primType != GEO_PRIMPOLY
				&& primType != GEO_PRIMPOLYSOUP)
			{
				continue;
			}

			if (prim->getTypeId().get() == GEO_PRIMPOLYSOUP) {
				const GU_PrimPolySoup *polySoup = static_cast<const GU_PrimPolySoup*>(prim);
				for (GEO_PrimPolySoup::PolygonIterator psIt(*polySoup); !psIt.atEnd(); ++psIt) {
					map_channel.faces[i++] = psIt.getVertexIndex(2);
					map_channel.faces[i++] = psIt.getVertexIndex(1);
					map_channel.faces[i++] = psIt.getVertexIndex(0);

					if (psIt.getVertexCount() == 4) {
						map_channel.faces[i++] = psIt.getVertexIndex(3);
						map_channel.faces[i++] = psIt.getVertexIndex(2);
						map_channel.faces[i++] = psIt.getVertexIndex(0);
					}
				}
			}
			else {
				map_channel.faces[i++] = vi + 2;
				map_channel.faces[i++] = vi + 1;
				map_channel.faces[i++] = vi;

				if (prim->getVertexCount() == 4) {
					map_channel.faces[i++] = vi + 3;
					map_channel.faces[i++] = vi + 2;
					map_channel.faces[i++] = vi;
				}
			}

			vi += prim->getVertexCount();
		}

		UT_ASSERT( i == map_channel.faces.size() );
	}
}


void exportVertexAttrs(const GU_Detail &gdp, GeomExportParams &expParams, GeomExportData &expData)
{
	// add all vector3 vertex attributes to map_channels_data
	GA_AttributeFilter float3Filter(GA_ATTRIB_FILTER_AND, GA_AttributeFilter::selectFloatTuple(), GA_AttributeFilter::selectByTupleSize(3));
	for (GA_AttributeDict::iterator attrIt = gdp.getAttributeDict(GA_ATTRIB_VERTEX).begin(GA_SCOPE_PUBLIC); !attrIt.atEnd(); ++attrIt) {
		const std::string attrName(attrIt.name());
		// "N" point attribute is handled separately as different plugin property
		// so skip it here
		if (StrEq(attrIt.name(), "N")) {
			continue;
		}

		if (attrIt.attrib() &&
			float3Filter.match(attrIt.attrib()) &&
			NOT(expData.map_channels_data.count(attrName)))
		{
			Mesh::MapChannel &map_channel = expData.map_channels_data[attrName];
			vertexAttrAsMapChannel(gdp, *attrIt.attrib(), expData.numFaces, expParams, map_channel);
		}
	}
}


void exportPointAttrs(const GU_Detail &gdp, GeomExportData &expData)
{
	// add all vector3 point attributes to map_channels_data
	GA_AttributeFilter float3Filter(GA_ATTRIB_FILTER_AND, GA_AttributeFilter::selectFloatTuple(), GA_AttributeFilter::selectByTupleSize(3));
	for (GA_AttributeDict::iterator attrIt = gdp.getAttributeDict(GA_ATTRIB_POINT).begin(GA_SCOPE_PUBLIC); !attrIt.atEnd(); ++attrIt) {
		const std::string attrName(attrIt.name());
		// "P" and "N" point attributes are handled separately as different plugin properties
		// so skip them here
		if (StrEq(attrIt.name(), "P") ||
			StrEq(attrIt.name(), "N")) {
			continue;
		}

		if (float3Filter.match(attrIt.attrib())) {
			GA_ROPageHandleV3 paPageHndl(attrIt.attrib());
			if (paPageHndl.isValid()) {
				if (NOT(expData.map_channels_data.count(attrName))) {
					Mesh::MapChannel &map_channel = expData.map_channels_data[attrName];
					map_channel.name = attrName;

					// we can use same face indices as for mesh vertices
					map_channel.vertices = VRay::VUtils::VectorRefList(gdp.getNumPoints());
					map_channel.faces = expData.faces;

					GA_Offset start, end;
					int vidx = 0;
					for (GA_Iterator it(gdp.getPointRange()); it.blockAdvance(start, end); ) {
						paPageHndl.setPage(start);
						for (GA_Offset offset = start; offset < end; ++offset) {
							const UT_Vector3 &val = paPageHndl.value(offset);
							map_channel.vertices[vidx++].set(val[0], val[1], val[2]);
						}
					}
					UT_ASSERT( vidx == gdp.getNumPoints() );
				}
			}
		}
	}
}


bool isPrimPoly(GA_Primitive &prim)
{
	return (
			   prim.getTypeId() == GEO_PRIMPOLY
			|| prim.getTypeId() == GEO_PRIMPOLYSOUP
			);
}


bool getDataFromAttribute(const GA_Attribute *attr, VRay::VUtils::VectorRefList &data)
{
	GA_ROAttributeRef attrRef(attr);
	if (attrRef.isInvalid()) {
		return false;
	}

	const GA_AIFTuple *aifTuple = attrRef.getAIFTuple();
	if (NOT(aifTuple)) {
		return false;
	}

	data = VRay::VUtils::VectorRefList(attr->getIndexMap().indexSize());
	return aifTuple->getRange(attr, GA_Range(attr->getIndexMap()), &(data.get()->x));
}


GA_Size getNumFaces(const GU_Detail &gdp)
{
	GA_Size nFaces = 0;
	for (GA_Iterator jt(gdp.getPrimitiveRange()); !jt.atEnd(); jt.advance()) {
		const GEO_Primitive *prim = gdp.getGEOPrimitive(*jt);

		switch (prim->getTypeId().get()) {
			case GEO_PRIMPOLYSOUP:
			{
				const GU_PrimPolySoup *polySoup = static_cast<const GU_PrimPolySoup*>(prim);
				for (GA_Size i = 0; i < polySoup->getPolygonCount(); ++i) {
					nFaces += std::max(polySoup->getPolygonSize(i) - 2, GA_Size(0));
				}
				break;
			}
			case GEO_PRIMPOLY:
			{
				nFaces += std::max(prim->getVertexCount() - 2, GA_Size(0));
				break;
			}
			default:
				;
		}
	}

	return nFaces;
}


GA_Size getMeshFaces(const GU_Detail &gdp, VRay::VUtils::IntRefList &faces, VRay::VUtils::IntRefList &edge_visibility)
{
	GA_Size nFaces = getNumFaces(gdp);
	if (nFaces <= 0) {
		return nFaces;
	}

	// NOTE: Support only tri-faces for now
	//   [ ] > 4 vertex faces support
	//   [x] edge_visibility
	//

	faces = VRay::VUtils::IntRefList(nFaces*3);
	edge_visibility = VRay::VUtils::IntRefList(nFaces/10 + (((nFaces%10) > 0)? 1 : 0));
	std::memset(edge_visibility.get(), 0, edge_visibility.size() * sizeof(int));

	int faceVertIndex = 0;
	int faceEdgeVisIndex = 0;
	for (GA_Iterator jt(gdp.getPrimitiveRange()); !jt.atEnd(); jt.advance()) {
		const GEO_Primitive *prim = gdp.getGEOPrimitive(*jt);

		switch (prim->getTypeId().get()) {
			case GEO_PRIMPOLYSOUP:
			{
				const GU_PrimPolySoup *polySoup = static_cast<const GU_PrimPolySoup*>(prim);
				for (GEO_PrimPolySoup::PolygonIterator pst(*polySoup); !pst.atEnd(); ++pst) {
					GA_Size vCnt = pst.getVertexCount();
					if ( vCnt > 2) {
						for (GA_Size i = 1; i < vCnt-1; ++i) {
							faces[faceVertIndex++] = pst.getPointIndex(i+1);
							faces[faceVertIndex++] = pst.getPointIndex(i);
							faces[faceVertIndex++] = pst.getPointIndex(0);

							// here the diagonal is invisible edge
							// v(1)___v(2)
							//    |  /|
							//    | / |
							//    |/__|
							// v(0)   v(3)
							// first edge v(i+1)->v(i) is always visible
							// second edge v(i)->v(0) is visible only when i == 1
							// third edge v(0)->v(i+1) is visible only when i+1 == vCnt-1
							unsigned char edgeMask = 1 | ((i == 1) << 1) | ((i == (vCnt-2)) << 2);
							edge_visibility[faceEdgeVisIndex/10] |= (edgeMask << ((faceEdgeVisIndex%10)*3));
							++faceEdgeVisIndex;
						}
					}
				}
				break;
			}
			case GEO_PRIMPOLY:
			{
				GA_Size vCnt = prim->getVertexCount();
				if ( vCnt > 2) {
					for (GA_Size i = 1; i < vCnt-1; ++i) {
						faces[faceVertIndex++] = prim->getPointIndex(i+1);
						faces[faceVertIndex++] = prim->getPointIndex(i);
						faces[faceVertIndex++] = prim->getPointIndex(0);

						unsigned char edgeMask = (1 | ((i == 1) << 1) | ((i == (vCnt-2)) << 2));
						edge_visibility[faceEdgeVisIndex/10] |= (edgeMask << ((faceEdgeVisIndex%10)*3));
						++faceEdgeVisIndex;
					}
				}
				break;
			}
			default:
				;
		}
	}

	UT_ASSERT( faceVertIndex == nFaces*3 );
	UT_ASSERT( faceEdgeVisIndex == nFaces );
	UT_ASSERT( edge_visibility.size() >= (faceEdgeVisIndex/10) );
	UT_ASSERT( edge_visibility.size() <= (faceEdgeVisIndex/10 + 1) );

	return nFaces;
}


GA_Size getMtlIds(const GU_Detail &gdp, VRay::VUtils::IntRefList &face_mtlIDs)
{
	GA_ROHandleS mtlpath(gdp.findAttribute(GA_ATTRIB_PRIMITIVE, GEO_STD_ATTRIB_MATERIAL));
	if (mtlpath.isInvalid()) {
		return 0;
	}

	GA_Size nFaces = getNumFaces(gdp);
	if (nFaces <= 0) {
		return 0;
	}

	face_mtlIDs = VRay::VUtils::IntRefList(nFaces);

	int faceIndex = 0;
	for (GA_Iterator jt(gdp.getPrimitiveRange()); !jt.atEnd(); jt.advance()) {
		const GEO_Primitive *prim = gdp.getGEOPrimitive(*jt);
		int shopID = SHOPHasher::getSHOPId(mtlpath.get(*jt));

		switch (prim->getTypeId().get()) {
			case GEO_PRIMPOLYSOUP:
			{
				const GU_PrimPolySoup *polySoup = static_cast<const GU_PrimPolySoup*>(prim);
				for (GA_Size i = 0; i < polySoup->getPolygonCount(); ++i) {
					GA_Size nCnt = std::max(polySoup->getPolygonSize(i) - 2, GA_Size(0));
					for (GA_Size j = 0; j < nCnt; ++j) {
						face_mtlIDs[faceIndex++] = shopID;
					}
				}
				break;
			}
			case GEO_PRIMPOLY:
			{
				GA_Size nCnt = std::max(prim->getVertexCount() - 2, GA_Size(0));
				for (GA_Size j = 0; j < nCnt; ++j) {
					face_mtlIDs[faceIndex++] = shopID;
				}
				break;
			}
			default:
				;
		}
	}

	UT_ASSERT( faceIndex == nFaces );
	return nFaces;
}



int getSHOPList(const GU_Detail &gdp, SHOPList &shopList)
{
	GA_ROHandleS mtlpath(gdp.findAttribute(GA_ATTRIB_PRIMITIVE, GEO_STD_ATTRIB_MATERIAL));
	if (mtlpath.isInvalid()) {
		return 0;
	}

	int shopCnt = 0;
	for (GA_Iterator jt(gdp.getPrimitiveRange()); !jt.atEnd(); jt.advance()) {
		const GEO_Primitive *prim = gdp.getGEOPrimitive(*jt);

		switch (prim->getTypeId().get()) {
			case GEO_PRIMPOLYSOUP:
			case GEO_PRIMPOLY:
			{
				UT_String shoppath(mtlpath.get(*jt), false);
				if (   OPgetDirector()->findSHOPNode(shoppath)
					&& NOT(shopList.count(shoppath)) )
				{
					shopList.insert(shoppath);
					++shopCnt;
				}
			}
			default:
				;
		}
	}

	return shopCnt;
}


void VRayExporter::exportGeomStaticMeshDesc(const GU_Detail &gdp, GeomExportParams &expParams, Attrs::PluginDesc &geomPluginDesc)
{
	GeomExportData expData;
	expData.numPoints = gdp.getNumPoints();

	Log::getLog().info("  Mesh: %i points", expData.numPoints);

	// vertex positions
	getDataFromAttribute(gdp.findAttribute(GA_ATTRIB_POINT, GEO_STD_ATTRIB_POSITION), expData.vertices);
	UT_ASSERT( gdp.getNumPoints() == expData.vertices.size() );
	// normals
	getDataFromAttribute(gdp.findAttribute(GA_ATTRIB_POINT, GEO_STD_ATTRIB_NORMAL), expData.normals);
	UT_ASSERT( gdp.getNumPoints() == expData.normals.size() );
	// faces
	expData.numFaces = getMeshFaces(gdp, expData.faces, expData.edge_visibility);
	// mtl ids
	expData.numMtlIDs = getMtlIds(gdp, expData.face_mtlIDs);
	UT_ASSERT( expData.numFaces == expData.numMtlIDs );

	getMtlOverrides(m_context, gdp, expData);
//	exportVertexAttrs(gdp, expParams, expData);
//	exportPointAttrs(gdp, expData);

	geomPluginDesc.addAttribute(Attrs::PluginAttr("vertices", expData.vertices));
	geomPluginDesc.addAttribute(Attrs::PluginAttr("faces", expData.faces));
	geomPluginDesc.addAttribute(Attrs::PluginAttr("face_mtlIDs", expData.face_mtlIDs));
	geomPluginDesc.addAttribute(Attrs::PluginAttr("edge_visibility", expData.edge_visibility));

	if (isIPR() && isGPU()) {
		geomPluginDesc.addAttribute(Attrs::PluginAttr("dynamic_geometry", true));
	}

	if (expData.normals.size()) {
		geomPluginDesc.addAttribute(Attrs::PluginAttr("normals", expData.normals));
		geomPluginDesc.addAttribute(Attrs::PluginAttr("faceNormals", expData.faces));
	}

	if (expData.map_channels_data.size()) {
		VRay::VUtils::ValueRefList map_channel_names(expData.map_channels_data.size());
		VRay::VUtils::ValueRefList map_channels(expData.map_channels_data.size());

		int i = 0;
		for (const auto &mcIt : expData.map_channels_data) {
			const std::string      &map_channel_name = mcIt.first;
			const Mesh::MapChannel &map_channel_data = mcIt.second;

			// Channel data
			VRay::VUtils::ValueRefList map_channel(3);
			map_channel[0].setDouble(i);
			map_channel[1].setListVector(map_channel_data.vertices);
			map_channel[2].setListInt(map_channel_data.faces);

			map_channels[i].setList(map_channel);
			// Channel name attribute
			map_channel_names[i].setString(map_channel_name.c_str());
			++i;
		}

		geomPluginDesc.addAttribute(Attrs::PluginAttr("map_channels_names", map_channel_names));
		geomPluginDesc.addAttribute(Attrs::PluginAttr("map_channels",      map_channels));
	}
}


VRay::Plugin VRayExporter::exportGeomStaticMesh(SOP_Node &sop_node, const GU_Detail &gdp, GeomExportParams &expParams)
{
	bool hasPolyGeometry = gdp.containsPrimitiveType(GEO_PRIMPOLY) || gdp.containsPrimitiveType(GEO_PRIMPOLYSOUP);
	if ( NOT(hasPolyGeometry)) {
		return VRay::Plugin();
	}

	Attrs::PluginDesc geomPluginDesc(VRayExporter::getPluginName(&sop_node, boost::str(Parm::FmtPrefixManual % "Geom" % std::to_string(gdp.getUniqueId()))), "GeomStaticMesh");
	exportGeomStaticMeshDesc(gdp, expParams, geomPluginDesc);
	return exportPlugin(geomPluginDesc);
}



class GeometryExporter
{
	typedef std::vector< VRay::Plugin > PluginList;
	typedef std::list< Attrs::PluginDesc > PluginDescList;
	typedef std::unordered_map< uint, PluginDescList > DetailToPluginDesc;

public:
	GeometryExporter(OBJ_Geometry &node, VRayExporter &pluginExporter);
	~GeometryExporter() { }

	bool hasSubdivApplied() const;
	int exportGeometry();
	VRay::Plugin &getPluginAt(int idx) { return m_pluginList.at(idx); }

private:
	void cleanup();
	int exportDetail(SOP_Node &sop, GU_DetailHandleAutoReadLock &gdl, PluginDescList &pluginList);
	int exportPacked(SOP_Node &sop, const GU_PrimPacked &prim, PluginDescList &pluginList);
	int exportPrimitive(SOP_Node &sop, const GU_PrimPacked &prim, PluginDescList &pluginList);

private:
	OBJ_Geometry &m_objNode;
	OP_Context   &m_context;
	VRayExporter &m_pluginExporter;

	// packed geometry detail
	DetailToPluginDesc   m_detailToPluginDesc;

	GeomExportParams     m_expParams;
	PluginList           m_pluginList;
};


GeometryExporter::GeometryExporter(OBJ_Geometry &node, VRayExporter &pluginExporter):
	m_objNode(node),
	m_context(pluginExporter.getContext()),
	m_pluginExporter(pluginExporter)
{ }


bool GeometryExporter::hasSubdivApplied() const
{
	bool res = false;

	fpreal t = m_context.getTime();
	bool hasDispl = m_objNode.hasParm("vray_use_displ") && m_objNode.evalInt("vray_use_displ", 0, t);
	if (NOT(hasDispl)) {
		return res;
	}

	const int displType = m_objNode.evalInt("vray_displ_type", 0, t);
	switch (displType) {
		// from shopnet
		case 0:
		{
			UT_String shopPath;
			m_objNode.evalString(shopPath, "vray_displshoppath", 0, t);
			SHOP_Node *shop = OPgetDirector()->findSHOPNode(shopPath.buffer());
			if (shop) {
				UT_ValArray<OP_Node *> outputNodes;
				if ( shop->getOpsByName("vray_material_output", outputNodes) ) {
					// there is at least 1 "vray_material_output" node so take the first one
					OP_Node *node = outputNodes(0);
					if (node->error() < UT_ERROR_ABORT) {
						const int idx = node->getInputFromName("Geometry");
						VOP::NodeBase *input = dynamic_cast< VOP::NodeBase * >(node->getInput(idx));
						if (input && input->getVRayPluginID() == "GeomStaticSmoothedMesh") {
							res = true;
						}
					}
				}
			}
			break;
		}
		// type is "GeomStaticSmoothedMesh"
		case 2:
		{
			res = true;
		}
		default:
			break;
	}

	return res;
}


void GeometryExporter::cleanup()
{
	m_detailToPluginDesc.clear();
	m_pluginList.clear();
}


int GeometryExporter::exportGeometry()
{
	m_expParams.uvWeldThreshold = hasSubdivApplied()? m_expParams.uvWeldThreshold: -1.f;
	SOP_Node *renderSOP = m_objNode.getRenderSopPtr();
	if (NOT(renderSOP)) {
		return 0;
	}

	GU_DetailHandleAutoReadLock gdl(renderSOP->getCookedGeoHandle(m_context));
	if (NOT(gdl.isValid())) {
		return 0;
	}

	uint detailHash = gdl.handle().hash();
	exportDetail(*renderSOP, gdl, m_detailToPluginDesc[detailHash]);

	PluginDescList &pluginList = m_detailToPluginDesc.at(detailHash);
	m_pluginList.reserve(pluginList.size());

	VRay::Transform tm = VRayExporter::getObjTransform(&m_objNode, m_context);
	SHOP_Node *shopNode = m_pluginExporter.getObjMaterial(&m_objNode, m_context.getTime());

	int i = 0;
	for (Attrs::PluginDesc &nodeDesc : pluginList) {
//		TODO: need to fill in node with appropriate names and export them
		nodeDesc.pluginName = VRayExporter::getPluginName(&m_objNode, boost::str(Parm::FmtPrefixManual % "Node" % std::to_string(i++)));

		Attrs::PluginAttr *attr = nullptr;

		attr = nodeDesc.get("transform");
		if (NOT(attr)) {
			nodeDesc.addAttribute(Attrs::PluginAttr("transform", tm));
		}
		else {
			attr->paramValue.valTransform = tm * attr->paramValue.valTransform;
		}

		attr = nodeDesc.get("material");
		if (NOT(attr)) {
			if (shopNode) {
				ExportContext objContext(CT_OBJ, m_pluginExporter, m_objNode);
				VRay::Plugin mtl = m_pluginExporter.exportMaterial(*shopNode, objContext);
				nodeDesc.addAttribute(Attrs::PluginAttr("material", mtl));

				// material overrides in "user_attributes"
				UT_String userAtrs;

				const OP_DependencyList &depList = shopNode->getOpDependents();
				for (OP_DependencyList::reverse_iterator it = depList.rbegin(); !it.atEnd(); it.advance()) {
					const OP_Dependency &dep = *it;
					OP_Node *opNode = shopNode->lookupNode(dep.getRefOpId(), false);
					if (shopNode->isSubNode(opNode)) {
						const PRM_Parm &shopPrm = shopNode->getParm(dep.getSourceRefId().getParmRef());
						const PRM_Parm *objPrm = m_objNode.getParmList()->getParmPtr(shopPrm.getToken());

						if (   objPrm
							&& objPrm->getType().isFloatType()
							&& NOT(objPrm->getBypassFlag()) )
						{
							// if we have parameter with matching name override on object level
							UT_StringArray prmValTokens;
							for (int i = 0; i < objPrm->getVectorSize(); ++i) {
								fpreal chval = m_objNode.evalFloat(objPrm, i, m_context.getTime());
								prmValTokens.append( std::to_string(chval) );
							}

							UT_String prmValToken;
							prmValTokens.join(",", prmValToken);

							userAtrs += shopPrm.getToken();
							userAtrs += "=";
							userAtrs += prmValToken;
							userAtrs += ";";
						}
					}
				}

				if (userAtrs.isstring()) {
					nodeDesc.addAttribute(Attrs::PluginAttr("user_attributes", userAtrs));
				}

			}
			else {
				VRay::Plugin mtl = m_pluginExporter.exportDefaultMaterial();
				nodeDesc.addAttribute(Attrs::PluginAttr("material", mtl));
			}
		}

		// TODO: adjust other Node attrs

		VRay::Plugin plugin = m_pluginExporter.exportPlugin(nodeDesc);
		if (plugin) {
			m_pluginList.emplace_back(plugin);
		}
	}

	return m_pluginList.size();
}


int GeometryExporter::exportDetail(SOP_Node &sop, GU_DetailHandleAutoReadLock &gdl, PluginDescList &pluginList)
{
//	TODO: verify nPlugins = ?
	int nPlugins = 0;

	const GU_Detail &gdp = *gdl.getGdp();

	// packed prims
	if (GU_PrimPacked::hasPackedPrimitives(gdp)) {
		UT_Array<const GA_Primitive *> prims;
		GU_PrimPacked::getPackedPrimitives(gdp, prims);
		for (const GA_Primitive *prim : prims) {
			const GU_PrimPacked *primPacked = dynamic_cast< const GU_PrimPacked * >(prim);
			nPlugins += exportPacked(sop, *primPacked, pluginList);
		}
	}

	// polygonal geometry
	VRay::Plugin geom = m_pluginExporter.exportGeomStaticMesh(sop, gdp, m_expParams);
	if (geom) {
		// add new node to our list of nodes
		pluginList.push_back(Attrs::PluginDesc("", "Node"));
		++nPlugins;

		Attrs::PluginDesc &nodeDesc = pluginList.back();
		// geoemtry
		nodeDesc.addAttribute(Attrs::PluginAttr("geometry", geom));

		// material
		SHOPList shopList;
		int nSHOPs = getSHOPList(gdp, shopList);
		if (nSHOPs > 0) {
			ExportContext objContext(CT_OBJ, m_pluginExporter, m_objNode);

			VRay::ValueList mtls_list;
			VRay::IntList   ids_list;
			mtls_list.reserve(nSHOPs);
			ids_list.reserve(nSHOPs);

			for (const UT_String &shoppath : shopList) {
				SHOP_Node *shopNode = OPgetDirector()->findSHOPNode(shoppath);
				UT_ASSERT( shopNode );
				mtls_list.emplace_back(m_pluginExporter.exportMaterial(*shopNode, objContext));
				ids_list.emplace_back(SHOPHasher::getSHOPId(shoppath));
			}

			if (mtls_list.size() > 0) {

				Attrs::PluginDesc mtlMultiDesc("", "MtlMulti");
				mtlMultiDesc.pluginName = VRayExporter::getPluginName(&sop, boost::str(Parm::FmtPrefixManual % "Mtl" % std::to_string(gdp.getUniqueId())));

				mtlMultiDesc.addAttribute(Attrs::PluginAttr("mtls_list", mtls_list));
				mtlMultiDesc.addAttribute(Attrs::PluginAttr("ids_list",  ids_list));
				VRay::Plugin mtl = m_pluginExporter.exportPlugin(mtlMultiDesc);

				nodeDesc.addAttribute(Attrs::PluginAttr("material", mtl));
			}
		}
	}

	return nPlugins;
}


int GeometryExporter::exportPacked(SOP_Node &sop, const GU_PrimPacked &prim, PluginDescList &pluginList)
{
	uint packedHash = prim.getPackedDetail().hash();
	if (NOT(m_detailToPluginDesc.count(packedHash))) {
		exportPrimitive(sop, prim, m_detailToPluginDesc[packedHash]);
	}

	if (NOT(m_detailToPluginDesc.count(packedHash))) {
		return 0;
	}

	UT_Matrix4D xform;
	prim.getFullTransform4(xform);
	VRay::Transform tm = VRayExporter::Matrix4ToTransform(xform);
	GA_ROHandleS mtlpath(prim.getDetail().findAttribute(GA_ATTRIB_PRIMITIVE, GEO_STD_ATTRIB_MATERIAL));
	GA_ROHandleS mtlo(prim.getDetail().findAttribute(GA_ATTRIB_PRIMITIVE, "material_override"));

	PluginDescList primPluginList = m_detailToPluginDesc.at(packedHash);
	for (Attrs::PluginDesc &pluginDesc : primPluginList) {
		pluginList.push_back(pluginDesc);
		Attrs::PluginDesc &nodeDesc = pluginList.back();

		Attrs::PluginAttr *attr = nullptr;

		attr = nodeDesc.get("transform");
		if (NOT(attr)) {
			nodeDesc.addAttribute(Attrs::PluginAttr("transform", tm));
		}
		else {
			attr->paramValue.valTransform =  tm * attr->paramValue.valTransform;
		}

		attr = nodeDesc.get("material");
		if (NOT(attr) && mtlpath.isValid()) {
			const char *shoppath =  mtlpath.get(prim.getMapOffset());
			SHOP_Node *shopNode = OPgetDirector()->findSHOPNode(shoppath);
			if (shopNode) {
				ExportContext objContext(CT_OBJ, m_pluginExporter, m_objNode);
				VRay::Plugin mtl = m_pluginExporter.exportMaterial(*shopNode, objContext);
				nodeDesc.addAttribute(Attrs::PluginAttr("material", mtl));

				// material overrides in "user_attributes"
				UT_Options overrides;
				if (mtlo.isValid() && overrides.setFromPyDictionary(mtlo.get(prim.getMapOffset()))) {
					UT_String userAtrs;

					while (overrides.getNumOptions() > 0) {
						UT_String key = overrides.begin().name();

						int chIdx = -1;
						PRM_Parm *prm = shopNode->getParmList()->getParmPtrFromChannel(key, &chIdx);
						if (   NOT(prm)
							|| NOT(prm->getType().isFloatType()) )
						{
							overrides.removeOption(key);
							continue;
						}

						UT_StringArray prmValTokens;
						for (int i = 0; i < prm->getVectorSize(); ++i) {
							prm->getChannelToken(key, i);
							fpreal chval = (overrides.hasOption(key))? overrides.getOptionF(key) : shopNode->evalFloat(prm, i, m_context.getTime());
							prmValTokens.append( std::to_string(chval) );
							overrides.removeOption(key);
						}

						UT_String prmValToken;
						prmValTokens.join(",", prmValToken);

						userAtrs += prm->getToken();
						userAtrs += "=";
						userAtrs += prmValToken;
						userAtrs += ";";
					}

					if (userAtrs.isstring()) {
						nodeDesc.addAttribute(Attrs::PluginAttr("user_attributes", userAtrs));
					}
				}
			}
		}
	}

	return primPluginList.size();
}


int GeometryExporter::exportPrimitive(SOP_Node &sop, const GU_PrimPacked &prim, PluginDescList &pluginList)
{
	// packed primitives usually have a standart attribute "path"
	// if exists it can eigther reference a file on disk or a sop node that created the paked primitive
	// so we need to check for the attribute and if existing then either :
	//                  interpret it as sop reference ( VRayProxy SOP / Pack SOP / etc.)
	//                  interpret it as file path
	//                  take geoemtry directly from packed GU_Detail
	// otherwise take geoemtry directly from packed GU_Detail
	//
	// vray proxy loads geometry as a single packed primitive
	// TODO: may have to use custom packed primitive for that
	// and creates prim attibute "path" pointing to it: op:<path to VRayProxy SOP>
	// in order to be able to query the plugin settings

	int nPlugins = 0;

	const GA_ROHandleS pathHndl(prim.getDetail().findAttribute(GA_ATTRIB_PRIMITIVE, "path"));
	if (NOT(pathHndl.isValid())) {
		// there is no path attribute =>
		// take geometry directly from primitive packed detail
		GU_DetailHandleAutoReadLock gdl(prim.getPackedDetail());
		if (gdl.isValid()) {
			nPlugins = exportDetail(sop, gdl, pluginList);
		}
	}
	else {
		UT_StringHolder path = pathHndl.get(prim.getMapOffset());
		if (NOT(path.isstring()) || NOT(path.length())) {
			// there is no path attribute assigned =>
			// take geometry directly from primitive packed detail
			GU_DetailHandleAutoReadLock gdl(prim.getPackedDetail());
			if (gdl.isValid()) {
				nPlugins = exportDetail(sop, gdl, pluginList);
			}
		}
		else {
			SOP_Node *sopref = OPgetDirector()->findSOPNode(path);
			if (NOT(sopref)) {
				// there is path attribute, but it is NOT holding a ref to a SOP node =>
				// interpret the string as filepath and export as VRayProxy plugin
				// TODO: need to test - probably not working properly
				Attrs::PluginDesc meshFileDesc(boost::str(Parm::FmtPrefixManual % "Geom" % std::to_string(path.hash())), "GeomMeshFile");
				meshFileDesc.addAttribute(Attrs::PluginAttr("file", path));

				pluginList.push_back(Attrs::PluginDesc("", "Node"));
				Attrs::PluginDesc &nodeDesc = pluginList.back();
				nodeDesc.addAttribute(Attrs::PluginAttr("geometry", m_pluginExporter.exportPlugin(meshFileDesc)));
				nPlugins = 1;
			}
			else {
				SOP::VRayProxy *proxy = dynamic_cast< SOP::VRayProxy * >(sopref);
				if (NOT(proxy)) {
					// there is path attribute referencing an existing SOP, but it is NOT VRayProxy SOP =>
					// take geometry from SOP's input detail if there is valid input
					// else take geometry directly from primitive packed detail
					OP_Node *inpnode = sopref->getInput(0);
					SOP_Node *inpsop = nullptr;
					if (inpnode && (inpsop = inpnode->castToSOPNode())) {
						GU_DetailHandleAutoReadLock gdl(inpsop->getCookedGeoHandle(m_context));
						if (gdl.isValid()) {
							nPlugins = exportDetail(*sopref, gdl, pluginList);
						}
					}
					else {
						GU_DetailHandleAutoReadLock gdl(prim.getPackedDetail());
						if (gdl.isValid()) {
							nPlugins = exportDetail(sop, gdl, pluginList);
						}
					}
				}
				else {
					// there is path attribute referencing a VRayProxy SOP =>
					// export VRayProxy plugin
					Attrs::PluginDesc proxyDesc;
					OP::VRayNode::PluginResult res = proxy->asPluginDesc(proxyDesc, m_pluginExporter);

					pluginList.push_back(Attrs::PluginDesc("", "Node"));
					Attrs::PluginDesc &nodeDesc = pluginList.back();
					nodeDesc.addAttribute(Attrs::PluginAttr("geometry", m_pluginExporter.exportPlugin(proxyDesc)));
					nPlugins = 1;
				}
			}
		}
	}

	return nPlugins;
}



VRay::Plugin VRayExporter::exportObject(OBJ_Node *obj_node)
{
	VRay::Plugin plugin;

	if (NOT(obj_node)) {
		return plugin;
	}

	OBJ_Geometry *obj_geo = obj_node->castToOBJGeometry();
	if (NOT(obj_geo)) {
		return plugin;
	}

	GeometryExporter geoExporter(*obj_geo, *this);
	int nPlugins = geoExporter.exportGeometry();

	if (nPlugins > 0) {
		plugin = geoExporter.getPluginAt(0);
	}

	return plugin;
}

// replace exporObject and exportNodeData
// need to traverse through all primitives
// polygonal primitives should be exported as single GeomStaticMesh
// for packed primitives - need to hash whats alreay been exported
// hash based on file path string, Node UID or GU_DetailHandle hash
// @path=op:<path to vrayproxy> => VRayProxy plugin
// @path =<any string, interpret as filepath> => VRayProxy plugin
// @path=op:<path to node> => need to recursively handle primitive GU_Detail if not hashed
// no @path => need to recursively handle primitive GU_Detail if not hashed
