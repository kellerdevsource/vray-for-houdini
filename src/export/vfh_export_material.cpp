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
#include "vfh_prm_templates.h"

#include "obj/obj_node_base.h"
#include "vop/vop_node_base.h"
#include "vop/material/vop_mtl_def.h"

#include <SHOP/SHOP_Node.h>
#include <SOP/SOP_Node.h>
#include <OP/OP_Options.h>
#include <OP/OP_PropertyLookupList.h>
#include <OP/OP_MultiparmInfo.h>
#include <GU/GU_Detail.h>

#include <PRM/PRM_ParmMicroNode.h>
#include <PRM/PRM_RefId.h>

#include <unordered_map>
#include <unordered_set>

using namespace VRayForHoudini;


struct MaterialOverrideLink {
	MaterialOverrideLink(OP_Node &fromObject, const std::string &fromAttr, OP_Node &targetVOP, const std::string &targetAttr)
		: fromObject(fromObject)
		, fromAttr(fromAttr)
		, targetVOP(targetVOP)
		, targetAttr(targetAttr)
	{}

	OP_Node     &fromObject; // Object that overrides the attribute value
	std::string  fromAttr;   // Object's attrubute name (could be different from target VOP name)
	OP_Node     &targetVOP;  // Target VOP to override property on
	std::string  targetAttr; // Target VOP attrubute name
};

typedef std::vector<MaterialOverrideLink>  MaterialOverrideLinks;


MtlContext& MtlContext::GetInstance()
{
	static MtlContext mtlContext;
	return mtlContext;
}


MtlContext::MtlContext():
	m_exporter(nullptr),
	m_shopNode(nullptr),
	m_objNode(nullptr),
	m_overrideType(MTLO_NONE)
{ }


MtlContext::~MtlContext()
{
	clear();
}

void MtlContext::initVOPOverrides()
{
	if ( !isValid() ) {
		return;
	}

	PRM_ParmList *shopPrmList = m_shopNode->getParmList();

	DEP_MicroNodeList micronodes;
	shopPrmList->getParmMicroNodes(micronodes);
	// loop through all param channels on our shop node that are linked
	for (DEP_MicroNode *micronode : micronodes) {
		PRM_ParmMicroNode *prmMicronode = dynamic_cast<PRM_ParmMicroNode*>(micronode);
		if (!prmMicronode) {
			continue;
		}

		PRM_Parm &shopPrm = prmMicronode->ownerParm();
		UT_String channelToken;
		shopPrm.getChannelToken(channelToken, prmMicronode->subIndex());

		// find all parameters linking to current channel
		UT_ValArray<PRM_Parm *> parms;
		UT_IntArray parmsubidxs;
		m_shopNode->getParmsThatReference(channelToken.buffer(), parms, parmsubidxs);
		for (PRM_Parm *parm : parms) {
			OP_Node *node = parm->getParmOwner()->castToOPNode();
			// if the owner node is a child of our shop node => is a vop node
			// add link <vop parm name> to <shop parm name> to the corresponding vop table
			if ( m_shopNode->isSubNode(node) ) {
				OverrideMap &vopOverrides = m_vopOverrides[ node->getUniqueId() ];
				if ( !vopOverrides.count( parm->getToken() ) ) {
					vopOverrides[ parm->getToken() ] = shopPrm.getToken();
				}
			}
		}
	}
}


