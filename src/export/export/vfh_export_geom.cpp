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
	int getPluginDesc(Attrs::PluginDesc &pluginDesc);

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
	VRay::VUtils::IntRefList faces;
	VRay::VUtils::IntRefList face_mtlIDs;
	VRay::VUtils::IntRefList edge_visibility;
	Mesh::MapChannels map_channels_data;
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



struct PrimOverride
{
	typedef std::unordered_map< std::string, VRay::Vector > MtlOverrides;

	PrimOverride(SHOP_Node *shopNode = nullptr):
		shopNode(shopNode)
	{}

	SHOP_Node *shopNode;
	MtlOverrides mtlOverrides;
};


int getPrimOverrides(const OP_Context &context, const GU_Detail &gdp, std::unordered_set< std::string > &o_mapChannelOverrides, std::vector< PrimOverride > &o_primOverrides)
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

		GA_PrimitiveTypeId primType = prim->getTypeId();
		if (   primType != GEO_PRIMPOLY
			&& primType != GEO_PRIMPOLYSOUP)
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
			// skip overrides on the 4th component of 4-tuple params
			// if prm is a 4-tuple and the override is on 4th component
			// we can not store the channel in VRay::Vector which has only 3 components
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


static void exportPrimitiveAttrs(const OP_Context &context, const GU_Detail &gdp, GeomExportParams &expParams, GeomExportData &expData)
{
	std::unordered_set< std::string > mapChannelOverrides;
	std::vector< PrimOverride > primOverrides;
	if ( getPrimOverrides(context, gdp, mapChannelOverrides, primOverrides) > 0) {

		for (const std::string channelName : mapChannelOverrides ) {
			Mesh::MapChannel &map_channel = expData.map_channels_data[ channelName ];
			// max number of different vertices int hte channel is bounded by number of primitives
			map_channel.vertices = VRay::VUtils::VectorRefList(gdp.getNumPrimitives());
			map_channel.faces = VRay::VUtils::IntRefList(expData.numFaces * 3);

			int k = 0;
			int vi = 0;
			for (GA_Iterator jt(gdp.getPrimitiveRange()); !jt.atEnd(); jt.advance(), ++k) {
				const GEO_Primitive *prim = gdp.getGEOPrimitive(*jt);
				GA_PrimitiveTypeId primType = prim->getTypeId();
				if (   primType != GEO_PRIMPOLY
					&& primType != GEO_PRIMPOLYSOUP)
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
							fpreal fval;
							prm.getValue(context.getTime(), fval, i, context.getThread());
							val[i] = fval;
						}
					}
					// finally there is no such param on the shop node so leave a default value of Vector(0,0,0)
				}

				// TODO: need to refactor this:
				// for all map channels "faces" will be same  array so no need to recalc it every time
				if (prim->getTypeId().get() == GEO_PRIMPOLYSOUP) {
					const GU_PrimPolySoup *polySoup = static_cast<const GU_PrimPolySoup*>(prim);
					for (GEO_PrimPolySoup::PolygonIterator psIt(*polySoup); !psIt.atEnd(); ++psIt) {
						map_channel.faces[vi++] = k;
						map_channel.faces[vi++] = k;
						map_channel.faces[vi++] = k;

						if (psIt.getVertexCount() == 4) {
							map_channel.faces[vi++] = k;
							map_channel.faces[vi++] = k;
							map_channel.faces[vi++] = k;
						}
					}
				}
				else {
					map_channel.faces[vi++] = k;
					map_channel.faces[vi++] = k;
					map_channel.faces[vi++] = k;

					if (prim->getVertexCount() == 4) {
						map_channel.faces[vi++] = k;
						map_channel.faces[vi++] = k;
						map_channel.faces[vi++] = k;
					}
				}
			}
		}
	}
}


void vertexAttrAsMapChannel(const GU_Detail &gdp, const GA_Attribute &vertexAttr, int numFaces, GeomExportParams &expParams, Mesh::MapChannel &map_channel)
{
	GA_ROPageHandleV3 vaPageHndl(&vertexAttr);
	GA_ROHandleV3 vaHndl(&vertexAttr);

	vassert(vaPageHndl.isValid());
	vassert(vaHndl.isValid());

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
		vassert(i == map_channel.vertices.size());

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
		vassert( faceMapVertIndex == map_channel.faces.size() );

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
		vassert(i == gdp.getNumVertices());

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

		vassert(i == map_channel.faces.size());
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


void exportPointAttrs(const GU_Detail &gdp, GeomExportParams &expParams, GeomExportData &expData)
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
					vassert(vidx == gdp.getNumPoints());
				}
			}
		}
	}
}


