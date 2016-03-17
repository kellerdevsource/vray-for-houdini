//
// Copyright (c) 2015, Chaos Software Ltd
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
#include <GA/GA_PageHandle.h>


using namespace VRayForHoudini;


bool PolyMeshExporter::isPrimPoly(GA_Primitive &prim)
{
	return (
			   prim.getTypeId() == GEO_PRIMPOLY
			|| prim.getTypeId() == GEO_PRIMPOLYSOUP
			);
}


bool PolyMeshExporter::getDataFromAttribute(const GA_Attribute *attr, VRay::VUtils::VectorRefList &data)
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


PolyMeshExporter::PolyMeshExporter(const GU_Detail &gdp, VRayExporter &pluginExporter):
	m_gdp(gdp),
	m_context(pluginExporter.getContext()),
	m_pluginExporter(pluginExporter),
	m_sopNode(nullptr),
	m_hasSubdivApplied(false),
	numFaces(-1),
	numMtlIDs(-1)
{ }


bool PolyMeshExporter::hasPolyGeometry() const
{
	return (
			   m_gdp.containsPrimitiveType(GEO_PRIMPOLY)
			|| m_gdp.containsPrimitiveType(GEO_PRIMPOLYSOUP)
			);
}


std::string PolyMeshExporter::getVRayPluginName() const
{
	std::string pluginName = boost::str(Parm::FmtPrefixManual % "Geom" % std::to_string(m_gdp.getUniqueId()));
	return (m_sopNode)? m_pluginExporter.getPluginName(m_sopNode, pluginName) : pluginName;
}


