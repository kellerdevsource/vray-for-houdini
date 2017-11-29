//
// Copyright (c) 2015-2017, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#include "vfh_export_primitive.h"
#include "vfh_exporter.h"
#include "vfh_attr_utils.h"
#include "vfh_op_utils.h"

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

static OP_Node* getPhoenixShaderSimNode(OP_Node *matNode)
{
	if (!matNode)
		return nullptr;
	return getVRayNodeFromOp(*matNode, "Simulation", "PhxShaderSim");
}

void HoudiniVolumeExporter::exportPrimitive(const PrimitiveItem &item, PluginSet &pluginsSet)
{
#if 0
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


	VRay::Transform nodeTm = VRayExporter::getObjTransform(&m_object, ctx);
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

	OP_Node *matNode = m_exporter.getObjMaterial(&m_object, ctx.getTime());;

	OP_Node *phxSimNode = getPhoenixShaderSimNode(matNode);
	if (!phxSimNode) {
		Log::getLog().error("Can't find shop node for %s", phxShaderCache.getName());
	}
	else {
		exportSim(*phxSimNode, overrides, phxShaderCache.getName());
	}
#endif
}

VRay::Plugin VolumeExporter::exportVRayVolumeGridRef(OBJ_Node &objNode, const GU_PrimPacked &prim) const
{
	static boost::format phxCacheNameFmt("PhxShaderCache|%i@%s");

	UT_Options opts;
	prim.saveOptions(opts, GA_SaveMap(prim.getDetail(), nullptr));

	const VRayVolumeGridRef *vrayVolumeGridRef = UTverify_cast<const VRayVolumeGridRef*>(prim.implementation());
	const int primID = vrayVolumeGridRef->getOptions().hash();

	Attrs::PluginDesc phxCache(boost::str(phxCacheNameFmt % primID % objNode.getName().buffer()),
							   "PhxShaderCache");
	pluginExporter.setAttrsFromUTOptions(phxCache, opts);

	return pluginExporter.exportPlugin(phxCache);
}

void VolumeExporter::exportPrimitive(const PrimitiveItem &item, PluginSet &pluginsSet)
{
	const GU_PrimPacked *primPacked = UTverify_cast<const GU_PrimPacked*>(item.prim);
	UT_ASSERT_MSG(primPacked, "PhxShaderCache plugin is not set!");

	VRay::Plugin cachePlugin = exportVRayVolumeGridRef(objNode, *primPacked);
	if (!cachePlugin) {
		UT_ASSERT_MSG(primPacked, "PhxShaderCache plugin is not exported!");
		return;
	}

	OP_Node *matNode = item.primMaterial.matNode;
	if (!matNode) {
		matNode = pluginExporter.getObjMaterial(&objNode, ctx.getTime());
	}

	OP_Node *phxSimNode = getPhoenixShaderSimNode(matNode);
	if (!phxSimNode) {
		Log::getLog().error("Can't find PhxShaderSim node for %s",
							objNode.getName().buffer());
		return;
	}

	VOP_Node *simVop = CAST_VOPNODE(phxSimNode);
	if (!simVop) {
		Log::getLog().error("PhxShaderSim can't be casted to VOP node!");
		return;
	}

	VOP::NodeBase &phxSimVopNode = static_cast<VOP::NodeBase&>(*simVop);

	static boost::format phxSimNameFmt("PhxShaderSim|%i@%s");
	Attrs::PluginDesc phxSim(boost::str(phxSimNameFmt % primID % objNode.getName().buffer()),
							 "PhxShaderSim");

	phxSimVopNode.asPluginDesc(phxSim, pluginExporter);

	pluginExporter.setAttrsFromOpNodeConnectedInputs(phxSim, simVop);

	// Handle VOP overrides if any
	pluginExporter.setAttrsFromSHOPOverrides(phxSim, *simVop);

	phxSim.add(Attrs::PluginAttr("node_transform", tm));
	phxSim.add(Attrs::PluginAttr("cache", cachePlugin));

	const auto rendModeAttr = phxSim.get("_vray_render_mode");
	UT_ASSERT_MSG(rendModeAttr, "Trying to export PhxShaderSim without setting it's _vray_render_mode.");

	VRay::Plugin overwriteSim = pluginExporter.exportPlugin(phxSim);
	if (rendModeAttr && overwriteSim) {
		typedef VOP::PhxShaderSim::RenderMode RMode;

		const auto rendMode = static_cast<RMode>(rendModeAttr->paramValue.valInt);
		if (rendMode == RMode::Volumetric) {
			// All volumetic simulations need to be listed in one PhxShaderSimVol,
			// so different intersecting volumes can be blended correctly.
			pluginExporter.phxAddSimumation(overwriteSim);
		}
		else {
			const bool isMesh = rendMode == RMode::Mesh;

			const char *wrapperType = isMesh ? "PhxShaderSimMesh" : "PhxShaderSimGeom";
			const char *wrapperPrefix = isMesh ? "Mesh" : "Geom";
			Attrs::PluginDesc phxWrapper(VRayExporter::getPluginName(simVop, wrapperPrefix, cachePlugin.getName()), wrapperType);
			phxWrapper.add(Attrs::PluginAttr("phoenix_sim", overwriteSim));
			VRay::Plugin phxWrapperPlugin = pluginExporter.exportPlugin(phxWrapper);

			if (!isMesh) {
				// make static mesh that wraps the geom plugin
				Attrs::PluginDesc meshWrapper(VRayExporter::getPluginName(simVop, "Mesh", cachePlugin.getName()), "GeomStaticMesh");
				meshWrapper.add(Attrs::PluginAttr("static_mesh", phxWrapperPlugin));

				const auto dynGeomAttr = phxSim.get("_vray_dynamic_geometry");
				UT_ASSERT_MSG(dynGeomAttr, "Exporting PhxShaderSim inside PhxShaderSimGeom with missing _vray_dynamic_geometry");
				const bool dynamic_geometry = dynGeomAttr ? dynGeomAttr->paramValue.valInt : false;

				meshWrapper.add(Attrs::PluginAttr("dynamic_geometry", dynamic_geometry));
				phxWrapperPlugin = pluginExporter.exportPlugin(meshWrapper);
			}

			Attrs::PluginDesc node(VRayExporter::getPluginName(simVop, "Node", cachePlugin.getName()), "Node");
			node.add(Attrs::PluginAttr("geometry", phxWrapperPlugin));
			node.add(Attrs::PluginAttr("visible", true));
			node.add(Attrs::PluginAttr("transform", VRay::Transform(1)));
			node.add(Attrs::PluginAttr("material", pluginExporter.exportDefaultMaterial()));
			pluginExporter.exportPlugin(node);
		}
	}
}

#endif // CGR_HAS_AUR