void MtlContext::initSHOPOverrides()
{
	if (!isValid()) {
		return;
	}

	if (!m_vopOverrides.size()) {
		return;
	}

	PRM_ParmList *objPrmList = m_objNode->getParmList();
	PRM_ParmList *shopPrmList = m_shopNode->getParmList();

	if (m_objNode->getMaterialNode(m_exporter->getContext().getTime()) == m_shopNode) {
		// overrides are specified on the object node if any
		DEP_MicroNodeList micronodes;
		shopPrmList->getParmMicroNodes(micronodes);
		for (DEP_MicroNode *micronode : micronodes) {
			PRM_ParmMicroNode *prmMicronode = dynamic_cast<PRM_ParmMicroNode*>(micronode);
			if (NOT(prmMicronode)) {
				continue;
			}

			PRM_Parm &prm = prmMicronode->ownerParm();
			if (m_shopOverrrides.count(prm.getToken())) {
				// we already have a link for this shop parameter
				continue;
			}

			PRM_Parm *objPrmOverride = objPrmList->getParmPtr(prm.getToken());
			if (    objPrmOverride
				&& !objPrmOverride->getBypassFlag())
			{
				const PRM_SpareData	*spare = objPrmOverride->getSparePtr();
				// If the parameter is for material override it has OBJ_MATERIAL_SPARE_TAG tag
				if (   spare
					&& spare->getValue(OBJ_MATERIAL_SPARE_TAG))
				{
					// we have override on object level
					m_shopOverrrides[prm.getToken()] = objPrmOverride->getToken();
				}
			}
		}

		if (m_shopOverrrides.size()) {
			m_overrideType = MTLO_OBJ;
		}
	}
	else {
		fpreal t = m_exporter->getContext().getTime();
		// overrides are per primitive if any
		OP_NodeList nodeList;
		m_shopNode->getExistingOpDependents(nodeList, true);
		for (OP_Node *depNode : nodeList) {
			if (    m_objNode->isSubNode(depNode)
				&& !depNode->getBypass()
				&& depNode->getOperator()->getName() == "material")
			{
				UT_String path;
				depNode->getFullPath(path);

				const int numMaterials = depNode->evalInt("num_materials", 0, t);
				for (int mtlIdx = 1; mtlIdx <= numMaterials; ++mtlIdx) {
					static boost::format FmtShopPath("shop_materialpath%i");

					UT_String shopMaterial;
					depNode->evalString(shopMaterial, boost::str(FmtShopPath % mtlIdx).c_str(), 0, t);
					if (OPgetDirector()->findSHOPNode(shopMaterial.buffer()) == m_shopNode) {
						static boost::format FmtNumLocal("num_local%i");

						const int numLocal = depNode->evalInt(boost::str(FmtNumLocal % mtlIdx).c_str(), 0, t);
						for (int localIdx = 1; localIdx <= numLocal; ++localIdx) {
							static boost::format FmtLocalName("local%i_name%i");

							UT_String localName;
							depNode->evalString(localName, boost::str(FmtLocalName % mtlIdx % localIdx).c_str(), 0, t);
							PRM_Parm *prm = shopPrmList->getParmPtr(localName);

							if (prm) {
								const PRM_Type &prmType = prm->getType();
								if (prmType.isFloatType()) {
									m_shopOverrrides[ prm->getToken() ] = prm->getToken();
								}
							}
						}
					}
				}
			}
		}

		if (m_shopOverrrides.size()) {
			m_overrideType = MTLO_GEO;
		}
	}
}

void MtlContext::init(VRayExporter &exporter, OBJ_Node &object, SHOP_Node &shopNode)
{
	clear();

	m_exporter = &exporter;
	m_shopNode = &shopNode;
	m_objNode = &object;

	initVOPOverrides();
	initSHOPOverrides();

//	m_exporter = &exporter;
//	m_objNode = &object;
//	m_shopNode = &shopNode;

//	std::cout << "  ========  " << std::endl;
//	shopNode.dumpOpDependents(m_objNode, 0, std::cout);
//	std::cout << " ~~~~~~~ " << std::endl;
//	shopNode.dumpDependencies();
//	std::cout << "  ========  " << std::endl;
//	object.dumpOpDependents(m_shopNode, 0, std::cout);
//	std::cout << " ~~~~~~~ " << std::endl;
//	object.dumpDependencies();
//	std::cout << "  ========  " << std::endl;

//	std::unordered_set< std::string > prmNames;


//	shop_node->dumpDependencies();

//	std::cout << "  ========  " << std::endl;
//	const OP_DependencyList &depList = shop_node->getOpDependents();
//	for (OP_DependencyList::const_iterator it = depList.begin(); !it.atEnd(); it.advance()) {
//		const OP_Dependency &dep = *it;
//		std::cout   << "  ======== depId= " << dep.getRefOpId()
//					<< " objId = " << m_objNode->getUniqueId()
//					<< " prmIdx = " << dep.getRefId().getParmRef()
//					<< std::endl;

//	}


//	PRM_ParmList *shopPrmList = m_shopNode->getParmList();
//	DEP_MicroNodeList micronodes;
//	shopPrmList->getParmMicroNodes(micronodes);
//	for (DEP_MicroNode *micronode : micronodes) {
//		PRM_ParmMicroNode *prmMicronode = dynamic_cast<PRM_ParmMicroNode*>(micronode);
//		PRM_Parm &prm = prmMicronode->ownerParm();

//		std::cout << "  ========  " << prm.getToken() << std::endl;

//		DEP_MicroNodeList microinps;
//		prmMicronode->getInputs(microinps);
//		DEP_MicroNodeList microouts;
//		prmMicronode->getOutputs(microouts);
//		std::cout << "  ==== microin = " << microinps.size() << " microout = " << microouts.size() << std::endl;

//		for (int i = 0; i < microinps.size(); ++i) {
//			PRM_ParmMicroNode *prmMicroinp = dynamic_cast<PRM_ParmMicroNode*>(microinps(i));
//			PRM_Parm &prmInp = prmMicroinp->ownerParm();
//			OP_Node *ownerNodeInp = prmInp.getParmOwner()->castToOPNode();

//			std::cout << "  ===== microin  " << i << "\t" << prmInp.getToken() << "\t" << ownerNodeInp->getUniqueId() << std::endl;
//		}

//		for (int i = 0; i < microouts.size(); ++i) {
//			PRM_ParmMicroNode *prmMicroout = dynamic_cast<PRM_ParmMicroNode*>(microouts(i));
//			PRM_Parm &prmOut = prmMicroout->ownerParm();
//			OP_Node *ownerNodeOut = prmOut.getParmOwner()->castToOPNode();

//			std::cout << "  ===== microin  " << i << "\t" << prmOut.getToken() << "\t" << ownerNodeOut->getUniqueId() << std::endl;
//		}

//		std::cout << "  ========  " << std::endl;
//		std::cout << "  ========  microinputs = " << microinps << std::endl;
//		std::cout << "  ========  " << std::endl;
//	}


}

