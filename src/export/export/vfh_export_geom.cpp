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
#include "vfh_export_hair.h"
#include "gu_vrayproxyref.h"
#include "gu_volumegridref.h"
#include "rop/vfh_rop.h"
#include "sop/sop_node_base.h"
#include "vop/vop_node_base.h"

#include <GEO/GEO_Primitive.h>
#include <GU/GU_Detail.h>
#include <OP/OP_Bundle.h>
#include <GA/GA_Types.h>

using namespace VRayForHoudini;

const char *const VFH_ATTR_MATERIAL_ID = "switchmtl";

typedef std::vector<bool> DynamicBitset;
namespace {
	/// Check all points inside the detail and clear the bitset's indecies of those points which belong to some primitive
	/// Additionally return the number of "free" points (thats the size of the map minus the non free point count)
	GA_Size fillFreePointMap(const GU_Detail &detail, DynamicBitset &map) {
		const GA_Size verticesCount = detail.getNumVertices();
		for (GA_Size c = 0; c < verticesCount; c++) {
			const GA_Offset vertOffset = detail.vertexOffset(c);
			const GA_Offset pointOffset = detail.vertexPoint(vertOffset);
			const GA_Index pointIndex = detail.pointIndex(pointOffset);
			// we cant just count these becase some vertices share points
			if (pointIndex < map.size()) {
				map[pointIndex] = false;
			}
		}
		GA_Size freePointCount = 0;
		for (GA_Size c = 0; c < map.size(); c++) {
			freePointCount += map[c];
		}
		return freePointCount;
	}
}


GeometryExporter::GeometryExporter(OBJ_Geometry &node, VRayExporter &pluginExporter):
	m_objNode(node),
	m_context(pluginExporter.getContext()),
	m_pluginExporter(pluginExporter),
	m_myDetailID(0),
	m_exportGeometry(true) {}


