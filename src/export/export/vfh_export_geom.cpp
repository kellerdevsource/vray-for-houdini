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


using namespace VRayForHoudini;


void VRayExporter::exportGeomStaticMeshDesc(const GU_Detail &gdp, SHOPToID &shopToID, Attrs::PluginDesc &geomPluginDesc)
{
	const int numPoints = gdp.getNumPoints();

	Log::getLog().info("  Mesh: %i points", numPoints);

	VUtils::VectorRefList vertices(numPoints);
	VUtils::VectorRefList normals;

	GA_ROAttributeRef h = gdp.findFloatTuple(GA_ATTRIB_POINT, "N", 3);
	const GA_ROHandleV3 N_h(h.getAttribute());
	if (N_h.isValid()) {
		normals = VUtils::VectorRefList(numPoints);
	}

	int v = 0;
	for (GA_Iterator pIt(gdp.getPointRange()); !pIt.atEnd(); pIt.advance(), ++v) {
#if UT_MAJOR_VERSION_INT < 14
		const GEO_Point *pt = gdp.getGEOPoint(*pIt);
		const UT_Vector4 &p = pt->getPos();
#else
		const UT_Vector3 &p = gdp.getPos3(*pIt);
#endif
		vertices[v].set(p[0], p[1], p[2]);

		if (N_h.isValid()) {
			UT_Vector3 N(N_h.get(*pIt));
			N.normalize();
			N = -N; // NOTE: Have no idea why...
			normals[v].set(N[0], N[1], N[2]);
		}
	}

	// NOTE: Support only tri-faces for now
	// TODO:
	//   [ ] > 4 vertex faces support (GU_PrimPacked)
	//   [x] edge_visibility
	//

	GA_ROAttributeRef ref_shop_materialpath = gdp.findStringTuple(GA_ATTRIB_PRIMITIVE, "shop_materialpath");
	const GA_ROHandleS hndl_shop_materialpath(ref_shop_materialpath.getAttribute());

	int numFaces = 0;
	int numMtlIDs = 0;
	for (GA_Iterator offIt(gdp.getPrimitiveRange()); !offIt.atEnd(); offIt.advance()) {
		const GA_Offset off = *offIt;
		const GEO_Primitive *prim = gdp.getGEOPrimitive(off);

		if (prim->getTypeId().get() == GEO_PRIMPOLYSOUP) {
			const GU_PrimPolySoup *polySoup = static_cast<const GU_PrimPolySoup*>(prim);

			for (GEO_PrimPolySoup::PolygonIterator psIt(*polySoup); !psIt.atEnd(); ++psIt) {
				numFaces += (psIt.getVertexCount() == 4) ? 2 : 1;
			}
		}
		else {
			numFaces += (prim->getVertexCount() == 4) ? 2 : 1;
		}

		if (hndl_shop_materialpath.isValid()) {
			const char *shop_materialpath = hndl_shop_materialpath.get(off);
			if (shop_materialpath && (shopToID.find(shop_materialpath) == shopToID.end())) {
				shopToID.insert(shop_materialpath, numMtlIDs++);
			}
		}
	}
#if 0
	if (shopToID.size()) {
		Log::getLog().info("Materials list: %i",
					shopToID.size());

		for (SHOPToID::iterator oIt = shopToID.begin(); oIt != shopToID.end(); ++oIt) {
			Log::getLog().info("  %i: \"%s\"",
						oIt.data(), oIt.key());
		}
	}
#endif
	VUtils::IntRefList faces(numFaces * 3);
	VUtils::IntRefList face_mtlIDs(numFaces);
	VUtils::IntRefList edge_visibility(numFaces / 10 + ((numFaces % 10 > 0) ? 1 : 0));

	// Reset some arrays
	memset(edge_visibility.ptr, 0, edge_visibility.size() * sizeof(int));

	Mesh::MapChannels map_channels_data;

	for (GA_Iterator jt(gdp.getPrimitiveRange()); !jt.atEnd(); jt.advance()) {
		const GEO_Primitive *prim = gdp.getGEOPrimitive(*jt);

		const GA_AttributeDict &vertex_attrs = gdp.vertexAttribs();
		for (const auto &vaIt : vertex_attrs) {
			const GA_Attribute *vertex_attr = vaIt;
			if (vertex_attr->getScope() == GA_SCOPE_PUBLIC) {
				const GA_ROHandleV3 hndl_vertex_attr(vertex_attr);
				if (hndl_vertex_attr.isValid()) {
					if (StrEq(vertex_attr->getName(), "N")) {
						continue;
					}

					const char *vertexAttrName = GA::getGaAttributeName(*vertex_attr);

					Mesh::MapChannel &map_channel = map_channels_data[vertexAttrName];

					if (prim->getTypeId().get() == GEO_PRIMPOLYSOUP) {
						const GU_PrimPolySoup *polySoup = static_cast<const GU_PrimPolySoup*>(prim);

						for (GEO_PrimPolySoup::PolygonIterator psIt(*polySoup); !psIt.atEnd(); ++psIt) {
							for (GEO_PrimPolySoup::VertexIterator vIt(psIt); !vIt.atEnd(); ++vIt) {
								const GA_Offset &vOff = gdp.vertexPoint(vIt.offset());
								const UT_Vector3 &v = hndl_vertex_attr.get(vOff);

								map_channel.verticesSet.insert(Mesh::MapVertex(v));
							}
						}
					}
					else {
						for (int c = 0; c < prim->getVertexCount(); ++c) {
							const GA_Offset  &vOff = prim->getVertexOffset(c);
							const UT_Vector3 &v = hndl_vertex_attr.get(vOff);

							map_channel.verticesSet.insert(Mesh::MapVertex(v));
						}
					}
				}
			}
		}
	}

	// Init map channels data
	for (auto &mcIt : map_channels_data) {
		const std::string &map_channel_name = mcIt.first;
		Mesh::MapChannel  &map_channel = mcIt.second;

		Log::getLog().info("  Found map channel: %s",
					map_channel_name.c_str());

#if CGR_USE_LIST_RAW_TYPES
		map_channel.vertices = VUtils::VectorRefList(map_channel.verticesSet.size());
#else
		map_channel.vertices.resize(map_channel.verticesSet.size());
#endif

		int i = 0;
		for (auto &mv: map_channel.verticesSet) {
			mv.index = i;
			map_channel.vertices[i++].set(mv.v[0], mv.v[1], mv.v[2]);
		}

#if CGR_USE_LIST_RAW_TYPES
		map_channel.faces = VUtils::IntRefList(numFaces * 3);
#else
		map_channel.faces.resize(numFaces * 3);
#endif
	}

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
				mtlId = shopToID[shop_materialpath];
			}
		}

		if (prim->getTypeId().get() == GEO_PRIMPOLYSOUP) {
			const GU_PrimPolySoup *polySoup = static_cast<const GU_PrimPolySoup*>(prim);

			for (GEO_PrimPolySoup::PolygonIterator psIt(*polySoup); !psIt.atEnd(); ++psIt) {
				faces[faceVertIndex++] = psIt.getPointIndex(0);
				faces[faceVertIndex++] = psIt.getPointIndex(1);
				faces[faceVertIndex++] = psIt.getPointIndex(2);

				face_mtlIDs[faceMtlIDIndex++] = mtlId;

				if (psIt.getVertexCount() == 4) {
					faces[faceVertIndex++] = psIt.getPointIndex(0);
					faces[faceVertIndex++] = psIt.getPointIndex(2);
					faces[faceVertIndex++] = psIt.getPointIndex(3);

					edge_visibility[faceEdgeVisIndex/10] |= (3 << ((faceEdgeVisIndex%10)*3));
					faceEdgeVisIndex++;
					edge_visibility[faceEdgeVisIndex/10] |= (6 << ((faceEdgeVisIndex%10)*3));
					faceEdgeVisIndex++;

					face_mtlIDs[faceMtlIDIndex++] = mtlId;
				}
				else {
					edge_visibility[faceEdgeVisIndex/10] |= (7 << ((faceEdgeVisIndex%10)*3));
					faceEdgeVisIndex++;
				}
			}
		}
		else {
			faces[faceVertIndex++] = prim->getPointIndex(0);
			faces[faceVertIndex++] = prim->getPointIndex(1);
			faces[faceVertIndex++] = prim->getPointIndex(2);

			face_mtlIDs[faceMtlIDIndex++] = mtlId;

			if (prim->getVertexCount() == 4) {
				faces[faceVertIndex++] = prim->getPointIndex(0);
				faces[faceVertIndex++] = prim->getPointIndex(2);
				faces[faceVertIndex++] = prim->getPointIndex(3);

				edge_visibility[faceEdgeVisIndex/10] |= (3 << ((faceEdgeVisIndex%10)*3));
				faceEdgeVisIndex++;
				edge_visibility[faceEdgeVisIndex/10] |= (6 << ((faceEdgeVisIndex%10)*3));
				faceEdgeVisIndex++;

				face_mtlIDs[faceMtlIDIndex++] = mtlId;
			}
			else {
				edge_visibility[faceEdgeVisIndex/10] |= (7 << ((faceEdgeVisIndex%10)*3));
				faceEdgeVisIndex++;
			}
		}
	}

	// Process map channels (uv and other tuple(3) attributes)
	//
	const GA_AttributeDict &vertex_attrs = gdp.vertexAttribs();
	for (const auto &vaIt : vertex_attrs) {
		const GA_Attribute *vertex_attr = vaIt;
		if (vertex_attr->getScope() == GA_SCOPE_PUBLIC) {
			const GA_ROHandleV3 hndl_vertex_attr(vertex_attr);
			if (hndl_vertex_attr.isValid()) {
				if (StrEq(vertex_attr->getName(), "N")) {
					continue;
				}

				int faceMapVertIndex = 0;

				const char *vertexAttrName = GA::getGaAttributeName(*vertex_attr);

				for (GA_Iterator jt(gdp.getPrimitiveRange()); !jt.atEnd(); jt.advance()) {
					const GEO_Primitive *face = gdp.getGEOPrimitive(*jt);
					Mesh::MapChannel &map_channel = map_channels_data[vertexAttrName];

					const int &v0 = map_channel.verticesSet.find(Mesh::MapVertex(hndl_vertex_attr.get(face->getVertexOffset(0))))->index;
					const int &v1 = map_channel.verticesSet.find(Mesh::MapVertex(hndl_vertex_attr.get(face->getVertexOffset(1))))->index;
					const int &v2 = map_channel.verticesSet.find(Mesh::MapVertex(hndl_vertex_attr.get(face->getVertexOffset(2))))->index;

					map_channel.faces[faceMapVertIndex++] = v0;
					map_channel.faces[faceMapVertIndex++] = v1;
					map_channel.faces[faceMapVertIndex++] = v2;

					if (face->getVertexCount() == 4) {
						const int &v3 = map_channel.verticesSet.find(Mesh::MapVertex(hndl_vertex_attr.get(face->getVertexOffset(3))))->index;

						map_channel.faces[faceMapVertIndex++] = v0;
						map_channel.faces[faceMapVertIndex++] = v2;
						map_channel.faces[faceMapVertIndex++] = v3;
					}
				}
			}
		}
	}

	// Cleanup hash
	for (auto &mcIt : map_channels_data) {
		Mesh::MapChannel &map_channel = mcIt.second;
		map_channel.verticesSet.clear();
	}

	//	description
	geomPluginDesc.addAttribute(Attrs::PluginAttr("vertices", vertices));
	geomPluginDesc.addAttribute(Attrs::PluginAttr("faces", faces));
	geomPluginDesc.addAttribute(Attrs::PluginAttr("face_mtlIDs", face_mtlIDs));
	geomPluginDesc.addAttribute(Attrs::PluginAttr("edge_visibility", edge_visibility));

	if (isIPR() && isGPU()) {
		geomPluginDesc.addAttribute(Attrs::PluginAttr("dynamic_geometry", true));
	}

	if (normals.size()) {
		geomPluginDesc.addAttribute(Attrs::PluginAttr("normals", normals));
		geomPluginDesc.addAttribute(Attrs::PluginAttr("faceNormals", faces));
	}

	if (map_channels_data.size()) {
		VRay::ValueList map_channel_names;
		VRay::ValueList map_channels;

		int i = 0;
		for (const auto &mcIt : map_channels_data) {
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

		geomPluginDesc.addAttribute(Attrs::PluginAttr("map_channel_names", map_channel_names));
		geomPluginDesc.addAttribute(Attrs::PluginAttr("map_channels",      map_channels));
	}
}


VRay::Plugin VRayExporter::exportGeomStaticMesh(SOP_Node &sop_node, const GU_Detail &gdp, SHOPToID &shopToID)
{
	Attrs::PluginDesc geomPluginDesc(VRayExporter::getPluginName(&sop_node, "Geom"), "GeomStaticMesh");
	exportGeomStaticMeshDesc(gdp, shopToID, geomPluginDesc);

	return exportPlugin(geomPluginDesc);
}
