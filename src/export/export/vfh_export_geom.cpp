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

#include <SOP/SOP_Node.h>
#include <GU/GU_Detail.h>
#include <GU/GU_PrimPolySoup.h>
#include <UT/UT_Version.h>
#include <GA/GA_PageHandle.h>


using namespace VRayForHoudini;


// Primitive color override example
//   "diffuser" : 1.0, "diffuseg" : 1.0, "diffuseb" : 1.0
// We don't care about the separate channels, we have to export attribute as "diffuse",
// so we need to go through all the "material" nodes inside the network and collect actual
// parameters
//
void VRayExporter::collectMaterialOverrideParameters(OBJ_Node &obj)
{
	const fpreal t = m_context.getTime();

	for (int c = 0; c < obj.getNchildren(); ++c) {
		OP_Node *node = obj.getChild(c);
		if (node) {
			const OP_Operator *nodeOp = node->getOperator();
			if (nodeOp && nodeOp->getName() == "material") {
				Log::getLog().msg("  Found material node: %s",
								  node->getName().buffer());

				const int numMaterials = node->evalInt("num_materials", 0, t);
				for (int mtlIdx = 1; mtlIdx <= numMaterials; ++mtlIdx) {
					static boost::format FmtShopPath("shop_materialpath%i");

					UT_String shopMaterial;
					node->evalString(shopMaterial, boost::str(FmtShopPath % mtlIdx).c_str(), 0, t);
					if (shopMaterial.length()) {
						static boost::format FmtNumLocal("num_local%i");

						const int numLocal = node->evalInt(boost::str(FmtNumLocal % mtlIdx).c_str(), 0, t);
						for (int localIdx = 1; localIdx <= numLocal; ++localIdx) {
							static boost::format FmtLocalName("local%i_name%i");
							static boost::format FmtLocalType("local%i_type%i");

							UT_String localName;
							node->evalString(localName, boost::str(FmtLocalName % mtlIdx % localIdx).c_str(), 0, t);
							if (localName.length()) {
								const int localType = node->evalInt(boost::str(FmtLocalType % mtlIdx % localIdx).c_str(), 0, t);

								Log::getLog().msg("  Found override \"%s\" [%i] for material \"%s\"",
												  localName.buffer(), localType, shopMaterial.buffer());
							}
						}
					}
				}
			}
		}
	}
}


struct GeomExportData {
	VUtils::VectorRefList vertices;
	VUtils::VectorRefList normals;
	VUtils::IntRefList faces;
	VUtils::IntRefList face_mtlIDs;
	VUtils::IntRefList edge_visibility;
	Mesh::MapChannels map_channels_data;
	int numFaces;
	int numMtlIDs;
};


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
#if CGR_USE_LIST_RAW_TYPES
		map_channel.vertices = VUtils::VectorRefList(map_channel.verticesSet.size());
		map_channel.faces = VUtils::IntRefList(numFaces * 3);
#else
		map_channel.vertices.resize(map_channel.verticesSet.size());
		map_channel.faces.resize(numFaces * 3);
