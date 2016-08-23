//
// Copyright (c) 2015-2016, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#include "vfh_export_primitive.h"
#include "gu_volumegridref.h"

#include "vfh_vray.h"
#include "vfh_exporter.h"
#include "vfh_material_override.h"
#include "vfh_export_primitive.h"

#include "gu_volumegridref.h"
#include "rop/vfh_rop.h"
#include "sop/sop_node_base.h"
#include "vop/vop_node_base.h"
#include "vop/material/vop_MaterialOutput.h"
#include "vop/material/vop_PhoenixSim.h"

using namespace VRayForHoudini;

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

	exportSim(prim, VRayExporter::getObjTransform(&m_object, m_context), m_exporter.exportPlugin(nodeDesc));
}

void VolumeExporter::exportSim(const GA_Primitive &prim, const VRay::Transform &tm, const VRay::Plugin &cache)
{
	GA_ROHandleS mtlpath(prim.getDetail().findAttribute(GA_ATTRIB_PRIMITIVE, GEO_STD_ATTRIB_MATERIAL));
	// TODO: add overrides
	// GA_ROHandleS mtlo(prim.getDetail().findAttribute(GA_ATTRIB_PRIMITIVE, "material_override"));

	auto path = mtlpath.isValid() ? mtlpath.get(prim.getMapOffset()) : nullptr;
	SHOP_Node *shop = mtlpath.isValid() ? OPgetDirector()->findSHOPNode(path) : nullptr;

	if (!shop) {
		shop = m_exporter.getObjMaterial(&m_object, m_context.getTime());
	}

	if (!shop) {
		Log::getLog().error("No simulation attached to \"%s\"", cache.getName());
		return;
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