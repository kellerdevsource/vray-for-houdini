//
// Copyright (c) 2015-2018, Chaos Software Ltd
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
#include <GEO/GEO_Normal.h> // for GEOcomputeNormals

#include <GSTY/GSTY_SubjectPrimGroup.h>
#include <STY/STY_StylerGroup.h>

using namespace VRayForHoudini;
using namespace Hash;

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

static MHash getMaterialIdListHash(const VRay::VUtils::IntRefList &idsList)
{ 
	return getVRayValueHash(idsList);
}

static MHash getMapChannelsHash(VRay::VUtils::ValueRefList &mapChannels)
{ 
	MHash hash = 0;

	for (int chanIdx = 0; chanIdx < mapChannels.count(); ++chanIdx) {
		VRay::VUtils::ValueRefList mapChannel = mapChannels[chanIdx].getList();

		const MHash nameHash = getVRayValueHash(mapChannel[0].getString());
		const MHash vertHash = getVRayValueHash(mapChannel[1].getListVector());
		const MHash faceHash = getVRayValueHash(mapChannel[2].getListInt());

#pragma pack(push, 1)
		struct MapChannelsHash {
			MHash nameHash;
			MHash vertHash;
			MHash faceHash;
		} mapChannelsHash = { nameHash, vertHash, faceHash };
#pragma pack(pop)

		MurmurHash3_x86_32(&mapChannelsHash, sizeof(MapChannelsHash), 42, &hash);
	}

	return hash;
}

/// Build supported attribute type filter.
static GA_AttributeFilter& supportedAttrFilter()
{
	static GA_AttributeFilter filterVector(GA_AttributeFilter::selectAnd(GA_AttributeFilter::selectFloatTuple(false),
	                                                                     GA_AttributeFilter::selectByTupleSize(3)));
	static GA_AttributeFilter filterFloat(GA_AttributeFilter::selectFloatNumeric(false));

	static GA_AttributeFilter filterAttr(GA_AttributeFilter::selectOr(filterVector,
	                                                                  filterFloat));
	return filterAttr;
}

static void rescaleVectorList(VRay::VUtils::VectorRefList &vectorList, float factor)
{
	for (int i = 0; i < vectorList.size(); ++i) {
		vectorList[i] /= factor;
	}
}

static void rescaleVelocity(VRay::VUtils::VectorRefList &vectorList)
{
	rescaleVectorList(vectorList, OPgetDirector()->getChannelManager()->getSamplesPerSec());
}

/// Converts numeric list to vector list.
/// @tparam T Numeric list type.
/// @param attr Attribute.
/// @param aifTuple Attribute value access helper.
/// @param vectorList Pre-allocated vector list.
/// @returns True on success, false otherwise.
template <typename T>
static int numericAttrToVectorRefList(const GA_Attribute &attr, const GA_AIFTuple &aifTuple, VRay::VUtils::VectorRefList &vectorList)
{
	T numericList(vectorList.size());

	if (!aifTuple.getRange(&attr, GA_Range(attr.getIndexMap()), numericList.get()))
		return false;

	for (int i = 0; i < vectorList.size(); ++i) {
		const float value = numericList[i];

		vectorList[i].set(value, value, value);
	}

	return true;
}

/// Helper funtion to copy data from Float3Tuple attribute into a vector list
/// @param attr[in] - the attribute to copy
/// @param data[out] - destination vector list
/// @returns true on success
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

	int res = 0;

	if (attr->getTupleSize() == 3) {
		res = aifTuple->getRange(attr, GA_Range(attr->getIndexMap()), &data.get()->x);

		const int isVelocity = attr->getName().equal(GEO_STD_ATTRIB_VELOCITY);
		if (isVelocity) {
			rescaleVelocity(data);
		}
	}
	else if (attr->getTupleSize() == 1) {
		if (attrRef.isFloat()) {
			res = numericAttrToVectorRefList<VRay::VUtils::FloatRefList>(*attr, *aifTuple, data);
		}
		else if (attrRef.isInt()) {
			res = numericAttrToVectorRefList<VRay::VUtils::IntRefList>(*attr, *aifTuple, data);
		}
	}

	if (!res) {
		data.freeMem();
	}

	return res;
}

