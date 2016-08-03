//
// Copyright (c) 2015-2016, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#include "vfh_export_geom.h"
#include "vfh_export_mesh.h"
#include "gu_vrayproxyref.h"
#include "gu_volumegridref.h"
#include "rop/vfh_rop.h"
#include "sop/sop_node_base.h"
#include "vop/vop_node_base.h"
#include "vop/material/vop_MaterialOutput.h"
#include "vop/material/vop_PhoenixSim.h"

#include <GEO/GEO_Primitive.h>
#include <GU/GU_PrimVolume.h>
#include <GU/GU_Detail.h>
#include <OP/OP_Bundle.h>
#include <GA/GA_Types.h>

using namespace VRayForHoudini;


const char *const VFH_ATTR_MATERIAL_ID = "switchmtl";

void VolumeExporter::exportPrimitives(const GU_Detail &detail, PluginDescList &plugins)
{
	auto & primList = detail.getPrimitiveList();
	const int primCount = primList.offsetSize();

	// check all primities if we can make PrimExporter for it and export it
	for (int c = 0; c < primCount; ++c) {
		auto prim = primList.get(c);
		if (prim && prim->getTypeId() == VRayVolumeGridRef::typeId()) {
			exportCache(*prim);
		}
	}
}

void VolumeExporter::exportCache(const GA_Primitive &prim)
{
	SOP_Node *sop = m_object.getRenderSopPtr();
	if (!sop) {
		return;
	}

	UT_String intrinPath;
	prim.getIntrinsic(prim.findIntrinsic("packedprimitivename"), intrinPath);
	// TODO: What if we have 2 caches in the same detail
	const auto name = VRayExporter::getPluginName(sop, "Cache", intrinPath.buffer() ? intrinPath.buffer() : "");

	Attrs::PluginDesc nodeDesc(name, "PhxShaderCache");

	auto packedPrim = UTverify_cast<const GU_PrimPacked *>(&prim);
	auto vrayproxyref = UTverify_cast< const VRayVolumeGridRef * >(packedPrim->implementation());
	m_exporter.setAttrsFromUTOptions(nodeDesc, vrayproxyref->getOptions());

	UT_Matrix4 xform;
	prim.getIntrinsic(prim.findIntrinsic("packedfulltransform"), xform);

	auto primTm = VRayExporter::Matrix4ToTransform(UT_Matrix4D(xform));
	auto objTm = VRayExporter::getObjTransform(&m_object, m_context);

	exportSim(prim, primTm * objTm, m_exporter.exportPlugin(nodeDesc));
}