#include <GA/GA_Options.h>
#include <GA/GA_AIFCopyData.h>
#include <GA/GA_AIFDelta.h>
#include <GA/GA_AIFSharedStringTuple.h>

void VRayExporter::exportGeomStaticMeshDesc(const GU_Detail &gdp, GeomExportParams &expParams, Attrs::PluginDesc &geomPluginDesc)
{
	GeomExportData expData;
	expData.numPoints = gdp.getNumPoints();

	Log::getLog().info("  Mesh: %i points", expData.numPoints);

	// vertex positions
	GA_ROAttributeRef p(gdp.findAttribute(GA_ATTRIB_POINT, GEO_STD_ATTRIB_POSITION));
	if (p.isValid()) {
		expData.vertices = VRay::VUtils::VectorRefList(expData.numPoints);
		const GA_AIFTuple *aifTuple = p.getAIFTuple();
		if (aifTuple) {
			VRay::real *dest = &(expData.vertices.get()->x);
			aifTuple->getRange(p, gdp.getPointRange(), dest);
		}
	}
	// vertex normals
	const GA_Attribute *nattr = gdp.findAttribute(GA_ATTRIB_POINT, GEO_STD_ATTRIB_NORMAL);
	GA_ROAttributeRef n(nattr);
	if (n.isValid()) {
		expData.normals = VRay::VUtils::VectorRefList(expData.numPoints);
		const GA_AIFTuple *aifTuple = n.getAIFTuple();
		if (aifTuple) {
			VRay::real *dest = &(expData.normals.get()->x);
			aifTuple->getRange(n, gdp.getPointRange(), dest);
		}
	}


	// NOTE: Support only tri-faces for now
	// TODO:
	//   [ ] > 4 vertex faces support
	//   [x] edge_visibility
	//
	GA_ROAttributeRef ref_shop_materialpath = gdp.findStringTuple(GA_ATTRIB_PRIMITIVE, GEO_STD_ATTRIB_MATERIAL);
	const GA_ROHandleS hndl_shop_materialpath(ref_shop_materialpath.getAttribute());

	expData.numFaces = 0;
	expData.numMtlIDs = 0;
	for (GA_Iterator jt(gdp.getPrimitiveRange()); !jt.atEnd(); jt.advance()) {
		const GA_Offset off = *jt;
		const GEO_Primitive *prim = gdp.getGEOPrimitive(off);

		GA_PrimitiveTypeId primType = prim->getTypeId();
		if (   primType != GEO_PRIMPOLY
			&& primType != GEO_PRIMPOLYSOUP)
		{
			continue;
		}

		if (prim->getTypeId() == GEO_PRIMPOLYSOUP) {
			const GU_PrimPolySoup *polySoup = static_cast<const GU_PrimPolySoup*>(prim);

			for (GEO_PrimPolySoup::PolygonIterator psIt(*polySoup); !psIt.atEnd(); ++psIt) {
				expData.numFaces += (psIt.getVertexCount() == 4) ? 2 : 1;
			}
		}
		else {
			expData.numFaces += (prim->getVertexCount() == 4) ? 2 : 1;
		}


		if (hndl_shop_materialpath.isValid()) {
			const char *shop_materialpath = hndl_shop_materialpath.get(off);
			if (shop_materialpath && (expParams.shopToID.find(shop_materialpath) == expParams.shopToID.end())) {
				expParams.shopToID.insert(shop_materialpath, expData.numMtlIDs++);
			}
		}
	}

	expData.faces = VRay::VUtils::IntRefList(expData.numFaces * 3);
	expData.face_mtlIDs = VRay::VUtils::IntRefList(expData.numFaces);
	expData.edge_visibility = VRay::VUtils::IntRefList(expData.numFaces / 10 + ((expData.numFaces % 10 > 0) ? 1 : 0));

	// Reset some arrays
	memset(expData.edge_visibility.get(), 0, expData.edge_visibility.size() * sizeof(int));

	int faceVertIndex = 0;
	int faceMtlIDIndex = 0;
	int faceEdgeVisIndex = 0;
	for (GA_Iterator jt(gdp.getPrimitiveRange()); !jt.atEnd(); jt.advance()) {
		const GA_Offset off = *jt;
		const GEO_Primitive *prim = gdp.getGEOPrimitive(off);

		GA_PrimitiveTypeId primType = prim->getTypeId();
		if (   primType != GEO_PRIMPOLY
			&& primType != GEO_PRIMPOLYSOUP)
		{
			continue;
		}

		int mtlId = 0;

		if (hndl_shop_materialpath.isValid()) {
			const char *shop_materialpath = hndl_shop_materialpath.get(off);
			if (shop_materialpath) {
				mtlId = expParams.shopToID[shop_materialpath];
			}
		}

		if (prim->getTypeId().get() == GEO_PRIMPOLYSOUP) {
			const GU_PrimPolySoup *polySoup = static_cast<const GU_PrimPolySoup*>(prim);

			for (GEO_PrimPolySoup::PolygonIterator psIt(*polySoup); !psIt.atEnd(); ++psIt) {
				expData.faces[faceVertIndex++] = psIt.getPointIndex(2);
				expData.faces[faceVertIndex++] = psIt.getPointIndex(1);
				expData.faces[faceVertIndex++] = psIt.getPointIndex(0);

				expData.face_mtlIDs[faceMtlIDIndex++] = mtlId;

				if (psIt.getVertexCount() == 4) {
					expData.faces[faceVertIndex++] = psIt.getPointIndex(3);
					expData.faces[faceVertIndex++] = psIt.getPointIndex(2);
					expData.faces[faceVertIndex++] = psIt.getPointIndex(0);

					expData.edge_visibility[faceEdgeVisIndex/10] |= (3 << ((faceEdgeVisIndex%10)*3));
					faceEdgeVisIndex++;
					expData.edge_visibility[faceEdgeVisIndex/10] |= (6 << ((faceEdgeVisIndex%10)*3));
					faceEdgeVisIndex++;

					expData.face_mtlIDs[faceMtlIDIndex++] = mtlId;
				}
				else {
					expData.edge_visibility[faceEdgeVisIndex/10] |= (7 << ((faceEdgeVisIndex%10)*3));
					faceEdgeVisIndex++;
				}
			}
		}
		else {
			expData.faces[faceVertIndex++] = prim->getPointIndex(2);
			expData.faces[faceVertIndex++] = prim->getPointIndex(1);
			expData.faces[faceVertIndex++] = prim->getPointIndex(0);

			expData.face_mtlIDs[faceMtlIDIndex++] = mtlId;

			if (prim->getVertexCount() == 4) {
				expData.faces[faceVertIndex++] = prim->getPointIndex(3);
				expData.faces[faceVertIndex++] = prim->getPointIndex(2);
				expData.faces[faceVertIndex++] = prim->getPointIndex(0);

				expData.edge_visibility[faceEdgeVisIndex/10] |= (3 << ((faceEdgeVisIndex%10)*3));
				faceEdgeVisIndex++;
				expData.edge_visibility[faceEdgeVisIndex/10] |= (6 << ((faceEdgeVisIndex%10)*3));
				faceEdgeVisIndex++;

				expData.face_mtlIDs[faceMtlIDIndex++] = mtlId;
			}
			else {
				expData.edge_visibility[faceEdgeVisIndex/10] |= (7 << ((faceEdgeVisIndex%10)*3));
				faceEdgeVisIndex++;
			}
		}
	}

	exportPrimitiveAttrs(m_context, gdp, expParams, expData);
	exportVertexAttrs(gdp, expParams, expData);
	exportPointAttrs(gdp, expParams, expData);

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
	bool hasPolyGeometry = gdp.containsPrimitiveType(GEO_PRIMPOLY) || gdp.containsPrimitiveType(GEO_PRIMPOLY);
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

	fpreal t = m_pluginExporter.getContext().getTime();
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

	VRay::Transform tm = VRayExporter::getObjTransform(&m_objNode, m_pluginExporter.getContext());

	int i = 0;
	for (Attrs::PluginDesc &nodeDesc : pluginList) {
//		TODO: need to fill in node with appropriate names and export them
		VRay::Plugin mtl = m_pluginExporter.exportDefaultMaterial();
		nodeDesc.pluginName = VRayExporter::getPluginName(&m_objNode, boost::str(Parm::FmtPrefixManual % "Node" % std::to_string(i++)));
		nodeDesc.addAttribute(Attrs::PluginAttr("material", mtl));

		Attrs::PluginAttr *attr = nodeDesc.get("transform");
		if (attr) {
			attr->paramValue.valTransform = tm * attr->paramValue.valTransform;
		}
		else {
			nodeDesc.addAttribute(Attrs::PluginAttr("transform", tm));
		}

		VRay::Plugin plugin = m_pluginExporter.exportPlugin(nodeDesc);
		if (plugin) {
			m_pluginList.emplace_back(plugin);
		}
	}

	return m_pluginList.size();
}