/// Pair of min and max value for some range of values
typedef std::pair<int, int> MinMaxPair;

/// Update min and max value with given param
/// @param pair - the min max pair
/// @param value - the value to update the pair if needed
void updateMinMaxPair(MinMaxPair & pair, int value)
{
	pair.first = std::min(pair.first, value);
	pair.second = std::max(pair.second, value);
}

/// Get all faces and edge visibility for a poly soup primitive
/// @param polySoup - pointer to the primitive
/// @param faces - int ref list that will be destination for faces
/// @param edge_visibility - int ref list, destination for edge visibility
/// @param faceVertIndex - offset in faces array to start writing
/// @param faceEdgeVisIndex - offset in edge _visibility to start writing
/// @return - the min and max value for face index (used for separate poly soup export)
static MinMaxPair getPolySoupFaces(const GU_PrimPolySoup *polySoup, VRay::VUtils::IntRefList &faces, VRay::VUtils::IntRefList &edge_visibility, int &faceVertIndex, int &faceEdgeVisIndex)
{
	std::pair<int, int> minMax = {INT_MAX, -1};
	for (GEO_PrimPolySoup::PolygonIterator pst(*polySoup); !pst.atEnd(); ++pst) {
		const GA_Size vCnt = pst.getVertexCount();
		// face is valid only if the vertex count is >= 3
		if (vCnt > 2) {
			for (GA_Size i = 1; i < vCnt - 1; ++i) {
				// polygon orientation seems to be clockwise in Houdini
				faces[faceVertIndex++] = pst.getPointIndex(i + 1);
				faces[faceVertIndex++] = pst.getPointIndex(i);
				faces[faceVertIndex++] = pst.getPointIndex(0);

				updateMinMaxPair(minMax, faces[faceVertIndex - 1]);
				updateMinMaxPair(minMax, faces[faceVertIndex - 2]);
				updateMinMaxPair(minMax, faces[faceVertIndex - 3]);

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
				const uint8 edgeMask = 1 | ((i == 1) << 1) | ((i == (vCnt - 2)) << 2);
				edge_visibility[faceEdgeVisIndex / 10] |= (edgeMask << ((faceEdgeVisIndex % 10) * 3));
				++faceEdgeVisIndex;
			}
		}
	}
	return minMax;
}

/// Count number of faces for a poly soup primitive
/// @param polySoup - pointer to the primitive
/// @return - the number of valid faces
static int getPolySopuFaceCount(const GU_PrimPolySoup *polySoup)
{
	int numFaces = 0;
	for (GA_Size i = 0; i < polySoup->getPolygonCount(); ++i) {
		// face is valid only if the vertex count is >= 3
		numFaces += std::max(polySoup->getPolygonSize(i) - 2, GA_Size(0));
	}
	return numFaces;
}

