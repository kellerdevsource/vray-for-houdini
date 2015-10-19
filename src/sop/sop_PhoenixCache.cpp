//
// Copyright (c) 2015, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#ifdef CGR_HAS_AUR

#include "sop_PhoenixCache.h"
#include "vfh_prm_templates.h"
#include "vfh_prm_json.h"

#include "windows-types.h"
#include "CacheIO.h"
#include "aura_enumfiles.h"
#include "FluidInterface.h"

#include <GU/GU_PrimVolume.h>
#include <GEO/GEO_Primitive.h>


#define GetCellIndex(x, y, z, res) (x + y * res[0] + z * res[1] * res[0])


using namespace VRayForHoudini;


static PRM_Name           AttrTabsSwitcher("PhxShaderCache");
static Parm::PRMDefList   AttrTabsSwitcherTitles;
static AttributesTabs     AttrTabs;
static Parm::PRMTmplList  AttrItems;


SOP::FluidCache  SOP::PhxShaderCache::FluidFiles;


PRM_Template* SOP::PhxShaderCache::GetPrmTemplate()
{
	if (AttrItems.size()) {
		return &AttrItems[0];
	}

	AttrTabs.push_back(AttributesTab("Cache",
									 "PhxShaderCache",
									 Parm::GeneratePrmTemplate("EFFECT", "PhxShaderCache", true, true, "SOP")));
	AttrTabs.push_back(AttributesTab("Simulation",
									 "PhxShaderSim",
									 Parm::GeneratePrmTemplate("EFFECT", "PhxShaderSim", true, true, "SOP")));

	// TODO: Move to function
	//
	for (const auto &tab : AttrTabs) {
		PRM_Template *prm = tab.items;
		int           prm_count = 0;
		while (prm->getType() != PRM_LIST_TERMINATOR) {
			prm_count++;
			prm++;
		}

		AttrTabsSwitcherTitles.push_back(PRM_Default(prm_count, tab.label.c_str()));
		for (int i = 0; i < prm_count; ++i) {
			AttrItems.push_back(tab.items[i]);
		}
	}

	AttrItems.insert(AttrItems.begin(),
					 PRM_Template(PRM_SWITCHER,
								  AttrTabsSwitcherTitles.size(),
								  &AttrTabsSwitcher,
								  &AttrTabsSwitcherTitles[0]));

	return &AttrItems[0];
}


void SOP::PhxShaderCache::setPluginType()
{
	pluginType = "GEOMETRY";
	pluginID   = "CustomPhxShaderCache";
}


OP_NodeFlags &SOP::PhxShaderCache::flags()
{
	OP_NodeFlags &flags = SOP_Node::flags();
	flags.setTimeDep(true);
	return flags;
}


SOP::FluidFrame* SOP::FluidCache::getData(const char *filePath, const int fluidResample)
{
	if (m_frames.find(filePath) != m_frames.end()) {
		return &m_frames[filePath];
	}

	CacheIOCommon::FrameData result;
	if (CacheIO::FrameGetInfo(filePath, &result)) {
		PRINT_INFO("Loading file \"%s\" [%i x %i x %i]...",
				   filePath, result.gridDimensions[0], result.gridDimensions[1], result.gridDimensions[2]);

		IFluidCore::VisualData vd;
		vd.Import(filePath);

		float *grid = vd.ExpandChannel(IFluidCore::ChSm);
		if (grid) {
			FluidFrame &fluidFrame = m_frames[filePath];

			if (fluidResample) {
				for (int i = 0; i < 3; ++i) {
					fluidFrame.size[i] = result.gridDimensions[i] > fluidResample ? fluidResample : result.gridDimensions[i];
				}
			}
			else {
				for (int i = 0; i < 3; ++i) {
					fluidFrame.size[i] = result.gridDimensions[i];
				}
			}

			try {
				fluidFrame.data = new float[fluidFrame.size[0] * fluidFrame.size[1] * fluidFrame.size[2]];
			}
			catch (...) {
				fluidFrame.data = nullptr;
			}

			if (fluidFrame.data) {
				for (int i = 0; i < fluidFrame.size[0]; ++i) {
					for (int j = 0; j < fluidFrame.size[1]; ++j) {
						for (int k = 0; k < fluidFrame.size[2]; ++k) {
							const int x = i * result.gridDimensions[0] / fluidFrame.size[0];
							const int y = j * result.gridDimensions[1] / fluidFrame.size[1];
							const int z = k * result.gridDimensions[2] / fluidFrame.size[2];

							const int fluidCell = GetCellIndex(x, y, z, result.gridDimensions);
							const int dataCell  = GetCellIndex(i, j, k, fluidFrame.size);

							fluidFrame.data[dataCell] = grid[fluidCell];
						}
					}
				}

				fluidFrame.c2n = result.n2c;
				fluidFrame.c2n.m[0][0] /= 0.5f * result.gridDimensions[0];
				fluidFrame.c2n.m[1][1] /= 0.5f * result.gridDimensions[1];
				fluidFrame.c2n.m[2][2] /= 0.5f * result.gridDimensions[2];

				for (int c = 0; c < 3; ++c) {
					fluidFrame.c2n.offs[c] /= result.gridDimensions[c];
				}

				fluidFrame.c2n.offs -= VUtils::Vector(0.5f,0.5f,1.0f);

				return &fluidFrame;
			}
		}
	}

	return nullptr;
}


