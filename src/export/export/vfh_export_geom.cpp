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
#include <SOP/SOP_Node.h>
#include <SHOP/SHOP_Node.h>
#include <SHOP/SHOP_GeoOverride.h>
#include <PRM/PRM_ParmMicroNode.h>
#include <CH/CH_Channel.h>

#include <GU/GU_Detail.h>
#include <GU/GU_PrimPolySoup.h>
#include <GU/GU_PrimPacked.h>
#include <GU/GU_PackedDisk.h>
#include <GU/GU_PackedGeometry.h>
#include <GA/GA_PageHandle.h>

#include <GEO/GEO_PrimType.h>


#include <UT/UT_Version.h>
#include <UT/UT_Assert.h>

#include <unordered_set>
#include <unordered_map>


using namespace VRayForHoudini;


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


enum GEO_PrimPackedType
{
	GEO_PACKEDGEOMETRY = 24,
	GEO_PACKEDDISK = 25,
	GEO_ALEMBICREF = 28,
};


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



class PolyMeshExporter
{
public:
	static bool isPrimPoly(GA_Primitive &prim);
	static bool getDataFromAttribute(const GA_Attribute *attr, VRay::VUtils::VectorRefList &data);

public:
	PolyMeshExporter(const GU_Detail &gdp, VRayExporter &pluginExporter);
	~PolyMeshExporter() { }

	PolyMeshExporter& setSOPContext(SOP_Node *sop) { m_sopNode = sop; return *this; }
	PolyMeshExporter& setSubdivApplied(bool val) { m_hasSubdivApplied = val; return *this; }

	bool                         hasPolyGeometry() const;
	bool                         hasSubdivApplied() const { return m_hasSubdivApplied; }
	int                          getNumVertices() { return getVertices().size(); }
	int                          getNumNormals() { return getNormals().size(); }
	int                          getNumFaces()  { if (numFaces <= 0 ) numFaces = countFaces(); return numFaces; }
	int                          getNumMtlIDs() { if (numMtlIDs <= 0 ) getFaceMtlIDs(); return numMtlIDs; }
	int                          getNumMapChannels() { return getMapChannels().size(); }
	VRay::VUtils::VectorRefList& getVertices();
	VRay::VUtils::VectorRefList& getNormals();
	VRay::VUtils::IntRefList&    getFaces();
	VRay::VUtils::IntRefList&    getFaceNormals() { return getFaces(); }
	VRay::VUtils::IntRefList&    getEdgeVisibility();
	VRay::VUtils::IntRefList&    getFaceMtlIDs();
	Mesh::MapChannels&           getMapChannels();

	int                          getSHOPList(SHOPList &shopList) const;

	std::string                  getVRayPluginType() const { return "GEOMETRY"; }
	std::string                  getVRayPluginID() const   { return "GeomStaticMesh"; }
	std::string                  getVRayPluginName() const;
	bool                         asPluginDesc(Attrs::PluginDesc &pluginDesc);

private:
	struct PrimOverride
	{
		typedef std::unordered_map< std::string, VRay::Vector > MtlOverrides;

		PrimOverride(SHOP_Node *shopNode = nullptr):
			shopNode(shopNode)
		{}

		SHOP_Node *shopNode;
		MtlOverrides mtlOverrides;
	};

private:
	GA_Size countFaces() const;
	int     getMeshFaces(VRay::VUtils::IntRefList &faces, VRay::VUtils::IntRefList &edge_visibility);
	int     getMtlIds(VRay::VUtils::IntRefList &face_mtlIDs);
	int     getPointAttrs(Mesh::MapChannels &mapChannels);
	int     getVertexAttrs(Mesh::MapChannels &mapChannels);
	void    getVertexAttrAsMapChannel(const GA_Attribute &attr, Mesh::MapChannel &mapChannel);
	int     getMtlOverrides(Mesh::MapChannels &mapChannels);
	int     getPerPrimMtlOverrides(std::unordered_set< std::string > &o_mapChannelOverrides, std::vector< PrimOverride > &o_primOverrides) const;

private:
	const GU_Detail  &m_gdp;
	OP_Context       &m_context;
	VRayExporter     &m_pluginExporter;
	SOP_Node         *m_sopNode;

