//
// Copyright (c) 2015-2018, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#include "obj_node_base.h"

#include "vfh_export_geom.h"
#include "vfh_attr_utils.h"

using namespace VRayForHoudini;

PRM_Template* OBJ::VRayClipper::GetPrmTemplate()
{
	static Parm::PRMList myPrmList;
	if (myPrmList.empty()) {
		PRM_Template *objprmlist = OBJ_Geometry::getTemplateList(OBJ_PARMS_PLAIN);
		myPrmList.addFromPRMTemplate(objprmlist);

		if (myPrmList.empty()) {
			myPrmList.switcherBegin("stdswitcher");
		}

		// add plugin params
		myPrmList.addFolder("V-Ray Clipper");
		Parm::addPrmTemplateForPlugin( getVRayPluginIDName(VRayPluginID::VRayClipper), myPrmList);

		myPrmList.switcherEnd();
	}

	return myPrmList.getPRMTemplate();
}

OP::VRayNode::PluginResult OBJ::VRayClipper::asPluginDesc(Attrs::PluginDesc &pluginDesc, VRayExporter& /*exporter*/, ExportContext* /*parentContext*/)
{
	pluginDesc.pluginID   = pluginID;
	pluginDesc.pluginName = VRayExporter::getPluginName(*this);

	return OP::VRayNode::PluginResultContinue;
}

void OBJ::VRayClipper::setPluginType()
{
	pluginType = VRayPluginType::GEOMETRY;
	pluginID = getVRayPluginIDName(VRayPluginID::VRayClipper);
	pluginIntID = static_cast<int>(VRayPluginID::VRayClipper);
}

