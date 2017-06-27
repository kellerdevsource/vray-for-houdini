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
#include "vfh_attr_utils.h"

#include "gu_volumegridref.h"

#include "vop/vop_node_base.h"
#include "vop/material/vop_MaterialOutput.h"
#include "vop/material/vop_PhoenixSim.h"

#include <GU/GU_PrimPacked.h>
#include <GU/GU_PrimVDB.h>
#include <SOP/SOP_Node.h>
#include <SHOP/SHOP_Node.h>
#include <GEO/GEO_Primitive.h>
#include <GU/GU_PrimVolume.h>
#include <GU/GU_Detail.h>
#include <GA/GA_SaveMap.h>
#include <GA/GA_Names.h>

using namespace VRayForHoudini;

#ifdef CGR_HAS_AUR

namespace {

/// Wrapper over GEO_PrimVolume and GEO_PrimVDB providing common interface
/// this wrapper *can* be INVALID - it can be initialized with unsuported primitive
/// the wrapper *must* be checkked before use with its bool operator
/// methods called on INVALID wrapper will return default (0/1) initialized data
struct VolumeProxy {
	/// Create a proxy from a primitive, could be nyllptr or not supported primitive
	VolumeProxy(const GEO_Primitive *prim): m_prim(prim), m_vol(nullptr), m_vdb(nullptr) {
		if (!prim) {
			return;
		} else if (prim->getTypeId() == GEO_PRIMVOLUME) {
			m_vol = dynamic_cast<const GEO_PrimVolume *>(m_prim);
		} else if (prim->getTypeId() == GEO_PRIMVDB) {
			m_vdb = dynamic_cast<const GEO_PrimVDB *>(m_prim);
		}
	};

	/// Get the resolution in voxels
	/// @res[out] - resolution in order x, y, z
	void getRes(int res[3]) const {
		if (m_vol) {
			m_vol->getRes(res[0], res[1], res[2]);
		} else if (m_vdb) {
			m_vdb->getRes(res[0], res[1], res[2]);
		}
	}

	/// Get total number of voxels in this volume
	/// @return - voxel count
	exint voxCount() const {
		int res[3] = {0, 0, 0};
		this->getRes(res);
		return res[0] * res[1] * res[2];
	}

	/// Copy the volume data to a PTrArray
	/// @T - the output data type - Color or float
	/// @F - the type of the accesor function for the elements of PtrArray
	/// @data - data destination
	/// @acc - function called for each element in @data, should return reference to which each voxel is assigned to
	///        if @T is color, @acc should return reference to either the red or green or blue channels - used to export velocities
	template <typename T, typename F>
	void copyTo(VRay::VUtils::PtrArray<T> & data, F acc) const {
		int res[3] = {0, 0, 0};
		getRes(res);

		auto GetCellIndex = [&res](int x, int y, int z) {
			return (x + y * res[0] + z * res[1] * res[0]);
		};

		if (m_vdb) {
			auto fGrid = openvdb::gridConstPtrCast<openvdb::FloatGrid>(m_vdb->getGridPtr());
			if (!fGrid) {
				return;
			}
			auto readHandle = fGrid->getConstAccessor();
			for (int x = 0; x < res[0]; ++x) {
				for (int y = 0; y < res[1]; ++y) {
					for (int z = 0; z < res[2]; ++z) {
						const float &val = readHandle.getValue(openvdb::Coord(x, y, z));
						const int   &idx = GetCellIndex(x, y, z);
						acc(data[idx]) = val;
					}
				}
			}
		} else if (m_vol) {
			UT_VoxelArrayReadHandleF vh = m_vol->getVoxelHandle();
			for (int x = 0; x < res[0]; ++x) {
				for (int y = 0; y < res[1]; ++y) {
					for (int z = 0; z < res[2]; ++z) {
						const float &val = vh->getValue(x, y, z);
						const int   &idx = GetCellIndex(x, y, z);
						acc(data[idx]) = val;
					}
				}
			}
		}
	}

	/// Get the bary center of the volume
	/// @return - UT_Vector3 in voxel space
	UT_Vector3 getBaryCenter() const {
		if (m_vol) {
			return m_vol->baryCenter();
		} else if (m_vdb) {
			return m_vdb->baryCenter();
		}
		return UT_Vector3(0, 0, 0);
	}

	/// Get voxel space transform
	/// @return - 4d matrix
	UT_Matrix4D getTransform() const {
		UT_Matrix4D res(1);
		if (m_vol) {
			m_vol->getTransform4(res);
		} else if (m_vdb) {
			m_vdb->getSpaceTransform().getTransform4(res);
		}
		return res;
	}

