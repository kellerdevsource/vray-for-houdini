//
// Copyright (c) 2015, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#include "vfh_export_geom.h"
#include "vfh_export_mesh.h"
#include "sop_vrayproxy.h"
#include "vop/vop_node_base.h"

#include <GU/GU_Detail.h>
#include <GU/GU_PackedDisk.h>
#include <GU/GU_PackedGeometry.h>


using namespace VRayForHoudini;


enum GEO_PrimPackedType
{
	GEO_PACKEDGEOMETRY = 24,
	GEO_PACKEDDISK = 25,
	GEO_ALEMBICREF = 28,
};


typedef std::unordered_set< UT_String , SHOPHasher > SHOPList;


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



GeometryExporter::GeometryExporter(OBJ_Geometry &node, VRayExporter &pluginExporter):
	m_objNode(node),
	m_context(pluginExporter.getContext()),
	m_pluginExporter(pluginExporter),
	m_myDetailID(0)
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


int GeometryExporter::getNumPluginDesc() const
{
	return (m_detailToPluginDesc.count(m_myDetailID))? m_detailToPluginDesc.at(m_myDetailID).size() : 0;
}


Attrs::PluginDesc& GeometryExporter::getPluginDescAt(int idx)
{
	PluginDescList &pluginList = m_detailToPluginDesc.at(m_myDetailID);

	int i = 0;
	for (auto &nodeDesc : pluginList) {
		if (i == idx) {
			return nodeDesc;
		}
		++i;
	}

	throw std::out_of_range("Invalid index");
}


