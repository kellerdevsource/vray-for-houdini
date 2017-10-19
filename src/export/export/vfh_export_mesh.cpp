//
// Copyright (c) 2015-2017, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#include "vfh_export_mesh.h"
#include "vfh_attr_utils.h"

#include <SHOP/SHOP_GeoOverride.h>
#include <GU/GU_Detail.h>
#include <GU/GU_PrimPolySoup.h>
#include <GU/GU_PrimPoly.h>
#include <GA/GA_PageHandle.h>
#include <GA/GA_Names.h>

#include <GSTY/GSTY_SubjectPrimGroup.h>
#include <STY/STY_StylerGroup.h>

using namespace VRayForHoudini;
using namespace Hash;

static boost::format extMapChannelFmt("ExtMapChannels|%i@%s");
static boost::format texExtMaterialIDFmt("TexExtMaterialID|%i@%s");
static boost::format mtlMultiFmt("MtlMulti|%i@%s");

/// This is a specific value for TexUserColor / TexUserScalar
/// to specify that attribute is unset for a particular face.
static const float ALMOST_FLT_MAX = FLT_MAX;

template <typename T>
static MHash getVRayValueHash(const VRay::VUtils::PtrArray<T> &idsList)
{
	MHash hash;
	MurmurHash3_x86_32(idsList.get(), idsList.count() * sizeof(T), 42, &hash);
	return hash;
}

static MHash getVRayValueHash(const VRay::VUtils::CharString &value)
{
	MHash hash;
	MurmurHash3_x86_32(value.ptr(), value.length() * sizeof(tchar), 42, &hash);
	return hash;
}

static MHash getMaterialIdListHash(VRay::VUtils::IntRefList idsList)
{ 
	return getVRayValueHash(idsList);
}

static MHash getMapChannelsHash(VRay::VUtils::ValueRefList mapChannels)
{ 
	MHash hash = 0;

	for (int chanIdx = 0; chanIdx < mapChannels.count(); ++chanIdx) {
		VRay::VUtils::ValueRefList mapChannel = mapChannels[chanIdx].getList();

		const MHash nameHash = getVRayValueHash(mapChannel[0].getString());
		const MHash vertHash = getVRayValueHash(mapChannel[1].getListVector());
		const MHash faceHash = getVRayValueHash(mapChannel[2].getListInt());

		hash ^= (nameHash ^ vertHash ^ faceHash);
	}

	return hash;
}

/// Helper funtion to copy data from Float3Tuple attribute into a vector list
/// @param attr[in] - the attribute to copy
/// @param data[out] - destination vector list
/// @retval true on success
static bool getDataFromAttribute(const GA_Attribute *attr, VRay::VUtils::VectorRefList &data)
{
	GA_ROAttributeRef attrRef(attr);
	if (attrRef.isInvalid()) {
		return false;
	}

	const GA_AIFTuple *aifTuple = attrRef.getAIFTuple();
	if (!aifTuple) {
		return false;
	}

	data = VRay::VUtils::VectorRefList(attr->getIndexMap().indexSize());
	return aifTuple->getRange(attr, GA_Range(attr->getIndexMap()), &(data.get()->x));
}


MeshExporter::MeshExporter(OBJ_Node &obj, const GU_Detail &gdp, OP_Context &ctx, VRayExporter &exp, ObjectExporter &objectExporter, const GEOPrimList &primList)
	: PrimitiveExporter(obj, ctx, exp)
	, primList(primList)
	, gdp(gdp)
	, objectExporter(objectExporter)
	, m_hasSubdivApplied(false)
	, numFaces(-1)
{}

void MeshExporter::reset()
{
	numFaces = -1;
	faces = VRay::VUtils::IntRefList();
	edge_visibility = VRay::VUtils::IntRefList();
	vertices = VRay::VUtils::VectorRefList();
	normals = VRay::VUtils::VectorRefList();
	m_faceNormals = VRay::VUtils::IntRefList();
	velocities = VRay::VUtils::VectorRefList();
	map_channels_data.clear();
}

