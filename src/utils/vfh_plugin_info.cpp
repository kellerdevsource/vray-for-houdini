//
// Copyright (c) 2015-2016, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#include "vfh_plugin_info.h"
#include "vfh_prm_templates.h"
#include "vfh_prm_globals.h"

#include <hash_map.h>

using namespace VRayForHoudini;
using namespace Parm;

/// A hash map for VOP types.
typedef VUtils::HashMap<VOP_Type, true, 512, false, 32> VRayVopTypes;

/// A hash map for parameters types.
typedef VUtils::HashMap<ParmType, true, 512, false, 32> VRayParmTypes;

/// A hash map type for plugins info storage.
typedef VUtils::HashMap<VRayPluginInfo*, true, 512, false, 512> VRayPluginsInfo;

/// A mapping between "vray_type" spare and VOP_Type.
/// Used to set socket type.
static VRayVopTypes vrayVopTypes;

/// A mapping between "vray_type" spare and ParmType.
/// Used to set attribute type.
static VRayParmTypes vrayParmTypes;

/// Plugins info storage.
static struct VRayPluginsInfoRAII
	: VRayPluginsInfo
{
	~VRayPluginsInfoRAII() {
		FOR_IT(VRayPluginsInfo, it, *this) {
			delete it.data();
		}
	}
} pluginsInfo;

// XXX: Move elsewhere...
static void initVRaySettingsNames()
{
	if (RenderSettingsPlugins.empty()) {
		RenderSettingsPlugins.insert("SettingsOptions");
		RenderSettingsPlugins.insert("SettingsColorMapping");
		RenderSettingsPlugins.insert("SettingsDMCSampler");
		RenderSettingsPlugins.insert("SettingsImageSampler");
		RenderSettingsPlugins.insert("SettingsGI");
		RenderSettingsPlugins.insert("SettingsIrradianceMap");
		RenderSettingsPlugins.insert("SettingsLightCache");
		RenderSettingsPlugins.insert("SettingsDMCGI");
		RenderSettingsPlugins.insert("SettingsRaycaster");
		RenderSettingsPlugins.insert("SettingsRegionsGenerator");
		RenderSettingsPlugins.insert("SettingsOutput");
		RenderSettingsPlugins.insert("SettingsCaustics");
		RenderSettingsPlugins.insert("SettingsDefaultDisplacement");
		// RenderSettingsPlugins.insert("SettingsRTEngine");
	}

	if (RenderGIPlugins.empty()) {
		RenderGIPlugins.insert("SettingsGI");
		RenderGIPlugins.insert("SettingsLightCache");
		RenderGIPlugins.insert("SettingsIrradianceMap");
		RenderGIPlugins.insert("SettingsDMCGI");
	}
}

static void initVRayParmTypesMap()
{
	if (!vrayParmTypes.empty())
		return;

	vrayParmTypes.insert("boolean", eBool);
	vrayParmTypes.insert("int", eInt);
	vrayParmTypes.insert("enum", eEnum);
	vrayParmTypes.insert("float", eFloat);
	vrayParmTypes.insert("color", eColor);
	vrayParmTypes.insert("string", eString);
	vrayParmTypes.insert("vector", eVector);

	vrayParmTypes.insert("Texture",       eTextureColor);
	vrayParmTypes.insert("OutputTexture", eOutputTextureColor);

	vrayParmTypes.insert("TextureFloat",        eTextureFloat);
	vrayParmTypes.insert("OutputTextureFloat",  eOutputTextureFloat);

	vrayParmTypes.insert("TextureInt",          eTextureInt);
	vrayParmTypes.insert("OutputTextureInt",    eOutputTextureInt);

	vrayParmTypes.insert("TextureVector",       eTextureVector);
	vrayParmTypes.insert("OutputTextureVector", eOutputTextureVector);

	vrayParmTypes.insert("TextureTransform",       eTextureTransform);
	vrayParmTypes.insert("OutputTextureTransform", eOutputTextureTransform);

	vrayParmTypes.insert("TextureMatrix",       eTextureMatrix);
	vrayParmTypes.insert("OutputTextureMatrix", eOutputTextureMatrix);

	vrayParmTypes.insert("Plugin",       ePlugin);
	vrayParmTypes.insert("OutputPlugin", eOutputPlugin);

	vrayParmTypes.insert("PluginMaterial",       ePlugin);
	vrayParmTypes.insert("OutputPluginMaterial", eOutputPlugin);

	vrayParmTypes.insert("PluginBRDF",       ePlugin);
	vrayParmTypes.insert("OutputPluginBRDF", eOutputPlugin);
}