void MtlContext::clear()
{
	m_vopOverrides.clear();
	m_shopOverrrides.clear();

	m_exporter = nullptr;
	m_shopNode = nullptr;
	m_objNode = nullptr;
	m_overrideType = MTLO_NONE;
}


bool MtlContext::getOverrideName(VOP_Node &vopNode, const std::string &prmName, std::string &o_overrideName) const
{
	if (!m_vopOverrides.count(vopNode.getUniqueId())) {
		return false;
	}

	const OverrideMap &vopOverrides = m_vopOverrides.at( vopNode.getUniqueId() );
	if (!vopOverrides.count(prmName)) {
		return false;
	}

	bool res = false;
	const std::string &shopPrmName = vopOverrides.at(prmName);
	if (m_shopOverrrides.count(shopPrmName)) {
		o_overrideName = m_shopOverrrides.at(shopPrmName);
		res = true;
	}
	return res;
}


//int MtlContext::hasMaterialOverrides()
//{
//	int hasMtlOverrides = false;

//	if (m_objNode) {
//		SOP_Node *sopNode = m_objNode->getRenderSopPtr();
//		if (sopNode) {
//			OP_Context ctx; // NOTE: May be use context from exporter?
//			GU_DetailHandleAutoReadLock gdl(sopNode->getCookedGeoHandle(ctx));

//			const GU_Detail *gdp = gdl.getGdp();
//			if (gdp) {
//				GA_ROAttributeRef materialOverridesHandle = gdp->findStringTuple(GA_ATTRIB_PRIMITIVE, "material_overrides");
//				if (materialOverridesHandle.isValid()) {
//					hasMtlOverrides = true;
//				}
//			}
//		}
//	}

//	return hasMtlOverrides;
//}


//int MtlContext::hasMaterialPromotes()
//{
//	int hasMtlPromotes = false;


//	if (m_objNode) {
//		// ...
//	}

//	return hasMtlPromotes;
//}