namespace VRayForHoudini {
namespace OBJ {

template< VRayPluginID PluginID >
PRM_Template* LightNodeBase< PluginID >::GetPrmTemplate()
{
	static Parm::PRMList myPrmList;
	if (myPrmList.empty()) {
		PRM_Template *objprmlist = OBJ_Light::getTemplateList(OBJ_PARMS_PLAIN);
		myPrmList.addFromPRMTemplate(objprmlist);

		if (myPrmList.empty()) {
			myPrmList.switcherBegin("stdswitcher");
		}

		for (int i = 0; i < myPrmList.size(); ++i) {
			PRM_Template *tmpl = myPrmList.getPRMTemplate(i);
			UT_ASSERT( tmpl );

			// skip switcher parameters
			if (tmpl->getType() == PRM_SWITCHER) {
				continue;
			}

			int switcher = -1;
			int folder = -1;
			PRM_Template::getEnclosingSwitcherFolder( myPrmList.getPRMTemplate(), i, switcher, folder);
			if (   switcher == 0
				&& folder == 0 ) {
				// skip parameters from Transform folder
				continue;
			}

			// adjust visibility
			const UT_StringRef prmName(tmpl->getToken());
			if (prmName == "dimmer") {
				tmpl->setInvisible(true);
			}
		}

		// add plugin params
		myPrmList.addFolder("V-Ray Light");
		GetMyPrmTemplate(myPrmList);

		myPrmList.switcherEnd();
	}

	return myPrmList.getPRMTemplate();
}

template< VRayPluginID PluginID >
int LightNodeBase< PluginID >::GetMyPrmTemplate(Parm::PRMList &myPrmList)
{
	const int idx = myPrmList.size();
	Parm::addPrmTemplateForPlugin( getVRayPluginIDName(PluginID), myPrmList);

	return myPrmList.size() - idx;
}

template<VRayPluginID PluginID>
OP::VRayNode::PluginResult LightNodeBase<PluginID>::asPluginDesc(Attrs::PluginDesc &pluginDesc, VRayExporter& /*exporter*/, ExportContext* /*parentContext*/)
{
	pluginDesc.pluginID   = pluginID;
	pluginDesc.pluginName = VRayExporter::getPluginName(*this);

	return OP::VRayNode::PluginResultContinue;
}

template <VRayPluginID PluginID>
void LightNodeBase<PluginID>::setPluginType()
{
	pluginType = VRayPluginType::LIGHT;
	pluginID = getVRayPluginIDName(PluginID);
	pluginIntID = static_cast<int>(PluginID);
}

// explicitly instantiate CustomPrmTemplates for LightDome op node
template<>
int LightNodeBase<VRayPluginID::LightDome>::GetMyPrmTemplate(Parm::PRMList &myPrmList)
{
	// filter default params and hide unnecessery ones
	for (int i = 0; i < myPrmList.size(); ++i) {
		PRM_Template *tmpl = myPrmList.getPRMTemplate(i);
		UT_ASSERT( tmpl );

		int switcher = -1;
		int folder = -1;
		PRM_Template::getEnclosingSwitcherFolder(myPrmList.getPRMTemplate(), i, switcher, folder);
		if (   switcher == 0
			&& folder == 0 )
		{
			// hide some parameters from Transform folder
			const UT_StringRef prmName(tmpl->getToken());
			if (   prmName != "xOrd"
				&& prmName != "rOrd"
				&& prmName != "r"
				&& prmName != "lookatpath"
				&& prmName != "lookup" )
			{
				tmpl->setInvisible(true);
			}
		}
	}

	const int idx = myPrmList.size();
	Parm::addPrmTemplateForPlugin(getVRayPluginIDName(VRayPluginID::LightDome), myPrmList);

	return myPrmList.size() - idx;
}

int isMeshLightSupportedGeometryType(const VRay::Plugin &geometry) {
	const UT_String geometryStaticMesh(geometry.getType());
	
	if (geometryStaticMesh.equal("GeomStaticMesh")) {
		return 1;
	}

	return 0; // Isn't a supported type
}

static int fillLightPluginDesc(Attrs::PluginDesc &pluginDesc, OP_Node &objLight, const InstancerItem &item, const VRay::Transform &objTm) {
	if (item.geometry.isEmpty() || !isMeshLightSupportedGeometryType(item.geometry)) {
		Log::getLog().warning("Unsupported geometry type for Mesh Light: %s ! Node name: %s",
		                      item.geometry.getType(), qPrintable(pluginDesc.pluginName));
		return 0;
	}

	pluginDesc.add(Attrs::PluginAttr("geometry", item.geometry));
	pluginDesc.add(Attrs::PluginAttr("transform", objTm * item.tm));
	if (item.objectID != objectIdUndefined) {
		pluginDesc.add(Attrs::PluginAttr("objectID", item.objectID));
	}
	pluginDesc.add(Attrs::PluginAttr("scene_name", VRayExporter::getSceneName(objLight)));

	return 1;
}

template<>
OP::VRayNode::PluginResult LightNodeBase<VRayPluginID::LightMesh>::asPluginDesc(Attrs::PluginDesc &pluginDesc, VRayExporter &exporter, ExportContext*)
{
	const fpreal t = exporter.getContext().getTime();

	OBJ_Node *obj_node = getOBJNodeFromAttr(*this, "geometry", t);
	if (!obj_node) {
		Log::getLog().error("Geometry node not found!");
		return PluginResultError;
	}

	OBJ_Geometry *obj_geom = obj_node->castToOBJGeometry();
	if (!obj_geom) {
		Log::getLog().error("Geometry node export failed!");
		return PluginResultError;
	}

	InstancerItems geomList;
	exporter.getObjectExporter().exportGeometry(*obj_node, geomList);

	const VRay::Transform &objTm =
		VRayExporter::getObjTransform(this, exporter.getContext());

	pluginDesc.pluginID = pluginID;
	pluginDesc.pluginName = VRayExporter::getPluginName(*this);

	if (geomList.count()) {
		const InstancerItem &item = geomList[0];
		fillLightPluginDesc(pluginDesc, *this, item, objTm);
	}

	for (int i = 1; i < geomList.count(); ++i) {
		const InstancerItem &item = geomList[i];

		const QString meshLightName =
			pluginDesc.pluginName % SL("|") % QString::number(i) + SL("|") + item.geometry.getName();

		Attrs::PluginDesc meshLightDesc(meshLightName, pluginID);
		if (!fillLightPluginDesc(meshLightDesc, *this, item, objTm)) {
			continue;
		}

		exporter.setAttrsFromOpNodePrms(meshLightDesc, this);
		exporter.exportPlugin(meshLightDesc);
	}

	return PluginResultContinue;
}

template<>
OP::VRayNode::PluginResult LightNodeBase<VRayPluginID::SunLight>::asPluginDesc(Attrs::PluginDesc &pluginDesc, VRayExporter &exporter, ExportContext* /*parentContext*/)
{
	pluginDesc.pluginID   = pluginID;
	pluginDesc.pluginName = VRayExporter::getPluginName(*this);

	pluginDesc.add(Attrs::PluginAttr("up_vector", VRay::Vector(0.f,1.f,0.f)));

	OP_Node *opNode = getOpNodeFromAttr(*this, "lookatpath", exporter.getContext().getTime());
	if (opNode) {
		OBJ_Node *targetNode = CAST_OBJNODE(opNode);
		if (targetNode) {
			const VRay::Transform &tm = VRayExporter::getObjTransform(targetNode, exporter.getContext());
			pluginDesc.add(Attrs::PluginAttr("target_transform", tm));
		}
	}

	return PluginResultContinue;
}

static const QString fmtToggle("use_%1_tex");
static const QString fmtTex("%1_tex");
static const QString fmtTexColorSpace("%1_tex_color_space");

static VRay::Plugin exportAttributeFromPathAuto(VRayExporter &exporter,
                                                const OP_Node &node,
                                                const QString &attrName,
                                                VRayExporter::DefaultMappingType mappingType,
                                                Attrs::PluginDesc &pluginDesc)
{
	const QString toggleAttrName(fmtToggle.arg(attrName));
	const QString texAttrName(fmtTex.arg(attrName));
	const QString texColorSpaceAttrName(fmtTexColorSpace.arg(attrName));

	const OP_Context &ctx = exporter.getContext();
	const fpreal t = ctx.getTime();

	if (!node.evalInt(qPrintable(toggleAttrName), 0, t))
		return VRay::Plugin();

	UT_String texPath;
	node.evalString(texPath, qPrintable(texAttrName), 0, t);

	const BitmapBufferColorSpace colorSpace =
		static_cast<BitmapBufferColorSpace>(Parm::getParmEnum(node, qPrintable(texColorSpaceAttrName), bitmapBufferColorSpaceLinear, 0.0));

	const VRay::Plugin texPlugin = exporter.exportNodeFromPathWithDefaultMapping(texPath, mappingType, colorSpace);
	if (texPlugin.isNotEmpty()) {
		pluginDesc.add(Attrs::PluginAttr(texAttrName, texPlugin));
	}

	return texPlugin;
}

template<>
OP::VRayNode::PluginResult LightNodeBase<VRayPluginID::LightDome>::asPluginDesc(Attrs::PluginDesc &pluginDesc, VRayExporter &exporter, ExportContext* /*parentContext*/)
{
	pluginDesc.pluginID = pluginID;
	pluginDesc.pluginName = VRayExporter::getPluginName(*this);
	
	const VRayExporter::DefaultMappingType domeMapping = VRayExporter::defaultMappingSpherical;

	const VRay::Plugin domeTex = exportAttributeFromPathAuto(exporter, *this, SL("dome"), domeMapping, pluginDesc);
	if (domeTex.isEmpty()) {
		pluginDesc.add(Attrs::PluginAttr(SL("use_dome_tex"), false));
	}

	exportAttributeFromPathAuto(exporter, *this, "color",       domeMapping, pluginDesc);
	exportAttributeFromPathAuto(exporter, *this, "intensity",   domeMapping, pluginDesc);
	exportAttributeFromPathAuto(exporter, *this, "shadowColor", domeMapping, pluginDesc);

	return PluginResultContinue;
}

template<>
OP::VRayNode::PluginResult LightNodeBase<VRayPluginID::LightRectangle>::asPluginDesc(Attrs::PluginDesc &pluginDesc, VRayExporter &exporter, ExportContext* /*parentContext*/)
{
	const fpreal t = exporter.getContext().getTime();

	pluginDesc.pluginID = pluginID;
	pluginDesc.pluginName = VRayExporter::getPluginName(*this);

	const VRayExporter::DefaultMappingType rectMapping = VRayExporter::defaultMappingChannel;

	const VRay::Plugin rectTex = exportAttributeFromPathAuto(exporter, *this, "rect", rectMapping, pluginDesc);
	if (rectTex.isEmpty()) {
		pluginDesc.add(Attrs::PluginAttr("use_rect_tex", false));
	}
	else {
		const int useTexAlpha = evalInt("use_rect_tex_alpha", 0, 0.0);
		const int clipTexAlpha = evalInt("use_rect_tex_alpha_clip", 0, 0.0);
		const float texAlphaOverride = evalFloat("tex_alpha", 0, t);

		Attrs::PluginDesc applyAlphaDesc(SL("AlphaCombine|") % rectTex.getName(),
		                                 SL("TexAColorOp"));
		if (!clipTexAlpha) {
			applyAlphaDesc.add(Attrs::PluginAttr("color_a", rectTex));
		}
		else {
			Attrs::PluginDesc clipAlphaDesc(SL("AlphaClip|") % rectTex.getName(),
											SL("TexAColorOp"));
			clipAlphaDesc.add(Attrs::PluginAttr("mode", 0));
			clipAlphaDesc.add(Attrs::PluginAttr("color_a", rectTex));
			if (useTexAlpha) {
				clipAlphaDesc.add(Attrs::PluginAttr("mult_a", rectTex, "out_alpha"));
			}
			else {
				clipAlphaDesc.add(Attrs::PluginAttr("mult_a", texAlphaOverride));
			}

			applyAlphaDesc.add(Attrs::PluginAttr("color_a", exporter.exportPlugin(clipAlphaDesc)));
		}

		if (useTexAlpha) {
			applyAlphaDesc.add(Attrs::PluginAttr("result_alpha", rectTex, "out_alpha"));
		}
		else {
			applyAlphaDesc.add(Attrs::PluginAttr("result_alpha", texAlphaOverride));
		}

		pluginDesc.add(Attrs::PluginAttr("rect_tex", exporter.exportPlugin(applyAlphaDesc)));
	}

	exportAttributeFromPathAuto(exporter, *this, "color",       rectMapping, pluginDesc);
	exportAttributeFromPathAuto(exporter, *this, "intensity",   rectMapping, pluginDesc);
	exportAttributeFromPathAuto(exporter, *this, "shadowColor", rectMapping, pluginDesc);

	pluginDesc.add(Attrs::PluginAttr("u_size", evalFloat("u_size", 0, t) / 2.0f));
	pluginDesc.add(Attrs::PluginAttr("v_size", evalFloat("v_size", 0, t) / 2.0f));

	return PluginResultContinue;
}

template class LightNodeBase<VRayPluginID::SunLight>;
template class LightNodeBase<VRayPluginID::LightDirect>;
template class LightNodeBase<VRayPluginID::LightAmbient>;
template class LightNodeBase<VRayPluginID::LightOmni>;
template class LightNodeBase<VRayPluginID::LightSphere>;
template class LightNodeBase<VRayPluginID::LightSpot>;
template class LightNodeBase<VRayPluginID::LightRectangle>;
template class LightNodeBase<VRayPluginID::LightMesh>;
template class LightNodeBase<VRayPluginID::LightIES>;
template class LightNodeBase<VRayPluginID::LightDome>;

} // namespace OBJ
} // namespace VRayForHoudini