OP_ERROR SOP::PhxShaderCache::cookMySop(OP_Context &context)
{
	PRINT_INFO("%s cookMySop(%.3f)",
			   getName().buffer(), context.getTime());

	gdp->clearAndDestroy();

	const float t = context.getTime();

	const int nInputs = nConnectedInputs();
	if (nInputs) {
		if (lockInputs(context) >= UT_ERROR_ABORT) {
			return error();
		}

		for(int i = 0; i < nInputs; i++) {
			gdp->copy(*inputGeo(i, context));
			unlockInput(i);
		}

		return error();
	}

	UT_String path;
	// NOTE: Path could be time dependent!
	evalString(path, "SOPPhxShaderCache.cache_path", 0, t);
	if (path.equal("")) {
		return error();
	}

	const SOP::FluidFrame *frameData = SOP::PhxShaderCache::FluidFiles.getData(path.buffer());
	if (frameData) {
		GU_PrimVolume *volumeGdp = (GU_PrimVolume *)GU_PrimVolume::build(gdp);

		UT_VoxelArrayWriteHandleF voxelHandle = volumeGdp->getVoxelWriteHandle();

		voxelHandle->size(frameData->size[0], frameData->size[1], frameData->size[2]);

		for (int i = 0; i < frameData->size[0]; ++i) {
			for (int j = 0; j < frameData->size[1]; ++j) {
				for (int k = 0; k < frameData->size[2]; ++k) {
					voxelHandle->setValue(i, j, k, frameData->data[GetCellIndex(i, j, k, frameData->size)]);
				}
			}
		}

		VUtils::Transform c2n(frameData->c2n);

		const bool flipAxis = evalInt("SOPPhxShaderCache.flip_yz", 0, 0.0f);
		if (flipAxis) {
			VUtils::swap(c2n.m[1], c2n.m[2]);
			c2n.m[2] = -c2n.m[2];
		}
		c2n.makeInverse();

		UT_Matrix4 m4;
		VRayExporter::TransformToMatrix4(c2n, m4);
		volumeGdp->setTransform4(m4);
	}

#if UT_MAJOR_VERSION_INT < 14
	gdp->notifyCache(GU_CACHE_ALL);
#endif

	return error();
}