VRay::Plugin VRayExporter::exportMaterial(SHOP_Node &shop_node, MtlContext &ctx)
{
	VRay::Plugin material;
	UT_ValArray<OP_Node *> mtlOutList;
	if ( shop_node.getOpsByName("vray_material_output", mtlOutList) ) {
		// there is at least 1 "vray_material_output" node so take the first one
		VOP::MaterialOutput *mtlOut = static_cast< VOP::MaterialOutput * >( mtlOutList(0) );
		addOpCallback(mtlOut, VRayExporter::RtCallbackSurfaceShop);

		if (mtlOut->error() < UT_ERROR_ABORT ) {
			Log::getLog().info("Exporting material output \"%s\"...",
							   mtlOut->getName().buffer());

			const int idx = mtlOut->getInputFromName("Material");
			OP_Node *inpNode = mtlOut->getInput(idx);
			if (inpNode) {
				VOP_Node *vopNode = inpNode->castToVOPNode();
				if (vopNode) {
					switch (mtlOut->getInputType(idx)) {
						case VOP_SURFACE_SHADER: {
							material = exportVop(vopNode);
							break;
						}
						case VOP_TYPE_BSDF: {
							VRay::Plugin pluginBRDF = exportVop(vopNode);

							// Wrap BRDF into MtlSingleBRDF for RT GPU to work properly
							Attrs::PluginDesc mtlPluginDesc(VRayExporter::getPluginName(vopNode, "Mtl@"), "MtlSingleBRDF");
							mtlPluginDesc.addAttribute(Attrs::PluginAttr("brdf", pluginBRDF));

							material = exportPlugin(mtlPluginDesc);
							break;
						}
						default:
							Log::getLog().error("Unsupported input type for node \"%s\", input %d!",
												mtlOut->getName().buffer(), idx);
					}

					if (material && isIPR()) {
						// Wrap material into MtlRenderStats to always have the same material name
						// Used when rewiring materials when running interactive RT session
						Attrs::PluginDesc pluginDesc(VRayExporter::getPluginName(mtlOut->getParent(), "Mtl@"), "MtlRenderStats");
						pluginDesc.addAttribute(Attrs::PluginAttr("base_mtl", material));
						material = exportPlugin(pluginDesc);
					}
				}
			}
		}
	}
	else {
		Log::getLog().error("Can't find \"V-Ray Material Output\" operator under \"%s\"!",
							shop_node.getName().buffer());
	}

	if ( NOT(material) ) {
		material = exportDefaultMaterial();
	}

	return material;
}

//		if (ctx.getObject()) {
//			const fpreal now = m_context.getTime();

//			OBJ_Node &obj = *ctx.getObject();

//			SHOP_Node *shopNode = getObjMaterial(&obj, now);
//			if (shopNode) {
//				// Collect material override parameters
//				MaterialOverrideLinks mtlOverLinks;

//				const PRM_ParmList *objParmList  = obj.getParmList();
//				PRM_ParmList       *shopParmList = shopNode->getParmList();

//				if (objParmList && shopParmList) {
//					for(int pi = 0; pi < objParmList->getEntries(); pi++) {
//						const PRM_Parm *parm = objParmList->getParmPtr(pi);
//						if (parm && !parm->getBypassFlag()) {
//							const PRM_SpareData	*spare = parm->getSparePtr();
//							if (spare) {
//								// If the parameter is for material override it has OBJ_MATERIAL_SPARE_TAG tag
//								if (spare->getValue(OBJ_MATERIAL_SPARE_TAG)) {
//									std::string propOverName(parm->getToken());

//									int componentIndex = 0;

//									// TODO: We are interested in a property name (like "diffuse") not component name (like "diffuser"),
//									// so if the property is a compound value (like color) ||| append component suffix to get
//									// the correct reference (because references are per-component).
//									if (parm->getVectorSize() > 1) {
//#if 0
//										propOverName += "r";
//										// We don't care about the actual index
//										componentIndex = 0;
//#endif
//									}

//									// Property index on a SHOP
//									const int propOverParmIdx = shopParmList->getParmIndex(propOverName.c_str());

//									if (propOverParmIdx >= 0) {
//										DEP_MicroNode &src = shopParmList->parmMicroNode(propOverParmIdx, componentIndex);

//										DEP_MicroNodeList outputs;
//										src.getOutputs(outputs);
//										for (int i = 0; i < outputs.entries(); ++i) {
//											PRM_ParmMicroNode *micronode = dynamic_cast<PRM_ParmMicroNode*>(outputs(i));
//											if (micronode) {
//												const PRM_Parm &referringParm = micronode->ownerParm();
//												PRM_ParmOwner  *referringParmOwner = referringParm.getParmOwner();
//												if (referringParmOwner) {
//													OP_Node *referringNode = referringParmOwner->castToOPNode();
//													if (referringNode) {
//														// Adding material override link
//														mtlOverLinks.emplace_back(static_cast<OP_Node&>(obj), propOverName, *referringNode, referringParm.getToken());
//														break;
//													}
//												}
//											}
//										}
//									}
//								}
//							}
//						}
//					}
//				}

