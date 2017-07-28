//
// Copyright (c) 2015-2017, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text:
//   https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#include "vfh_op_utils.h"
#include "vfh_attr_utils.h"

#include "vop_node_base.h"

#include <SHOP/SHOP_Node.h>
#include <OP/OP_Bundle.h>
#include <OP/OP_BundleList.h>

using namespace VRayForHoudini;

OP_Node* VRayForHoudini::getVRayNodeFromOp(OP_Node &matNode, const char *socketName, const char *pluginID)
{
	OP_Node *res = &matNode;

	SHOP_Node *shopNode = CAST_SHOPNODE(res);
	if (shopNode) {
		UT_ValArray<OP_Node*> opsByName;
		if (shopNode->getOpsByName("vray_material_output", opsByName)) {
			OP_Node *node = opsByName(0);
			if (node) {
				const int socketIdx = node->getInputFromName(socketName);
				res = node->getInput(socketIdx);
			}
		}
	}

	VOP_Node *vopNode = CAST_VOPNODE(res);
	if (vopNode && pluginID && vopNode->getOperator()->getName().startsWith("VRayNode")) {
		VOP::NodeBase *vrayVopNode = static_cast<VOP::NodeBase*>(vopNode);
		if (vrayVopNode->getVRayPluginID() != pluginID) {
			return nullptr;
		}
	}

	return res;
}


static OP_Bundle* getBundleFromOpNodePrm(OP_Node &node, const char *pn, fpreal time)
{
	if (!UTisstring(pn)) {
		return nullptr;
	}

	UT_String mask;
	PRM_Parm *prm = nullptr;
	node.evalParameterOrProperty(pn, 0, time, mask, &prm);

	OP_Network *opcreator = nullptr;
	const char *opfilter = nullptr;

	if (prm && prm->getSparePtr()) {
		const PRM_SpareData	&prmSpareData = *prm->getSparePtr();
		opcreator = UTverify_cast<OP_Network*>(getOpNodeFromPath(prmSpareData.getOpRelative()));
		opfilter = prmSpareData.getOpFilter();
	}

	if (!opcreator) {
		opcreator = node.getCreator();
	}

	UT_String bundleName;
	bundleName.itoa(node.getUniqueId());
	bundleName.prepend(pn);

	OP_Bundle *bundle = OPgetDirector()->getBundles()->getPattern(bundleName,
																  opcreator,
																  opcreator,
																  mask,
																  opfilter,
																  0,
																  false,
																  false);

	return bundle;
}

OP_Bundle* VRayForHoudini::getActiveLightsBundle(OP_Node &rop, fpreal t)
{
	// if "sololight" parm is set ignore others
	OP_Bundle *sbundle = getBundleFromOpNodePrm(rop, "sololight", t);
	if (sbundle && sbundle->entries() > 0) {
		return sbundle;
	}

	UT_String bundleName;
	bundleName.itoa(rop.getUniqueId());
	bundleName.prepend("VRayROPLights@");

	OP_BundleList *blist = OPgetDirector()->getBundles();
	OP_Bundle *bundle = blist->getBundle(bundleName);
	if (!bundle)
		bundle = blist->createBundle(bundleName, true);
	if (!bundle)
		return bundle;

	bundle->clear();

	OP_Bundle *fbundle = getBundleFromOpNodePrm(rop, "forcelights", t);
	if (fbundle) {
		OP_NodeList list;
		fbundle->getMembers(list);
		bundle->addOpList(list);
	}

	OP_Bundle *abundle = getBundleFromOpNodePrm(rop, "alights", t);
	if (abundle) {
		OP_NodeList list;
		abundle->getMembers(list);
		for (exint i = 0; i < list.size(); ++i) {
			OBJ_Node *light = list(i)->castToOBJNode();
			if (light &&
				light->isObjectRenderable(t) &&
				light->getVisible())
			{
				UT_StringHolder name = light->getFullPath();

				int enabled = 0;
				light->evalParameterOrProperty("enabled", 0, t, enabled);
				if (enabled > 0) {
					bundle->addOp(light);
				}
			}
		}
	}

	OP_Bundle *exbundle = getBundleFromOpNodePrm(rop, "excludelights", t);
	if (exbundle) {
		OP_NodeList list;
		exbundle->getMembers(list);
		for (int i = 0; i < list.size(); ++i) {
			bundle->removeOp(list(i));
		}
	}

	return bundle;
}