OP::VRayNode::PluginResult SOP::PhxShaderCache::asPluginDesc(Attrs::PluginDesc &pluginDesc, VRayExporter *exporter, OP_Node *parent)
{
	OP_Context &ctx = exporter->getContext();

	const float t = ctx.getTime();

	OP_Node     *vdbFileNode = nullptr;
	VRay::Plugin phxShaderCache;

	VRay::Transform nodeTm = VRayExporter::GetOBJTransform(parent->castToOBJNode(), ctx);

	VRay::Transform phxTm;
	phxTm.matrix.makeIdentity();

	typedef std::map<std::string, VRay::Plugin> CustomFluidData;
	CustomFluidData customFluidData;

	if (nConnectedInputs() == 1) {
		OP_Node *conNode = getInput(0);
		if (conNode->getOperator()->getName().equal("file")) {
			vdbFileNode = conNode;
		}

		if (!vdbFileNode) {
			// Export simulation data as MayaFluid
			//
			SOP_Node *conSop = conNode->castToSOPNode();

			GU_DetailHandleAutoReadLock gdl(conSop->getCookedGeoHandle(ctx));

			const GU_Detail *gdp = gdl.getGdp();
			if (NOT(gdp)) {
				PRINT_ERROR("%s: gdp is not ready!",
							getName().buffer());
			}
			else {
				GA_ROAttributeRef ref_name = gdp->findStringTuple(GA_ATTRIB_PRIMITIVE, "name");
				const GA_ROHandleS hnd_name(ref_name.getAttribute());
				if (hnd_name.isInvalid()) {
					PRINT_ERROR("%s: \"name\" attribute not found! Can't export fluid data!",
								getName().buffer());
				}
				else {
					int res[3];

					VUtils::ColorRefList vel;

					for (GA_Iterator offIt(gdp->getPrimitiveRange()); !offIt.atEnd(); offIt.advance()) {
						const GA_Offset off = *offIt;
						const GEO_Primitive *prim = gdp->getGEOPrimitive(off);

						if (prim->getTypeId() == GEO_PRIMVOLUME) {
							const GEO_PrimVolume *vol = static_cast<const GEO_PrimVolume *>(prim);
							if (vol) {
								const std::string texType = hnd_name.get(off);

								vol->getRes(res[0], res[1], res[2]);

								const int voxCount = res[0] * res[1] * res[2];

								UT_Matrix4D m4;
								vol->getTransform4(m4);
								UT_Vector3 center = vol->baryCenter();

//								phxTm matrix to convert from voxel space to object local space
//								voxel space is defined to be the 2-radius cube from (-1,-1,-1) to (1,1,1) centered at (0,0,0)
//								need to scale uniformly by 2 as for TexMayaFluid seems to span from (0,0,0) to (1,1,1)
								phxTm = VRayExporter::Matrix4ToTransform(m4);
								phxTm.offset.set(center.x(), center.y(), center.z());
#if 1
								phxTm.matrix.v0.x *= 2.0f;
								phxTm.matrix.v1.y *= 2.0f;
								phxTm.matrix.v2.z *= 2.0f;
#endif
//								phxMatchTm matrix to convert from voxel space to world space
//								needed for TexMayaFluidTransformed
//								sould match with transform for PhxShaderSim, WHY?
								VRay::Transform phxMatchTm;
								phxMatchTm.offset = nodeTm.matrix * phxTm.offset + nodeTm.offset;
								phxMatchTm.matrix = nodeTm.matrix * phxTm.matrix;

								PRINT_INFO("Volume \"%s\": %i x %i x %i",
										   texType.c_str(), res[0], res[1], res[2]);
								PRINT_APPSDK_TM("Volume tm", phxTm);

								UT_VoxelArrayReadHandleF vh = vol->getVoxelHandle();

								const bool isVelX = (texType == "vel.x");
								const bool isVelY = (texType == "vel.y");
								const bool isVelZ = (texType == "vel.z");

								if (isVelX || isVelY || isVelZ) {
									if (NOT(vel.count())) {
										vel = VUtils::ColorRefList(voxCount);
									}
								}

								VUtils::FloatRefList values(voxCount);
								for (int x = 0; x < res[0]; ++x) {
									for (int y = 0; y < res[1]; ++y) {
										for (int z = 0; z < res[2]; ++z) {
											const float &val = vh->getValue(x, y, z);
											const int   &idx = GetCellIndex(x, y, z, res);

											if (isVelX) {
												vel[idx].r = val;
											}
											else if (isVelY) {
												vel[idx].g = val;
											}
											else if (isVelZ) {
												vel[idx].b = val;
											}
											else {
												values[idx] = val;
											}
										}
									}
								}

								if (isVelX || isVelY || isVelZ) {
									continue;
								}

								const std::string primPluginNamePrefix = texType + "@";

								Attrs::PluginDesc fluidTex(conNode, "TexMayaFluid", primPluginNamePrefix);
								fluidTex.addAttribute(Attrs::PluginAttr("size_x", res[0]));
								fluidTex.addAttribute(Attrs::PluginAttr("size_y", res[1]));
								fluidTex.addAttribute(Attrs::PluginAttr("size_z", res[2]));
								fluidTex.addAttribute(Attrs::PluginAttr("values", values));

								Attrs::PluginDesc fluidTexTm(conNode, "TexMayaFluidTransformed", primPluginNamePrefix+"Tm@");
								fluidTexTm.addAttribute(Attrs::PluginAttr("fluid_tex", exporter->exportPlugin(fluidTex)));
								fluidTexTm.addAttribute(Attrs::PluginAttr("fluid_value_scale", 1.0f));
								fluidTexTm.addAttribute(Attrs::PluginAttr("object_to_world", phxMatchTm));

								VRay::Plugin fluidTexPlugin = exporter->exportPlugin(fluidTexTm);

								if (texType == "density") {
									Attrs::PluginDesc fluidTexAlpha(conNode, "PhxShaderTexAlpha", primPluginNamePrefix+"Alpha@");
									fluidTexAlpha.addAttribute(Attrs::PluginAttr("ttex", fluidTexPlugin));

									fluidTexPlugin = exporter->exportPlugin(fluidTexAlpha);
								}

								customFluidData[texType] = fluidTexPlugin;
							}
						}
					}

					if (vel.count()) {
						Attrs::PluginDesc velTexDesc(conNode, "TexMayaFluid", "vel@");
						velTexDesc.addAttribute(Attrs::PluginAttr("size_x", res[0]));
						velTexDesc.addAttribute(Attrs::PluginAttr("size_y", res[1]));
						velTexDesc.addAttribute(Attrs::PluginAttr("size_z", res[2]));
						velTexDesc.addAttribute(Attrs::PluginAttr("color_values", vel));

						Attrs::PluginDesc velTexTmDesc(conNode, "TexMayaFluidTransformed", "Vel@Tm@");
						velTexTmDesc.addAttribute(Attrs::PluginAttr("fluid_tex", exporter->exportPlugin(velTexDesc)));
						velTexTmDesc.addAttribute(Attrs::PluginAttr("fluid_value_scale", 1.0f));

						VRay::Plugin velTmTex = exporter->exportPlugin(velTexTmDesc);

						velTmTex = exporter->exportPlugin(velTexTmDesc);

						customFluidData["velocity"] = velTmTex;
					}

					Attrs::PluginDesc phxShaderCacheDesc(conNode, "PhxShaderCache");
					phxShaderCacheDesc.addAttribute(Attrs::PluginAttr("grid_size_x", (float)res[0]));
					phxShaderCacheDesc.addAttribute(Attrs::PluginAttr("grid_size_y", (float)res[1]));
					phxShaderCacheDesc.addAttribute(Attrs::PluginAttr("grid_size_z", (float)res[2]));
					exporter->setAttrsFromOpNode(phxShaderCacheDesc, this, true, "SOP");

					// Skip "cache_path" exporting
					phxShaderCacheDesc.get("cache_path")->paramType = Attrs::PluginAttr::AttrTypeIgnore;

					phxShaderCache = exporter->exportPlugin(phxShaderCacheDesc);
				}
			}
		}
	}

	UT_String path;
	// NOTE: Path could be time dependent!
	evalString(path, "SOPPhxShaderCache.cache_path", 0, t);
	if (path.equal("") && vdbFileNode) {
		vdbFileNode->evalString(path, "file", 0, t);
	}

	if (path.equal("") && NOT(phxShaderCache)) {
		PRINT_ERROR("%s: \"Cache Path\" is not set!",
					getName().buffer());
		return OP::VRayNode::PluginResultError;
	}

	// Export simulation
	//
	if (NOT(phxShaderCache)) {
		Attrs::PluginDesc phxShaderCacheDesc(this, "PhxShaderCache", "Cache@");
		phxShaderCacheDesc.addAttribute(Attrs::PluginAttr("cache_path", path.buffer()));
		exporter->setAttrsFromOpNode(phxShaderCacheDesc, this, true, "SOP");

		phxShaderCache = exporter->exportPlugin(phxShaderCacheDesc);
	}

	Attrs::PluginDesc phxShaderSimDesc(this, "PhxShaderSim", "Sim@");
	if (phxShaderCache) {
		phxShaderSimDesc.addAttribute(Attrs::PluginAttr("cache", phxShaderCache));
	}
	if (customFluidData.size()) {
		if (customFluidData.count("heat")) {
			phxShaderSimDesc.addAttribute(Attrs::PluginAttr("darg", 4));
			phxShaderSimDesc.addAttribute(Attrs::PluginAttr("dtex", customFluidData["heat"]));
		}
		if (customFluidData.count("density")) {
			phxShaderSimDesc.addAttribute(Attrs::PluginAttr("targ", 4));
			phxShaderSimDesc.addAttribute(Attrs::PluginAttr("ttex", customFluidData["density"]));
		}
		if (customFluidData.count("temperature")) {
			phxShaderSimDesc.addAttribute(Attrs::PluginAttr("earg", 4));
			phxShaderSimDesc.addAttribute(Attrs::PluginAttr("etex", customFluidData["temperature"]));
		}
		if (customFluidData.count("velocity")) {
			phxShaderSimDesc.addAttribute(Attrs::PluginAttr("varg", 2));
			phxShaderSimDesc.addAttribute(Attrs::PluginAttr("vtex", customFluidData["velocity"]));
		}
	}

	nodeTm.offset = nodeTm.matrix * phxTm.offset + nodeTm.offset;
	nodeTm.matrix = nodeTm.matrix * phxTm.matrix;

	phxShaderSimDesc.addAttribute(Attrs::PluginAttr("node_transform", nodeTm));

	exporter->setAttrsFromOpNode(phxShaderSimDesc, this, true, "SOP");

	VRay::Plugin phxShaderSim = exporter->exportPlugin(phxShaderSimDesc);
	if (phxShaderSim) {
		exporter->phxAddSimumation(phxShaderSim);
	}

	// Plugin must be created here and do nothing after
	return OP::VRayNode::PluginResultSuccess;
}

#endif // CGR_HAS_AUR