void VolumeExporter::exportSim(const GA_Primitive &prim, const VRay::Transform &tm, VRay::Plugin &cache)
{
	GA_ROHandleS mtlpath(prim.getDetail().findAttribute(GA_ATTRIB_PRIMITIVE, GEO_STD_ATTRIB_MATERIAL));
	// TODO: add overrides
	// GA_ROHandleS mtlo(prim.getDetail().findAttribute(GA_ATTRIB_PRIMITIVE, "material_override"));

	auto path = mtlpath.isValid() ? mtlpath.get(prim.getMapOffset()) : nullptr;
	SHOP_Node *shop = mtlpath.isValid() ? OPgetDirector()->findSHOPNode(path) : nullptr;

	if (!shop) {
		shop = m_exporter.getObjMaterial(&m_object, m_context.getTime());
	}

	UT_ValArray<OP_Node *> mtlOutList;
	if (shop->getOpsByName("vray_material_output", mtlOutList)) {
		// there is at least 1 "vray_material_output" node so take the first one
		VOP::MaterialOutput *mtlOut = static_cast< VOP::MaterialOutput * >( mtlOutList(0) );
		if (mtlOut->error() < UT_ERROR_ABORT ) {
			const int simIdx = mtlOut->getInputFromName("PhxShaderSim");
			if (auto simNode = mtlOut->getInput(simIdx)) {
				if(VOP_Node * simVop = simNode->castToVOPNode()) {
					UT_ASSERT_MSG(mtlOut->getInputType(simIdx) == VOP_ATMOSPHERE_SHADER, "PhxShaderSim's socket is not of type VOP_ATMOSPHERE_SHADER");
					Log::getLog().msg("Exporting PhxShaderSim for node \"%s\", input %d!", mtlOut->getName().buffer(), simIdx);

					VOP::NodeBase *vrayNode = static_cast<VOP::NodeBase*>(simVop);
					Attrs::PluginDesc pluginDesc;

					//TODO: is this unique enough
					pluginDesc.pluginName = VRayExporter::getPluginName(simVop, "Sim", cache.getName());
					pluginDesc.pluginID   = vrayNode->getVRayPluginID();

					OP::VRayNode::PluginResult res = vrayNode->asPluginDesc(pluginDesc, m_exporter);
					m_exporter.setAttrsFromOpNodeConnectedInputs(pluginDesc, simVop);

					// handle VOP overrides if any
					m_exporter.setAttrsFromSHOPOverrides(pluginDesc, *simVop);

					pluginDesc.add(Attrs::PluginAttr("node_transform", tm));
					pluginDesc.add(Attrs::PluginAttr("cache", cache));


					const auto rendModeAttr = pluginDesc.get("_vray_render_mode");
					UT_ASSERT_MSG(rendModeAttr, "Trying to export PhxShaderSim without setting it's _vray_render_mode.");
					VRay::Plugin overwriteSim = m_exporter.exportPlugin(pluginDesc);
					if (rendModeAttr && overwriteSim) {
						typedef VOP::PhxShaderSim::RenderMode RMode;

						const auto rendMode = static_cast<RMode>(rendModeAttr->paramValue.valInt);
						if (rendMode == RMode::Volumetric) {
							// merge all volumetrics
							m_exporter.phxAddSimumation(overwriteSim);
						} else {
							const bool isMesh = rendMode == RMode::Mesh;

							const char *wrapperType = isMesh ? "PhxShaderSimMesh" : "PhxShaderSimGeom";
							const char *wrapperPrefix = isMesh ? "Mesh" : "Geom";
							Attrs::PluginDesc phxWrapper(VRayExporter::getPluginName(simVop, wrapperPrefix, cache.getName()), wrapperType);
							phxWrapper.add(Attrs::PluginAttr("phoenix_sim", overwriteSim));
							VRay::Plugin phxWrapperPlugin = m_exporter.exportPlugin(phxWrapper);

							if (!isMesh) {
								// make static mesh that wraps the geom plugin
								Attrs::PluginDesc meshWrapper(VRayExporter::getPluginName(simVop, "Mesh", cache.getName()), "GeomStaticMesh");
								meshWrapper.add(Attrs::PluginAttr("static_mesh", phxWrapperPlugin));

								const auto dynGeomAttr = pluginDesc.get("_vray_dynamic_geometry");
								UT_ASSERT_MSG(dynGeomAttr, "Exporting PhxShaderSim inside PhxShaderSimGeom with missing _vray_dynamic_geometry");
								const bool dynamic_geometry = dynGeomAttr ? dynGeomAttr->paramValue.valInt : false;

								meshWrapper.add(Attrs::PluginAttr("dynamic_geometry", dynamic_geometry));
								phxWrapperPlugin = m_exporter.exportPlugin(meshWrapper);
							}

							Attrs::PluginDesc node(VRayExporter::getPluginName(simVop, "Node", cache.getName()), "Node");
							node.add(Attrs::PluginAttr("geometry", phxWrapperPlugin));
							node.add(Attrs::PluginAttr("visible", true));
							node.add(Attrs::PluginAttr("transform", VRay::Transform(1)));
							node.add(Attrs::PluginAttr("material", m_exporter.exportDefaultMaterial()));
							m_exporter.exportPlugin(node);
						}
					}
				} else {
					UT_ASSERT_MSG(false, "PhxShaderSim cannot be casted to VOP node!");
				}
			}
		}
	}
}