//				for (const auto &mtlOverLink : mtlOverLinks) {
//					Log::getLog().msg("  Object's \'%s\' prop \'%s\' overrides \'%s\' from \'%s\'",
//									  mtlOverLink.fromObject.getName().buffer(),
//									  mtlOverLink.fromAttr.c_str(),
//									  mtlOverLink.targetAttr.c_str(),
//									  mtlOverLink.targetVOP.getName().buffer());
//				}
//			}
//		}




void VRayExporter::exportVOPOverrides(MtlContext &mtlContext, VOP_Node *vopNode, Attrs::PluginDesc &pluginDesc)
{
	vassert( mtlContext.hasOverrides() );

	VOP::NodeBase *vrayVOP = static_cast< VOP::NodeBase* >(vopNode);
	if (NOT(vrayVOP)) {
		return;
	}

	const Parm::VRayPluginInfo *pluginInfo = Parm::GetVRayPluginInfo(vrayVOP->getVRayPluginID());
	if (NOT(pluginInfo)) {
		return;
	}

	MtlContext::MTLOverrideType overrideType = mtlContext.getOverridesType();
	vassert( overrideType != MtlContext::MTLO_NONE );

	for (const auto &aIt : pluginInfo->attributes) {
		const std::string    &attrName = aIt.first;
		const Parm::AttrDesc &attrDesc = aIt.second;

		// we have override
		// TODO: need a mapping function from vop attrName to param/input name
		int inpIdx = vrayVOP->getInputFromName(attrName.c_str());
		OP_Node *inpNode = vrayVOP->getInput(inpIdx);
		if (inpNode) {
			// if we have input connected for this attribute skip override
			// inputs take priority over everything else
			continue;
		}

		switch (overrideType) {
			case MtlContext::MTLO_OBJ:
			// MTLO_OBJ = override value should be taken from param on the corresponding object node
			{
				std::string overridingObjPrmName;
				if (mtlContext.getOverrideName(*vrayVOP, attrName, overridingObjPrmName)) {
					setAttrValueFromOpNode(pluginDesc, attrDesc, *mtlContext.getObject(), overridingObjPrmName);
				}
				break;
			}
			case MtlContext::MTLO_GEO:
			// MTLO_GEO = override value should be taken from a map channel of the mesh
			{
				std::string overridingChannelName;
				if (mtlContext.getOverrideName(*vrayVOP, attrName, overridingChannelName))
				{
					OBJ_Node *srcOverride = mtlContext.getObject();
					switch (attrDesc.value.type) {
						case Parm::eTextureInt:
						{
							Attrs::PluginDesc mtlOverrideDesc;
							mtlOverrideDesc.pluginName = VRayExporter::getPluginName(vopNode, "MtlOverride@", srcOverride->getName().toStdString());
							mtlOverrideDesc.pluginID = "TexUserScalar";
							mtlOverrideDesc.addAttribute(Attrs::PluginAttr("user_attribute", overridingChannelName));
							VRay::Plugin mtlOverridePlg = exportPlugin(mtlOverrideDesc);
							pluginDesc.addAttribute(Attrs::PluginAttr(attrName, mtlOverridePlg, "scalar"));

							break;
						}
						case Parm::eTextureFloat:
						{
							Attrs::PluginDesc mtlOverrideDesc;
							mtlOverrideDesc.pluginName = VRayExporter::getPluginName(vopNode, "MtlOverride@", srcOverride->getName().toStdString());
							mtlOverrideDesc.pluginID = "TexUserScalar";
							mtlOverrideDesc.addAttribute(Attrs::PluginAttr("user_attribute", overridingChannelName));
							VRay::Plugin mtlOverridePlg = exportPlugin(mtlOverrideDesc);
							pluginDesc.addAttribute(Attrs::PluginAttr(attrName, mtlOverridePlg, "scalar"));

							break;
						}
						case Parm::eTextureColor:
						{
							Attrs::PluginDesc mtlOverrideDesc;
							mtlOverrideDesc.pluginName = VRayExporter::getPluginName(vopNode, "MtlOverride@", srcOverride->getName().toStdString());
							mtlOverrideDesc.pluginID = "TexUserColor";
							mtlOverrideDesc.addAttribute(Attrs::PluginAttr("user_attribute", overridingChannelName));
							VRay::Plugin mtlOverridePlg = exportPlugin(mtlOverrideDesc);
							pluginDesc.addAttribute(Attrs::PluginAttr(attrName, mtlOverridePlg, "color"));

							break;
						}
						default:
						// ignore other types for now
							;
					}

				}
				break;
			}
			default:
				;
		}
	}
}
