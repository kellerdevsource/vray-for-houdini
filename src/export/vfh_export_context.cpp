//
// Copyright (c) 2015, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#include "vfh_export_context.h"
#include "vfh_exporter.h"

#include <PRM/PRM_ParmMicroNode.h>
#include <OBJ/OBJ_Node.h>
#include <SHOP/SHOP_Node.h>
#include <SHOP/SHOP_Util.h>


using namespace VRayForHoudini;


ExportContext::ExportContext():
	m_type(CT_NULL),
	m_exporter(nullptr),
	m_target(nullptr),
	m_parentContext(nullptr)
{ }


ExportContext::ExportContext(ContextType type, VRayExporter &exporter, OP_Node &node):
	m_type(type),
	m_exporter(&exporter),
	m_target(&node),
	m_parentContext(nullptr)
{ }


ExportContext::ExportContext(ContextType type, VRayExporter &exporter, OP_Node &node, ExportContext &parentContext):
	m_type(type),
	m_exporter(&exporter),
	m_target(&node),
	m_parentContext(&parentContext)
{ }



SHOPExportContext::SHOPExportContext(VRayExporter &exporter, SHOP_Node &shopNode, ExportContext &parentContext):
	ExportContext(CT_SHOP, exporter, shopNode, parentContext),
	m_overrideType(MTLO_NONE)
{ }


ECFnOBJNode::ECFnOBJNode(ExportContext *ctx):
	m_context(nullptr)
{
	if (   ctx
		&& ctx->hasType(CT_OBJ)
		&& ctx->getExporter()
		&& ctx->getTarget()
		&& ctx->getTarget()->castToOBJNode())
	{
		m_context = ctx;
	}
}


bool ECFnOBJNode::isValid() const
{
	return (m_context);
}


OBJ_Node* ECFnOBJNode::getTargetNode() const
{
	return m_context->getTarget()->castToOBJNode();
}


ECFnSHOPOverrides::ECFnSHOPOverrides(ExportContext *ctx):
	m_context(nullptr)
{
	SHOPExportContext *shopContext = dynamic_cast< SHOPExportContext *>(ctx);
	if (   shopContext
		&& shopContext->getParentContext() )
	{
		ECFnOBJNode objContext(shopContext->getParentContext());
		if (   shopContext->hasType(CT_SHOP)
			&& shopContext->getExporter()
			&& shopContext->getTarget()
			&& shopContext->getTarget()->castToSHOPNode()
			&& objContext.isValid() )
		{
			m_context = shopContext;
		}
	}
}


bool ECFnSHOPOverrides::isValid() const
{
	return (m_context);
}


SHOP_Node* ECFnSHOPOverrides::getTargetNode() const
{
	return m_context->getTarget()->castToSHOPNode();
}


OBJ_Node* ECFnSHOPOverrides::getObjectNode() const
{
	return m_context->getParentContext()->getTarget()->castToOBJNode();
}


void ECFnSHOPOverrides::initOverrides()
{
	m_context->m_overrideType = MTLO_NONE;
	m_context->m_vopOverrides.clear();
	m_context->m_shopOverrrides.clear();

	initVOPOverrides();
	// there are no params on the shop node that override params of any child vop
	if (!m_context->m_vopOverrides.size()) {
		return;
	}

	initSHOPOverrides();
}


void ECFnSHOPOverrides::initVOPOverrides()
{
	SHOP_Node *shopNode = getTargetNode();

	const OP_DependencyList &depList = shopNode->getOpDependents();
	for (OP_DependencyList::reverse_iterator it = depList.rbegin(); !it.atEnd(); it.advance()) {
		const OP_Dependency &dep = *it;
		OP_Node *opNode = shopNode->lookupNode(dep.getRefOpId(), false);
		if (shopNode->isSubNode(opNode)) {
			const PRM_Parm &vopPrm = opNode->getParm(dep.getRefId().getParmRef());
			const PRM_Parm &shopPrm = shopNode->getParm(dep.getSourceRefId().getParmRef());

			SHOPExportContext::OverrideMap &vopOverrides = m_context->m_vopOverrides[ opNode->getUniqueId() ];
			if ( !vopOverrides.count( vopPrm.getToken() ) ) {
				vopOverrides[ vopPrm.getToken() ] = shopPrm.getToken();
			}
		}
	}
}