bool MeshExporter::asPluginDesc(const GU_Detail &gdp, Attrs::PluginDesc &pluginDesc)
{
	if (!primList.size()) {
		return false;
	}

	if (pluginExporter.isIPR() && pluginExporter.isGPU()) {
		pluginDesc.addAttribute(Attrs::PluginAttr("dynamic_geometry", true));
	}

	pluginDesc.addAttribute(Attrs::PluginAttr("vertices", getVertices()));
	pluginDesc.addAttribute(Attrs::PluginAttr("faces", getFaces()));
	pluginDesc.addAttribute(Attrs::PluginAttr("edge_visibility", getEdgeVisibility()));

	if (getNumNormals() > 0) {
		pluginDesc.addAttribute(Attrs::PluginAttr("normals", getNormals()));
		pluginDesc.addAttribute(Attrs::PluginAttr("faceNormals", getFaceNormals()));
	}

	if (getNumVelocities() > 0) {
		pluginDesc.addAttribute(Attrs::PluginAttr("velocities", getVelocities()));
	}

	if (getNumMapChannels() > 0) {
		VRay::VUtils::ValueRefList map_channel_names(map_channels_data.size());
		VRay::VUtils::ValueRefList map_channels(map_channels_data.size());

		int i = 0;
		for (const auto &mcIt : map_channels_data) {
			const std::string &map_channel_name = mcIt.first;
			const MapChannel &map_channel_data = mcIt.second;

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

		pluginDesc.addAttribute(Attrs::PluginAttr("map_channels_names", map_channel_names));
		pluginDesc.addAttribute(Attrs::PluginAttr("map_channels", map_channels));
	}

	return true;
}

VRay::VUtils::VectorRefList& MeshExporter::getVertices()
{
	if (vertices.size() <= 0) {
		// if we don't have vertices cached, grab them from P attribute
		getDataFromAttribute(gdp.getP(), vertices);
	}

	return vertices;
}

VRay::VUtils::IntRefList MeshExporter::getFaceNormals()
{
	if (m_faceNormals.size() <= 0) {
		// if we don't have face normals cached, grab them from N attribute
		getNormals();
	}

	return m_faceNormals;
}

VRay::VUtils::VectorRefList& MeshExporter::getNormals()
{
	if (normals.size() <= 0) {
		// if we don't have normals cached, grab them from N attribute
		// first check for point attribute
		const GA_Attribute *nattr = gdp.findNormalAttribute(GA_ATTRIB_POINT);
		if (!nattr) {
			// second check for vertex attribute
			nattr = gdp.findNormalAttribute(GA_ATTRIB_VERTEX);
		}
		if (!nattr) {
			// last check for Houdini internal normal attribute
			// which is always a point attribute
			nattr = gdp.findInternalNormalAttribute();
		}

		if (getDataFromAttribute(nattr, normals)) {
			// calculate normals and m_faceNormals simultaneously
			// valid normals attr found and copied into normals
			// deal with face normals now
			UT_ASSERT(nattr);
			switch (nattr->getOwner()) {
			case GA_ATTRIB_VERTEX:
			{
				// if N is vertex attribute, need to calculate normal faces
				m_faceNormals = VRay::VUtils::IntRefList(getNumFaces() * 3);

				int faceVertIndex = 0;
				for (const GEO_Primitive *prim : primList) {
					switch (prim->getTypeId().get()) {
					case GEO_PRIMPOLYSOUP:
					{
						const GU_PrimPolySoup *polySoup = static_cast<const GU_PrimPolySoup*>(prim);
						for (GEO_PrimPolySoup::PolygonIterator pst(*polySoup); !pst.atEnd(); ++pst) {
							// face is valid only if the vertex count is >= 3
							const GA_Size vCnt = pst.getVertexCount();
							if (vCnt > 2) {
								for (GA_Size i = 1; i < vCnt - 1; ++i) {
									// polygon orientation seems to be clockwise in Houdini
									m_faceNormals[faceVertIndex++] = pst.getVertexIndex(i + 1);
									m_faceNormals[faceVertIndex++] = pst.getVertexIndex(i);
									m_faceNormals[faceVertIndex++] = pst.getVertexIndex(0);
								}
							}
						}
						break;
					}
					case GEO_PRIMPOLY:
					{
						// face is valid only if the vertex count is >= 3
						const GA_Size vCnt = prim->getVertexCount();
						if (vCnt > 2) {
							const GU_PrimPoly *poly = static_cast<const GU_PrimPoly*>(prim);
							for (GA_Size i = 1; i < vCnt - 1; ++i) {
								// polygon orientation seems to be clockwise in Houdini
								m_faceNormals[faceVertIndex++] = poly->getVertexIndex(i + 1);
								m_faceNormals[faceVertIndex++] = poly->getVertexIndex(i);
								m_faceNormals[faceVertIndex++] = poly->getVertexIndex(0);
							}
						}
						break;
					}
					default:
					;
					}
				}
				break;
			}
			case GA_ATTRIB_POINT:
			default:
			{
				// if N is point attribute, faces is used to index normals as well
				m_faceNormals = getFaces();
				break;
			}
			}
		}
	}

	return normals;
}

VRay::VUtils::VectorRefList& MeshExporter::getVelocities()
{
	if (velocities.size() <= 0) {
		// if we don't have velocities cached, grab them from v attribute
		// for V-Ray velocity makes sense only when assigned on points, so
		// faces is used to index velocity as well
		getDataFromAttribute(gdp.findVelocityAttribute(GA_ATTRIB_POINT), velocities);
	}

	return velocities;
}

int MeshExporter::getNumFaces()
{
	if (numFaces <= 0) {
		// if we don't have cached face count, recalculate number of faces
		// for current geometry detail
		numFaces = 0;

		for (const GEO_Primitive *prim : primList) {
			switch (prim->getTypeId().get()) {
			case GEO_PRIMPOLYSOUP:
			{
				const GU_PrimPolySoup *polySoup = static_cast<const GU_PrimPolySoup*>(prim);
				for (GA_Size i = 0; i < polySoup->getPolygonCount(); ++i) {
					// face is valid only if the vertex count is >= 3
					numFaces += std::max(polySoup->getPolygonSize(i) - 2, GA_Size(0));
				}
				break;
			}
			case GEO_PRIMPOLY:
			{
				// face is valid only if the vertex count is >= 3
				numFaces += std::max(prim->getVertexCount() - 2, GA_Size(0));
				break;
			}
			default:
			;
			}
		}
	}

	return numFaces;
}

VRay::VUtils::IntRefList& MeshExporter::getFaces()
{
	if (faces.size() <= 0) {
		// if we don't have faces cached, digest valid poly primitives
		// for current geometry detail
		// count faces and proceed only if we do have such
		int nFaces = getNumFaces();
		if (nFaces > 0) {
			// calculate faces and edge visibility simultaneously
			faces = VRay::VUtils::IntRefList(nFaces * 3);
			edge_visibility = VRay::VUtils::IntRefList(nFaces / 10 + (((nFaces % 10) > 0) ? 1 : 0));
			std::memset(edge_visibility.get(), 0, edge_visibility.size() * sizeof(int));

			int faceVertIndex = 0;
			int faceEdgeVisIndex = 0;

			for (const GEO_Primitive *prim : primList) {
				switch (prim->getTypeId().get()) {
				case GEO_PRIMPOLYSOUP:
				{
					const GU_PrimPolySoup *polySoup = static_cast<const GU_PrimPolySoup*>(prim);
					for (GEO_PrimPolySoup::PolygonIterator pst(*polySoup); !pst.atEnd(); ++pst) {
						const GA_Size vCnt = pst.getVertexCount();
						// face is valid only if the vertex count is >= 3
						if (vCnt > 2) {
							for (GA_Size i = 1; i < vCnt - 1; ++i) {
								// polygon orientation seems to be clockwise in Houdini
								faces[faceVertIndex++] = pst.getPointIndex(i + 1);
								faces[faceVertIndex++] = pst.getPointIndex(i);
								faces[faceVertIndex++] = pst.getPointIndex(0);

								// v0 = i+1
								// v1 = i
								// v2 = 0
								// here the diagonal is invisible edge
								// v(1)___v(2)
								//    |  /|
								//    | / |
								//    |/__|
								// v(0)   v(3)
								// first edge v(i+1)->v(i) is always visible
								// second edge v(i)->v(0) is visible only when i == 1
								// third edge v(0)->v(i+1) is visible only when i+1 == vCnt-1
								unsigned char edgeMask = 1 | ((i == 1) << 1) | ((i == (vCnt - 2)) << 2);
								edge_visibility[faceEdgeVisIndex / 10] |= (edgeMask << ((faceEdgeVisIndex % 10) * 3));
								++faceEdgeVisIndex;
							}
						}
					}
					break;
				}
				case GEO_PRIMPOLY:
				{
					const GA_Size vCnt = prim->getVertexCount();
					// face is valid only if the vertex count is >= 3
					if (vCnt > 2) {
						for (GA_Size i = 1; i < vCnt - 1; ++i) {
							// polygon orientation seems to be clockwise in Houdini
							faces[faceVertIndex++] = prim->getPointIndex(i + 1);
							faces[faceVertIndex++] = prim->getPointIndex(i);
							faces[faceVertIndex++] = prim->getPointIndex(0);

							unsigned char edgeMask = (1 | ((i == 1) << 1) | ((i == (vCnt - 2)) << 2));
							edge_visibility[faceEdgeVisIndex / 10] |= (edgeMask << ((faceEdgeVisIndex % 10) * 3));
							++faceEdgeVisIndex;
						}
					}
					break;
				}
				default:
				;
				}
			}

			UT_ASSERT(faceVertIndex == nFaces * 3);
			UT_ASSERT(faceEdgeVisIndex == nFaces);
			UT_ASSERT(edge_visibility.size() >= (faceEdgeVisIndex / 10));
			UT_ASSERT(edge_visibility.size() <= (faceEdgeVisIndex / 10 + 1));
		}
	}

	return faces;
}

VRay::VUtils::IntRefList& MeshExporter::getEdgeVisibility()
{
	if (edge_visibility.size() <= 0) {
		// if we don't have edge visibility list cached,
		// digest valid poly primitives for current geometry detail
		// faces and edge visibility are handled simultaneously in getFaces()
		getFaces();
	}

	return edge_visibility;
}

VRay::Plugin MeshExporter::getMaterial()
{
	const int numFaces = getNumFaces();
	if (!numFaces) {
		return VRay::Plugin();
	}

	PrimMaterial topPrimMaterial;
	objectExporter.getPrimMaterial(topPrimMaterial);

	OP_Node *objMatNode = topPrimMaterial.matNode
	                      ? topPrimMaterial.matNode
	                      : objNode.getMaterialNode(ctx.getTime());
	VRay::Plugin objectMaterial = pluginExporter.exportMaterial(objMatNode);

	GA_ROHandleS materialStyleSheetHndl(gdp.findAttribute(GA_ATTRIB_PRIMITIVE, VFH_ATTR_MATERIAL_STYLESHEET));
	GA_ROHandleS materialPathHndl(gdp.findAttribute(GA_ATTRIB_PRIMITIVE, GEO_STD_ATTRIB_MATERIAL));

	const int hasStyleSheetAttr = materialStyleSheetHndl.isValid();
	const int hasMaterialPathAttr = materialPathHndl.isValid();

	VRay::VUtils::IntRefList face_mtlIDs(numFaces);

	typedef VUtils::HashMapKey<OP_Node*, VRay::Plugin> OpPluginCache;
	typedef VUtils::HashMapKey<OP_Node*, int> MatOpToID;

	MatOpToID matNameToID;
	OpPluginCache matPluginCache;

	int matIndex = 0;

	if (objectMaterial) {
		matPluginCache.insert(objMatNode, objectMaterial);
		matNameToID.insert(objMatNode, matIndex++);
	}

	const STY_Styler &geoStyler = objectExporter.getStyler();

	GSTY_SubjectPrimGroup primSubjects(gdp, primList);
	STY_StylerGroup primStylers;
	primStylers.append(geoStyler, primSubjects);

	int faceIndex = 0;
	GA_Index primIndex = 0;
	for (const GEO_Primitive *prim : primList) {
		const GA_Offset primOffset = prim->getMapOffset();

		// Check if material comes from style sheet
		PrimMaterial primMaterial;
		primMaterial.matNode = topPrimMaterial.matNode;

		const STY_Styler &primStyler = primStylers.getStyler(primIndex);
		appendOverrideValues(primStyler, primMaterial, overrideMerge, true);

		OP_Node *primMtlNode = primMaterial.matNode;

		// If there is no material from stylesheet, try top level material.
		if (!primMtlNode) {
			primMtlNode = objMatNode;
		}

		// If still no material then check material attributes.
		if (!primMtlNode) {
			if (hasStyleSheetAttr) {
				const UT_String styleSheet(materialStyleSheetHndl.get(primOffset), true);
				appendStyleSheet(primMaterial, styleSheet, ctx.getTime(), overrideAppend, true);
			}
			else if (hasMaterialPathAttr) {
				const UT_String &matPath = materialPathHndl.get(primOffset);
				if (!matPath.equal("")) {
					primMtlNode = getOpNodeFromPath(matPath, ctx.getTime());
				}
			}
		}

		// Object material is always 0.
		int faceMtlID = 0;

		VRay::Plugin matPlugin;
		OpPluginCache::iterator opIt = matPluginCache.find(primMtlNode);
		if (opIt != matPluginCache.end()) {
			matPlugin = opIt.data();
		}
		else {
			matPlugin = pluginExporter.exportMaterial(primMtlNode);
			matPluginCache.insert(primMtlNode, matPlugin);
		}

		if (matPlugin) {
			MatOpToID::iterator mIt = matNameToID.find(primMtlNode);
			if (mIt != matNameToID.end()) {
				faceMtlID = mIt.data();
			}
			else {
				faceMtlID = matIndex++;
			}
			matNameToID.insert(primMtlNode, faceMtlID);
		}

		switch (prim->getTypeId().get()) {
			case GEO_PRIMPOLYSOUP: {
				const GU_PrimPolySoup *polySoup = static_cast<const GU_PrimPolySoup*>(prim);
				for (int i = 0; i < polySoup->getPolygonCount(); ++i) {
					const GA_Size numVertices = std::max(polySoup->getPolygonSize(i) - 2, GA_Size(0));
					for (int j = 0; j < numVertices; ++j) {
						face_mtlIDs[faceIndex++] = faceMtlID;
					}
				}
				break;
			}
			case GEO_PRIMPOLY: {
				const GA_Size numVertices = std::max(prim->getVertexCount() - 2, GA_Size(0));
				for (int j = 0; j < numVertices; ++j) {
					face_mtlIDs[faceIndex++] = faceMtlID;
				}
				break;
			}
			default: {
				break;
			}
		}

		primIndex++;
	}

	UT_ASSERT(faceIndex == numFaces);

	const int numMaterials = matNameToID.size();
	if (numMaterials) {
		ObjectExporter &objExproter = pluginExporter.getObjectExporter();

		VRay::VUtils::ValueRefList materialList(numMaterials);
		VRay::VUtils::IntRefList idsList(numMaterials);

		FOR_IT (MatOpToID, mIt, matNameToID) {
			VRay::Plugin mtlPlugin;

			OpPluginCache::iterator opIt = matPluginCache.find(mIt.key());
			if (opIt != matPluginCache.end()) {
				mtlPlugin = opIt.data();
			}

			materialList[mItIdx].setPlugin(mtlPlugin);
			idsList[mItIdx] = mIt.data();
		}

		VRay::Plugin texExtMaterialID;
		const MHash mtlIdListHash = getMaterialIdListHash(face_mtlIDs);

		if (!objExproter.getPluginFromCache(mtlIdListHash, texExtMaterialID)) {
			Attrs::PluginDesc texExtMaterialIDDesc(boost::str(texExtMaterialIDFmt % mtlIdListHash % objNode.getName().buffer()),
												   "TexExtMaterialID");
			texExtMaterialIDDesc.addAttribute(Attrs::PluginAttr("ids_list", face_mtlIDs));

			texExtMaterialID = pluginExporter.exportPlugin(texExtMaterialIDDesc);

			objExproter.addPluginToCache(mtlIdListHash, texExtMaterialID);
		}

		const MHash mtlMultiIdHash = mtlIdListHash ^ primID;

		if (!objExproter.getPluginFromCache(mtlMultiIdHash, objectMaterial)) {
			Attrs::PluginDesc mtlMulti(boost::str(mtlMultiFmt % mtlMultiIdHash % objNode.getName().buffer()),
									   "MtlMulti");
			mtlMulti.addAttribute(Attrs::PluginAttr("mtls_list", materialList));
			mtlMulti.addAttribute(Attrs::PluginAttr("ids_list", idsList));
			mtlMulti.addAttribute(Attrs::PluginAttr("mtlid_gen", texExtMaterialID));

			objectMaterial = pluginExporter.exportPlugin(mtlMulti);

			objExproter.addPluginToCache(mtlMultiIdHash, objectMaterial);
		}
	}

	return objectMaterial;
}

VRay::Plugin MeshExporter::exportExtMapChannels(const MapChannels &mapChannelOverrides) const
{
	if (mapChannelOverrides.size() <= 0)
		return VRay::Plugin();

	ObjectExporter &objExproter = pluginExporter.getObjectExporter();

	VRay::VUtils::ValueRefList map_channels(mapChannelOverrides.size());

	int i = 0;
	for (const auto &mcIt : mapChannelOverrides) {
		const std::string &map_channel_name = mcIt.first;
		const MapChannel &map_channel_data = mcIt.second;

		VRay::VUtils::ValueRefList map_channel(3);
		map_channel[0].setString(map_channel_name.c_str());
		map_channel[1].setListVector(map_channel_data.vertices);
		map_channel[2].setListInt(map_channel_data.faces);

		map_channels[i].setList(map_channel);

		++i;
	}

	const MHash mapChannelsHash = getMapChannelsHash(map_channels);

	VRay::Plugin texExtMapChannels;
	if (!objExproter.getPluginFromCache(mapChannelsHash, texExtMapChannels)) {
		Attrs::PluginDesc extMapChannels(boost::str(extMapChannelFmt % mapChannelsHash % objNode.getName().buffer()),
											"ExtMapChannels");
		extMapChannels.addAttribute(Attrs::PluginAttr("map_channels", map_channels));

		texExtMapChannels = pluginExporter.exportPlugin(extMapChannels);

		objExproter.addPluginToCache(mapChannelsHash, texExtMapChannels);
	}

	return texExtMapChannels;
}

VRay::Plugin MeshExporter::getExtMapChannels()
{
	MapChannels mapChannelOverrides;
	getMtlOverrides(mapChannelOverrides);
	getPointAttrs(mapChannelOverrides, skipMapChannelUV);
	getVertexAttrs(mapChannelOverrides, skipMapChannelUV);

	return exportExtMapChannels(mapChannelOverrides);
}

MapChannels& MeshExporter::getMapChannels()
{
	if (map_channels_data.size() <= 0) {
		getVertexAttrs(map_channels_data, skipMapChannelNonUV);
		getPointAttrs(map_channels_data, skipMapChannelNonUV);
	}
	return map_channels_data;
}

/// Returns true if face is at least a tri-face.
static int validNumVertices(const GA_Size numVertices)
{
	return numVertices >= 3;
}

/// Allocated map channel data.
static void allocateOverrideMapChannel(MapChannel &mapChannel, const GEOPrimList &primList)
{
	int numFaces = 0;

	for (const GEO_Primitive *prim : primList) {
		switch (prim->getTypeId().get()) {
			case GEO_PRIMPOLYSOUP: {
				const GU_PrimPolySoup *polySoup = static_cast<const GU_PrimPolySoup*>(prim);
				for (GEO_PrimPolySoup::PolygonIterator pst(*polySoup); !pst.atEnd(); ++pst) {
					numFaces += VUtils::Max(pst.getVertexCount() - 2, GA_Size(0));
				}
				break;
			}
			case GEO_PRIMPOLY: {
				numFaces += VUtils::Max(prim->getVertexCount() - 2, GA_Size(0));
				break;
			}
			default: {
				UT_ASSERT(false);
			}
		}
	}

	mapChannel.vertices = VRay::VUtils::VectorRefList(numFaces * 3);
	mapChannel.faces = VRay::VUtils::IntRefList(numFaces * 3);

	for (int i = 0; i < mapChannel.vertices.count(); ++i) {
		mapChannel.vertices[i].set(ALMOST_FLT_MAX, 0.0f, 0.0f);
		mapChannel.faces[i] = i;
	}
}

static void setMapChannelOverrideData(const MtlOverrideItem &overrideItem, VRay::VUtils::VectorRefList &vertices, int v0, int v1, int v2)
{
	UT_ASSERT(v0 < vertices.count());
	UT_ASSERT(v1 < vertices.count());
	UT_ASSERT(v2 < vertices.count());

	VRay::Vector &vert0 = vertices[v0];
	VRay::Vector &vert1 = vertices[v1];
	VRay::Vector &vert2 = vertices[v2];

	switch (overrideItem.getType()) {
		case MtlOverrideItem::itemTypeInt: {
			vert0.set(static_cast<float>(overrideItem.valueInt),
					  static_cast<float>(overrideItem.valueInt),
					  static_cast<float>(overrideItem.valueInt));
			vert1.set(static_cast<float>(overrideItem.valueInt),
					  static_cast<float>(overrideItem.valueInt),
					  static_cast<float>(overrideItem.valueInt));
			vert2.set(static_cast<float>(overrideItem.valueInt),
					  static_cast<float>(overrideItem.valueInt),
					  static_cast<float>(overrideItem.valueInt));
			break;
		}
		case MtlOverrideItem::itemTypeDouble: {
			vert0.set(static_cast<float>(overrideItem.valueDouble),
					  static_cast<float>(overrideItem.valueDouble),
					  static_cast<float>(overrideItem.valueDouble));
			vert1.set(static_cast<float>(overrideItem.valueDouble),
					  static_cast<float>(overrideItem.valueDouble),
					  static_cast<float>(overrideItem.valueDouble));
			vert2.set(static_cast<float>(overrideItem.valueDouble),
					  static_cast<float>(overrideItem.valueDouble),
					  static_cast<float>(overrideItem.valueDouble));
			break;
		}
		case MtlOverrideItem::itemTypeVector: {
			vert0 = overrideItem.valueVector;
			vert1 = overrideItem.valueVector;
			vert2 = overrideItem.valueVector;
			break;
		}
		default: {
			vert0.makeZero();
			vert1.makeZero();
			vert2.makeZero();
		}
	}
}

static void setMapChannelOverrideFaceData(MapChannels &mapChannels, const GEOPrimList &primList, const int faceIndex, const PrimMaterial &primMaterial)
{
	if (!primMaterial.overrides.size()) {
		return;
	}

	const int v0 = (faceIndex * 3) + 0;
	const int v1 = (faceIndex * 3) + 1;
	const int v2 = (faceIndex * 3) + 2;

	FOR_CONST_IT(MtlOverrideItems, oiIt, primMaterial.overrides) {
		const MtlOverrideItem &overrideItem = oiIt.data();

		UT_ASSERT(overrideItem.getType() != MtlOverrideItem::itemTypeNone);

		if (overrideItem.getType() == MtlOverrideItem::itemTypeString) {
			continue;
		}

		const tchar *paramName = oiIt.key();

		MapChannel &mapChannel = mapChannels[paramName];

		// Allocate data if not yet allocated
		if (!mapChannel.vertices.count() || !mapChannel.faces.count()) {
			allocateOverrideMapChannel(mapChannel, primList);
		}

		UT_ASSERT(v0 < mapChannel.faces.count());
		UT_ASSERT(v1 < mapChannel.faces.count());
		UT_ASSERT(v2 < mapChannel.faces.count());

		mapChannel.faces[v0] = v0;
		mapChannel.faces[v1] = v1;
		mapChannel.faces[v2] = v2;

		setMapChannelOverrideData(overrideItem, mapChannel.vertices, v0, v1, v2);
	}
}

void MeshExporter::getMtlOverrides(MapChannels &mapChannels) const
{
	GA_ROHandleS materialStyleSheetHndl(gdp.findAttribute(GA_ATTRIB_PRIMITIVE, VFH_ATTR_MATERIAL_STYLESHEET));
	GA_ROHandleS materialPathHndl(gdp.findAttribute(GA_ATTRIB_PRIMITIVE, GEO_STD_ATTRIB_MATERIAL));
	GA_ROHandleS materialOverrideHndl(gdp.findAttribute(GA_ATTRIB_PRIMITIVE, VFH_ATTR_MATERIAL_OVERRIDE));

	int faceIndex = 0;

	MtlOverrideAttrExporter attrExporter(gdp);

	const STY_Styler &geoStyler = objectExporter.getStyler();

	GSTY_SubjectPrimGroup primSubjects(gdp, primList);
	STY_StylerGroup primStylers;
	primStylers.append(geoStyler, primSubjects);

	GA_Index primIndex = 0;
	for (const GEO_Primitive *prim : primList) {
		const GA_Offset primOffset = prim->getMapOffset();

		// Parent override will be exported as user attribute,
		// so don't merge anything here.
		PrimMaterial primMaterial;

		// Style sheet overrides.
		const STY_Styler &primStyler = primStylers.getStyler(primIndex);
		appendOverrideValues(primStyler, primMaterial, overrideMerge);

		// Overrides from primitive style sheet / material attributes.
		appendMaterialOverride(primMaterial, materialStyleSheetHndl, materialPathHndl, materialOverrideHndl, primOffset, ctx.getTime());

		// Overrides from primitive attributes.
		attrExporter.fromPrimitive(primMaterial.overrides, primOffset);

		// Merge other primitive attributes.
		// Merge vertex attributes.
		// Merge point attributes.

		switch (prim->getTypeId().get()) {
			case GEO_PRIMPOLYSOUP: {
				const GU_PrimPolySoup *polySoup = static_cast<const GU_PrimPolySoup*>(prim);
				for (GEO_PrimPolySoup::PolygonIterator pst(*polySoup); !pst.atEnd(); ++pst) {
					const GA_Size numVertices = pst.getVertexCount();
					if (validNumVertices(numVertices)) {
						for (GA_Size i = 1; i < numVertices - 1; ++i) {
							setMapChannelOverrideFaceData(mapChannels, primList, faceIndex, primMaterial);
							++faceIndex;
						}
					}
				}
				break;
			}
			case GEO_PRIMPOLY: {
				const GA_Size numVertices = prim->getVertexCount();
				if (validNumVertices(numVertices)) {
					for (GA_Size i = 1; i < numVertices - 1; ++i) {
						setMapChannelOverrideFaceData(mapChannels, primList, faceIndex, primMaterial);
						++faceIndex;
					}
				}

				break;
			}
			default: {
				UT_ASSERT(false);
			}
		}

		primIndex++;
	}
}

/// Returns true if we need to skip this attribute.
/// @param attr Mesh attribute.
/// @param skipChannels Skip type.
static int skipMapChannel(const GA_Attribute *attr, SkipMapChannel skipChannels)
{
	if (!attr)
		return true;

	const UT_StringHolder &attrName = attr->getName();

	// "P", "N", "v" attributes are handled separately.
	const int isMeshAttr = attrName == GEO_STD_ATTRIB_POSITION ||
	                       attrName == GEO_STD_ATTRIB_NORMAL ||
	                       attrName == GEO_STD_ATTRIB_VELOCITY;
	if (isMeshAttr)
		return true;

	const int isUvAttr = attrName.startsWith(GEO_STD_ATTRIB_TEXTURE);

	if (skipChannels == skipMapChannelUV) {
		return isUvAttr;
	}

	if (skipChannels == skipMapChannelNonUV) {
		return !isUvAttr;
	}

	return false;
}

int MeshExporter::getPointAttrs(MapChannels &mapChannels, SkipMapChannel skipChannels)
{
	int nMapChannels = 0;

	GEOAttribList attrList;
	gdp.getAttributes().matchAttributes(GEOgetV3AttribFilter(), GA_ATTRIB_POINT, attrList);

	for (const GA_Attribute *attr : attrList) {
		if (skipMapChannel(attr, skipChannels))
			continue;

		const std::string attrName = attr->getName().toStdString();
		if (mapChannels.count(attrName))
			continue;

		MapChannel &mapChannel = mapChannels[attrName];
		mapChannel.name = attrName;
		mapChannel.vertices = VRay::VUtils::VectorRefList(getNumVertices());
		// We can use the same face indices as for the mesh vertices.
		mapChannel.faces = getFaces();

		getDataFromAttribute(attr, mapChannel.vertices);

		UT_ASSERT(gdp.getNumPoints() == mapChannel.vertices.size());

		++nMapChannels;
	}

	return nMapChannels;
}

int MeshExporter::getVertexAttrs(MapChannels &mapChannels, SkipMapChannel skipChannels)
{
	int nMapChannels = 0;

	GEOAttribList attrList;
	gdp.getAttributes().matchAttributes(GEOgetV3AttribFilter(), GA_ATTRIB_VERTEX, attrList);

	for (const GA_Attribute *attr : attrList) {
		if (skipMapChannel(attr, skipChannels))
			continue;

		const std::string attrName = attr->getName().toStdString();
		if (mapChannels.count(attrName))
			continue;

		MapChannel &map_channel = mapChannels[attrName];
		getVertexAttrAsMapChannel(*attr, map_channel);
		++nMapChannels;
	}

	return nMapChannels;
}

void MeshExporter::getVertexAttrAsMapChannel(const GA_Attribute &attr, MapChannel &mapChannel)
{
	mapChannel.name = attr.getName();
	Log::getLog().debug("Found map channel: %s", mapChannel.name.c_str());

	GA_ROPageHandleV3 vaPageHndl(&attr);
	GA_ROHandleV3 vaHndl(&attr);

	if (m_hasSubdivApplied) {
		// weld vertex attribute values before populating the map channel
		GA_Offset start;
		GA_Offset end;
		for (GA_Iterator it(gdp.getVertexRange()); it.blockAdvance(start, end); ) {
			vaPageHndl.setPage(start);
			for (GA_Offset offset = start; offset < end; ++offset) {
				const UT_Vector3 &val = vaPageHndl.value(offset);
				mapChannel.verticesSet.insert(MapVertex(val));
			}
		}

		// init map channel data
		mapChannel.vertices = VRay::VUtils::VectorRefList(mapChannel.verticesSet.size());
		mapChannel.faces = VRay::VUtils::IntRefList(getNumFaces() * 3);

		int i = 0;
		for (auto &mv : mapChannel.verticesSet) {
			mv.index = i;
			mapChannel.vertices[i++].set(mv.v[0], mv.v[1], mv.v[2]);
		}

		UT_ASSERT(i == mapChannel.vertices.size());

		// Process map channels (uv and other tuple(3) attributes)
		int faceVertIndex = 0;

		for (const GEO_Primitive *prim : primList) {
			switch (prim->getTypeId().get()) {
			case GEO_PRIMPOLYSOUP:
			{
				const GU_PrimPolySoup *polySoup = static_cast<const GU_PrimPolySoup*>(prim);
				for (GEO_PrimPolySoup::PolygonIterator pst(*polySoup); !pst.atEnd(); ++pst) {
					GA_Size vCnt = pst.getVertexCount();
					if (vCnt > 2) {
						for (GA_Size i = 1; i < vCnt - 1; ++i) {
							// polygon orientation seems to be clockwise in Houdini
							mapChannel.faces[faceVertIndex++] = mapChannel.verticesSet.find(MapVertex(vaHndl.get(prim->getVertexOffset(i + 1))))->index;
							mapChannel.faces[faceVertIndex++] = mapChannel.verticesSet.find(MapVertex(vaHndl.get(prim->getVertexOffset(i))))->index;
							mapChannel.faces[faceVertIndex++] = mapChannel.verticesSet.find(MapVertex(vaHndl.get(prim->getVertexOffset(0))))->index;
						}
					}
				}
				break;
			}
			case GEO_PRIMPOLY:
			{
				GA_Size vCnt = prim->getVertexCount();
				if (vCnt > 2) {
					for (GA_Size i = 1; i < vCnt - 1; ++i) {
						// polygon orientation seems to be clockwise in Houdini
						mapChannel.faces[faceVertIndex++] = mapChannel.verticesSet.find(MapVertex(vaHndl.get(prim->getVertexOffset(i + 1))))->index;
						mapChannel.faces[faceVertIndex++] = mapChannel.verticesSet.find(MapVertex(vaHndl.get(prim->getVertexOffset(i))))->index;
						mapChannel.faces[faceVertIndex++] = mapChannel.verticesSet.find(MapVertex(vaHndl.get(prim->getVertexOffset(0))))->index;
					}
				}
				break;
			}
			default:
			;
			}
		}

		UT_ASSERT(faceVertIndex == mapChannel.faces.size());

		// cleanup hash
		mapChannel.verticesSet.clear();
	}
	else {
		// populate map channel with original values

		// init map channel data
		mapChannel.vertices = VRay::VUtils::VectorRefList(gdp.getNumVertices());
		mapChannel.faces = VRay::VUtils::IntRefList(getNumFaces() * 3);

		getDataFromAttribute(&attr, mapChannel.vertices);

		int faceVertIndex = 0;

		for (const GEO_Primitive *prim : primList) {
			switch (prim->getTypeId().get()) {
			case GEO_PRIMPOLYSOUP:
			{
				const GU_PrimPolySoup *polySoup = static_cast<const GU_PrimPolySoup*>(prim);
				for (GEO_PrimPolySoup::PolygonIterator pst(*polySoup); !pst.atEnd(); ++pst) {
					const GA_Size vCnt = pst.getVertexCount();
					if (vCnt > 2) {
						for (GA_Size i = 1; i < vCnt - 1; ++i) {
							// polygon orientation seems to be clockwise in Houdini
							mapChannel.faces[faceVertIndex++] = pst.getVertexIndex(i + 1);
							mapChannel.faces[faceVertIndex++] = pst.getVertexIndex(i);
							mapChannel.faces[faceVertIndex++] = pst.getVertexIndex(0);
						}
					}
				}
				break;
			}
			case GEO_PRIMPOLY:
			{
				const GA_Size vCnt = prim->getVertexCount();
				if (vCnt > 2) {
					for (GA_Size i = 1; i < vCnt - 1; ++i) {
						// polygon orientation seems to be clockwise in Houdini
						mapChannel.faces[faceVertIndex++] = gdp.getVertexMap().indexFromOffset(prim->getVertexOffset(i + 1));
						mapChannel.faces[faceVertIndex++] = gdp.getVertexMap().indexFromOffset(prim->getVertexOffset(i));
						mapChannel.faces[faceVertIndex++] = gdp.getVertexMap().indexFromOffset(prim->getVertexOffset(0));
					}
				}
				break;
			}
			default:
			;
			}
		}

		UT_ASSERT(faceVertIndex == mapChannel.faces.size());
	}
}