/// Get face normals for a poly soup primitive (optionally offset the indices)
/// @param polySoup - pointer to the primitive
/// @param faceNormals - int ref list, destination for the normal indices
/// @param faceVertIndex - offset in faceNormals to start writing from
/// @param baseIndex - optional offset that will be substracted from each faceNormal index (used for separate poly soup export)
static void getPolySoupNormals(const GU_PrimPolySoup *polySoup, VRay::VUtils::IntRefList &faceNormals, int &faceVertIndex, int baseIndex = 0)
{
	for (GEO_PrimPolySoup::PolygonIterator pst(*polySoup); !pst.atEnd(); ++pst) {
		// face is valid only if the vertex count is >= 3
		const GA_Size vCnt = pst.getVertexCount();
		if (vCnt > 2) {
			for (GA_Size i = 1; i < vCnt - 1; ++i) {
				// polygon orientation seems to be clockwise in Houdini
				faceNormals[faceVertIndex++] = pst.getVertexIndex(i + 1) - baseIndex;
				faceNormals[faceVertIndex++] = pst.getVertexIndex(i) - baseIndex;
				faceNormals[faceVertIndex++] = pst.getVertexIndex(0) - baseIndex;
			}
		}
	}
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

bool MeshExporter::asPolySoupPrimitives(const GU_Detail &gdp, PrimitiveItems &instancerItems, const PrimitiveItem &topItem, VRayExporter &exporter)
{
	using namespace VRay::VUtils;
	const VectorRefList & allVertices = getVertices();
	if (allVertices.count() == 0) {
		return false;
	}

	const VectorRefList & allNormals = getNormals();
	const bool hasNormals = allNormals.count() != 0;
	m_faceNormals.freeMem();

	int normalsOffset = 0;

	for (auto & prim : primList) {
		if (prim->getTypeId() == GEO_PRIMPOLY) {
			const GA_Attribute *nattr = gdp.findNormalAttribute(GA_ATTRIB_POINT);
			if (!nattr) {
				// second check for vertex attribute
				nattr = gdp.findNormalAttribute(GA_ATTRIB_VERTEX);
			}

			normalsOffset += GA_Range(nattr->getIndexMap()).getEntries();
		}

		if (prim->getTypeId() != GEO_PRIMPOLYSOUP) {
			continue;
		}
		const GU_PrimPolySoup *soup = static_cast<const GU_PrimPolySoup *>(prim);
		const int faceCount = getPolySopuFaceCount(soup);

		IntRefList faces(faceCount * 3);
		IntRefList edge_visibility(faceCount / 10 + (((faceCount % 10) > 0) ? 1 : 0));
		int faceIdx = 0, edgeIdx = 0;
		const MinMaxPair &vertexRange = getPolySoupFaces(soup, faces, edge_visibility, faceIdx, edgeIdx);

		// we need exactly those vertices from all to get
		// NOTE: we can't use point count, since it will count points which point to same vertex multiple times
		const int vertexCount = vertexRange.second - vertexRange.first + 1;

		for (int c = 0; c < faceCount * 3; c++) {
			faces[c] -= vertexRange.first;
		}

		VectorRefList vertices(vertexCount);
		memcpy(vertices.get(), allVertices.get() + vertexRange.first, vertexCount * sizeof(vertices[0]));

		PrimitiveItem item;
		item.primID = soup->getMapIndex();
		item.prim = soup;
		item.tm = topItem.tm;
		item.vel = topItem.vel;
		item.material = topItem.material;

		char geomName[512];
		snprintf(geomName, 512, "GeomStaticMesh|%lld@%s", item.primID, objNode.getName().buffer());

		Attrs::PluginDesc geomDesc(geomName, "GeomStaticMesh");
		geomDesc.add(Attrs::PluginAttr("faces", faces));
		geomDesc.add(Attrs::PluginAttr("vertices", vertices));
		geomDesc.add(Attrs::PluginAttr("edge_visibility", edge_visibility));

		if (hasNormals) {
			GA_Range nrange = soup->getPointRange();
			if (!gdp.findNormalAttribute(GA_ATTRIB_POINT)) {
				// second check for vertex attribute
				nrange = soup->getVertexRange();
			}

			const int normalsCount = GA_Range(nrange).getEntries();

			VectorRefList normals(normalsCount);
			memcpy(normals.get(), allNormals.get() + normalsOffset, normalsCount * sizeof(normals[0]));
			geomDesc.add(Attrs::PluginAttr("normals", normals));

			IntRefList faceNormals(faceCount * 3);
			int faceNormalIdx = 0;
			getPolySoupNormals(soup, faceNormals, faceNormalIdx, normalsOffset);
			geomDesc.add(Attrs::PluginAttr("faceNormals", faceNormals));

			normalsOffset += normalsCount;
		}

		item.geometry = exporter.exportPlugin(geomDesc);
		if (item.geometry.isNotEmpty()) {
			instancerItems += item;
		}
	}

	return true;
}

bool MeshExporter::asPluginDesc(const GU_Detail &gdp, Attrs::PluginDesc &pluginDesc)
{
	if (!primList.size()) {
		return false;
	}

	if (pluginExporter.isInteractive() && pluginExporter.isGPU()) {
		pluginDesc.add(Attrs::PluginAttr("dynamic_geometry", true));
	}

	pluginDesc.add(Attrs::PluginAttr("vertices", getVertices()));
	pluginDesc.add(Attrs::PluginAttr("faces", getFaces()));
	pluginDesc.add(Attrs::PluginAttr("edge_visibility", getEdgeVisibility()));

	if (getNumNormals() > 0) {
		pluginDesc.add(Attrs::PluginAttr("normals", getNormals()));
		pluginDesc.add(Attrs::PluginAttr("faceNormals", getFaceNormals()));
	}

	if (getNumVelocities() > 0) {
		pluginDesc.add(Attrs::PluginAttr("velocities", getVelocities()));
	}

	if (getNumMapChannels() > 0) {
		VRay::VUtils::ValueRefList map_channel_names(map_channels_data.size());
		VRay::VUtils::ValueRefList map_channels(map_channels_data.size());

		FOR_IT (MapChannels, mcIt, map_channels_data) {
			const QString map_channel_name = mcIt.key();
			const MapChannel &map_channel_data = mcIt.value();

			// Channel data
			VRay::VUtils::ValueRefList map_channel(3);
			map_channel[0].setDouble(mcItIdx);
			map_channel[1].setListVector(map_channel_data.vertices);
			map_channel[2].setListInt(map_channel_data.faces);
			map_channels[mcItIdx].setList(map_channel);

			// Channel name attribute
			map_channel_names[mcItIdx].setString(_toChar(map_channel_name));
		}

		pluginDesc.add(Attrs::PluginAttr("map_channels_names", map_channel_names));
		pluginDesc.add(Attrs::PluginAttr("map_channels", map_channels));
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
		bool removeAttr = false;
		const GA_AttributeOwner cumputedAttrOwnder = GA_ATTRIB_VERTEX;

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

		// there is no normal attribute, so we will compute one now
		if (!nattr) {
			nattr = SYSconst_cast(gdp).addNormalAttribute(cumputedAttrOwnder);
			if (nattr) {
				// now we compute the normals with the default params (60.06 cusp angle)
				// the same as attaching default Normal node
				GEOcomputeNormals(gdp, GA_RWHandleV3(SYSconst_cast(nattr)));
				removeAttr = true;
			}
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
						getPolySoupNormals(polySoup, m_faceNormals, faceVertIndex);
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

			// remove coputed normal attribute so we don't modify the scene on export
			if (removeAttr) {
					SYSconst_cast(gdp).destroyNormalAttribute(cumputedAttrOwnder);
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
				numFaces += getPolySopuFaceCount(polySoup);
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
		const int nFaces = getNumFaces();
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
					getPolySoupFaces(polySoup, faces, edge_visibility, faceVertIndex, faceEdgeVisIndex);
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

							const uint8 edgeMask = (1 | ((i == 1) << 1) | ((i == (vCnt - 2)) << 2));
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

	struct SubMaterial {
		explicit SubMaterial(VRay::Plugin mtl, int index)
			: mtl(mtl)
			, index(index)
		{}

		VRay::Plugin mtl;
		int index;
	};

	typedef VUtils::HashMap<OP_Node*, SubMaterial> OpNodeToSubMaterial;

	OpNodeToSubMaterial matOpNodeToMatPlugin;

	int matIndex = 0;

	if (objectMaterial.isNotEmpty()) {
		matOpNodeToMatPlugin.insert(objMatNode, SubMaterial(objectMaterial, matIndex));
	}

	const STY_Styler &geoStyler = objectExporter.getStyler();

	const GSTY_SubjectPrimGroup primSubjects(gdp, primList);
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
				const UT_String matPath(materialPathHndl.get(primOffset));
				primMtlNode = getOpNodeFromPath(objNode, matPath, ctx.getTime());
			}
		}

		int faceMtlID;

		OpNodeToSubMaterial::iterator opIt = matOpNodeToMatPlugin.find(primMtlNode);
		if (opIt != matOpNodeToMatPlugin.end()) {
			faceMtlID = opIt.data().index;
		}
		else {
			faceMtlID = ++matIndex;

			const VRay::Plugin matPlugin = pluginExporter.exportMaterial(primMtlNode);

			matOpNodeToMatPlugin.insert(primMtlNode, SubMaterial(matPlugin, faceMtlID));
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

	const int numMaterials = matOpNodeToMatPlugin.size();
	if (numMaterials) {
		ObjectExporter &objExproter = pluginExporter.getObjectExporter();

		VRay::VUtils::ValueRefList materialList(numMaterials);
		VRay::VUtils::IntRefList idsList(numMaterials);

		FOR_IT (OpNodeToSubMaterial, mIt, matOpNodeToMatPlugin) {
			const SubMaterial &subMat = mIt.data();

			materialList[mItIdx].setPlugin(subMat.mtl);
			idsList[mItIdx] = subMat.index;
		}

		VRay::Plugin texExtMaterialID;
		const MHash mtlIdListHash = getMaterialIdListHash(face_mtlIDs);

		if (!objExproter.getPluginFromCache(mtlIdListHash, texExtMaterialID)) {
			Attrs::PluginDesc texExtMaterialIDDesc(SL("TexExtMaterialID|") % QString::number(mtlIdListHash) % SL("@") % objNode.getName().buffer(),
												   SL("TexExtMaterialID"));
			texExtMaterialIDDesc.add(Attrs::PluginAttr("ids_list", face_mtlIDs));

			texExtMaterialID = pluginExporter.exportPlugin(texExtMaterialIDDesc);

			objExproter.addPluginToCache(mtlIdListHash, texExtMaterialID);
		}

		const MHash mtlMultiIdHash = mtlIdListHash ^ primID;

		if (!objExproter.getPluginFromCache(mtlMultiIdHash, objectMaterial)) {
			Attrs::PluginDesc mtlMulti(SL("MtlMulti|") % QString::number(mtlMultiIdHash) % SL("@") % objNode.getName().buffer(),
									   SL("MtlMulti"));
			mtlMulti.add(Attrs::PluginAttr("mtls_list", materialList));
			mtlMulti.add(Attrs::PluginAttr("ids_list", idsList));
			mtlMulti.add(Attrs::PluginAttr("mtlid_gen", texExtMaterialID));

			objectMaterial = pluginExporter.exportPlugin(mtlMulti);

			objExproter.addPluginToCache(mtlMultiIdHash, objectMaterial);
		}
	}

	return objectMaterial;
}

VRay::Plugin MeshExporter::exportExtMapChannels(const MapChannels &mapChannelOverrides) const
{
	if (mapChannelOverrides.empty())
		return VRay::Plugin();

	ObjectExporter &objExproter = pluginExporter.getObjectExporter();

	VRay::VUtils::ValueRefList map_channels(mapChannelOverrides.size());

	FOR_CONST_IT (MapChannels, mcIt, mapChannelOverrides) {
		const QString map_channel_name = mcIt.key();
		const MapChannel &map_channel_data = mcIt.value();

		VRay::VUtils::ValueRefList map_channel(3);
		map_channel[0].setString(_toChar(map_channel_name));
		if (map_channel_data.type == MapChannel::mapChannelTypeVertex) {
			map_channel[1].setListVector(map_channel_data.vertices);
		}
		else {
#if EXT_MAPCHANNEL_STRING_CHANNEL_SUPPORT
			map_channel[1].setListString(map_channel_data.strings.toRefList());
#else
			vassert(false && "EXT_MAPCHANNEL_STRING_CHANNEL_SUPPORT");
#endif
		}
		map_channel[2].setListInt(map_channel_data.faces);

		map_channels[mcItIdx].setList(map_channel);
	}

	const MHash mapChannelsHash = getMapChannelsHash(map_channels);

	VRay::Plugin texExtMapChannels;
	if (!objExproter.getPluginFromCache(mapChannelsHash, texExtMapChannels)) {
		Attrs::PluginDesc extMapChannels(SL("ExtMapChannels|") % QString::number(mapChannelsHash) % SL("@") % objNode.getName().buffer(),
		                                 SL("ExtMapChannels"));
		extMapChannels.add(Attrs::PluginAttr("map_channels", map_channels));

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
	if (map_channels_data.empty()) {
		getVertexAttrs(map_channels_data, skipMapChannelNonUV);
		getPointAttrs(map_channels_data, skipMapChannelNonUV);
	}
	return map_channels_data;
}

/// Returns the number of tri-faces in GEOPrimList.
/// @param primList Primitives list.
static int getNumFaces(const GEOPrimList &primList)
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

	return numFaces;
}

/// Returns true if face is at least a tri-face.
static int validNumVertices(const GA_Size numVertices)
{
	return numVertices >= 3;
}

#if EXT_MAPCHANNEL_STRING_CHANNEL_SUPPORT

/// Allocates string channel data.
static void allocateOverrideStringChannel(MapChannel &mapChannel, const GEOPrimList &primList)
{
	// Allocate data if not yet allocated
	if (mapChannel.faces.count())
		return;

	const int numFaces = getNumFaces(primList);

	mapChannel.type = MapChannel::mapChannelTypeString;
	mapChannel.faces = VRay::VUtils::IntRefList(numFaces);
}

static void setStringChannelOverrideData(MapChannel &mapChannel, int faceIndex, const MtlOverrideItem &overrideItem)
{
	vassert(overrideItem.getType() == MtlOverrideItem::itemTypeString);

	const char *strItem = overrideItem.getString();

	int stringTableIndex;

	MapChannel::StringToTableIndex::const_iterator tIt = mapChannel.stringToTableIndex.find(strItem);
	if (tIt != mapChannel.stringToTableIndex.end()) {
		stringTableIndex = tIt.data();
	}
	else {
		VRay::VUtils::CharString &newItem = *mapChannel.strings.newElement();
		newItem.set(strItem);

		stringTableIndex = mapChannel.strings.count()-1;

		mapChannel.stringToTableIndex.insert(strItem, stringTableIndex);
	}

	mapChannel.faces[faceIndex] = stringTableIndex;
}

#endif // EXT_MAPCHANNEL_STRING_CHANNEL_SUPPORT

/// Allocates map channel data.
static void allocateOverrideMapChannel(MapChannel &mapChannel, const GEOPrimList &primList)
{
	// Allocate data if not yet allocated
	if (mapChannel.vertices.count() &&
		mapChannel.faces.count())
		return;

	const int numFaces = getNumFaces(primList);

	mapChannel.type = MapChannel::mapChannelTypeVertex;
	mapChannel.vertices = VRay::VUtils::VectorRefList(numFaces * 3);
	mapChannel.faces = VRay::VUtils::IntRefList(numFaces * 3);

	for (int i = 0; i < mapChannel.vertices.count(); ++i) {
		mapChannel.vertices[i].set(ALMOST_FLT_MAX, 0.0f, 0.0f);
		mapChannel.faces[i] = i;
	}
}

static void setMapChannelOverrideData(MapChannel &mapChannel, const MtlOverrideItem &overrideItem, int v0, int v1, int v2)
{
	vassert(v0 < mapChannel.faces.count());
	vassert(v1 < mapChannel.faces.count());
	vassert(v2 < mapChannel.faces.count());

	mapChannel.faces[v0] = v0;
	mapChannel.faces[v1] = v1;
	mapChannel.faces[v2] = v2;

	VRay::Vector &vert0 = mapChannel.vertices[v0];
	VRay::Vector &vert1 = mapChannel.vertices[v1];
	VRay::Vector &vert2 = mapChannel.vertices[v2];

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
	if (primMaterial.overrides.empty())
		return;

	const int v0 = (faceIndex * 3) + 0;
	const int v1 = (faceIndex * 3) + 1;
	const int v2 = (faceIndex * 3) + 2;

	FOR_CONST_IT(MtlOverrideItems, oiIt, primMaterial.overrides) {
		const QString paramName = oiIt.key();
		const MtlOverrideItem &overrideItem = oiIt.value();

		vassert(overrideItem.getType() != MtlOverrideItem::itemTypeNone);

		if (overrideItem.getType() == MtlOverrideItem::itemTypeVector) {
			MapChannel &mapChannel = mapChannels[paramName];
			allocateOverrideMapChannel(mapChannel, primList);
			setMapChannelOverrideData(mapChannel, overrideItem, v0, v1, v2);
		}
		else if (overrideItem.getType() == MtlOverrideItem::itemTypeDouble ||
		         overrideItem.getType() == MtlOverrideItem::itemTypeInt)
		{
			MapChannel &mapChannel = mapChannels[paramName];
			allocateOverrideMapChannel(mapChannel, primList);
			setMapChannelOverrideData(mapChannel, overrideItem, v0, v1, v2);
		}
#if EXT_MAPCHANNEL_STRING_CHANNEL_SUPPORT
		else if (overrideItem.getType() == MtlOverrideItem::itemTypeString) {
			MapChannel &mapChannel = mapChannels[paramName];
			allocateOverrideStringChannel(mapChannel, primList);
			setStringChannelOverrideData(mapChannel, faceIndex, overrideItem);
		}
#endif
	}
}

void MeshExporter::getMtlOverrides(MapChannels &mapChannels) const
{
	const GA_ROHandleS materialStyleSheetHndl(gdp.findAttribute(GA_ATTRIB_PRIMITIVE, VFH_ATTR_MATERIAL_STYLESHEET));
	const GA_ROHandleS materialPathHndl(gdp.findAttribute(GA_ATTRIB_PRIMITIVE, GEO_STD_ATTRIB_MATERIAL));
	const GA_ROHandleS materialOverrideHndl(gdp.findAttribute(GA_ATTRIB_PRIMITIVE, VFH_ATTR_MATERIAL_OVERRIDE));

	int faceIndex = 0;

	MtlOverrideAttrExporter attrExporter(gdp);

	const STY_Styler &geoStyler = objectExporter.getStyler();

	const GSTY_SubjectPrimGroup primSubjects(gdp, primList);
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
	gdp.getAttributes().matchAttributes(supportedAttrFilter(), GA_ATTRIB_POINT, attrList);

	for (const GA_Attribute *attr : attrList) {
		if (skipMapChannel(attr, skipChannels))
			continue;

		const char *attrName = attr->getName().buffer();
		if (mapChannels.find(attrName) != mapChannels.end())
			continue;

		MapChannel &mapChannel = mapChannels[attrName];
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
	gdp.getAttributes().matchAttributes(supportedAttrFilter(), GA_ATTRIB_VERTEX, attrList);

	for (const GA_Attribute *attr : attrList) {
		if (skipMapChannel(attr, skipChannels))
			continue;

		const char *attrName = attr->getName().buffer();
		if (mapChannels.find(attrName) != mapChannels.end())
			continue;

		MapChannel &map_channel = mapChannels[attrName];
		getVertexAttrAsMapChannel(*attr, map_channel);
		++nMapChannels;
	}

	return nMapChannels;
}

void MeshExporter::getVertexAttrAsMapChannel(const GA_Attribute &attr, MapChannel &mapChannel)
{
	GA_ROPageHandleV3 vaPageHndl(&attr);
	GA_ROHandleV3 vaHndl(&attr);

	if (m_hasSubdivApplied) {
		// weld vertex attribute values before populating the map channel
		GA_Offset start;
		GA_Offset end;
		for (GA_Iterator it(gdp.getVertexRange()); it.blockAdvance(start, end); ) {
			vaPageHndl.setPage(start);
			for (GA_Offset offset = start; offset < end; ++offset) {
				mapChannel.verticesSet.insert(MapVertex(vaPageHndl.value(offset)));
			}
		}

		// init map channel data
		mapChannel.vertices = VRay::VUtils::VectorRefList(mapChannel.verticesSet.size());
		mapChannel.faces = VRay::VUtils::IntRefList(getNumFaces() * 3);

		int i = 0;
		for (const MapVertex &mapVertex : mapChannel.verticesSet) {
			mapVertex.index = i;
			mapChannel.vertices[i++].set(mapVertex.v.x(), mapVertex.v.y(), mapVertex.v.z());
		}

		UT_ASSERT(i == mapChannel.vertices.size());

		// Process map channels (uv and other tuple(3) attributes)
		int faceVertIndex = 0;

		for (const GEO_Primitive *prim : primList) {
			switch (prim->getTypeId().get()) {
				case GEO_PRIMPOLYSOUP: {
					const GU_PrimPolySoup *polySoup = static_cast<const GU_PrimPolySoup*>(prim);
					for (GEO_PrimPolySoup::PolygonIterator pst(*polySoup); !pst.atEnd(); ++pst) {
						const GA_Size numVertices = pst.getVertexCount();
						if (validNumVertices(numVertices)) {
							for (GA_Size vertIdx = 1; vertIdx < numVertices - 1; ++vertIdx) {
								// polygon orientation seems to be clockwise in Houdini
								mapChannel.faces[faceVertIndex++] = mapChannel.verticesSet.find(MapVertex(vaHndl.get(prim->getVertexOffset(vertIdx + 1))))->index;
								mapChannel.faces[faceVertIndex++] = mapChannel.verticesSet.find(MapVertex(vaHndl.get(prim->getVertexOffset(vertIdx))))->index;
								mapChannel.faces[faceVertIndex++] = mapChannel.verticesSet.find(MapVertex(vaHndl.get(prim->getVertexOffset(0))))->index;
							}
						}
					}
					break;
				}
				case GEO_PRIMPOLY: {
					const GA_Size numVertices = prim->getVertexCount();
					if (validNumVertices(numVertices)) {
						for (GA_Size vertIdx = 1; vertIdx < numVertices - 1; ++vertIdx) {
							// polygon orientation seems to be clockwise in Houdini
							mapChannel.faces[faceVertIndex++] = mapChannel.verticesSet.find(MapVertex(vaHndl.get(prim->getVertexOffset(vertIdx + 1))))->index;
							mapChannel.faces[faceVertIndex++] = mapChannel.verticesSet.find(MapVertex(vaHndl.get(prim->getVertexOffset(vertIdx))))->index;
							mapChannel.faces[faceVertIndex++] = mapChannel.verticesSet.find(MapVertex(vaHndl.get(prim->getVertexOffset(0))))->index;
						}
					}
					break;
				}
				default:
					break;
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
				case GEO_PRIMPOLYSOUP: {
					const GU_PrimPolySoup *polySoup = static_cast<const GU_PrimPolySoup*>(prim);
					for (GEO_PrimPolySoup::PolygonIterator pst(*polySoup); !pst.atEnd(); ++pst) {
						const GA_Size numVertices = pst.getVertexCount();
						if (validNumVertices(numVertices)) {
							for (GA_Size vertIdx = 1; vertIdx < numVertices - 1; ++vertIdx) {
								// polygon orientation seems to be clockwise in Houdini
								mapChannel.faces[faceVertIndex++] = pst.getVertexIndex(vertIdx + 1);
								mapChannel.faces[faceVertIndex++] = pst.getVertexIndex(vertIdx);
								mapChannel.faces[faceVertIndex++] = pst.getVertexIndex(0);
							}
						}
					}
					break;
				}
				case GEO_PRIMPOLY: {
					const GA_Size numVertices = prim->getVertexCount();
					if (validNumVertices(numVertices)) {
						for (GA_Size vertIdx = 1; vertIdx < numVertices - 1; ++vertIdx) {
							// polygon orientation seems to be clockwise in Houdini
							mapChannel.faces[faceVertIndex++] = gdp.getVertexMap().indexFromOffset(prim->getVertexOffset(vertIdx + 1));
							mapChannel.faces[faceVertIndex++] = gdp.getVertexMap().indexFromOffset(prim->getVertexOffset(vertIdx));
							mapChannel.faces[faceVertIndex++] = gdp.getVertexMap().indexFromOffset(prim->getVertexOffset(0));
						}
					}
					break;
				}
				default:
					break;
			}
		}

		UT_ASSERT(faceVertIndex == mapChannel.faces.size());
	}
}