OP_Bundle* VRayForHoudini::getForcedLightsBundle(OP_Node &rop, fpreal t)
{
	// If "sololight" parameter is set - ignore others.
	OP_Bundle *sbundle = getBundleFromOpNodePrm(rop, "sololight", t);
	if (sbundle && sbundle->entries() > 0) {
		return sbundle;
	}

	OP_Bundle *fbundle = getBundleFromOpNodePrm(rop, "forcelights", t);
	return fbundle;
}


OP_Bundle* VRayForHoudini::getActiveGeometryBundle(OP_Node &rop, fpreal t)
{
	OP_BundleList *blist = OPgetDirector()->getBundles();

	UT_String bundleName;
	bundleName.itoa(rop.getUniqueId());
	bundleName.prepend("VRayROPGeo@");

	OP_Bundle *bundle = blist->getBundle(bundleName);
	if (!bundle)
		bundle = blist->createBundle(bundleName, true);
	if (!bundle)
		return nullptr;

	bundle->clear();

	OP_Bundle *fbundle = getForcedGeometryBundle(rop, t);
	if (fbundle && fbundle->entries() > 0) {
		OP_NodeList list;
		fbundle->getMembers(list);
		bundle->addOpList(list);
	}

	OP_Bundle *vbundle = getBundleFromOpNodePrm(rop, "vobject", t);
	if (vbundle) {
		OP_NodeList list;
		vbundle->getMembers(list);

		for (int i = 0; i < list.size(); ++i) {
			OBJ_Node *node = CAST_OBJNODE(list(i));
			if (node &&
				node->isObjectRenderable(t) &&
				node->getVisible() )
			{
				bundle->addOp(node);
			}
		}
	}

	OP_Bundle *exbundle = getBundleFromOpNodePrm(rop, "excludeobject", t);
	if (exbundle) {
		OP_NodeList list;
		exbundle->getMembers(list);

		for (int i = 0; i < list.size(); ++i) {
			bundle->removeOp(list(i));
		}
	}

	return bundle;
}

OP_Bundle* VRayForHoudini::getForcedGeometryBundle(OP_Node &rop, fpreal t)
{
	OP_BundleList *blist = OPgetDirector()->getBundles();

	UT_String bundleName;
	bundleName.itoa(rop.getUniqueId());
	bundleName.prepend("VRayROPForcedGeo@");

	OP_Bundle *bundle = blist->getBundle(bundleName);
	if (!bundle)
		bundle = blist->createBundle(bundleName, true);
	if (!bundle)
		return nullptr;

	bundle->clear();

	OP_Bundle *fbundle = getBundleFromOpNodePrm(rop, "forceobject", t);
	if (fbundle && fbundle->entries() > 0) {
		OP_NodeList list;
		fbundle->getMembers(list);
		bundle->addOpList(list);
	}

	fbundle = getMatteGeometryBundle(rop, t);
	if (fbundle && fbundle->entries() > 0) {
		OP_NodeList list;
		fbundle->getMembers(list);
		bundle->addOpList(list);
	}

	fbundle = getPhantomGeometryBundle(rop, t);
	if (fbundle && fbundle->entries() > 0) {
		OP_NodeList list;
		fbundle->getMembers(list);
		bundle->addOpList(list);
	}

	return bundle;
}

OP_Bundle* VRayForHoudini::getMatteGeometryBundle(OP_Node &rop, fpreal t)
{
	return getBundleFromOpNodePrm(rop, "matte_objects", t);
}

OP_Bundle* VRayForHoudini::getPhantomGeometryBundle(OP_Node &rop, fpreal t)
{
	return getBundleFromOpNodePrm(rop, "phantom_objects", t);
}
