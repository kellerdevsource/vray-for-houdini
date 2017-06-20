//
// Copyright (c) 2015-2016, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#include "vfh_export_mesh.h"

#include <SHOP/SHOP_GeoOverride.h>
#include <GU/GU_Detail.h>
#include <GU/GU_PrimPolySoup.h>
#include <GU/GU_PrimPoly.h>
#include <GA/GA_PageHandle.h>


using namespace VRayForHoudini;

namespace {

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
	if (NOT(aifTuple)) {
		return false;
	}

	data = VRay::VUtils::VectorRefList(attr->getIndexMap().indexSize());
	return aifTuple->getRange(attr, GA_Range(attr->getIndexMap()), &(data.get()->x));
}

}


bool MeshExporter::isPrimPoly(const GEO_Primitive *prim)
{
	if (!prim) {
		return false;
	}

	return (   prim->getTypeId() == GEO_PRIMPOLYSOUP
			|| (prim->getTypeId() == GEO_PRIMPOLY && UTverify_cast< const GEO_PrimPoly* >(prim)->isClosed()) );
}


bool MeshExporter::containsPolyPrimitives(const GU_Detail &gdp)
{
	return     gdp.containsPrimitiveType(GEO_PRIMPOLY)
			|| gdp.containsPrimitiveType(GEO_PRIMPOLYSOUP);
}


MeshExporter::MeshExporter(OBJ_Node &obj, OP_Context &ctx, VRayExporter &exp):
	PrimitiveExporter(obj, ctx, exp),
	m_gdp(nullptr),
	m_hasSubdivApplied(false),
	numFaces(-1)
{ }


bool MeshExporter::init(const GU_Detail &gdp)
{
	if (!m_gdp || m_gdp->getUniqueId() != gdp.getUniqueId()) {
		// we are not initialized
		// or the gdp is different
		reset();
		m_gdp = &gdp;
		return true;
	}
	// the gdp is the same so we do nothing
	return false;
}


void MeshExporter::reset()
{
	// clear cached data from current gdp
	m_primList.clear();
	numFaces = -1;
	faces = VRay::VUtils::IntRefList();
	edge_visibility = VRay::VUtils::IntRefList();
	vertices = VRay::VUtils::VectorRefList();
	normals = VRay::VUtils::VectorRefList();
	m_faceNormals = VRay::VUtils::IntRefList();
	velocities = VRay::VUtils::VectorRefList();
	face_mtlIDs = VRay::VUtils::IntRefList();
	map_channels_data.clear();
	// clear current gdp
	m_gdp = nullptr;
}


const GEOPrimList& MeshExporter::getPrimList()
{
	UT_ASSERT( m_gdp );

	if (   m_primList.size() <= 0
		&& containsPolyPrimitives(*m_gdp) )
	{
		m_primList.setCapacity(m_gdp->countPrimitiveType(GEO_PRIMPOLY)
							 + m_gdp->countPrimitiveType(GEO_PRIMPOLYSOUP));

		for (GA_Iterator jt(m_gdp->getPrimitiveRange()); !jt.atEnd(); jt.advance()) {
			const GEO_Primitive *prim = m_gdp->getGEOPrimitive(*jt);
			if (isPrimPoly(prim)) {
				m_primList.append(prim);
			}
		}
	}

	return m_primList;
}