bool GeometryExporter::hasSubdivApplied() const {
	// here we check if subdivision has been assigned to this node
	// at render time. V-Ray subdivision is implemented in 2 ways:
	// 1. as a custom VOP available in V-Ray material context
	// 2. as spare parameters added to the object node

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
			if (shop->getOpsByName("vray_material_output", outputNodes)) {
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


int GeometryExporter::isNodeVisible() const {
	VRayRendererNode &rop = m_pluginExporter.getRop();
	OP_Bundle *bundle = rop.getForcedGeometryBundle();
	if (!bundle) {
		return m_objNode.getVisible();
	}

	return bundle->contains(&m_objNode, false) || m_objNode.getVisible();
}


int GeometryExporter::isNodeMatte() const {
	VRayRendererNode &rop = m_pluginExporter.getRop();
	OP_Bundle *bundle = rop.getMatteGeometryBundle();
	if (!bundle) {
		return false;
	}

	return bundle->contains(&m_objNode, false);
}


int GeometryExporter::isNodePhantom() const {
	VRayRendererNode &rop = m_pluginExporter.getRop();
	OP_Bundle *bundle = rop.getPhantomGeometryBundle();
	if (!bundle) {
		return false;
	}

	return bundle->contains(&m_objNode, false);
}



int GeometryExporter::getNumPluginDesc() const {
	return (m_detailToPluginDesc.count(m_myDetailID)) ? m_detailToPluginDesc.at(m_myDetailID).size() : 0;
}


Attrs::PluginDesc& GeometryExporter::getPluginDescAt(int idx) {
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


void GeometryExporter::reset() {
	m_myDetailID = 0;
	m_detailToPluginDesc.clear();
	m_shopList.clear();
}


int GeometryExporter::exportNodes() {
	SOP_Node *renderSOP = m_objNode.getRenderSopPtr();
	if (NOT(renderSOP)) {
		// we don't have a valid render SOP
		// nothing else to do
		return 0;
	}

	GU_DetailHandleAutoReadLock gdl(renderSOP->getCookedGeoHandle(m_context));
	if (NOT(gdl.isValid())) {
		// we don't have a valid render geometry gdp
		// nothing else to do
		return 0;
	}

	// geometry is exported in 2 steps:
	// 1. we traverse the render gdp, export the geometry that
	//    we can handle and wrap it in a Node  plugin description
	//    which is accumulated into m_detailToPluginDesc map
	// 2. we get the list of Nodes generated for the render gdp
	//    and adjust properties like transform, material,
	//    user_attributes, etc.

	m_myDetailID = gdl.handle().hash();
	const GU_Detail &gdp = *gdl.getGdp();

	if (renderSOP->getOperator()->getName().startsWith("VRayNode")
		&& !renderSOP->getOperator()->getName().startsWith("VRayNodePhxShaderCache")
		&& !renderSOP->getOperator()->getName().startsWith("VRayNodeVRayProxy")) {
		// V-Ray plane SOP and V-Ray scene SOP are still implemented such as
		// they expect to be final SOP in the SOP network
		// TODO: need to fix this in future
		exportVRaySOP(*renderSOP, m_detailToPluginDesc[m_myDetailID]);
	} else {
		// handle geometry export from the render gdp
		exportDetail(*renderSOP, gdl, m_detailToPluginDesc[m_myDetailID]);
	}

	// get the OBJ transform
	VRay::Transform tm = VRayExporter::getObjTransform(&m_objNode, m_context);

	// format material overrides specified on the object as user_attributes
	UT_String userAttrs;
	getSHOPOverridesAsUserAttributes(userAttrs);

	// handle export of material for the object node
	VRay::Plugin mtl;
	if (m_exportGeometry) {
		mtl = exportMaterial();
	}

	// adjust Node parameters
	int i = 0;
	PluginDescList &pluginList = m_detailToPluginDesc.at(m_myDetailID);
	for (Attrs::PluginDesc &nodeDesc : pluginList) {
		// TODO: need to figure out how to generate names for Node plugins
		//       comming from different primitives
		if (nodeDesc.pluginName != "") {
			continue;
		}
		nodeDesc.pluginName = VRayExporter::getPluginName(&m_objNode, boost::str(Parm::FmtPrefixManual % "Node" % std::to_string(i++)));

		Attrs::PluginAttr *attr = nullptr;

		attr = nodeDesc.get("transform");
		if (NOT(attr)) {
			nodeDesc.addAttribute(Attrs::PluginAttr("transform", tm));
		} else {
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
			// add the material only if it hasn't been specified yet
			if (mtl) {
				nodeDesc.addAttribute(Attrs::PluginAttr("material", mtl));
			}

			attr = nodeDesc.get(VFH_ATTR_MATERIAL_ID);
			if (NOT(attr)) {
				// pass material overrides with "user_attributes"
				if (userAttrs.isstring()) {
					nodeDesc.addAttribute(Attrs::PluginAttr("user_attributes", userAttrs.toStdString()));
				}
			}
		}

		static const char parmObjectID[] = "objectID";
		PRM_Parm *prmObjectID = m_objNode.getParmList()->getParmPtr(parmObjectID);
		if (prmObjectID) {
			const int objectID = m_objNode.evalInt(parmObjectID, 0, m_context.getTime());
			nodeDesc.addAttribute(Attrs::PluginAttr("objectID", objectID));
		}

		// TODO: adjust other Node attrs
	}

	return pluginList.size();
}


VRay::Plugin GeometryExporter::exportMaterial()
{
	VRay::ValueList mtls_list;
	VRay::IntList   ids_list;
	mtls_list.reserve(m_shopList.size() + 1);
	ids_list.reserve(m_shopList.size() + 1);

	// object material is always exported with id 0
	OP_Node *matNode = m_pluginExporter.getObjMaterial(&m_objNode, m_context.getTime());
	if (matNode) {
		mtls_list.emplace_back(m_pluginExporter.exportMaterial(matNode));
		ids_list.emplace_back(0);
	}
	else {
		mtls_list.emplace_back(m_pluginExporter.exportDefaultMaterial());
		ids_list.emplace_back(0);
	}

	// generate id for each SHOP and add it to the material list
	SHOPHasher hasher;
	for (const UT_String &matPath : m_shopList) {
		OP_Node *opNode = getOpNodeFromPath(matPath);
		UT_ASSERT(opNode);
		mtls_list.emplace_back(m_pluginExporter.exportMaterial(opNode));
		ids_list.emplace_back(hasher(opNode));
	}

	// export single MtlMulti material
	Attrs::PluginDesc mtlDesc;
	mtlDesc.pluginID = "MtlMulti";
	mtlDesc.pluginName = VRayExporter::getPluginName(&m_objNode, "Mtl");

	mtlDesc.addAttribute(Attrs::PluginAttr("mtls_list", mtls_list));
	mtlDesc.addAttribute(Attrs::PluginAttr("ids_list", ids_list));

	Attrs::PluginDesc myMtlIDDesc;
	myMtlIDDesc.pluginID = "TexUserScalar";
	myMtlIDDesc.pluginName = VRayExporter::getPluginName(&m_objNode, "MtlID");

	myMtlIDDesc.addAttribute(Attrs::PluginAttr("default_value", 0));
	myMtlIDDesc.addAttribute(Attrs::PluginAttr("user_attribute", VFH_ATTR_MATERIAL_ID));

	VRay::Plugin myMtlID = m_pluginExporter.exportPlugin(myMtlIDDesc);

	mtlDesc.addAttribute(Attrs::PluginAttr("mtlid_gen_float", myMtlID, "scalar"));

	VRay::Plugin mtl = m_pluginExporter.exportPlugin(mtlDesc);

	// handle if object is forced as matte
	if (isNodeMatte()) {
		Attrs::PluginDesc mtlWrapperDesc;
		mtlWrapperDesc.pluginID = "MtlWrapper";
		mtlWrapperDesc.pluginName = VRayExporter::getPluginName(&m_objNode, "MtlWrapper");

		mtlWrapperDesc.addAttribute(Attrs::PluginAttr("base_material", mtl));
		mtlWrapperDesc.addAttribute(Attrs::PluginAttr("matte_surface", 1));
		mtlWrapperDesc.addAttribute(Attrs::PluginAttr("alpha_contribution", -1));
		mtlWrapperDesc.addAttribute(Attrs::PluginAttr("affect_alpha", 1));
		mtlWrapperDesc.addAttribute(Attrs::PluginAttr("reflection_amount", 0));
		mtlWrapperDesc.addAttribute(Attrs::PluginAttr("refraction_amount", 0));

		mtl = m_pluginExporter.exportPlugin(mtlWrapperDesc);
	}

	// handle if object is forced as phantom
	if (isNodePhantom()) {
		Attrs::PluginDesc mtlStatsDesc;
		mtlStatsDesc.pluginID = "MtlRenderStats";
		mtlStatsDesc.pluginName = VRayExporter::getPluginName(&m_objNode, "MtlRenderStats");

		mtlStatsDesc.addAttribute(Attrs::PluginAttr("base_mtl", mtl));
		mtlStatsDesc.addAttribute(Attrs::PluginAttr("camera_visibility", 0));

		mtl = m_pluginExporter.exportPlugin(mtlStatsDesc);
	}


	return mtl;
}


int GeometryExporter::getSHOPOverridesAsUserAttributes(UT_String &userAttrs) const {
	int nOverrides = 0;

	OP_Node *shopNode = m_pluginExporter.getObjMaterial(&m_objNode, m_context.getTime());
	if (!shopNode) {
		return nOverrides;
	}

	// specify the id of the material to use
	userAttrs += VFH_ATTR_MATERIAL_ID;
	userAttrs += "=0;";

	// handle shop overrides specified on the object node
	const PRM_ParmList *shopParmList = shopNode->getParmList();
	const PRM_ParmList *objParmList = m_objNode.getParmList();

	for (int i = 0; i < shopParmList->getEntries(); ++i) {
		const PRM_Parm *shopPrm = shopParmList->getParmPtr(i);
		const PRM_Parm *objPrm = objParmList->getParmPtr(shopPrm->getToken());

		if (objPrm
			&& shopPrm->getType() == objPrm->getType()
			&& objPrm->getType().isFloatType()
			&& NOT(objPrm->getBypassFlag())) {
			// we have parameter with matching name on the OBJ_Node
			// => treat as override
			UT_StringArray prmValTokens;
			for (int i = 0; i < objPrm->getVectorSize(); ++i) {
				fpreal chval = m_objNode.evalFloat(objPrm, i, m_context.getTime());
				prmValTokens.append(std::to_string(chval));
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


int GeometryExporter::exportVRaySOP(SOP_Node &sop, PluginDescList &pluginList) {
	// add new node to our list of nodes
	pluginList.push_back(Attrs::PluginDesc("", "Node"));
	Attrs::PluginDesc &nodeDesc = pluginList.back();
	int nPlugins = 1;

	if (NOT(m_exportGeometry)) {
		// we do not need to export the geometry
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
	} else if (res == OP::VRayNode::PluginResultNA ||
		res == OP::VRayNode::PluginResultContinue) {
		m_pluginExporter.setAttrsFromOpNodePrms(geomDesc, &sop);
	}

	VRay::Plugin geom = m_pluginExporter.exportPlugin(geomDesc);
	nodeDesc.addAttribute(Attrs::PluginAttr("geometry", geom));

	return nPlugins;
}

int GeometryExporter::exportRenderPoints(const GU_Detail &gdp, VMRenderPoints renderPoints, PluginDescList &pluginList) {
	const GA_Size numPoints = gdp.getNumPoints();
	if (!numPoints) {
		return 0;
	}
	if (renderPoints == vmRenderPointsNone) {
		return 0;
	}

	DynamicBitset freePointMap(numPoints, true);
	GA_Size freePointCount = numPoints;
	if (renderPoints != vmRenderPointsAll) {
		freePointCount = fillFreePointMap(gdp, freePointMap);
	}

	// Parameters.
	const fpreal renderScale = m_objNode.evalFloat("vm_pointscale", 0, 0.0);

	int renderType;
	fpreal radiusMult = renderScale;

	const VMRenderPointsAs renderPointsAs =
		static_cast<VMRenderPointsAs>(m_objNode.evalInt("vm_renderpointsas", 0, 0.0));
	switch (renderPointsAs) {
	case vmRenderPointsAsSphere: {
		renderType = 7;
		radiusMult *= 0.01;
		break;
	}
	case vmRenderPointsAsCirle: {
		renderType = 6;
		radiusMult *= 5.0;
		break;
	}
	}

	// Particles positions.
	VRay::VUtils::VectorRefList positions(freePointCount);

	VRay::VUtils::VectorRefList velocities;
	GA_ROHandleV3 velocityHndl(gdp.findAttribute(GA_ATTRIB_POINT, GEO_STD_ATTRIB_VELOCITY));
	if (velocityHndl.isValid()) {
		velocities = VRay::VUtils::VectorRefList(freePointCount);
	}

	VRay::VUtils::ColorRefList color;
	GA_ROHandleV3 cdHndl(gdp.findAttribute(GA_ATTRIB_POINT, GEO_STD_ATTRIB_DIFFUSE));
	if (cdHndl.isValid()) {
		color = VRay::VUtils::ColorRefList(freePointCount);
	}

	VRay::VUtils::FloatRefList opacity;
	GA_ROHandleF opacityHndl(gdp.findAttribute(GA_ATTRIB_POINT, GEO_STD_ATTRIB_ALPHA));
	if (opacityHndl.isValid()) {
		opacity = VRay::VUtils::FloatRefList(freePointCount);
	}

	VRay::VUtils::FloatRefList pscale;
	const GA_Attribute *pscaleAttr = gdp.findAttribute(GA_ATTRIB_POINT, GEO_STD_ATTRIB_PSCALE);
	if (!pscaleAttr) {
		pscaleAttr = gdp.findAttribute(GA_ATTRIB_POINT, GEO_STD_ATTRIB_WIDTH);
	}
	GA_ROHandleF pscaleHndl(pscaleAttr);
	if (pscaleHndl.isValid()) {
		pscale = VRay::VUtils::FloatRefList(freePointCount);
	}

	int positionsIdx = 0;
	for (GA_Index i = 0; i < numPoints; ++i) {
		const GA_Offset ptOff = gdp.pointOffset(i);

		int isValidPoint;
		if (renderPoints == vmRenderPointsAll) {
			isValidPoint = true;
		} else {
			isValidPoint = freePointMap[i];
		}

		if (isValidPoint) {
			UT_ASSERT_MSG(positionsIdx < positions.size(), "Incorrect calculation of free points inside detail!");

			const UT_Vector3 &point = gdp.getPos3(ptOff);

			positions[positionsIdx].set(point.x(), point.y(), point.z());

			if (velocityHndl.isValid()) {
				const UT_Vector3F &v = velocityHndl.get(ptOff);
				velocities[positionsIdx].set(v.x(), v.y(), v.z());
			}
			if (pscaleHndl.isValid()) {
				pscale[positionsIdx] = radiusMult * pscaleHndl.get(ptOff);
			}

			if (cdHndl.isValid()) {
				const UT_Vector3F &cd = cdHndl.get(ptOff);
				color[positionsIdx].r = cd.x();
				color[positionsIdx].g = cd.y();
				color[positionsIdx].b = cd.z();
			}

			if (opacityHndl.isValid()) {
				opacity[positionsIdx] = opacityHndl.get(ptOff);
			}

			++positionsIdx;
		}
	}

	if (!positions.size()) {
		return 0;
	}

	Attrs::PluginDesc partDesc("", "GeomParticleSystem");
	partDesc.addAttribute(Attrs::PluginAttr("positions", positions));
	partDesc.addAttribute(Attrs::PluginAttr("render_type", renderType));
	if (velocities.size()) {
		partDesc.addAttribute(Attrs::PluginAttr("velocities", velocities));
	}
	if (color.size()) {
		partDesc.addAttribute(Attrs::PluginAttr("colors", color));
	}
	if (opacity.size()) {
		partDesc.addAttribute(Attrs::PluginAttr("opacity_pp", opacity));
	}
	if (pscale.size()) {
		partDesc.addAttribute(Attrs::PluginAttr("radii", pscale));
		partDesc.addAttribute(Attrs::PluginAttr("point_radii", true));
	} else {
		partDesc.addAttribute(Attrs::PluginAttr("radius", radiusMult));
		partDesc.addAttribute(Attrs::PluginAttr("point_size", radiusMult));
	}

	Attrs::PluginDesc partNode("", "Node");
	partNode.addAttribute(Attrs::PluginAttr("geometry", m_pluginExporter.exportPlugin(partDesc)));

	pluginList.push_back(partNode);

	return 1;
}

int GeometryExporter::exportDetail(SOP_Node &sop, GU_DetailHandleAutoReadLock &gdl, PluginDescList &pluginList) {
	int nPlugins = 0;

	const GU_Detail &gdp = *gdl.getGdp();

#ifdef CGR_HAS_AUR
	// handle export of volume primitives
	VolumeExporter volExp(m_objNode, m_context, m_pluginExporter);
	HoudiniVolumeExporter hVoldExp(m_objNode, m_context, m_pluginExporter);
	volExp.exportPrimitives(gdp, pluginList);
	hVoldExp.exportPrimitives(gdp, pluginList);
#endif // CGR_HAS_AUR

	// handle export of hair geometry
	HairPrimitiveExporter hairExp(m_objNode, m_context, m_pluginExporter);
	hairExp.exportPrimitives(gdp, pluginList);

	// handle export of packed primitives:
	// alembic, vray proxy, packed geometry
	// TODO: need to implement these as separate primitive exporter
	// per packed primitive type
	if (GU_PrimPacked::hasPackedPrimitives(gdp)) {
		UT_Array<const GA_Primitive *> prims;
		GU_PrimPacked::getPackedPrimitives(gdp, prims);
		for (const GA_Primitive *prim : prims) {
			auto *primPacked = UTverify_cast< const GU_PrimPacked * >(prim);
			nPlugins += exportPacked(sop, *primPacked, pluginList);
		}
	}

	// Export mesh geometry.
	const VMRenderPoints renderPoints =
		static_cast<VMRenderPoints>(m_objNode.evalInt("vm_renderpoints", 0, 0.0));

	if (renderPoints != vmRenderPointsAll) {
		const int numPolyPlugins = exportPolyMesh(sop, gdp, pluginList);
		if (numPolyPlugins) {
			nPlugins += numPolyPlugins;
		}
	}

	if (gdp.getNumPoints() &&
		(renderPoints == vmRenderPointsAll || renderPoints == vmRenderPointsUnconnected)) {
		nPlugins += exportRenderPoints(gdp, renderPoints, pluginList);
	}

	return nPlugins;
}


int GeometryExporter::exportPolyMesh(SOP_Node &sop, const GU_Detail &gdp, PluginDescList &pluginList) {
	int nPlugins = 0;

	MeshExporter polyMeshExporter(m_objNode, m_context, m_pluginExporter);
	polyMeshExporter.init(gdp);
	polyMeshExporter.setSubdivApplied(hasSubdivApplied());
	if (polyMeshExporter.hasPolyGeometry()) {
		if (m_exportGeometry) {
			polyMeshExporter.exportPrimitives(gdp, pluginList);
		} else {
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

		nPlugins = 1;
	}

	return nPlugins;
}


int GeometryExporter::exportPacked(SOP_Node &sop, const GU_PrimPacked &prim, PluginDescList &pluginList) {
	// get primitive unique id and check if we've already
	// processed that geometry
	uint packedID = getPrimPackedID(prim);
	if (NOT(m_detailToPluginDesc.count(packedID))) {
		// we haven't, so export geometry from that primitive
		exportPrimPacked(sop, prim, m_detailToPluginDesc[packedID]);
	}

	// get the list of Node descriptions for the primitive and
	// adjust it's properties as transform, material, user_attributes, etc.
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
		} else {
			attr->paramValue.valTransform = tm * attr->paramValue.valTransform;
		}

		// we assign material and user_attributes only if material
		// has not been previously assigned. Existing VFH_ATTR_MATERIAL_ID
		// property signals that the node already has material and overrides set
		attr = nodeDesc.get(VFH_ATTR_MATERIAL_ID);
		if (NOT(attr)
			&& mtlpath.isValid()) {
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
				if (mtlo.isValid()
					&& mtlOverridesDict.setFromPyDictionary(mtlo.get(prim.getMapOffset()))) {
					while (mtlOverridesDict.getNumOptions() > 0) {
						UT_String key(mtlOverridesDict.begin().name().c_str());

						int chIdx = -1;
						PRM_Parm *prm = shopNode->getParmList()->getParmPtrFromChannel(key, &chIdx);
						if (NOT(prm)
							|| NOT(prm->getType().isFloatType())) {
							mtlOverridesDict.removeOption(key);
							continue;
						}

						UT_StringArray prmValTokens;
						for (int i = 0; i < prm->getVectorSize(); ++i) {
							prm->getChannelToken(key, i);
							fpreal chval = (mtlOverridesDict.hasOption(key)) ? mtlOverridesDict.getOptionF(key) : shopNode->evalFloat(prm, i, m_context.getTime());
							prmValTokens.append(std::to_string(chval));
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
					nodeDesc.addAttribute(Attrs::PluginAttr("user_attributes", userAtrs.toStdString()));
				}
			}
		}
	}

	return primPluginList.size();
}


uint GeometryExporter::getPrimPackedID(const GU_PrimPacked &prim) {
	uint packedID = 0;

	if (prim.getTypeId() == GU_PrimPacked::lookupTypeId("AlembicRef")) {
		UT_String primname;
		prim.getIntrinsic(prim.findIntrinsic("packedprimitivename"), primname);
		packedID = primname.hash();
	} else if (prim.getTypeId() == GU_PrimPacked::lookupTypeId("PackedDisk")) {
		UT_String primname;
		prim.getIntrinsic(prim.findIntrinsic("packedprimitivename"), primname);
		packedID = primname.hash();
	} else if (prim.getTypeId() == GU_PrimPacked::lookupTypeId("VRayProxyRef")) {
		auto vrayproxyref = UTverify_cast< const VRayProxyRef * >(prim.implementation());
		packedID = vrayproxyref->getOptions().hash();
	} else if (prim.getTypeId() == GU_PrimPacked::lookupTypeId("PackedGeometry")) {
		int geoid = -1;
		prim.getIntrinsic(prim.findIntrinsic("geometryid"), geoid);
		packedID = geoid;
	}

	return packedID;
}


int GeometryExporter::exportPrimPacked(SOP_Node &sop, const GU_PrimPacked &prim, PluginDescList &pluginList) {
	// packed primitives can be of different types
	// currently supporting:
	//   AlembicRef - geometry in alembic file on disk
	//   PackedDisk - geometry in partio file on disk
	//   PackedGeometry - in-mem geometry

	int nPlugins = 0;

	if (prim.getTypeId() == GU_PrimPacked::lookupTypeId("AlembicRef")) {
		nPlugins = exportAlembicRef(sop, prim, pluginList);
	} else if (prim.getTypeId() == GU_PrimPacked::lookupTypeId("PackedDisk")) {
		nPlugins = exportPackedDisk(sop, prim, pluginList);
	} else if (prim.getTypeId() == GU_PrimPacked::lookupTypeId("VRayProxyRef")) {
		nPlugins = exportVRayProxyRef(sop, prim, pluginList);
	} else if (prim.getTypeId() == GU_PrimPacked::lookupTypeId("PackedGeometry")) {
		nPlugins = exportPackedGeometry(sop, prim, pluginList);
	}

	return nPlugins;
}


int GeometryExporter::exportAlembicRef(SOP_Node &sop, const GU_PrimPacked &prim, PluginDescList &pluginList) {
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
	pluginDesc.addAttribute(Attrs::PluginAttr("file", filename.toStdString()));
	pluginDesc.addAttribute(Attrs::PluginAttr("use_alembic_offset", true));

	VRay::Plugin geom = m_pluginExporter.exportPlugin(pluginDesc);
	nodeDesc.addAttribute(Attrs::PluginAttr("geometry", geom));

	return nPlugins;
}


int GeometryExporter::exportVRayProxyRef(SOP_Node &sop, const GU_PrimPacked &prim, PluginDescList &pluginList) {
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
	UT_Options options = vrayproxyref->getOptions();
	if (options.hasOption("anim_offset")) {
		options.setOptionF("anim_offset", options.getOptionF("anim_offset") - m_context.getFrame());
	}
	m_pluginExporter.setAttrsFromUTOptions(pluginDesc, options);

	VRay::Plugin geom = m_pluginExporter.exportPlugin(pluginDesc);
	nodeDesc.addAttribute(Attrs::PluginAttr("geometry", geom));

	return 1;
}


int GeometryExporter::exportPackedDisk(SOP_Node &sop, const GU_PrimPacked &prim, PluginDescList &pluginList) {
	// TODO: implement this

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

	pluginDesc.addAttribute(Attrs::PluginAttr("file", filename.toStdString()));

	VRay::Plugin geom = m_pluginExporter.exportPlugin(pluginDesc);
	nodeDesc.addAttribute(Attrs::PluginAttr("geometry", geom));

	return nPlugins;
}


int GeometryExporter::exportPackedGeometry(SOP_Node &sop, const GU_PrimPacked &prim, PluginDescList &pluginList) {
	// PackedGeometry - in-mem geometry
	// "path" attibute references the Pack SOP
	// so take its input gdp
	// TODO: just unpack the packed detail

	int nPlugins = 0;

	const GA_ROHandleS pathHndl(prim.getDetail().findAttribute(GA_ATTRIB_PRIMITIVE, "path"));
	if (NOT(pathHndl.isValid())) {
		// there is no path attribute =>
		// take geometry directly from primitive packed detail
		GU_DetailHandleAutoReadLock gdl(prim.getPackedDetail());
		if (gdl.isValid()) {
			nPlugins = exportDetail(sop, gdl, pluginList);
		}
	} else {
		UT_StringHolder path = pathHndl.get(prim.getMapOffset());
		SOP_Node *sopref = OPgetDirector()->findSOPNode(path);
		if (NOT(sopref)) {
			// path is not referencing a valid sop =>
			// take geometry directly from primitive packed detail
			GU_DetailHandleAutoReadLock gdl(prim.getPackedDetail());
			if (gdl.isValid()) {
				nPlugins = exportDetail(sop, gdl, pluginList);
			}
		} else {
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
			} else {
				GU_DetailHandleAutoReadLock gdl(prim.getPackedDetail());
				if (gdl.isValid()) {
					nPlugins = exportDetail(sop, gdl, pluginList);
				}
			}
		}
	}

	return nPlugins;
}