GeometryExporter::GeometryExporter(OBJ_Geometry &node, VRayExporter &pluginExporter):
	m_objNode(node),
	m_context(pluginExporter.getContext()),
	m_pluginExporter(pluginExporter),
	m_myDetailID(0),
	m_exportGeometry(true)
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


int GeometryExporter::isNodeVisible() const
{
	VRayRendererNode &rop = m_pluginExporter.getRop();
	OP_Bundle *bundle = rop.getForcedGeometryBundle();
	if (!bundle) {
		return m_objNode.getVisible();
	}

	return bundle->contains(&m_objNode, false) || m_objNode.getVisible();
}


int GeometryExporter::isNodeMatte() const
{
	VRayRendererNode &rop = m_pluginExporter.getRop();
	OP_Bundle *bundle = rop.getMatteGeometryBundle();
	if (!bundle) {
		return false;
	}

	return bundle->contains(&m_objNode, false);
}


int GeometryExporter::isNodePhantom() const
{
	VRayRendererNode &rop = m_pluginExporter.getRop();
	OP_Bundle *bundle = rop.getPhantomGeometryBundle();
	if (!bundle) {
		return false;
	}

	return bundle->contains(&m_objNode, false);
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


int GeometryExporter::exportNodes()
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

	const bool isVolume = renderSOP->getOperator()->getName().startsWith("VRayNodePhxShaderCache");

	if (!isVolume && renderSOP->getOperator()->getName().startsWith("VRayNode")) {
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

	VRay::Transform tm = VRayExporter::getObjTransform(&m_objNode, m_context);

	UT_String userAttrs;
	getSHOPOverridesAsUserAttributes(userAttrs);

	VRay::Plugin mtl;
	if (m_exportGeometry) {
		mtl = exportMaterial();
	}

	int i = 0;
	PluginDescList &pluginList = m_detailToPluginDesc.at(m_myDetailID);
	for (Attrs::PluginDesc &nodeDesc : pluginList) {
//		TODO: need to fill in node with appropriate names
		if (nodeDesc.pluginName != "") {
			continue;
		}
		nodeDesc.pluginName = VRayExporter::getPluginName(&m_objNode, boost::str(Parm::FmtPrefixManual % "Node" % std::to_string(i++)));

		Attrs::PluginAttr *attr = nullptr;

		attr = nodeDesc.get("transform");
		if (NOT(attr)) {
			nodeDesc.addAttribute(Attrs::PluginAttr("transform", tm));
		}
		else {
			attr->paramValue.valTransform = tm * attr->paramValue.valTransform;
		}

		nodeDesc.addAttribute(Attrs::PluginAttr("visible", isNodeVisible()));

		attr = nodeDesc.get("geometry");
		if (attr) {
			VRay::Plugin geomDispl = m_pluginExporter.exportDisplacement(&m_objNode, attr->paramValue.valPlugin);
			if (geomDispl) {
				attr->paramValue.valPlugin = geomDispl;
			}
		}

		attr = nodeDesc.get("material");
		if (NOT(attr)) {
			if (mtl) {
				nodeDesc.addAttribute(Attrs::PluginAttr("material", mtl));
			}

			attr = nodeDesc.get(VFH_ATTR_MATERIAL_ID);
			if (NOT(attr)) {
				// pass material overrides with "user_attributes"
				if (userAttrs.isstring()) {
					nodeDesc.addAttribute(Attrs::PluginAttr("user_attributes", userAttrs));
				}
			}
		}

		// TODO: adjust other Node attrs
	}

	return pluginList.size();
}


VRay::Plugin GeometryExporter::exportMaterial()
{
	VRay::Plugin mtl;

	VRay::ValueList mtls_list;
	VRay::IntList   ids_list;
	mtls_list.reserve(m_shopList.size() + 1);
	ids_list.reserve(m_shopList.size() + 1);

	SHOP_Node *shopNode = m_pluginExporter.getObjMaterial(&m_objNode, m_context.getTime());
	if (shopNode) {
		mtls_list.emplace_back(m_pluginExporter.exportMaterial(*shopNode));
		ids_list.emplace_back(0);
	}
	else {
		mtls_list.emplace_back(m_pluginExporter.exportDefaultMaterial());
		ids_list.emplace_back(0);
	}

	SHOPHasher hasher;
	for (const UT_String &shoppath : m_shopList) {
		SHOP_Node *shopNode = OPgetDirector()->findSHOPNode(shoppath);
		UT_ASSERT( shopNode );
		mtls_list.emplace_back(m_pluginExporter.exportMaterial(*shopNode));
		ids_list.emplace_back(hasher(shopNode));
	}

	Attrs::PluginDesc mtlDesc;
	mtlDesc.pluginID = "MtlMulti";
	mtlDesc.pluginName = VRayExporter::getPluginName(&m_objNode, "Mtl");

	mtlDesc.addAttribute(Attrs::PluginAttr("mtls_list", mtls_list));
	mtlDesc.addAttribute(Attrs::PluginAttr("ids_list",  ids_list));

	Attrs::PluginDesc myMtlIDDesc;
	myMtlIDDesc.pluginID = "TexUserScalar";
	myMtlIDDesc.pluginName = VRayExporter::getPluginName(&m_objNode, "MtlID");

	myMtlIDDesc.addAttribute(Attrs::PluginAttr("default_value",  0));
	myMtlIDDesc.addAttribute(Attrs::PluginAttr("user_attribute",  VFH_ATTR_MATERIAL_ID));

	VRay::Plugin myMtlID = m_pluginExporter.exportPlugin(myMtlIDDesc);

	mtlDesc.addAttribute(Attrs::PluginAttr("mtlid_gen_float", myMtlID, "scalar"));

	mtl = m_pluginExporter.exportPlugin(mtlDesc);


	if (isNodeMatte()) {
		Attrs::PluginDesc mtlDesc;
		mtlDesc.pluginID = "MtlWrapper";
		mtlDesc.pluginName = VRayExporter::getPluginName(&m_objNode, "MtlWrapper");

		mtlDesc.addAttribute(Attrs::PluginAttr("base_material", mtl));
		mtlDesc.addAttribute(Attrs::PluginAttr("matte_surface", 1));
		mtlDesc.addAttribute(Attrs::PluginAttr("alpha_contribution", -1));
		mtlDesc.addAttribute(Attrs::PluginAttr("affect_alpha", 1));
		mtlDesc.addAttribute(Attrs::PluginAttr("reflection_amount", 0));
		mtlDesc.addAttribute(Attrs::PluginAttr("refraction_amount", 0));

		mtl = m_pluginExporter.exportPlugin(mtlDesc);
	}

	if (isNodePhantom()) {
		Attrs::PluginDesc mtlDesc;
		mtlDesc.pluginID = "MtlRenderStats";
		mtlDesc.pluginName = VRayExporter::getPluginName(&m_objNode, "MtlRenderStats");

		mtlDesc.addAttribute(Attrs::PluginAttr("base_mtl", mtl));
		mtlDesc.addAttribute(Attrs::PluginAttr("camera_visibility", 0));

		mtl = m_pluginExporter.exportPlugin(mtlDesc);
	}


	return mtl;
}


int GeometryExporter::getSHOPOverridesAsUserAttributes(UT_String &userAttrs) const
{
	int nOverrides = 0;

	SHOP_Node *shopNode = m_pluginExporter.getObjMaterial(&m_objNode, m_context.getTime());
	if (NOT(shopNode)) {
		return nOverrides;
	}

	userAttrs += VFH_ATTR_MATERIAL_ID;
	userAttrs += "=0;";

	const PRM_ParmList *shopParmList = shopNode->getParmList();
	const PRM_ParmList *objParmList = m_objNode.getParmList();

	for (int i = 0; i < shopParmList->getEntries(); ++i) {
		const PRM_Parm *shopPrm = shopParmList->getParmPtr(i);
		const PRM_Parm *objPrm = objParmList->getParmPtr(shopPrm->getToken());

		if (   objPrm
			&& shopPrm->getType() == objPrm->getType()
			&& objPrm->getType().isFloatType()
			&& NOT(objPrm->getBypassFlag()) )
		{
			// we have parameter with matching name on the OBJ_Node
			// => treat as override
			UT_StringArray prmValTokens;
			for (int i = 0; i < objPrm->getVectorSize(); ++i) {
				fpreal chval = m_objNode.evalFloat(objPrm, i, m_context.getTime());
				prmValTokens.append( std::to_string(chval) );
			}

			UT_String prmValToken;
			prmValTokens.join(",", prmValToken);

			userAttrs += shopPrm->getToken();
			userAttrs += "=";
			userAttrs += prmValToken;
			userAttrs += ";";

			++nOverrides;
		}
	}

	return nOverrides;
}


int GeometryExporter::exportVRaySOP(SOP_Node &sop, PluginDescList &pluginList)
{
	// add new node to our list of nodes
	pluginList.push_back(Attrs::PluginDesc("", "Node"));
	Attrs::PluginDesc &nodeDesc = pluginList.back();
	int nPlugins = 1;

	if (NOT(m_exportGeometry)) {
		return nPlugins;
	}

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

	if (NOT(m_exportGeometry)) {
		return nPlugins;
	}

	VRay::Plugin geom = m_pluginExporter.exportGeomMayaHair(&sop, gdl.getGdp());
	nodeDesc.addAttribute(Attrs::PluginAttr("geometry", geom));

	return nPlugins;
}


// traverse through all primitives
// polygonal primitives should be exported as single GeomStaticMesh
// for packed primitives - need to hash what has alreay been exported
// hash based on primitive id / detail id
int GeometryExporter::exportDetail(SOP_Node &sop, GU_DetailHandleAutoReadLock &gdl, PluginDescList &pluginList)
{
	int nPlugins = 0;

	const GU_Detail &gdp = *gdl.getGdp();

	PrimitiveExporterPtr volExp(new VolumeExporter(m_objNode, m_context, m_pluginExporter));
	volExp->exportPrimitives(gdp, pluginList);

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

	MeshExporter polyMeshExporter(gdp, m_pluginExporter);
	polyMeshExporter.setSOPContext(&sop)
					.setSubdivApplied(hasSubdivApplied());

	if (polyMeshExporter.hasPolyGeometry()) {
		// add new node to our list of nodes
		pluginList.push_back(Attrs::PluginDesc("", "Node"));
		Attrs::PluginDesc &nodeDesc = pluginList.back();
		nPlugins = 1;

		SHOPList shopList;
		int nSHOPs = polyMeshExporter.getSHOPList(shopList);
		if (nSHOPs > 0) {
			nodeDesc.addAttribute(Attrs::PluginAttr(VFH_ATTR_MATERIAL_ID, -1));
		}

		if (NOT(m_exportGeometry)) {
			return nPlugins;
		}

		// geometry
		Attrs::PluginDesc geomDesc;
		polyMeshExporter.asPluginDesc(geomDesc);
		VRay::Plugin geom = m_pluginExporter.exportPlugin(geomDesc);
		nodeDesc.addAttribute(Attrs::PluginAttr("geometry", geom));

		// material
		if (nSHOPs > 0) {
			VRay::ValueList mtls_list;
			VRay::IntList   ids_list;
			mtls_list.reserve(nSHOPs);
			ids_list.reserve(nSHOPs);

			SHOPHasher hasher;
			for (const UT_String &shoppath : shopList) {
				SHOP_Node *shopNode = OPgetDirector()->findSHOPNode(shoppath);
				UT_ASSERT( shopNode );
				mtls_list.emplace_back(m_pluginExporter.exportMaterial(*shopNode));
				ids_list.emplace_back(hasher(shopNode));
			}

			Attrs::PluginDesc mtlDesc;
			mtlDesc.pluginID = "MtlMulti";
			mtlDesc.pluginName = VRayExporter::getPluginName(&sop, boost::str(Parm::FmtPrefixManual % "Mtl" % std::to_string(gdp.getUniqueId())));

			mtlDesc.addAttribute(Attrs::PluginAttr("mtls_list", mtls_list));
			mtlDesc.addAttribute(Attrs::PluginAttr("ids_list",  ids_list));

			nodeDesc.addAttribute(Attrs::PluginAttr("material", m_pluginExporter.exportPlugin(mtlDesc)));
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

	SHOPHasher hasher;
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


		attr = nodeDesc.get(VFH_ATTR_MATERIAL_ID);
		if (   NOT(attr)
			&& mtlpath.isValid() )
		{
			const char *shoppath = mtlpath.get(prim.getMapOffset());
			SHOP_Node *shopNode = OPgetDirector()->findSHOPNode(shoppath);
			if (shopNode) {
				// add material for export
				m_shopList.insert(shoppath);

				// pass material id with "user_attributes"
				int shopID = hasher(shopNode);
				nodeDesc.addAttribute(Attrs::PluginAttr(VFH_ATTR_MATERIAL_ID, shopID));

				UT_String userAtrs;

				userAtrs += VFH_ATTR_MATERIAL_ID;
				userAtrs += "=";
				userAtrs += std::to_string(shopID);
				userAtrs += ";";

				// pass material overrides with "user_attributes"
				UT_Options mtlOverridesDict;
				if (   mtlo.isValid()
					&& mtlOverridesDict.setFromPyDictionary(mtlo.get(prim.getMapOffset())) )
				{
					while (mtlOverridesDict.getNumOptions() > 0) {
						UT_String key = mtlOverridesDict.begin().name();

						int chIdx = -1;
						PRM_Parm *prm = shopNode->getParmList()->getParmPtrFromChannel(key, &chIdx);
						if (   NOT(prm)
							|| NOT(prm->getType().isFloatType()) )
						{
							mtlOverridesDict.removeOption(key);
							continue;
						}

						UT_StringArray prmValTokens;
						for (int i = 0; i < prm->getVectorSize(); ++i) {
							prm->getChannelToken(key, i);
							fpreal chval = (mtlOverridesDict.hasOption(key))? mtlOverridesDict.getOptionF(key) : shopNode->evalFloat(prm, i, m_context.getTime());
							prmValTokens.append( std::to_string(chval) );
							mtlOverridesDict.removeOption(key);
						}

						UT_String prmValToken;
						prmValTokens.join(",", prmValToken);

						userAtrs += prm->getToken();
						userAtrs += "=";
						userAtrs += prmValToken;
						userAtrs += ";";
					}
				}

				if (userAtrs.isstring()) {
					nodeDesc.addAttribute(Attrs::PluginAttr("user_attributes", userAtrs));
				}
			}
		}
	}

	return primPluginList.size();
}


uint GeometryExporter::getPrimPackedID(const GU_PrimPacked &prim)
{
	uint packedID = 0;

	if (prim.getTypeId() == GU_PrimPacked::lookupTypeId("AlembicRef")) {
		UT_String primname;
		prim.getIntrinsic(prim.findIntrinsic("packedprimitivename"), primname);
		packedID = primname.hash();
	}
	else if (prim.getTypeId() == GU_PrimPacked::lookupTypeId("PackedDisk")) {
		UT_String primname;
		prim.getIntrinsic(prim.findIntrinsic("packedprimitivename"), primname);
		packedID = primname.hash();
	}
	else if (prim.getTypeId() == GU_PrimPacked::lookupTypeId("VRayProxyRef")) {
		auto vrayproxyref = UTverify_cast< const VRayProxyRef * >(prim.implementation());
		packedID = vrayproxyref->getOptions().hash();
	}
	else if (prim.getTypeId() == GU_PrimPacked::lookupTypeId("PackedGeometry")) {
		int geoid = -1;
		prim.getIntrinsic(prim.findIntrinsic("geometryid"), geoid);
		packedID = geoid;
	}

	return packedID;
}


int GeometryExporter::exportPrimPacked(SOP_Node &sop, const GU_PrimPacked &prim, PluginDescList &pluginList)
{
	// packed primitives can be of different types
	// currently supporting:
	//   AlembicRef - geometry in alembic file on disk
	//   PackedDisk - geometry file on disk
	//   PackedGeometry - in-mem geometry

	int nPlugins = 0;

	if (prim.getTypeId() == GU_PrimPacked::lookupTypeId("AlembicRef")) {
		nPlugins = exportAlembicRef(sop, prim, pluginList);
	}
	else if (prim.getTypeId() == GU_PrimPacked::lookupTypeId("PackedDisk")) {
		nPlugins = exportPackedDisk(sop, prim, pluginList);
	}
	else if (prim.getTypeId() == GU_PrimPacked::lookupTypeId("VRayProxyRef")) {
		nPlugins = exportVRayProxyRef(sop, prim, pluginList);
	}
	else if (prim.getTypeId() == GU_PrimPacked::lookupTypeId("PackedGeometry")) {
		nPlugins = exportPackedGeometry(sop, prim, pluginList);
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

	if (NOT(m_exportGeometry)) {
		return nPlugins;
	}

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


int GeometryExporter::exportVRayProxyRef(SOP_Node &sop, const GU_PrimPacked &prim, PluginDescList &pluginList)
{
	pluginList.push_back(Attrs::PluginDesc("", "Node"));
	Attrs::PluginDesc &nodeDesc = pluginList.back();

	// transform
	UT_Matrix4 xform;
	prim.getIntrinsic(prim.findIntrinsic("packedlocaltransform"), xform);
	xform.invert();

	VRay::Transform tm = VRayExporter::Matrix4ToTransform(UT_Matrix4D(xform));
	nodeDesc.addAttribute(Attrs::PluginAttr("transform", tm));

	if (NOT(m_exportGeometry)) {
		return 1;
	}

	// geometry
	UT_String primname;
	prim.getIntrinsic(prim.findIntrinsic("packedprimitivename"), primname);

	Attrs::PluginDesc pluginDesc;
	pluginDesc.pluginID = "GeomMeshFile";
	pluginDesc.pluginName = VRayExporter::getPluginName(&sop, primname.toStdString());

	auto vrayproxyref = UTverify_cast< const VRayProxyRef * >(prim.implementation());
	m_pluginExporter.setAttrsFromUTOptions(pluginDesc, vrayproxyref->getOptions());

	VRay::Plugin geom = m_pluginExporter.exportPlugin(pluginDesc);
	nodeDesc.addAttribute(Attrs::PluginAttr("geometry", geom));

	return 1;
}


int GeometryExporter::exportPackedDisk(SOP_Node &sop, const GU_PrimPacked &prim, PluginDescList &pluginList)
{
	// there is path attribute, but it is NOT holding a ref to a SOP node =>
	// interpret the string as filepath and export as VRayProxy plugin
	// TODO: need to test - probably not working properly

	pluginList.push_back(Attrs::PluginDesc("", "Node"));
	Attrs::PluginDesc &nodeDesc = pluginList.back();
	int nPlugins = 1;

	if (NOT(m_exportGeometry)) {
		return nPlugins;
	}

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
	//PackedGeometry - in-mem geometry
	//               "path" attibute references a SOP ( Pack )
	//               otherwise take geometry directly from packed GU_Detail

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
			// there is path attribute referencing a valid SOP =>
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
	}

	return nPlugins;
}