static void initVRayVopTypesMap()
{
	if (!vrayVopTypes.empty())
		return;

	vrayVopTypes.insert("Texture",       VOP_TYPE_COLOR);
	vrayVopTypes.insert("OutputTexture", VOP_TYPE_COLOR);

	vrayVopTypes.insert("TextureFloat",        VOP_TYPE_FLOAT);
	vrayVopTypes.insert("OutputTextureFloat",  VOP_TYPE_FLOAT);

	vrayVopTypes.insert("TextureInt",          VOP_TYPE_INTEGER);
	vrayVopTypes.insert("OutputTextureInt",    VOP_TYPE_INTEGER);

	vrayVopTypes.insert("TextureVector",       VOP_TYPE_VECTOR);
	vrayVopTypes.insert("OutputTextureVector", VOP_TYPE_VECTOR);

	vrayVopTypes.insert("TextureTransform",       VOP_TYPE_MATRIX4);
	vrayVopTypes.insert("OutputTextureTransform", VOP_TYPE_MATRIX4);

	vrayVopTypes.insert("TextureMatrix",       VOP_TYPE_MATRIX3);
	vrayVopTypes.insert("OutputTextureMatrix", VOP_TYPE_MATRIX3);

	vrayVopTypes.insert("Plugin",       VOP_TYPE_VOID);
	vrayVopTypes.insert("OutputPlugin", VOP_TYPE_VOID);

	vrayVopTypes.insert("PluginBRDF",       VOP_TYPE_BSDF);
	vrayVopTypes.insert("OutputPluginBRDF", VOP_TYPE_BSDF);

	vrayVopTypes.insert("PluginMaterial",       VOP_SURFACE_SHADER);
	vrayVopTypes.insert("OutputPluginMaterial", VOP_SURFACE_SHADER);
}

static void initVRayTypeMaps()
{
	initVRayVopTypesMap();
	initVRayParmTypesMap();
	initVRaySettingsNames();
}

/// Collect sockets from parameter's spare data.
static void initSockets(const PRMList &parmList, VRayPluginInfo &pluginInfo)
{
	for (int i = 0; i < parmList.size(); ++i) {
		const PRM_Template *parm = parmList.getPRMTemplate(i);
		if (!isVRayParm(parm))
			continue;

		const PRM_SpareData *parmSpare = parm->getSparePtr();
		if (!parmSpare)
			continue;

		const UT_String vrayType(parmSpare->getValue("vray_type"));
		if (!vrayType.isstring())
			continue;

		// Not all types are texturable.
		if (vrayVopTypes.find(vrayType.buffer()) == vrayVopTypes.end())
			continue;

		const char *vrayPluginAttr = parmSpare->getValue("vray_pluginattr");
		if (!vrayPluginAttr)
			continue;

		const char *vrayLabel = parmSpare->getValue("vray_label");
		const char *vraySocketLabel = UTisstring(vrayLabel) ? vrayLabel : vrayPluginAttr;

		SocketDesc socketDesk;
		socketDesk.label = parm->getLabel();
		socketDesk.attrName = vrayPluginAttr;
		socketDesk.attrType = vrayParmTypes[vrayType];
		socketDesk.socketLabel = vraySocketLabel;
		socketDesk.socketType  = vrayVopTypes[vrayType.buffer()];

		VRayNodeSockets &sockets = vrayType.startsWith("Output") ? pluginInfo.outputs : pluginInfo.inputs;
		sockets += socketDesk;
	}
}