	bool              m_hasSubdivApplied;
	int               numFaces;
	int               numMtlIDs;
	VRay::VUtils::VectorRefList vertices;
	VRay::VUtils::VectorRefList normals;
	VRay::VUtils::IntRefList    faces;
	VRay::VUtils::IntRefList    edge_visibility;
	VRay::VUtils::IntRefList    face_mtlIDs;
	Mesh::MapChannels           map_channels_data;
};


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


int PolyMeshExporter::getSHOPList(SHOPList &shopList) const
{
	GA_ROHandleS mtlpath(m_gdp.findAttribute(GA_ATTRIB_PRIMITIVE, GEO_STD_ATTRIB_MATERIAL));
	if (mtlpath.isInvalid()) {
		return 0;
	}

	int shopCnt = 0;
	for (GA_Iterator jt(m_gdp.getPrimitiveRange()); !jt.atEnd(); jt.advance()) {
		const GEO_Primitive *prim = m_gdp.getGEOPrimitive(*jt);

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
		VRay::VUtils::ValueRefList map_channel_names(map_channels_data.size());
		VRay::VUtils::ValueRefList map_channels(map_channels_data.size());

		int i = 0;
		for (const auto &mcIt : map_channels_data) {
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

Mesh::MapChannels& PolyMeshExporter::getMapChannels()
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
	std::memset(edge_visibility.get(), unsigned(-1), edge_visibility.size() * sizeof(int));

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


int PolyMeshExporter::getMtlOverrides(Mesh::MapChannels &mapChannels)
{
	int nMapChannels = 0;

	std::unordered_set< std::string > mapChannelOverrides;
	std::vector< PrimOverride > primOverrides;
	if ( getPerPrimMtlOverrides(mapChannelOverrides, primOverrides) > 0) {

		for (const std::string channelName : mapChannelOverrides ) {
			Mesh::MapChannel &mapChannel = mapChannels[ channelName ];
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


int PolyMeshExporter::getPointAttrs(Mesh::MapChannels &mapChannels)
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
				Mesh::MapChannel &mapChannel = mapChannels[attrName];
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


int PolyMeshExporter::getVertexAttrs(Mesh::MapChannels &mapChannels)
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
				Mesh::MapChannel &map_channel = mapChannels[attrName];
				getVertexAttrAsMapChannel(*attrIt.attrib(), map_channel);

				++nMapChannels;
			}
		}
	}

	return nMapChannels;
}


void PolyMeshExporter::getVertexAttrAsMapChannel(const GA_Attribute &attr, Mesh::MapChannel &mapChannel)
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
				mapChannel.verticesSet.insert(Mesh::MapVertex(val));
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
								mapChannel.faces[faceVertIndex++] = mapChannel.verticesSet.find(Mesh::MapVertex(vaHndl.get(prim->getVertexOffset(i+1))))->index;
								mapChannel.faces[faceVertIndex++] = mapChannel.verticesSet.find(Mesh::MapVertex(vaHndl.get(prim->getVertexOffset(i))))->index;
								mapChannel.faces[faceVertIndex++] = mapChannel.verticesSet.find(Mesh::MapVertex(vaHndl.get(prim->getVertexOffset(0))))->index;
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
							mapChannel.faces[faceVertIndex++] = mapChannel.verticesSet.find(Mesh::MapVertex(vaHndl.get(prim->getVertexOffset(i+1))))->index;
							mapChannel.faces[faceVertIndex++] = mapChannel.verticesSet.find(Mesh::MapVertex(vaHndl.get(prim->getVertexOffset(i))))->index;
							mapChannel.faces[faceVertIndex++] = mapChannel.verticesSet.find(Mesh::MapVertex(vaHndl.get(prim->getVertexOffset(0))))->index;
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


VRay::Plugin VRayExporter::exportGeomStaticMesh(SOP_Node &sop_node, const GU_Detail &gdp, GeomExportParams &expParams)
{
	return VRay::Plugin();

//	bool hasPolyGeometry = gdp.containsPrimitiveType(GEO_PRIMPOLY) || gdp.containsPrimitiveType(GEO_PRIMPOLYSOUP);
//	if ( NOT(hasPolyGeometry)) {
//		return VRay::Plugin();
//	}

//	Attrs::PluginDesc geomPluginDesc(VRayExporter::getPluginName(&sop_node, boost::str(Parm::FmtPrefixManual % "Geom" % std::to_string(gdp.getUniqueId()))), "GeomStaticMesh");
//	exportGeomStaticMeshDesc(gdp, expParams, geomPluginDesc);
//	return exportPlugin(geomPluginDesc);
}



class GeometryExporter
{
	typedef std::vector< VRay::Plugin > PluginList;
	typedef std::list< Attrs::PluginDesc > PluginDescList;
	typedef std::unordered_map< uint, PluginDescList > DetailToPluginDesc;

public:
	GeometryExporter(OBJ_Geometry &node, VRayExporter &pluginExporter);
	~GeometryExporter() { }

	bool             hasSubdivApplied() const;
	int              exportGeometry();
	int              getNumPlugins() const { return m_pluginList.size(); }
	VRay::Plugin&    getPluginAt(int idx) { return m_pluginList.at(idx); }

private:
	void             cleanup();
	int              exportVRaySOP(SOP_Node &sop, PluginDescList &pluginList);
	int              exportHair(SOP_Node &sop, GU_DetailHandleAutoReadLock &gdl, PluginDescList &pluginList);
	int              exportDetail(SOP_Node &sop, GU_DetailHandleAutoReadLock &gdl, PluginDescList &pluginList);
	int              exportPolyMesh(SOP_Node &sop, const GU_Detail &gdp, PluginDescList &pluginList);

	int              exportPacked(SOP_Node &sop, const GU_PrimPacked &prim, PluginDescList &pluginList);
	uint             getPrimPackedID(const GU_PrimPacked &prim);
	int              exportPrimPacked(SOP_Node &sop, const GU_PrimPacked &prim, PluginDescList &pluginList);
	int              exportAlembicRef(SOP_Node &sop, const GU_PrimPacked &prim, PluginDescList &pluginList);
	int              exportPackedDisk(SOP_Node &sop, const GU_PrimPacked &prim, PluginDescList &pluginList);
	int              exportPackedGeometry(SOP_Node &sop, const GU_PrimPacked &prim, PluginDescList &pluginList);

private:
	OBJ_Geometry &m_objNode;
	OP_Context   &m_context;
	VRayExporter &m_pluginExporter;

	// packed geometry detail
	DetailToPluginDesc   m_detailToPluginDesc;
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
	SOP_Node *renderSOP = m_objNode.getRenderSopPtr();
	if (NOT(renderSOP)) {
		return 0;
	}

	GU_DetailHandleAutoReadLock gdl(renderSOP->getCookedGeoHandle(m_context));
	if (NOT(gdl.isValid())) {
		return 0;
	}

	uint detailHash = gdl.handle().hash();
	const GU_Detail &gdp = *gdl.getGdp();

	if (renderSOP->getOperator()->getName().startsWith("VRayNode")) {
		exportVRaySOP(*renderSOP, m_detailToPluginDesc[detailHash]);
	}
	else {
		GA_ROAttributeRef ref_guardhair(gdp.findAttribute(GA_ATTRIB_PRIMITIVE, "guardhair"));
		GA_ROAttributeRef ref_hairid(gdp.findAttribute(GA_ATTRIB_PRIMITIVE, "hairid"));

		if (ref_guardhair.isValid() && ref_hairid .isValid()) {
			exportHair(*renderSOP, gdl, m_detailToPluginDesc[detailHash]);
		}
		else {
			exportDetail(*renderSOP, gdl, m_detailToPluginDesc[detailHash]);
		}
	}

	PluginDescList &pluginList = m_detailToPluginDesc.at(detailHash);
	m_pluginList.reserve(pluginList.size());

	SHOP_Node *shopNode = m_pluginExporter.getObjMaterial(&m_objNode, m_context.getTime());

	int i = 0;
	for (Attrs::PluginDesc &nodeDesc : pluginList) {
//		TODO: need to fill in node with appropriate names and export them
		nodeDesc.pluginName = VRayExporter::getPluginName(&m_objNode, boost::str(Parm::FmtPrefixManual % "Node" % std::to_string(i++)));

		Attrs::PluginAttr *attr = nullptr;

		attr = nodeDesc.get("geometry");
		if (attr) {
			VRay::Plugin geomDispl = m_pluginExporter.exportDisplacement(&m_objNode, attr->paramValue.valPlugin);
			if (geomDispl) {
				attr->paramValue.valPlugin = geomDispl;
			}
		}

		attr = nodeDesc.get("visible");
		if (NOT(attr)) {
			nodeDesc.addAttribute(Attrs::PluginAttr("visible", m_objNode.getVisible()));
		}
		else {
			attr->paramValue.valInt = m_objNode.getVisible();
		}

		bool flipTm = (nodeDesc.pluginID == "GeomPlane")? true : false;
		VRay::Transform tm = VRayExporter::getObjTransform(&m_objNode, m_context, flipTm);
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


int GeometryExporter::exportVRaySOP(SOP_Node &sop, PluginDescList &pluginList)
{
	int nPlugins = 0;
	SOP::NodeBase *vrayNode = UTverify_cast< SOP::NodeBase * >(&sop);

	Attrs::PluginDesc geomDesc;
	OP::VRayNode::PluginResult res = vrayNode->asPluginDesc(geomDesc, m_pluginExporter);

	if (res == OP::VRayNode::PluginResultError) {
		Log::getLog().error("Error creating plugin descripion for node: \"%s\" [%s]",
					sop.getName().buffer(),
					sop.getOperator()->getName().buffer());
	}
	else if (res == OP::VRayNode::PluginResultNA ||
			 res == OP::VRayNode::PluginResultContinue)
	{
		m_pluginExporter.setAttrsFromOpNodePrms(geomDesc, &sop);
	}

	VRay::Plugin geom = m_pluginExporter.exportPlugin(geomDesc);

	// add new node to our list of nodes
	pluginList.push_back(Attrs::PluginDesc("", "Node"));
	Attrs::PluginDesc &nodeDesc = pluginList.back();
	nPlugins = 1;

	// geometry
	nodeDesc.addAttribute(Attrs::PluginAttr("geometry", geom));

	return nPlugins;
}


int GeometryExporter::exportHair(SOP_Node &sop, GU_DetailHandleAutoReadLock &gdl, PluginDescList &pluginList)
{
	int nPlugins = 0;

	VRay::Plugin geom = m_pluginExporter.exportGeomMayaHair(&sop, gdl.getGdp());
	if (geom) {
		// add new node to our list of nodes
		pluginList.push_back(Attrs::PluginDesc("", "Node"));
		Attrs::PluginDesc &nodeDesc = pluginList.back();
		nPlugins = 1;

		// geometry
		nodeDesc.addAttribute(Attrs::PluginAttr("geometry", geom));
	}

	return nPlugins;
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
			auto *primPacked = UTverify_cast< const GU_PrimPacked * >(prim);
			nPlugins += exportPacked(sop, *primPacked, pluginList);
		}
	}

	// polygonal geometry
	nPlugins += exportPolyMesh(sop, gdp, pluginList);

	return nPlugins;
}


int GeometryExporter::exportPolyMesh(SOP_Node &sop, const GU_Detail &gdp, PluginDescList &pluginList)
{
	int nPlugins = 0;

	PolyMeshExporter polyMeshExporter(gdp, m_pluginExporter);
	polyMeshExporter.setSOPContext(&sop)
					.setSubdivApplied(hasSubdivApplied());

	Attrs::PluginDesc geomDesc;
	if (polyMeshExporter.asPluginDesc(geomDesc)) {
		VRay::Plugin geom = m_pluginExporter.exportPlugin(geomDesc);

		// add new node to our list of nodes
		pluginList.push_back(Attrs::PluginDesc("", "Node"));
		nPlugins = 1;

		Attrs::PluginDesc &nodeDesc = pluginList.back();
		// geoemtry
		nodeDesc.addAttribute(Attrs::PluginAttr("geometry", geom));

		// material
		SHOPList shopList;
		int nSHOPs = polyMeshExporter.getSHOPList(shopList);
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
	uint packedID = getPrimPackedID(prim);
	if (NOT(m_detailToPluginDesc.count(packedID))) {
		exportPrimPacked(sop, prim, m_detailToPluginDesc[packedID]);
	}

	PluginDescList primPluginList = m_detailToPluginDesc.at(packedID);

	UT_Matrix4D fullxform;
	prim.getFullTransform4(fullxform);
	VRay::Transform tm = VRayExporter::Matrix4ToTransform(fullxform);

	GA_ROHandleS mtlpath(prim.getDetail().findAttribute(GA_ATTRIB_PRIMITIVE, GEO_STD_ATTRIB_MATERIAL));
	GA_ROHandleS mtlo(prim.getDetail().findAttribute(GA_ATTRIB_PRIMITIVE, "material_override"));

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


uint GeometryExporter::getPrimPackedID(const GU_PrimPacked &prim)
{
	uint packedID = 0;

	switch (prim.getTypeId().get()) {
		case GEO_ALEMBICREF:
		case GEO_PACKEDDISK:
		{
			UT_String primname;
			prim.getIntrinsic(prim.findIntrinsic("packedprimitivename"), primname);
			packedID = primname.hash();
			break;
		}
		case GEO_PACKEDGEOMETRY:
		{
			int geoid = 0;
			prim.getIntrinsic(prim.findIntrinsic("geometryid"), geoid);
			packedID = geoid;
			break;
		}
		default:
		{
			break;
		}
	}

	return packedID;
}


int GeometryExporter::exportPrimPacked(SOP_Node &sop, const GU_PrimPacked &prim, PluginDescList &pluginList)
{
	// packed primitives can be of different types:
	//   AlembicRef - geometry in alembic file on disk
	//   PackedDisk - geometry file on disk
	//   PackedGeometry - in-mem geometry
	//                  "path" attibute references a SOP ( Pack SOP/VRayProxy SOP )
	//                  otherwise take geoemtry directly from packed GU_Detail
	// VRayProxy SOP loads geometry as PackedGeometry
	// "path" attibute references the VRayProxy SOP: op:<path to VRayProxy SOP>
	// to be able to query the plugin settings
	// TODO: have to use custom packed primitive for that

	int nPlugins = 0;

	switch (prim.getTypeId().get()) {
		case GEO_ALEMBICREF:
		{
			nPlugins = exportAlembicRef(sop, prim, pluginList);
			break;
		}
		case GEO_PACKEDDISK:
		{
			nPlugins = exportPackedDisk(sop, prim, pluginList);
			break;
		}
		case GEO_PACKEDGEOMETRY:
		{
			nPlugins = exportPackedGeometry(sop, prim, pluginList);
			break;
		}
		default:
		{
			break;
		}
	}


	return nPlugins;
}


int GeometryExporter::exportAlembicRef(SOP_Node &sop, const GU_PrimPacked &prim, PluginDescList &pluginList)
{
	UT_String primname;
	prim.getIntrinsic(prim.findIntrinsic("packedprimitivename"), primname);

	UT_String filename;
	prim.getIntrinsic(prim.findIntrinsic("abcfilename"), filename);

	UT_String objname;
	prim.getIntrinsic(prim.findIntrinsic("abcobjectpath"), objname);

	VRay::VUtils::CharStringRefList visibilityList(1);
	visibilityList[0] = objname;

	UT_Matrix4 xform;
	prim.getIntrinsic(prim.findIntrinsic("packedlocaltransform"), xform);
	xform.invert();

	VRay::Transform tm = VRayExporter::Matrix4ToTransform(UT_Matrix4D(xform));

	Attrs::PluginDesc pluginDesc;
	pluginDesc.pluginID = "GeomMeshFile";
	pluginDesc.pluginName = VRayExporter::getPluginName(&sop, primname.toStdString());

	pluginDesc.addAttribute(Attrs::PluginAttr("use_full_names", true));
	pluginDesc.addAttribute(Attrs::PluginAttr("visibility_lists_type", 1));
	pluginDesc.addAttribute(Attrs::PluginAttr("visibility_list_names", visibilityList));
	pluginDesc.addAttribute(Attrs::PluginAttr("file", filename));

	pluginList.push_back(Attrs::PluginDesc("", "Node"));
	Attrs::PluginDesc &nodeDesc = pluginList.back();

	nodeDesc.addAttribute(Attrs::PluginAttr("geometry", m_pluginExporter.exportPlugin(pluginDesc)));
	nodeDesc.addAttribute(Attrs::PluginAttr("transform", tm));

	int nPlugins = 1;
	return nPlugins;
}


int GeometryExporter::exportPackedDisk(SOP_Node &sop, const GU_PrimPacked &prim, PluginDescList &pluginList)
{
	// there is path attribute, but it is NOT holding a ref to a SOP node =>
	// interpret the string as filepath and export as VRayProxy plugin
	// TODO: need to test - probably not working properly

	UT_String primname;
	prim.getIntrinsic(prim.findIntrinsic("packedprimname"), primname);

	UT_String filename;
	prim.getIntrinsic(prim.findIntrinsic("filename"), filename);

	Attrs::PluginDesc pluginDesc(primname.toStdString(), "GeomMeshFile");
	pluginDesc.addAttribute(Attrs::PluginAttr("file", filename));

	pluginList.push_back(Attrs::PluginDesc("", "Node"));
	Attrs::PluginDesc &nodeDesc = pluginList.back();
	nodeDesc.addAttribute(Attrs::PluginAttr("geometry", m_pluginExporter.exportPlugin(pluginDesc)));

	int nPlugins = 1;
	return nPlugins;
}


int GeometryExporter::exportPackedGeometry(SOP_Node &sop, const GU_PrimPacked &prim, PluginDescList &pluginList)
{
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
		SOP_Node *sopref = OPgetDirector()->findSOPNode(path);
		if (NOT(sopref)) {
			// path is not referencing a valid sop =>
			// take geometry directly from primitive packed detail
			GU_DetailHandleAutoReadLock gdl(prim.getPackedDetail());
			if (gdl.isValid()) {
				nPlugins = exportDetail(sop, gdl, pluginList);
			}
		}
		else {
			SOP::VRayProxy *proxy = dynamic_cast< SOP::VRayProxy * >(sopref);
			if (NOT(proxy)) {
				// there is path attribute referencing a valid SOP, but it is NOT VRayProxy SOP =>
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
				Attrs::PluginDesc pluginDesc;
				OP::VRayNode::PluginResult res = proxy->asPluginDesc(pluginDesc, m_pluginExporter);

				pluginList.push_back(Attrs::PluginDesc("", "Node"));
				Attrs::PluginDesc &nodeDesc = pluginList.back();
				nodeDesc.addAttribute(Attrs::PluginAttr("geometry", m_pluginExporter.exportPlugin(pluginDesc)));
				nPlugins = 1;
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

	if (obj_node->getOperator()->getName().equal("VRayNodeVRayClipper")) {
		plugin = exportVRayClipper(*obj_node);
	}
	else {
		OBJ_Geometry *obj_geo = obj_node->castToOBJGeometry();
		if (NOT(obj_geo)) {
			return plugin;
		}

		GeometryExporter geoExporter(*obj_geo, *this);
		if (geoExporter.exportGeometry() > 0) {
			plugin = geoExporter.getPluginAt(0);
		}
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