void GeometryExporter::cleanup()
{
	m_myDetailID = 0;
	m_detailToPluginDesc.clear();
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

	m_myDetailID = gdl.handle().hash();
	const GU_Detail &gdp = *gdl.getGdp();

	if (renderSOP->getOperator()->getName().startsWith("VRayNode")) {
		exportVRaySOP(*renderSOP, m_detailToPluginDesc[m_myDetailID]);
	}
	else {
		GA_ROAttributeRef ref_guardhair(gdp.findAttribute(GA_ATTRIB_PRIMITIVE, "guardhair"));
		GA_ROAttributeRef ref_hairid(gdp.findAttribute(GA_ATTRIB_PRIMITIVE, "hairid"));

		if (ref_guardhair.isValid() && ref_hairid .isValid()) {
			exportHair(*renderSOP, gdl, m_detailToPluginDesc[m_myDetailID]);
		}
		else {
			exportDetail(*renderSOP, gdl, m_detailToPluginDesc[m_myDetailID]);
		}
	}

	PluginDescList &pluginList = m_detailToPluginDesc.at(m_myDetailID);

	SHOP_Node *shopNode = m_pluginExporter.getObjMaterial(&m_objNode, m_context.getTime());

	int i = 0;
	for (Attrs::PluginDesc &nodeDesc : pluginList) {
//		TODO: need to fill in node with appropriate names and export them
		nodeDesc.pluginName = VRayExporter::getPluginName(&m_objNode, boost::str(Parm::FmtPrefixManual % "Node" % std::to_string(i++)));

		Attrs::PluginAttr *attr = nullptr;

		bool flipTm = false;
		attr = nodeDesc.get("geometry");
		if (attr) {
			flipTm = (std::string("GeomPlane") ==  attr->paramValue.valPlugin.getType())? true : false;
			VRay::Plugin geomDispl = m_pluginExporter.exportDisplacement(&m_objNode, attr->paramValue.valPlugin);
			if (geomDispl) {
				attr->paramValue.valPlugin = geomDispl;
			}
		}

		VRay::Transform tm = VRayExporter::getObjTransform(&m_objNode, m_context, flipTm);
		attr = nodeDesc.get("transform");
		if (NOT(attr)) {
			nodeDesc.addAttribute(Attrs::PluginAttr("transform", tm));
		}
		else {
			attr->paramValue.valTransform = tm * attr->paramValue.valTransform;
		}

		attr = nodeDesc.get("visible");
		if (NOT(attr)) {
			nodeDesc.addAttribute(Attrs::PluginAttr("visible", m_objNode.getVisible()));
		}
		else {
			attr->paramValue.valInt = m_objNode.getVisible();
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
	}

	return pluginList.size();
}


int GeometryExporter::exportVRaySOP(SOP_Node &sop, PluginDescList &pluginList)
{
	// add new node to our list of nodes
	pluginList.push_back(Attrs::PluginDesc("", "Node"));
	Attrs::PluginDesc &nodeDesc = pluginList.back();
	int nPlugins = 1;

	// geometry
	SOP::NodeBase *vrayNode = UTverify_cast< SOP::NodeBase * >(&sop);

	ExportContext ctx(CT_OBJ, m_pluginExporter, *sop.getParent());

	Attrs::PluginDesc geomDesc;
	OP::VRayNode::PluginResult res = vrayNode->asPluginDesc(geomDesc, m_pluginExporter, &ctx);

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
	nodeDesc.addAttribute(Attrs::PluginAttr("geometry", geom));

	return nPlugins;
}


int GeometryExporter::exportHair(SOP_Node &sop, GU_DetailHandleAutoReadLock &gdl, PluginDescList &pluginList)
{
	// add new node to our list of nodes
	pluginList.push_back(Attrs::PluginDesc("", "Node"));
	Attrs::PluginDesc &nodeDesc = pluginList.back();
	int nPlugins = 1;

	VRay::Plugin geom = m_pluginExporter.exportGeomMayaHair(&sop, gdl.getGdp());
	nodeDesc.addAttribute(Attrs::PluginAttr("geometry", geom));

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

	if (polyMeshExporter.hasPolyGeometry()) {
		// add new node to our list of nodes
		pluginList.push_back(Attrs::PluginDesc("", "Node"));
		Attrs::PluginDesc &nodeDesc = pluginList.back();
		nPlugins = 1;

		// geometry
		Attrs::PluginDesc geomDesc;
		polyMeshExporter.asPluginDesc(geomDesc);
		VRay::Plugin geom = m_pluginExporter.exportPlugin(geomDesc);
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
	pluginList.push_back(Attrs::PluginDesc("", "Node"));
	Attrs::PluginDesc &nodeDesc = pluginList.back();
	int nPlugins = 1;

	// transform
	UT_Matrix4 xform;
	prim.getIntrinsic(prim.findIntrinsic("packedlocaltransform"), xform);
	xform.invert();

	VRay::Transform tm = VRayExporter::Matrix4ToTransform(UT_Matrix4D(xform));
	nodeDesc.addAttribute(Attrs::PluginAttr("transform", tm));

	// geometry
	UT_String primname;
	prim.getIntrinsic(prim.findIntrinsic("packedprimitivename"), primname);

	UT_String filename;
	prim.getIntrinsic(prim.findIntrinsic("abcfilename"), filename);

	UT_String objname;
	prim.getIntrinsic(prim.findIntrinsic("abcobjectpath"), objname);

	VRay::VUtils::CharStringRefList visibilityList(1);
	visibilityList[0] = objname;

	Attrs::PluginDesc pluginDesc;
	pluginDesc.pluginID = "GeomMeshFile";
	pluginDesc.pluginName = VRayExporter::getPluginName(&sop, primname.toStdString());

	pluginDesc.addAttribute(Attrs::PluginAttr("use_full_names", true));
	pluginDesc.addAttribute(Attrs::PluginAttr("visibility_lists_type", 1));
	pluginDesc.addAttribute(Attrs::PluginAttr("visibility_list_names", visibilityList));
	pluginDesc.addAttribute(Attrs::PluginAttr("file", filename));

	VRay::Plugin geom = m_pluginExporter.exportPlugin(pluginDesc);
	nodeDesc.addAttribute(Attrs::PluginAttr("geometry", geom));

	return nPlugins;
}


int GeometryExporter::exportPackedDisk(SOP_Node &sop, const GU_PrimPacked &prim, PluginDescList &pluginList)
{
	// there is path attribute, but it is NOT holding a ref to a SOP node =>
	// interpret the string as filepath and export as VRayProxy plugin
	// TODO: need to test - probably not working properly

	pluginList.push_back(Attrs::PluginDesc("", "Node"));
	Attrs::PluginDesc &nodeDesc = pluginList.back();
	int nPlugins = 1;

	// geometry
	UT_String primname;
	prim.getIntrinsic(prim.findIntrinsic("packedprimname"), primname);

	UT_String filename;
	prim.getIntrinsic(prim.findIntrinsic("filename"), filename);

	Attrs::PluginDesc pluginDesc;
	pluginDesc.pluginID = "GeomMeshFile";
	pluginDesc.pluginName = VRayExporter::getPluginName(&sop, primname.toStdString());

	pluginDesc.addAttribute(Attrs::PluginAttr("file", filename));

	VRay::Plugin geom = m_pluginExporter.exportPlugin(pluginDesc);
	nodeDesc.addAttribute(Attrs::PluginAttr("geometry", geom));

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
				pluginList.push_back(Attrs::PluginDesc("", "Node"));
				Attrs::PluginDesc &nodeDesc = pluginList.back();
				nPlugins = 1;

				// geometry
				Attrs::PluginDesc pluginDesc;
				OP::VRayNode::PluginResult res = proxy->asPluginDesc(pluginDesc, m_pluginExporter);

				VRay::Plugin geom = m_pluginExporter.exportPlugin(pluginDesc);
				nodeDesc.addAttribute(Attrs::PluginAttr("geometry", geom));
			}
		}
	}

	return nPlugins;
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