	/// Check if this volume is PrimVDB
	bool isVDB() const {
		return m_vdb;
	}

	/// Check if this volume is PrimVolume
	bool isVOL() const {
		return m_vol;
	}

	/// Check if this is a valid volume
	operator bool() const {
		return m_prim && (m_vol || m_vdb);
	}

	const GEO_PrimVDB    *m_vdb; ///< pointer to the VDB primitive in this is vdb
	const GEO_PrimVolume *m_vol; ///< pointer to the VOL primitive if this is vol
	const GEO_Primitive  *m_prim; ///< pointer to the primitive passed to constructor
};
}


void HoudiniVolumeExporter::exportPrimitives(const GU_Detail &detail, InstancerItems&)
{
	bool hasExportableVolumes = false;
	for (GA_Iterator offIt(detail.getPrimitiveRange()); !offIt.atEnd(); offIt.advance()) {
		VolumeProxy vol(detail.getGEOPrimitive(*offIt));
		hasExportableVolumes = hasExportableVolumes || !!vol;
	}

	if (!hasExportableVolumes) {
		// this is not an error since we try for volumes in every detail
		return;
	}

	GA_ROAttributeRef ref_name = detail.findStringTuple(GA_ATTRIB_PRIMITIVE, "name");
	const GA_ROHandleS hnd_name(ref_name.getAttribute());
	if (hnd_name.isInvalid()) {
		SOP_Node *sop = m_object.getRenderSopPtr();
		Log::getLog().error("%s: \"name\" attribute not found! Can't export fluid data!",
							sop ? sop->getFullPath().buffer() : "UNKNOWN");
		return;
	}

	typedef std::map<std::string, VRay::Plugin> CustomFluidData;
	CustomFluidData customFluidData;
	int res[3] = {0, 0, 0};

	// will hold resolution for velocity channels as it can be different
	int velocityRes[3] = {0, 0, 0};


	VRay::Transform nodeTm = VRayExporter::getObjTransform(&m_object, m_context);
	VRay::Transform phxTm;

	VRay::VUtils::ColorRefList vel;

	// get the largest velocity count, sometimes they are not the same sizes!!
	int velVoxCount = -1;
	bool missmatchedSizes = false;
	for (GA_Iterator offIt(detail.getPrimitiveRange()); !offIt.atEnd(); offIt.advance()) {
		const GA_Offset off = *offIt;
		const GEO_Primitive *prim = detail.getGEOPrimitive(off);
		VolumeProxy vol(prim);
		const std::string texType = hnd_name.get(off);
		if (vol && (texType == "vel.x" || texType == "vel.y" || texType == "vel.z")) {
			int chRes[3];
			vol.getRes(res);
			const int voxCount = res[0] * res[1] * res[2];
			if (velVoxCount != -1 && velVoxCount != voxCount) {
				missmatchedSizes = true;
			}
			velVoxCount = std::max(voxCount, velVoxCount);
			// set max res of the 3 components
			for (int c = 0; c <3; ++c) {
				velocityRes[c] =  std::max(velocityRes[c], chRes[c]);
			}
		}
	}

	if (velVoxCount > 0) {
		vel = VRay::VUtils::ColorRefList(velVoxCount);
		if (missmatchedSizes) {
			memset(vel.get(), 0, vel.size() * sizeof(VRay::Color));
		}
	}

	UT_Matrix4 channelTm;

	// check all primitives if we can make PrimExporter for it and export it
	for (GA_Iterator offIt(detail.getPrimitiveRange()); !offIt.atEnd(); offIt.advance()) {
		const GA_Offset off = *offIt;
		const GEO_Primitive *prim = detail.getGEOPrimitive(off);

		VolumeProxy volume(prim);
		const std::string texType = hnd_name.get(off);

		// unknown primitive, or unknow volume type
		if (!volume || texType.empty()) {
			continue;
		}

		volume.getRes(res);
		const int voxCount = res[0] * res[1] * res[2];

		UT_Matrix4D m4 = volume.getTransform();
		UT_Vector3 center = volume.getBaryCenter();

		// phxTm matrix to convert from voxel space to object local space
		// Voxel space is defined to be the 2-radius cube from (-1,-1,-1) to (1,1,1) centered at (0,0,0)
		// Need to scale uniformly by 2 as for TexMayaFluid seems to span from (0,0,0) to (1,1,1)
		phxTm = utMatrixToVRayTransform(m4);
		phxTm.offset.set(center.x(), center.y(), center.z());
		phxTm.matrix.v0.x *= 2.0f;
		phxTm.matrix.v1.y *= 2.0f;
		phxTm.matrix.v2.z *= 2.0f;

		// phxMatchTm matrix to convert from voxel space to world space
		// Needed for TexMayaFluidTransformed
		// Should match with transform for PhxShaderSim (?)
		VRay::Transform phxMatchTm = nodeTm * phxTm;

		Log::getLog().debug("Volume \"%s\": %i x %i x %i",
							texType.c_str(), res[0], res[1], res[2]);

		// extract data
		if (texType == "vel.x") {
			volume.copyTo(vel, std::bind(&VRay::Color::r, std::placeholders::_1));
			continue;
		} else if (texType == "vel.y") {
			volume.copyTo(vel, std::bind(&VRay::Color::g, std::placeholders::_1));
			continue;
		} else if (texType == "vel.z") {
			volume.copyTo(vel, std::bind(&VRay::Color::b, std::placeholders::_1));
			continue;
		}

		VRay::VUtils::FloatRefList values(voxCount);
		volume.copyTo(values, [](float & c) -> float & { return c; });

		const std::string primPluginNamePrefix = texType + "|";

		Attrs::PluginDesc fluidTex(VRayExporter::getPluginName(&m_object, primPluginNamePrefix), "TexMayaFluid");
		fluidTex.addAttribute(Attrs::PluginAttr("size_x", res[0]));
		fluidTex.addAttribute(Attrs::PluginAttr("size_y", res[1]));
		fluidTex.addAttribute(Attrs::PluginAttr("size_z", res[2]));
		fluidTex.addAttribute(Attrs::PluginAttr("values", values));

		Attrs::PluginDesc fluidTexTm(VRayExporter::getPluginName(&m_object, primPluginNamePrefix+"Tm"), "TexMayaFluidTransformed");
		fluidTexTm.addAttribute(Attrs::PluginAttr("fluid_tex", m_exporter.exportPlugin(fluidTex)));
		fluidTexTm.addAttribute(Attrs::PluginAttr("fluid_value_scale", 1.0f));
		fluidTexTm.addAttribute(Attrs::PluginAttr("object_to_world", phxMatchTm));

		VRay::Plugin fluidTexPlugin = m_exporter.exportPlugin(fluidTexTm);

		if (texType == "density") {
			Attrs::PluginDesc fluidTexAlpha(VRayExporter::getPluginName(&m_object, primPluginNamePrefix+"Alpha"), "PhxShaderTexAlpha");
			fluidTexAlpha.addAttribute(Attrs::PluginAttr("ttex", fluidTexPlugin));

			fluidTexPlugin = m_exporter.exportPlugin(fluidTexAlpha);
		}

		customFluidData[texType] = fluidTexPlugin;
	}

	// write velocities if we have any
	if (vel.count()) {
		Attrs::PluginDesc velTexDesc(VRayExporter::getPluginName(&m_object, "vel"), "TexMayaFluid");
		velTexDesc.addAttribute(Attrs::PluginAttr("size_x", velocityRes[0]));
		velTexDesc.addAttribute(Attrs::PluginAttr("size_y", velocityRes[1]));
		velTexDesc.addAttribute(Attrs::PluginAttr("size_z", velocityRes[2]));
		velTexDesc.addAttribute(Attrs::PluginAttr("color_values", vel));

		Attrs::PluginDesc velTexTmDesc(VRayExporter::getPluginName(&m_object, "Vel@Tm@"), "TexMayaFluidTransformed");
		velTexTmDesc.addAttribute(Attrs::PluginAttr("fluid_tex", m_exporter.exportPlugin(velTexDesc)));
		velTexTmDesc.addAttribute(Attrs::PluginAttr("fluid_value_scale", 1.0f));

		VRay::Plugin velTmTex = m_exporter.exportPlugin(velTexTmDesc);

		velTmTex = m_exporter.exportPlugin(velTexTmDesc);

		customFluidData["velocity"] = velTmTex;
	}

	Attrs::PluginDesc phxShaderCacheDesc(VRayExporter::getPluginName(&m_object), "PhxShaderCache");
	phxShaderCacheDesc.addAttribute(Attrs::PluginAttr("grid_size_x", (float)res[0]));
	phxShaderCacheDesc.addAttribute(Attrs::PluginAttr("grid_size_y", (float)res[1]));
	phxShaderCacheDesc.addAttribute(Attrs::PluginAttr("grid_size_z", (float)res[2]));

	// Skip "cache_path" exporting - we don't have chache but texture plugins
	phxShaderCacheDesc.add(Attrs::PluginAttr("cache_path", Attrs::PluginAttr::AttrTypeIgnore));

	VRay::Plugin phxShaderCache = m_exporter.exportPlugin(phxShaderCacheDesc);

	nodeTm = nodeTm * phxTm;
	Attrs::PluginAttrs overrides;
	overrides.push_back(Attrs::PluginAttr("node_transform", nodeTm));
	overrides.push_back(Attrs::PluginAttr("cache", phxShaderCache));

	if (customFluidData.size()) {
		if (customFluidData.count("heat")) {
			overrides.push_back(Attrs::PluginAttr("darg", 4)); // 4 == Texture
			overrides.push_back(Attrs::PluginAttr("dtex", customFluidData["heat"]));
		}
		if (customFluidData.count("density")) {
			overrides.push_back(Attrs::PluginAttr("targ", 4)); // 4 == Texture
			overrides.push_back(Attrs::PluginAttr("ttex", customFluidData["density"]));
		}
		if (customFluidData.count("temperature")) {
			overrides.push_back(Attrs::PluginAttr("earg", 4)); // 4 == Texture
			overrides.push_back(Attrs::PluginAttr("etex", customFluidData["temperature"]));
		}
		if (customFluidData.count("velocity")) {
			overrides.push_back(Attrs::PluginAttr("varg", 2)); // 2 == Texture
			overrides.push_back(Attrs::PluginAttr("vtex", customFluidData["velocity"]));
		}
	}

	OP_Node *matNode = m_exporter.getObjMaterial(&m_object, m_context.getTime());;

	SHOP_Node *shopNode = CAST_SHOPNODE(matNode);
	if (!shopNode) {
		Log::getLog().error("Can't find shop node for %s", phxShaderCache.getName());
	}
	else {
		exportSim(shopNode, overrides, phxShaderCache.getName());
	}
}