bool PolyMeshExporter::asPluginDesc(Attrs::PluginDesc &pluginDesc)
{
	if (NOT(hasPolyGeometry())) {
		return false;
	}

	Log::getLog().info("  Mesh: %i points", m_gdp.getNumPoints());

	pluginDesc.pluginName = getVRayPluginName();
	pluginDesc.pluginID = getVRayPluginID();

	if (m_pluginExporter.isIPR() && m_pluginExporter.isGPU()) {
		pluginDesc.addAttribute(Attrs::PluginAttr("dynamic_geometry", true));
	}

	pluginDesc.addAttribute(Attrs::PluginAttr("vertices", getVertices()));
	pluginDesc.addAttribute(Attrs::PluginAttr("faces", getFaces()));
	pluginDesc.addAttribute(Attrs::PluginAttr("edge_visibility", getEdgeVisibility()));
	pluginDesc.addAttribute(Attrs::PluginAttr("face_mtlIDs", getFaceMtlIDs()));

	if (getNumNormals() > 0) {
		pluginDesc.addAttribute(Attrs::PluginAttr("normals", getNormals()));
		pluginDesc.addAttribute(Attrs::PluginAttr("faceNormals", getFaceNormals()));
	}

	if (getNumMapChannels() > 0) {
		// TODO: use CharStringList for map_channel_names
		VRay::VUtils::ValueRefList map_channel_names(map_channels_data.size());
		VRay::VUtils::ValueRefList map_channels(map_channels_data.size());

		int i = 0;
		for (const auto &mcIt : map_channels_data) {
			const std::string      &map_channel_name = mcIt.first;
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


VRay::VUtils::VectorRefList& PolyMeshExporter::getVertices()
{
	if (vertices.size() <= 0) {
		getDataFromAttribute(m_gdp.findAttribute(GA_ATTRIB_POINT, GEO_STD_ATTRIB_POSITION), vertices);
		UT_ASSERT( m_gdp.getNumPoints() == vertices.size() );
	}

	return vertices;
}


VRay::VUtils::VectorRefList& PolyMeshExporter::getNormals()
{
	if (normals.size() <= 0) {
		getDataFromAttribute(m_gdp.findAttribute(GA_ATTRIB_POINT, GEO_STD_ATTRIB_NORMAL), normals);
		UT_ASSERT( m_gdp.getNumPoints() == normals.size() );
	}

	return normals;
}


VRay::VUtils::IntRefList& PolyMeshExporter::getFaces()
{
	if (faces.size() <= 0) {
		numFaces = getMeshFaces(faces, edge_visibility);
	}

	return faces;
}


VRay::VUtils::IntRefList& PolyMeshExporter::getEdgeVisibility()
{
	if (edge_visibility.size() <= 0) {
		numFaces = getMeshFaces(faces, edge_visibility);
	}

	return edge_visibility;
}


VRay::VUtils::IntRefList& PolyMeshExporter::getFaceMtlIDs()
{
	if (face_mtlIDs.size() <= 0) {
		numMtlIDs = getMtlIds(face_mtlIDs);
		UT_ASSERT( face_mtlIDs.size() == 0 || face_mtlIDs.size() == numFaces );
	}

	return face_mtlIDs;
}

MapChannels& PolyMeshExporter::getMapChannels()
{
	if (map_channels_data.size() <= 0) {
		int nMapChannels = 0;
		nMapChannels += getMtlOverrides(map_channels_data);
		nMapChannels += getVertexAttrs(map_channels_data);
		nMapChannels += getPointAttrs(map_channels_data);
		UT_ASSERT( map_channels_data.size() == nMapChannels );
	}

	return map_channels_data;
}


GA_Size PolyMeshExporter::countFaces() const
{
	GA_Size nFaces = 0;
	for (GA_Iterator jt(m_gdp.getPrimitiveRange()); !jt.atEnd(); jt.advance()) {
		const GEO_Primitive *prim = m_gdp.getGEOPrimitive(*jt);

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


int PolyMeshExporter::getMeshFaces(VRay::VUtils::IntRefList &faces, VRay::VUtils::IntRefList &edge_visibility)
{
	int nFaces = getNumFaces();
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
	for (GA_Iterator jt(m_gdp.getPrimitiveRange()); !jt.atEnd(); jt.advance()) {
		const GEO_Primitive *prim = m_gdp.getGEOPrimitive(*jt);

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


int PolyMeshExporter::getMtlIds(VRay::VUtils::IntRefList &face_mtlIDs)
{
	GA_ROHandleS mtlpath(m_gdp.findAttribute(GA_ATTRIB_PRIMITIVE, GEO_STD_ATTRIB_MATERIAL));
	if (mtlpath.isInvalid()) {
		return 0;
	}

	int nFaces = getNumFaces();
	if (nFaces <= 0) {
		return 0;
	}

	face_mtlIDs = VRay::VUtils::IntRefList(nFaces);

	int faceIndex = 0;
	for (GA_Iterator jt(m_gdp.getPrimitiveRange()); !jt.atEnd(); jt.advance()) {
		const GEO_Primitive *prim = m_gdp.getGEOPrimitive(*jt);
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


int PolyMeshExporter::getPerPrimMtlOverrides(std::unordered_set< std::string > &o_mapChannelOverrides, std::vector< PrimOverride > &o_primOverrides) const
{
	const GA_ROHandleS materialPathHndl(m_gdp.findStringTuple(GA_ATTRIB_PRIMITIVE, "shop_materialpath"));
	const GA_ROHandleS materialOverrideHndl(m_gdp.findStringTuple(GA_ATTRIB_PRIMITIVE, "material_override"));
	if (   !materialPathHndl.isValid()
		|| !materialOverrideHndl.isValid())
	{
		return 0;
	}

	o_primOverrides.resize(m_gdp.getNumPrimitives());

	int k = 0;
	for (GA_Iterator jt(m_gdp.getPrimitiveRange()); !jt.atEnd(); jt.advance(), ++k) {
		const GA_Offset off = *jt;
		const GEO_Primitive *prim = m_gdp.getGEOPrimitive(off);
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


int PolyMeshExporter::getMtlOverrides(MapChannels &mapChannels)
{
	int nMapChannels = 0;

	std::unordered_set< std::string > mapChannelOverrides;
	std::vector< PrimOverride > primOverrides;
	if ( getPerPrimMtlOverrides(mapChannelOverrides, primOverrides) > 0) {

		for (const std::string channelName : mapChannelOverrides ) {
			MapChannel &mapChannel = mapChannels[ channelName ];
			// max number of different vertices int the channel is bounded by number of primitives
			mapChannel.vertices = VRay::VUtils::VectorRefList(m_gdp.getNumPrimitives());
			mapChannel.faces = VRay::VUtils::IntRefList(numFaces * 3);

			++nMapChannels;

			int k = 0;
			int faceVertIndex = 0;
			for (GA_Iterator jt(m_gdp.getPrimitiveRange()); !jt.atEnd(); jt.advance(), ++k) {
				const GEO_Primitive *prim = m_gdp.getGEOPrimitive(*jt);
				if (   prim->getTypeId() != GEO_PRIMPOLY
					&& prim->getTypeId() != GEO_PRIMPOLYSOUP)
				{
					continue;
				}

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
			}
		}
	}

	return nMapChannels;
}


int PolyMeshExporter::getPointAttrs(MapChannels &mapChannels)
{
	// add all vector3 point attributes to map_channels_data
	GA_AttributeFilter float3Filter = GA_AttributeFilter::selectAnd(
				GA_AttributeFilter::selectFloatTuple(),
				GA_AttributeFilter::selectByTupleSize(3)
				);

	int nMapChannels = 0;

	for (GA_AttributeDict::iterator attrIt = m_gdp.getAttributeDict(GA_ATTRIB_POINT).begin(GA_SCOPE_PUBLIC); !attrIt.atEnd(); ++attrIt) {
		const std::string attrName(attrIt.name());
		// "P" and "N" point attributes are handled separately as different plugin properties
		// so skip them here
		if (   float3Filter.match(attrIt.attrib())
			&& attrName != "P"
			&& attrName != "N" )
		{
			if (NOT(mapChannels.count(attrName))) {
				MapChannel &mapChannel = mapChannels[attrName];
				mapChannel.name = attrName;
				mapChannel.vertices = VRay::VUtils::VectorRefList(getNumVertices());
				// we can use same face indices as for mesh vertices
				mapChannel.faces = getFaces();

				++nMapChannels;

				getDataFromAttribute(attrIt.attrib(), mapChannel.vertices);
				UT_ASSERT( m_gdp.getNumPoints() == mapChannel.vertices.size() );

			}
		}
	}

	return nMapChannels;
}


int PolyMeshExporter::getVertexAttrs(MapChannels &mapChannels)
{
	// add all vector3 vertex attributes to map_channels_data
	GA_AttributeFilter float3Filter = GA_AttributeFilter::selectAnd(
				GA_AttributeFilter::selectFloatTuple(),
				GA_AttributeFilter::selectByTupleSize(3)
				);

	int nMapChannels = 0;

	for (GA_AttributeDict::iterator attrIt = m_gdp.getAttributeDict(GA_ATTRIB_VERTEX).begin(GA_SCOPE_PUBLIC); !attrIt.atEnd(); ++attrIt) {
		const std::string attrName(attrIt.name());
		// "P" and "N" point attributes are handled separately as different plugin properties
		// so skip them here
		if (   float3Filter.match(attrIt.attrib())
			&& attrName != "P"
			&& attrName != "N" )
		{
			if (NOT(mapChannels.count(attrName))) {
				MapChannel &map_channel = mapChannels[attrName];
				getVertexAttrAsMapChannel(*attrIt.attrib(), map_channel);

				++nMapChannels;
			}
		}
	}

	return nMapChannels;
}


void PolyMeshExporter::getVertexAttrAsMapChannel(const GA_Attribute &attr, MapChannel &mapChannel)
{
	mapChannel.name = attr.getName();
	Log::getLog().info("  Found map channel: %s",
					   mapChannel.name.c_str());


	GA_ROPageHandleV3 vaPageHndl(&attr);
	GA_ROHandleV3 vaHndl(&attr);

	if (m_hasSubdivApplied) {
		// weld vertex attribute values before populating the map channel
		GA_Offset start, end;
		for (GA_Iterator it(m_gdp.getVertexRange()); it.blockAdvance(start, end); ) {
			vaPageHndl.setPage(start);
			for (GA_Offset offset = start; offset < end; ++offset) {
				const UT_Vector3 &val = vaPageHndl.value(offset);
				mapChannel.verticesSet.insert(MapVertex(val));
			}
		}

		// Init map channel data
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
		for (GA_Iterator jt(m_gdp.getPrimitiveRange()); !jt.atEnd(); jt.advance()) {
			const GEO_Primitive *prim = m_gdp.getGEOPrimitive(*jt);

			switch (prim->getTypeId().get()) {
				case GEO_PRIMPOLYSOUP:
				{
					const GU_PrimPolySoup *polySoup = static_cast<const GU_PrimPolySoup*>(prim);
					for (GEO_PrimPolySoup::PolygonIterator pst(*polySoup); !pst.atEnd(); ++pst) {
						GA_Size vCnt = pst.getVertexCount();
						if ( vCnt > 2) {
							for (GA_Size i = 1; i < vCnt-1; ++i) {
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

		// Cleanup hash
		mapChannel.verticesSet.clear();
	}
	else {
		// populate map channel with original values

		// Init map channel data
		mapChannel.vertices = VRay::VUtils::VectorRefList(m_gdp.getNumVertices());
		mapChannel.faces = VRay::VUtils::IntRefList(getNumFaces() * 3);

		getDataFromAttribute(&attr, mapChannel.vertices);

		int faceVertIndex = 0;
		for (GA_Iterator jt(m_gdp.getPrimitiveRange()); !jt.atEnd(); jt.advance()) {
			const GEO_Primitive *prim = m_gdp.getGEOPrimitive(*jt);

			switch (prim->getTypeId().get()) {
				case GEO_PRIMPOLYSOUP:
				{
					const GU_PrimPolySoup *polySoup = static_cast<const GU_PrimPolySoup*>(prim);
					for (GEO_PrimPolySoup::PolygonIterator pst(*polySoup); !pst.atEnd(); ++pst) {
						GA_Size vCnt = pst.getVertexCount();
						if ( vCnt > 2) {
							for (GA_Size i = 1; i < vCnt-1; ++i) {
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
					GA_Size vCnt = prim->getVertexCount();
					if ( vCnt > 2) {
						for (GA_Size i = 1; i < vCnt-1; ++i) {
							mapChannel.faces[faceVertIndex++] = m_gdp.getVertexMap().indexFromOffset(prim->getVertexOffset(i+1));
							mapChannel.faces[faceVertIndex++] = m_gdp.getVertexMap().indexFromOffset(prim->getVertexOffset(i));
							mapChannel.faces[faceVertIndex++] = m_gdp.getVertexMap().indexFromOffset(prim->getVertexOffset(0));
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