void ECFnSHOPOverrides::initSHOPOverrides()
{
	SHOP_Node *shopNode = getTargetNode();
	OBJ_Node *objNode = getObjectNode();

	PRM_ParmList *objPrmList = objNode->getParmList();
	PRM_ParmList *shopPrmList = shopNode->getParmList();
	fpreal t = m_context->getExporter()->getContext().getTime();

	if (objNode->getMaterialNode(t) == shopNode) {
		// overrides are specified on the object node if any
		const OP_DependencyList &depList = shopNode->getOpDependents();
		for (OP_DependencyList::reverse_iterator it = depList.rbegin(); !it.atEnd(); it.advance()) {
			const OP_Dependency &dep = *it;
			OP_Node *opNode = shopNode->lookupNode(dep.getRefOpId(), false);
			if (shopNode->isSubNode(opNode)) {
				const PRM_Parm &shopPrm = shopNode->getParm(dep.getSourceRefId().getParmRef());
				const PRM_Parm *objPrm = objPrmList->getParmPtr(shopPrm.getToken());

				if (objPrm && !objPrm->getBypassFlag()) {
					// if we have parameter with matching name override on object level
					m_context->m_shopOverrrides[ shopPrm.getToken() ] = objPrm->getToken();
				}
			}
		}

		if (m_context->m_shopOverrrides.size()) {
			m_context->m_overrideType = MTLO_OBJ;
		}
	}
	else {
		// overrides are per primitive if any
		OP_NodeList nodeList;
		shopNode->getExistingOpDependents(nodeList, true);
		for (OP_Node *depNode : nodeList) {
			if (    objNode->isSubNode(depNode)
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
					if (OPgetDirector()->findSHOPNode(shopMaterial.buffer()) == shopNode) {
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
									m_context->m_shopOverrrides[ prm->getToken() ] = prm->getToken();
								}
							}
						}
					}
				}
			}
		}

		if (m_context->m_shopOverrrides.size()) {
			m_context->m_overrideType = MTLO_GEO;
		}
	}
}


MTLOverrideType ECFnSHOPOverrides::getOverrideType() const
{
	return m_context->m_overrideType;
}


bool ECFnSHOPOverrides::hasOverrides() const
{
	return (m_context->m_overrideType != MTLO_NONE && m_context->m_shopOverrrides.size());
}


bool ECFnSHOPOverrides::hasOverrides(VOP_Node &vopNode) const
{
	return (hasOverrides() && m_context->m_vopOverrides.count( vopNode.getUniqueId() ));
}


bool ECFnSHOPOverrides::getOverrideName(VOP_Node &vopNode, const std::string &prmName, std::string &o_overrideName) const
{
	if (!m_context->m_vopOverrides.count(vopNode.getUniqueId())) {
		return false;
	}

	const SHOPExportContext::OverrideMap &vopOverrides = m_context->m_vopOverrides.at( vopNode.getUniqueId() );
	if (!vopOverrides.count(prmName)) {
		return false;
	}

	bool res = false;
	const std::string &shopPrmName = vopOverrides.at(prmName);
	if (m_context->m_shopOverrrides.count(shopPrmName)) {
		o_overrideName = m_context->m_shopOverrrides.at(shopPrmName);
		res = true;
	}
	return res;
}




//void MtlContext::init(VRayExporter &exporter, OBJ_Node &object, SHOP_Node &shopNode)
//{
//	clear();

//	m_exporter = &exporter;
//	m_shopNode = &shopNode;
//	m_objNode = &object;

//	initVOPOverrides();
//	initSHOPOverrides();

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


//}