#include <GEO/GEO_PrimType.h>

int GeometryExporter::exportDetail(SOP_Node &sop, GU_DetailHandleAutoReadLock &gdl, PluginDescList &pluginList)
{
//	TODO: verify nPlugins = ?
	int nPlugins = 0;

	const GU_Detail &gdp = *gdl.getGdp();
	for (GA_Iterator jt(gdp.getPrimitiveRange()); !jt.atEnd(); jt.advance()) {
		const GA_Primitive *prim = gdp.getPrimitive(*jt);
		if (GU_PrimPacked::isPackedPrimitive(*prim)) {
			const GU_PrimPacked *primPacked = dynamic_cast< const GU_PrimPacked * >(prim);
			nPlugins += exportPacked(sop, *primPacked, pluginList);
		}
	}

	VRay::Plugin plugin = m_pluginExporter.exportGeomStaticMesh(sop, gdp, m_expParams);
	if (plugin) {
		pluginList.push_back(Attrs::PluginDesc("", "Node"));
		++nPlugins;

		Attrs::PluginDesc &nodeDesc = pluginList.back();
		nodeDesc.addAttribute(Attrs::PluginAttr("geometry", plugin));
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

	PluginDescList primPluginList = m_detailToPluginDesc.at(packedHash);
	for (Attrs::PluginDesc &pluginDesc : primPluginList) {
		pluginList.push_back(pluginDesc);
		Attrs::PluginDesc &newNodeDesc = pluginList.back();
		// TODO: adjust other props like mtls
		Attrs::PluginAttr *attr = newNodeDesc.get("transform");
		if (attr) {
			attr->paramValue.valTransform =  tm * attr->paramValue.valTransform;
		}
		else {
			newNodeDesc.addAttribute(Attrs::PluginAttr("transform", tm));
		}
	}

	return primPluginList.size();
}


int GeometryExporter::exportPrimitive(SOP_Node &sop, const GU_PrimPacked &prim, PluginDescList &pluginList)
{
	// vray proxy should load geometry as a single packed primitive
	// may have to use custom packed primitive for that
	// and create prim attibute "path" pointing to : op:<path to VRayProxy SOP>
	// in order to be able to get query rest of the plugin settings
	//
	// packed primitives usually have a standart attribute "path"
	// if exists it can eigther reference a file on disk or a sop node that created the paked primitive
	// so we need to check for the attribute and if existing then either :
	//                  interpret it as sop reference ( VRayProxy SOP / Pack SOP / etc.)
	//                  interpret it as file path
	//                  take geoemtry directly from packed GU_Detail
	// otherwise take geoemtry directly from packed GU_Detail

	int nPlugins = 0;

	const GA_ROHandleS pathHndl(prim.getDetail().findAttribute(GA_ATTRIB_PRIMITIVE, "path"));
	if (NOT(pathHndl.isValid())) {
		// there is no path attribute =>
		// take geometry taken directly from primitive's packed detail
		GU_DetailHandleAutoReadLock gdl(prim.getPackedDetail());
		if (gdl.isValid()) {
			nPlugins = exportDetail(sop, gdl, pluginList);
		}
	}
	else {
		UT_StringRef path = pathHndl.get(prim.getMapOffset());
		if (NOT(path.isstring()) || NOT(path.length())) {
			// there is no path attribute assigned =>
			// take geometry taken directly from primitive's packed detail
			GU_DetailHandleAutoReadLock gdl(prim.getPackedDetail());
			if (gdl.isValid()) {
				nPlugins = exportDetail(sop, gdl, pluginList);
			}
		}
		else {
			SOP_Node *sopref = OPgetDirector()->findSOPNode(path);
			if (NOT(sopref)) {
				// there is path attribute, but it is NOT holding a ref to SOP node =>
				// interpret the string as filepath and export geometry as VRayProxy plugin
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
					// take geometry from SOP's input detail if there is valid input else
					// take geometry from primitive's packed detail
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