void VolumeExporter::exportPrimitives(const GU_Detail &detail, InstancerItems&)
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
	UT_Options opts;
	packedPrim->saveOptions(opts, GA_SaveMap(prim.getDetail(), nullptr));
	m_exporter.setAttrsFromUTOptions(nodeDesc, opts);

	UT_Matrix4 xform;
	prim.getIntrinsic(prim.findIntrinsic("packedfulltransform"), xform);

	auto primTm = utMatrixToVRayTransform(xform);
	auto objTm = VRayExporter::getObjTransform(&m_object, m_context);
	auto cachePlugin = m_exporter.exportPlugin(nodeDesc);

	Attrs::PluginAttrs overrides;
	overrides.push_back(Attrs::PluginAttr("node_transform", objTm));
	overrides.push_back(Attrs::PluginAttr("cache", cachePlugin));

	GA_ROHandleS mtlpath(prim.getDetail().findAttribute(GA_ATTRIB_PRIMITIVE, GEO_STD_ATTRIB_MATERIAL));
	// TODO: add overrides
	// GA_ROHandleS mtlo(prim.getDetail().findAttribute(GA_ATTRIB_PRIMITIVE, "material_override"));

	OP_Node *matNode = nullptr;
	if (mtlpath.isValid()) {
		const UT_String &path = mtlpath.get(prim.getMapOffset());
		matNode = getOpNodeFromPath(path, m_context.getTime());
	}
	if (!matNode) {
		matNode = m_exporter.getObjMaterial(&m_object, m_context.getTime());
	}

	SHOP_Node *shopNode = CAST_SHOPNODE(matNode);
	if (!shopNode) {
		Log::getLog().error("Can't find shop node for %s", cachePlugin.getName());
	}
	else {
		exportSim(shopNode, overrides, cachePlugin.getName());
	}
}