/// Collect attributes from parameter's spare data.
static void initAttributes(const PRMList &parmList, VRayPluginInfo &pluginInfo)
{
	for (int i = 0; i < parmList.size(); ++i) {
		const PRM_Template *parm = parmList.getPRMTemplate(i);
		vassert(parm);

		const bool isCurve = parm->getMultiType() == PRM_MULTITYPE_RAMP_FLT;
		const bool isRamp = parm->getMultiType() == PRM_MULTITYPE_RAMP_RGB;

		const PRM_SpareData *parmSpare = parm->getSparePtr();
		if (!parmSpare)
			continue;

		// Storage key.
		const char *parmKey = parm->getToken();

		const UT_String vrayType(parmSpare->getValue("vray_type"));
		if (vrayType.isstring()) {
			// We don't store output and list attributes.
			// XXX: Refactor code that relates on output attribute info.
			if (vrayType.startsWith("Output") || vrayType.startsWith("List"))
				continue;

			vassert(vrayParmTypes.find(vrayType.buffer()) != vrayParmTypes.end());

			const char *vrayPluginAttr = parmSpare->getValue("vray_pluginattr");
			if (!UTisstring(vrayPluginAttr))
				continue;

			uint32_t attrFlags = attrFlagNone;

			// Attribute flags.
			const UT_String vrayUnits(parmSpare->getValue("vray_units"));
			if (vrayUnits.equal("radians")) {
				attrFlags |= attrFlagToRadians;
			}

			const UT_String vrayLinkedOnly(parmSpare->getValue("vray_linked_only"));
			if (vrayLinkedOnly.equal("1")) {
				attrFlags |= attrFlagLinkedOnly;
			}
			const UT_String vrayCustomHandling(parmSpare->getValue("vray_custom_handling"));
			if (vrayCustomHandling.isstring()) {
				attrFlags |= attrFlagCustomHandling;
			}

			const char *vrayLabel = parmSpare->getValue("vray_label");
			const char *uiLabel = UTisstring(vrayLabel) ? vrayLabel : parm->getLabel();

			vassert(pluginInfo.attributes.find(parmKey) == pluginInfo.attributes.end());			

			AttrDesc &attrDesc = pluginInfo.attributes[parmKey];
			attrDesc.label = uiLabel;
			attrDesc.attr = vrayPluginAttr;
			attrDesc.value.type = vrayParmTypes.find(vrayType).data();
			attrDesc.flags = attrFlags;
		}
		else if (isCurve || isRamp) {
			const UT_String rampKeys(parmSpare->getValue("rampkeys_var"));
			const UT_String rampValue(parmSpare->getValue("rampvalues_var"));
			const UT_String rampInterp(parmSpare->getValue("rampbasis_var"));

			AttrDesc &attrDesc = pluginInfo.attributes[parmKey];
			attrDesc.attr = parm->getToken();
			attrDesc.label = parm->getLabel();

			if (isRamp) {
				attrDesc.value.type = eRamp;
				if (rampValue.isstring()) {
					attrDesc.value.colorRampInfo.colors = rampValue.buffer();
				}
				if (rampKeys.isstring()) {
					attrDesc.value.colorRampInfo.positions = rampKeys.buffer();
				}
				if (rampInterp.isstring()) {
					attrDesc.value.colorRampInfo.interpolations = rampInterp.buffer();
				}
			}
			else if (isCurve) {
				attrDesc.value.type = eCurve;
				if (rampValue.isstring()) {
					attrDesc.value.curveRampInfo.values = rampValue.buffer();
				}
				if (rampKeys.isstring()) {
					attrDesc.value.curveRampInfo.positions = rampKeys.buffer();
				}
				if (rampInterp.isstring()) {
					attrDesc.value.curveRampInfo.interpolations = rampInterp.buffer();
				}
			}
		}
	}
}

static VRayPluginInfo *generatePluginInfo(const std::string &pluginID)
{
	VRayPluginInfo *pluginInfo = new VRayPluginInfo;

	PRMList prmTemplates;
	prmTemplates.addFromFile(pluginID.c_str());

	initVRayTypeMaps();

	initSockets(prmTemplates, *pluginInfo);
	initAttributes(prmTemplates, *pluginInfo);

	return pluginInfo;
}

bool VRayPluginInfo::hasAttribute(const tchar *attrName) const
{
	if (!UTisstring(attrName))
		return false;
	return attributes.find(attrName) != attributes.end();
}

const AttrDesc& VRayPluginInfo::getAttribute(const tchar *attrName) const
{
	return attributes.find(attrName).data();
}

const VRayPluginInfo* Parm::getVRayPluginInfo(const char *pluginID)
{
	VRayPluginsInfo::iterator it = pluginsInfo.find(pluginID);
	if (it == pluginsInfo.end()) {
		it = pluginsInfo.insert(pluginID, generatePluginInfo(pluginID));
	}
	return it.data();
}