int MeshExporter::getSHOPList(SHOPList &shopList)
{
	UT_ASSERT( m_gdp );

	GA_ROHandleS mtlpath(m_gdp->findAttribute(GA_ATTRIB_PRIMITIVE, GEO_STD_ATTRIB_MATERIAL));
	if (mtlpath.isInvalid()) {
		return 0;
	}

	int shopCnt = 0;
	const GEOPrimList &primList = getPrimList();
	for (const GEO_Primitive *prim : primList) {
		switch (prim->getTypeId().get()) {
			case GEO_PRIMPOLYSOUP:
			case GEO_PRIMPOLY:
			{
				UT_String shoppath(mtlpath.get(prim->getMapOffset()), false);
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


bool MeshExporter::asPluginDesc(const GU_Detail &gdp, Attrs::PluginDesc &pluginDesc)
{
	init(gdp);
	if (!hasPolyGeometry()) {
		return false;
	}

	Log::getLog().info("Mesh: %i points", m_gdp->getNumPoints());

	const std::string meshName = boost::str(Parm::FmtPrefixManual % "Geom" % std::to_string(m_gdp->getUniqueId()));
	pluginDesc.pluginName = VRayExporter::getPluginName(&m_object, meshName);
	pluginDesc.pluginID = "GeomStaticMesh";

	if (m_exporter.isIPR() && m_exporter.isGPU()) {
		pluginDesc.addAttribute(Attrs::PluginAttr("dynamic_geometry", true));
	}

	pluginDesc.addAttribute(Attrs::PluginAttr("vertices", getVertices()));
	pluginDesc.addAttribute(Attrs::PluginAttr("faces", getFaces()));
	pluginDesc.addAttribute(Attrs::PluginAttr("edge_visibility", getEdgeVisibility()));

	if (getNumMtlIDs() > 0) {
		pluginDesc.addAttribute(Attrs::PluginAttr("face_mtlIDs", getFaceMtlIDs()));
	}

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
		pluginDesc.addAttribute(Attrs::PluginAttr("map_channels",      map_channels));
	}

	return true;
}


void MeshExporter::exportPrimitives(const GU_Detail &gdp, InstancerItems&)
{
	// Check if this is needed / could be reused
#if 0
	else {
		// we don't want to reexport the geometry so just
		// add new node to our list of nodes
		pluginList.push_back(Attrs::PluginDesc("", "Node"));
		Attrs::PluginDesc &nodeDesc = pluginList.back();

		SHOPList shopList;
		int nSHOPs = polyMeshExporter.getSHOPList(shopList);
		if (nSHOPs > 0) {
			nodeDesc.addAttribute(Attrs::PluginAttr(VFH_ATTR_MATERIAL_ID, -1));
		}
	}
#endif
#if 0
	// add new node to our list of nodes
	plugins.push_back(Attrs::PluginDesc("", "Node"));
	Attrs::PluginDesc &nodeDesc = plugins.back();
	nodeDesc.addAttribute(Attrs::PluginAttr("geometry", geom));

	// handle material
	SHOPList shopList;
	int nSHOPs = getSHOPList(shopList);
	if (nSHOPs > 0) {
		VRay::ValueList mtls_list;
		VRay::IntList   ids_list;
		mtls_list.reserve(nSHOPs);
		ids_list.reserve(nSHOPs);

		SHOPHasher hasher;
		for (const UT_String &shopPath : shopList) {
			OP_Node *matNode = getOpNodeFromPath(shopPath);
			UT_ASSERT(matNode);
			mtls_list.emplace_back(m_exporter.exportMaterial(matNode));
			ids_list.emplace_back(hasher(matNode));
		}

		Attrs::PluginDesc mtlDesc;
		mtlDesc.pluginID = "MtlMulti";
		mtlDesc.pluginName = VRayExporter::getPluginName(&m_object,
															boost::str(Parm::FmtPrefixManual % "Mtl" % std::to_string(gdp.getUniqueId())));

		mtlDesc.addAttribute(Attrs::PluginAttr("mtls_list", mtls_list));
		mtlDesc.addAttribute(Attrs::PluginAttr("ids_list",  ids_list));

		nodeDesc.addAttribute(Attrs::PluginAttr("material", m_exporter.exportPlugin(mtlDesc)));
	}
#endif
}


VRay::VUtils::VectorRefList& MeshExporter::getVertices()
{
	UT_ASSERT( m_gdp );

	if (vertices.size() <= 0) {
		// if we don't have vertices cached, grab them from P attribute
		getDataFromAttribute(m_gdp->getP(), vertices);
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
	UT_ASSERT( m_gdp );

	if (normals.size() <= 0) {
		// if we don't have normals cached, grab them from N attribute
		// first check for point attribute
		const GA_Attribute *nattr = m_gdp->findNormalAttribute(GA_ATTRIB_POINT);
		if (!nattr) {
			// second check for vertex attribute
			nattr = m_gdp->findNormalAttribute(GA_ATTRIB_VERTEX);
		}

		if (!nattr) {
			// last check for Houdini internal normal attribute
			// which is always a point attribute
			nattr = m_gdp->findInternalNormalAttribute();
		}

		if (getDataFromAttribute(nattr, normals)) {
			// calculate normals and m_faceNormals simultaneously
			// valid normals attr found and copied into normals
			// deal with face normals now
			UT_ASSERT( nattr );
			switch (nattr->getOwner()) {
				case GA_ATTRIB_VERTEX:
				{
					// if N is vertex attribute, need to calculate normal faces
					m_faceNormals = VRay::VUtils::IntRefList(getNumFaces() * 3);

					int faceVertIndex = 0;
					const GEOPrimList &primList = getPrimList();
					for (const GEO_Primitive *prim : primList) {
						switch (prim->getTypeId().get()) {
							case GEO_PRIMPOLYSOUP:
							{
								const GU_PrimPolySoup *polySoup = static_cast<const GU_PrimPolySoup*>(prim);
								for (GEO_PrimPolySoup::PolygonIterator pst(*polySoup); !pst.atEnd(); ++pst) {
									// face is valid only if the vertex count is >= 3
									const GA_Size vCnt = pst.getVertexCount();
									if ( vCnt > 2) {
										for (GA_Size i = 1; i < vCnt-1; ++i) {
											// polygon orientation seems to be clockwise in Houdini
											m_faceNormals[faceVertIndex++] = pst.getVertexIndex(i+1);
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
								if ( vCnt > 2) {
									const GU_PrimPoly *poly = static_cast<const GU_PrimPoly*>(prim);
									for (GA_Size i = 1; i < vCnt-1; ++i) {
										// polygon orientation seems to be clockwise in Houdini
										m_faceNormals[faceVertIndex++] = poly->getVertexIndex(i+1);
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
	UT_ASSERT( m_gdp );

	if (velocities.size() <= 0) {
		// if we don't have velocities cached, grab them from v attribute
		// for V-Ray velocity makes sense only when assigned on points, so
		// faces is used to index velocity as well
		getDataFromAttribute(m_gdp->findVelocityAttribute(GA_ATTRIB_POINT), velocities);
	}

	return velocities;
}


int MeshExporter::getNumFaces()
{
	if (numFaces <= 0 ) {
		// if we don't have cached face count, recalculate number of faces
		// for current geometry detail
		numFaces = 0;
		const GEOPrimList &primList = getPrimList();
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
			faces = VRay::VUtils::IntRefList(nFaces*3);
			edge_visibility = VRay::VUtils::IntRefList(nFaces/10 + (((nFaces%10) > 0)? 1 : 0));
			std::memset(edge_visibility.get(), 0, edge_visibility.size() * sizeof(int));

			int faceVertIndex = 0;
			int faceEdgeVisIndex = 0;
			const GEOPrimList &primList = getPrimList();
			for (const GEO_Primitive *prim : primList) {
				switch (prim->getTypeId().get()) {
					case GEO_PRIMPOLYSOUP:
					{
						const GU_PrimPolySoup *polySoup = static_cast<const GU_PrimPolySoup*>(prim);
						for (GEO_PrimPolySoup::PolygonIterator pst(*polySoup); !pst.atEnd(); ++pst) {
							const GA_Size vCnt = pst.getVertexCount();
							// face is valid only if the vertex count is >= 3
							if ( vCnt > 2) {
								for (GA_Size i = 1; i < vCnt-1; ++i) {
									// polygon orientation seems to be clockwise in Houdini
									faces[faceVertIndex++] = pst.getPointIndex(i+1);
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
						const GA_Size vCnt = prim->getVertexCount();
						// face is valid only if the vertex count is >= 3
						if ( vCnt > 2) {
							for (GA_Size i = 1; i < vCnt-1; ++i) {
								// polygon orientation seems to be clockwise in Houdini
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


VRay::VUtils::IntRefList& MeshExporter::getFaceMtlIDs()
{
	UT_ASSERT( m_gdp );

	if (face_mtlIDs.size() <= 0) {
		// if we don't have list of face mtl ids cached,
		// digest the material primitive attribute (if any)
		// into a list of face mtl ids
		GA_ROHandleS mtlpath(m_gdp->findAttribute(GA_ATTRIB_PRIMITIVE, GEO_STD_ATTRIB_MATERIAL));
		if (mtlpath.isValid()) {
			// count faces and proceed only if we do have such
			const int nFaces = getNumFaces();
			if (nFaces > 0) {
				face_mtlIDs = VRay::VUtils::IntRefList(nFaces);

				int faceIndex = 0;
				SHOPHasher hasher;
				const GEOPrimList &primList = getPrimList();
				for (const GEO_Primitive *prim : primList) {

					int shopID = hasher(mtlpath.get(prim->getMapOffset()));

					switch (prim->getTypeId().get()) {
						case GEO_PRIMPOLYSOUP:
						{
							const GU_PrimPolySoup *polySoup = static_cast<const GU_PrimPolySoup*>(prim);
							for (GA_Size i = 0; i < polySoup->getPolygonCount(); ++i) {
								// face is valid only if the vertex count is >= 3
								const GA_Size nCnt = std::max(polySoup->getPolygonSize(i) - 2, GA_Size(0));
								for (GA_Size j = 0; j < nCnt; ++j) {
									face_mtlIDs[faceIndex++] = shopID;
								}
							}
							break;
						}
						case GEO_PRIMPOLY:
						{
							// face is valid only if the vertex count is >= 3
							const GA_Size nCnt = std::max(prim->getVertexCount() - 2, GA_Size(0));
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
			}
		}

		UT_ASSERT( face_mtlIDs.size() == 0 || face_mtlIDs.size() == getNumFaces() );
	}

	return face_mtlIDs;
}


MapChannels& MeshExporter::getMapChannels()
{
	if (map_channels_data.size() <= 0) {
		int nMapChannels = 0;
		nMapChannels += getVertexAttrs(map_channels_data);
		nMapChannels += getPointAttrs(map_channels_data);
		nMapChannels += getMtlOverrides(map_channels_data);

		UT_ASSERT( map_channels_data.size() == nMapChannels );
	}

	return map_channels_data;
}


//
// MATERIAL OVERRIDE
//
// Primitive color override example
//   "diffuser" : 1.0, "diffuseg" : 1.0, "diffuseb" : 1.0
//
// We don't care about the separate channel names,
// We need find the actual SHOP parameter name and export attribute as "diffuse".
// Then we bake float and color attributes as map channels.
//
// * Bake color and float attributes into mesh's map channles named after the SHOP parameter name.
// * Check override type and descide whether to export a separate material or:
//   - Override attribute with mesh's map channel (using TexUserColor or TexUserScalar)
//   - Override attribute with texture id map (using TexMultiID)


int MeshExporter::getPerPrimMtlOverrides(std::unordered_set< std::string > &o_mapChannelOverrides,
										 std::vector< PrimOverride > &o_primOverrides)
{
	UT_ASSERT( m_gdp );

	GA_ROHandleS materialPathHndl(m_gdp->findAttribute(GA_ATTRIB_PRIMITIVE, GEO_STD_ATTRIB_MATERIAL));
	GA_ROHandleS materialOverrideHndl(m_gdp->findAttribute(GA_ATTRIB_PRIMITIVE, "material_override"));
	if (   !materialPathHndl.isValid()
		|| !materialOverrideHndl.isValid())
	{
		return 0;
	}

	const GEOPrimList &primList = getPrimList();
	o_primOverrides.resize(primList.size());

	int k = 0;
	for (const GEO_Primitive *prim : primList) {
		const GA_Offset off = prim->getMapOffset();

		PrimOverride &primOverride = o_primOverrides[k++];
		primOverride.shopNode = OPgetDirector()->findSHOPNode( materialPathHndl.get(off) );
		if (!primOverride.shopNode) {
			// material not set for this primitive => leave overrides empty
			continue;
		}

		// material override is python dict as string
		// using HDK helper class SHOP_GeoOverride to parse that
		SHOP_GeoOverride mtlOverride;
		mtlOverride.load( materialOverrideHndl.get(off) );
		if (mtlOverride.entries() <= 0) {
			// no overrides set for this primitive => leave overrides empty
			continue;
		}

		const PRM_ParmList *shopPrmList = primOverride.shopNode->getParmList();
		UT_StringArray mtlOverrideChs;
		mtlOverride.getKeys(mtlOverrideChs);
		for ( const UT_StringHolder &chName : mtlOverrideChs) {
			int chIdx = -1;
			PRM_Parm *prm = shopPrmList->getParmPtrFromChannel(chName, &chIdx);
			if (!prm) {
				// no corresponding parameter on the shop node
				continue;
			}
			// skip overrides of 4th component on 4-tuple params
			// we can not store the 4th channel in VRay::Vector which has only 3 components
			// TODO: need a way to export these
			if (   chIdx < 0
				|| chIdx >= 3)
			{
				continue;
			}
			// skip overrides on string and toggle params
			// they can't be exported as map channels
			const PRM_Type &prmType = prm->getType();
			if (   mtlOverride.isString(chName.buffer())
				|| NOT(prmType.isFloatType()) )
			{
				continue;
			}

			std::string channelName = prm->getToken();
			if ( primOverride.mtlOverrides.count(channelName) == 0 ) {
				// if we encounter this for first time for this primitive
				// get default value from the shop param. Thus if NOT all param channels
				// are overridden we still get the default value for the channel
				// from the shop param
				o_mapChannelOverrides.insert(channelName);
				VRay::Vector &val = primOverride.mtlOverrides[ channelName ];
				for (int i = 0; i < prm->getVectorSize() && i < 3; ++i) {
					fpreal fval = 0;
					prm->getValue(m_context.getTime(), fval, i, m_context.getThread());
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


int MeshExporter::getMtlOverrides(MapChannels &mapChannels)
{
	int nMapChannels = 0;

	std::unordered_set< std::string > mapChannelOverrides;
	std::vector< PrimOverride > primOverrides;
	if ( getPerPrimMtlOverrides(mapChannelOverrides, primOverrides) > 0) {

		const GEOPrimList &primList = getPrimList();
		for (const std::string channelName : mapChannelOverrides ) {
			if (mapChannels.count(channelName) > 0) {
				// if map channel with this name already exists
				// skip override
				continue;
			}

			MapChannel &mapChannel = mapChannels[ channelName ];
			// max number of different vertices in the channel is bounded by number of primitives
			mapChannel.vertices = VRay::VUtils::VectorRefList(primList.size());
			mapChannel.faces = VRay::VUtils::IntRefList(numFaces * 3);

			int k = 0;
			int faceVertIndex = 0;
			for (const GEO_Primitive *prim : primList) {
				PrimOverride &primOverride = primOverrides[k];
				if ( primOverride.shopNode ) {
					VRay::Vector &val = mapChannel.vertices[k];
					// if the parameter is overriden by the primitive get the overriden value
					if ( primOverride.mtlOverrides.count(channelName) ) {
						val = primOverride.mtlOverrides[channelName];
					}
					// else if the parameter exists on the shop node get the default value from there
					else if ( primOverride.shopNode->hasParm(channelName.c_str()) ) {
						const PRM_Parm &prm = primOverride.shopNode->getParm(channelName.c_str());
						for (int i = 0; i < prm.getVectorSize() && i < 3; ++i) {
							fpreal fval = 0;
							prm.getValue(m_context.getTime(), fval, i, m_context.getThread());
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
								mapChannel.faces[faceVertIndex++] = k;
								mapChannel.faces[faceVertIndex++] = k;
								mapChannel.faces[faceVertIndex++] = k;
							}
						}
						break;
					}
					case GEO_PRIMPOLY:
					{
						GA_Size vCnt = std::max(prim->getVertexCount() - 2, GA_Size(0));
						for (GA_Size j = 0; j < vCnt; ++j) {
							mapChannel.faces[faceVertIndex++] = k;
							mapChannel.faces[faceVertIndex++] = k;
							mapChannel.faces[faceVertIndex++] = k;
						}
						break;
					}
					default:
						;
				}

				++k;
			}

			++nMapChannels;
		}
	}

	return nMapChannels;
}


int MeshExporter::getPointAttrs(MapChannels &mapChannels)
{
	UT_ASSERT( m_gdp );

	int nMapChannels = 0;

	// add all vector3 vertex attributes to map_channels_data
	GEOAttribList attrList;
	m_gdp->getAttributes().matchAttributes(GEOgetV3AttribFilter(),
										GA_ATTRIB_POINT,
										attrList);
	for (const GA_Attribute *attr : attrList) {
		// "P","N","v" point attributes are handled separately
		// as different plugin properties so skip them here
		if (   attr
			&& attr->getName() != GEO_STD_ATTRIB_POSITION
			&& attr->getName() != GEO_STD_ATTRIB_NORMAL
			&& attr->getName() != GEO_STD_ATTRIB_VELOCITY )
		{
			const std::string attrName = attr->getName().toStdString();
			if (!mapChannels.count(attrName)) {
				MapChannel &mapChannel = mapChannels[attrName];
				mapChannel.name = attrName;
				mapChannel.vertices = VRay::VUtils::VectorRefList(getNumVertices());
				// we can use same face indices as for mesh vertices
				mapChannel.faces = getFaces();

				getDataFromAttribute(attr, mapChannel.vertices);
				UT_ASSERT( m_gdp->getNumPoints() == mapChannel.vertices.size() );

				++nMapChannels;
			}
		}
	}

	return nMapChannels;
}


int MeshExporter::getVertexAttrs(MapChannels &mapChannels)
{
	UT_ASSERT( m_gdp );

	int nMapChannels = 0;

	// add all vector3 vertex attributes to map_channels_data
	GEOAttribList attrList;
	m_gdp->getAttributes().matchAttributes(GEOgetV3AttribFilter(),
										GA_ATTRIB_VERTEX,
										attrList);
	for (const GA_Attribute *attr : attrList) {
		// "P","N","v" vertex attributes are handled separately
		// as different plugin properties so skip them here
		if (   attr
			&& attr->getName() != GEO_STD_ATTRIB_POSITION
			&& attr->getName() != GEO_STD_ATTRIB_NORMAL
			&& attr->getName() != GEO_STD_ATTRIB_VELOCITY )
		{
			const std::string attrName = attr->getName().toStdString();
			if (!mapChannels.count(attrName)) {
				MapChannel &map_channel = mapChannels[attrName];
				getVertexAttrAsMapChannel(*attr, map_channel);
				++nMapChannels;
			}
		}
	}

	return nMapChannels;
}


void MeshExporter::getVertexAttrAsMapChannel(const GA_Attribute &attr, MapChannel &mapChannel)
{
	UT_ASSERT( m_gdp );

	mapChannel.name = attr.getName();
	Log::getLog().info("Found map channel: %s", mapChannel.name.c_str());

	GA_ROPageHandleV3 vaPageHndl(&attr);
	GA_ROHandleV3 vaHndl(&attr);

	if (m_hasSubdivApplied) {
		// weld vertex attribute values before populating the map channel
		GA_Offset start, end;
		for (GA_Iterator it(m_gdp->getVertexRange()); it.blockAdvance(start, end); ) {
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
		for (auto &mv: mapChannel.verticesSet) {
			mv.index = i;
			mapChannel.vertices[i++].set(mv.v[0], mv.v[1], mv.v[2]);
		}

		UT_ASSERT( i == mapChannel.vertices.size() );

		// Process map channels (uv and other tuple(3) attributes)
		int faceVertIndex = 0;
		const GEOPrimList &primList = getPrimList();
		for (const GEO_Primitive *prim : primList) {

			switch (prim->getTypeId().get()) {
				case GEO_PRIMPOLYSOUP:
				{
					const GU_PrimPolySoup *polySoup = static_cast<const GU_PrimPolySoup*>(prim);
					for (GEO_PrimPolySoup::PolygonIterator pst(*polySoup); !pst.atEnd(); ++pst) {
						GA_Size vCnt = pst.getVertexCount();
						if ( vCnt > 2) {
							for (GA_Size i = 1; i < vCnt-1; ++i) {
								// polygon orientation seems to be clockwise in Houdini
								mapChannel.faces[faceVertIndex++] = mapChannel.verticesSet.find(MapVertex(vaHndl.get(prim->getVertexOffset(i+1))))->index;
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
					if ( vCnt > 2) {
						for (GA_Size i = 1; i < vCnt-1; ++i) {
							// polygon orientation seems to be clockwise in Houdini
							mapChannel.faces[faceVertIndex++] = mapChannel.verticesSet.find(MapVertex(vaHndl.get(prim->getVertexOffset(i+1))))->index;
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

		UT_ASSERT( faceVertIndex == mapChannel.faces.size() );

		// cleanup hash
		mapChannel.verticesSet.clear();
	}
	else {
		// populate map channel with original values

		// init map channel data
		mapChannel.vertices = VRay::VUtils::VectorRefList(m_gdp->getNumVertices());
		mapChannel.faces = VRay::VUtils::IntRefList(getNumFaces() * 3);

		getDataFromAttribute(&attr, mapChannel.vertices);

		int faceVertIndex = 0;
		const GEOPrimList &primList = getPrimList();
		for (const GEO_Primitive *prim : primList) {

			switch (prim->getTypeId().get()) {
				case GEO_PRIMPOLYSOUP:
				{
					const GU_PrimPolySoup *polySoup = static_cast<const GU_PrimPolySoup*>(prim);
					for (GEO_PrimPolySoup::PolygonIterator pst(*polySoup); !pst.atEnd(); ++pst) {
						const GA_Size vCnt = pst.getVertexCount();
						if ( vCnt > 2) {
							for (GA_Size i = 1; i < vCnt-1; ++i) {
								// polygon orientation seems to be clockwise in Houdini
								mapChannel.faces[faceVertIndex++] = pst.getVertexIndex(i+1);
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
					if ( vCnt > 2) {
						for (GA_Size i = 1; i < vCnt-1; ++i) {
							// polygon orientation seems to be clockwise in Houdini
							mapChannel.faces[faceVertIndex++] = m_gdp->getVertexMap().indexFromOffset(prim->getVertexOffset(i+1));
							mapChannel.faces[faceVertIndex++] = m_gdp->getVertexMap().indexFromOffset(prim->getVertexOffset(i));
							mapChannel.faces[faceVertIndex++] = m_gdp->getVertexMap().indexFromOffset(prim->getVertexOffset(0));
						}
					}
					break;
				}
				default:
					;
			}
		}

		UT_ASSERT( faceVertIndex == mapChannel.faces.size() );
	}
}