void VolumeExporter::exportSim(SHOP_Node *shop, const Attrs::PluginAttrs &overrideAttrs, const std::string &cacheName)
{
	UT_ValArray<OP_Node *> mtlOutList;
	// find our output node
	if (!shop->getOpsByName("vray_material_output", mtlOutList)) {
		return;
	}

	// there is at least 1 "vray_material_output" node so take the first one
	VOP::MaterialOutput *mtlOut = static_cast< VOP::MaterialOutput * >( mtlOutList(0) );
	if (mtlOut->error() >= UT_ERROR_ABORT ) {
		return;
	}

	// find the sim node
	const int simIdx = mtlOut->getInputFromName("PhxShaderSim");
	if (auto simNode = mtlOut->getInput(simIdx)) {
		VOP_Node * simVop = simNode->castToVOPNode();
		if (!simVop) {
			Log::getLog().error("PhxShaderSim cannot be casted to VOP node!");
			UT_ASSERT_MSG(false, "PhxShaderSim cannot be casted to VOP node!");
			return;
		}

		UT_ASSERT_MSG(mtlOut->getInputType(simIdx) == VOP_ATMOSPHERE_SHADER, "PhxShaderSim's socket is not of type VOP_ATMOSPHERE_SHADER");
		Log::getLog().msg("Exporting PhxShaderSim for node \"%s\", input %d!", mtlOut->getName().buffer(), simIdx);

		VOP::NodeBase *vrayNode = static_cast<VOP::NodeBase*>(simVop);
		Attrs::PluginDesc pluginDesc;

		//TODO: is this unique enough
		pluginDesc.pluginName = VRayExporter::getPluginName(simVop, "Sim", cacheName);
		pluginDesc.pluginID   = vrayNode->getVRayPluginID();

		OP::VRayNode::PluginResult res = vrayNode->asPluginDesc(pluginDesc, m_exporter);
		m_exporter.setAttrsFromOpNodeConnectedInputs(pluginDesc, simVop);

		// handle VOP overrides if any
		m_exporter.setAttrsFromSHOPOverrides(pluginDesc, *simVop);

		for (const auto & oAttr : overrideAttrs) {
			pluginDesc.add(oAttr);
		}

		const auto rendModeAttr = pluginDesc.get("_vray_render_mode");
		UT_ASSERT_MSG(rendModeAttr, "Trying to export PhxShaderSim without setting it's _vray_render_mode.");
		VRay::Plugin overwriteSim = m_exporter.exportPlugin(pluginDesc);
		if (rendModeAttr && overwriteSim) {
			typedef VOP::PhxShaderSim::RenderMode RMode;

			const auto rendMode = static_cast<RMode>(rendModeAttr->paramValue.valInt);
			if (rendMode == RMode::Volumetric) {
				// all volumetic simulations need to be listed in one PhxShaderSimVol so different interescting
				// volumes can be blended correctly
				m_exporter.phxAddSimumation(overwriteSim);
			} else {
				const bool isMesh = rendMode == RMode::Mesh;

				const char *wrapperType = isMesh ? "PhxShaderSimMesh" : "PhxShaderSimGeom";
				const char *wrapperPrefix = isMesh ? "Mesh" : "Geom";
				Attrs::PluginDesc phxWrapper(VRayExporter::getPluginName(simVop, wrapperPrefix, cacheName), wrapperType);
				phxWrapper.add(Attrs::PluginAttr("phoenix_sim", overwriteSim));
				VRay::Plugin phxWrapperPlugin = m_exporter.exportPlugin(phxWrapper);

				if (!isMesh) {
					// make static mesh that wraps the geom plugin
					Attrs::PluginDesc meshWrapper(VRayExporter::getPluginName(simVop, "Mesh", cacheName), "GeomStaticMesh");
					meshWrapper.add(Attrs::PluginAttr("static_mesh", phxWrapperPlugin));

					const auto dynGeomAttr = pluginDesc.get("_vray_dynamic_geometry");
					UT_ASSERT_MSG(dynGeomAttr, "Exporting PhxShaderSim inside PhxShaderSimGeom with missing _vray_dynamic_geometry");
					const bool dynamic_geometry = dynGeomAttr ? dynGeomAttr->paramValue.valInt : false;

					meshWrapper.add(Attrs::PluginAttr("dynamic_geometry", dynamic_geometry));
					phxWrapperPlugin = m_exporter.exportPlugin(meshWrapper);
				}

				Attrs::PluginDesc node(VRayExporter::getPluginName(simVop, "Node", cacheName), "Node");
				node.add(Attrs::PluginAttr("geometry", phxWrapperPlugin));
				node.add(Attrs::PluginAttr("visible", true));
				node.add(Attrs::PluginAttr("transform", VRay::Transform(1)));
				node.add(Attrs::PluginAttr("material", m_exporter.exportDefaultMaterial()));
				m_exporter.exportPlugin(node);
			}
		}
	}
}

#endif // CGR_HAS_AUR