#endif

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
			const GEO_Primitive *face = gdp.getGEOPrimitive(*jt);

			const int &v0 = map_channel.verticesSet.find(Mesh::MapVertex(vaHndl.get(face->getVertexOffset(0))))->index;
			const int &v1 = map_channel.verticesSet.find(Mesh::MapVertex(vaHndl.get(face->getVertexOffset(1))))->index;
			const int &v2 = map_channel.verticesSet.find(Mesh::MapVertex(vaHndl.get(face->getVertexOffset(2))))->index;

			map_channel.faces[faceMapVertIndex++] = v0;
			map_channel.faces[faceMapVertIndex++] = v1;
			map_channel.faces[faceMapVertIndex++] = v2;

			if (face->getVertexCount() == 4) {
				const int &v3 = map_channel.verticesSet.find(Mesh::MapVertex(vaHndl.get(face->getVertexOffset(3))))->index;

				map_channel.faces[faceMapVertIndex++] = v0;
				map_channel.faces[faceMapVertIndex++] = v2;
				map_channel.faces[faceMapVertIndex++] = v3;
			}
		}
		vassert( faceMapVertIndex == map_channel.faces.size() );

		// Cleanup hash
		map_channel.verticesSet.clear();
	}
	else {
		// populate map channel with original values

		// Init map channel data
#if CGR_USE_LIST_RAW_TYPES
		map_channel.vertices = VUtils::VectorRefList(gdp.getNumVertices());
		map_channel.faces = VUtils::IntRefList(numFaces * 3);
#else
		map_channel.vertices.resize(gdp.getNumVertices());
		map_channel.faces.resize(numFaces * 3);
#endif

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

			if (prim->getTypeId().get() == GEO_PRIMPOLYSOUP) {
				const GU_PrimPolySoup *polySoup = static_cast<const GU_PrimPolySoup*>(prim);
				for (GEO_PrimPolySoup::PolygonIterator psIt(*polySoup); !psIt.atEnd(); ++psIt) {
					map_channel.faces[i++] = psIt.getVertexIndex(0);
					map_channel.faces[i++] = psIt.getVertexIndex(1);
					map_channel.faces[i++] = psIt.getVertexIndex(2);

					if (psIt.getVertexCount() == 4) {
						map_channel.faces[i++] = psIt.getVertexIndex(0);
						map_channel.faces[i++] = psIt.getVertexIndex(2);
						map_channel.faces[i++] = psIt.getVertexIndex(3);
					}
				}
			}
			else {
				map_channel.faces[i++] = vi;
				map_channel.faces[i++] = vi + 1;
				map_channel.faces[i++] = vi + 2;

				if (prim->getVertexCount() == 4) {
					map_channel.faces[i++] = vi;
					map_channel.faces[i++] = vi + 2;
					map_channel.faces[i++] = vi + 3;
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
#if CGR_USE_LIST_RAW_TYPES
					map_channel.vertices = VUtils::VectorRefList(gdp.getNumPoints());
					map_channel.faces = expData.faces;
#else
					map_channel.vertices.resize(gdp.getNumPoints());
					map_channel.faces.assign(expData.faces.get(), expData.faces.get() + expData.faces.size());
#endif
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


void VRayExporter::exportGeomStaticMeshDesc(const GU_Detail &gdp, GeomExportParams &expParams, Attrs::PluginDesc &geomPluginDesc)
{
	const int numPoints = gdp.getNumPoints();
	Log::getLog().info("  Mesh: %i points", numPoints);

	GeomExportData expData;
	expData.vertices = VUtils::VectorRefList(numPoints);

	GA_ROAttributeRef h = gdp.findFloatTuple(GA_ATTRIB_POINT, "N", 3);
	const GA_ROHandleV3 N_h(h.getAttribute());
	if (N_h.isValid()) {
		expData.normals = VUtils::VectorRefList(numPoints);
	}

	int v = 0;
	for (GA_Iterator pIt(gdp.getPointRange()); !pIt.atEnd(); pIt.advance(), ++v) {
#if UT_MAJOR_VERSION_INT < 14
		const GEO_Point *pt = gdp.getGEOPoint(*pIt);
		const UT_Vector4 &p = pt->getPos();
#else
		const UT_Vector3 &p = gdp.getPos3(*pIt);
#endif
		expData.vertices[v].set(p[0], p[1], p[2]);

		if (N_h.isValid()) {
			UT_Vector3 N(N_h.get(*pIt));
			N.normalize();
			N = -N; // NOTE: Have no idea why...
			expData.normals[v].set(N[0], N[1], N[2]);
		}
	}

	// NOTE: Support only tri-faces for now
	// TODO:
	//   [ ] > 4 vertex faces support
	//   [x] edge_visibility
	//
	GA_ROAttributeRef ref_shop_materialpath = gdp.findStringTuple(GA_ATTRIB_PRIMITIVE, "shop_materialpath");
	const GA_ROHandleS hndl_shop_materialpath(ref_shop_materialpath.getAttribute());

	expData.numFaces = 0;
	expData.numMtlIDs = 0;
	for (GA_Iterator offIt(gdp.getPrimitiveRange()); !offIt.atEnd(); offIt.advance()) {
		const GA_Offset off = *offIt;
		const GEO_Primitive *prim = gdp.getGEOPrimitive(off);

		if (prim->getTypeId().get() == GEO_PRIMPOLYSOUP) {
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

	expData.faces = VUtils::IntRefList(expData.numFaces * 3);
	expData.face_mtlIDs = VUtils::IntRefList(expData.numFaces);
	expData.edge_visibility = VUtils::IntRefList(expData.numFaces / 10 + ((expData.numFaces % 10 > 0) ? 1 : 0));

	// Reset some arrays
	memset(expData.edge_visibility.ptr, 0, expData.edge_visibility.size() * sizeof(int));

	int faceVertIndex = 0;
	int faceMtlIDIndex = 0;
	int faceEdgeVisIndex = 0;
	for (GA_Iterator jt(gdp.getPrimitiveRange()); !jt.atEnd(); jt.advance()) {
		const GA_Offset off = *jt;
		const GEO_Primitive *prim = gdp.getGEOPrimitive(off);

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
				expData.faces[faceVertIndex++] = psIt.getPointIndex(0);
				expData.faces[faceVertIndex++] = psIt.getPointIndex(1);
				expData.faces[faceVertIndex++] = psIt.getPointIndex(2);

				expData.face_mtlIDs[faceMtlIDIndex++] = mtlId;

				if (psIt.getVertexCount() == 4) {
					expData.faces[faceVertIndex++] = psIt.getPointIndex(0);
					expData.faces[faceVertIndex++] = psIt.getPointIndex(2);
					expData.faces[faceVertIndex++] = psIt.getPointIndex(3);

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
			expData.faces[faceVertIndex++] = prim->getPointIndex(0);
			expData.faces[faceVertIndex++] = prim->getPointIndex(1);
			expData.faces[faceVertIndex++] = prim->getPointIndex(2);

			expData.face_mtlIDs[faceMtlIDIndex++] = mtlId;

			if (prim->getVertexCount() == 4) {
				expData.faces[faceVertIndex++] = prim->getPointIndex(0);
				expData.faces[faceVertIndex++] = prim->getPointIndex(2);
				expData.faces[faceVertIndex++] = prim->getPointIndex(3);

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
		VRay::ValueList map_channel_names;
		VRay::ValueList map_channels;

		int i = 0;
		for (const auto &mcIt : expData.map_channels_data) {
			const std::string      &map_channel_name = mcIt.first;
			const Mesh::MapChannel &map_channel_data = mcIt.second;

			// Channel name attribute
			map_channel_names.push_back(VRay::Value(map_channel_name));

			// Channel data
			VRay::ValueList map_channel;
			map_channel.push_back(VRay::Value(i++));
			map_channel.push_back(VRay::Value(map_channel_data.vertices));
			map_channel.push_back(VRay::Value(map_channel_data.faces));

			map_channels.push_back(VRay::Value(map_channel));
		}

		geomPluginDesc.addAttribute(Attrs::PluginAttr("map_channels_names", map_channel_names));
		geomPluginDesc.addAttribute(Attrs::PluginAttr("map_channels",      map_channels));
	}
}


VRay::Plugin VRayExporter::exportGeomStaticMesh(SOP_Node &sop_node, const GU_Detail &gdp, GeomExportParams &expParams)
{
	Attrs::PluginDesc geomPluginDesc(VRayExporter::getPluginName(&sop_node, "Geom"), "GeomStaticMesh");
	exportGeomStaticMeshDesc(gdp, expParams, geomPluginDesc);

	return exportPlugin(geomPluginDesc);
}
