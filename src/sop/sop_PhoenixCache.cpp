//
// Copyright (c) 2015-2016, Chaos Software Ltd
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

#include <aurinterface.h>
#include <aurloader.h>

#include <GU/GU_PrimVolume.h>
#include <GEO/GEO_Primitive.h>

#include <vector>
#include <string>

using std::vector;
using std::string;

#define GetCellIndex(x, y, z, res) (x + y * res[0] + z * res[1] * res[0])

const int MAX_CHAN_MAP_LEN = 2048;

using namespace VRayForHoudini;

SOP::FluidCache  SOP::PhxShaderCache::FluidFiles;
static PRM_Template *AttrItems;


PRM_Template* SOP::PhxShaderCache::GetPrmTemplate()
{
	if (!AttrItems) {
		AttrItems = Parm::PRMList::loadFromFile(Parm::PRMList::expandUiPath("CustomPhxShaderCache.ds").c_str());
	}

	return AttrItems;
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
	FluidFrame *fluidFrame = nullptr;

	if (m_frames.find(filePath) != m_frames.end()) {
		fluidFrame = &m_frames[filePath];
	}
	else {
		IAur *pAuraInterface = newIAur(filePath);
		if (pAuraInterface) {
			if (pAuraInterface->ChannelPresent(PHX_SM)) {
				int gridDimensions[3];
				pAuraInterface->GetDim(gridDimensions);

				VUtils::Transform n2c;

				float flTransform[12];
				pAuraInterface->GetObject2GridTransform(flTransform);

				n2c.m.f[0].set(flTransform[0], flTransform[1], flTransform[2]);
				n2c.m.f[1].set(flTransform[3], flTransform[4], flTransform[5]);
				n2c.m.f[2].set(flTransform[6], flTransform[7], flTransform[8]);
				n2c.offs.set(flTransform[9], flTransform[10], flTransform[12]);

				Log::getLog().info("Loading file \"%s\" [%i x %i x %i]...",
								   filePath, gridDimensions[0], gridDimensions[1], gridDimensions[2]);

				const float *grid = pAuraInterface->ExpandChannel(PHX_SM);
				if (grid) {
					fluidFrame = &m_frames[filePath];

					for (int i = 0; i < 3; ++i) {
						fluidFrame->size[i] = (fluidResample && (gridDimensions[i] > fluidResample))
											  ? fluidResample
											  : gridDimensions[i];
					}

					try {
						fluidFrame->data = new float[fluidFrame->size[0] * fluidFrame->size[1] * fluidFrame->size[2]];
					}
					catch (...) {
						fluidFrame->data = nullptr;
					}

					if (fluidFrame->data) {
						for (int i = 0; i < fluidFrame->size[0]; ++i) {
							for (int j = 0; j < fluidFrame->size[1]; ++j) {
								for (int k = 0; k < fluidFrame->size[2]; ++k) {
									const int x = i * gridDimensions[0] / fluidFrame->size[0];
									const int y = j * gridDimensions[1] / fluidFrame->size[1];
									const int z = k * gridDimensions[2] / fluidFrame->size[2];

									const int fluidCell = GetCellIndex(x, y, z, gridDimensions);
									const int dataCell  = GetCellIndex(i, j, k, fluidFrame->size);

									fluidFrame->data[dataCell] = grid[fluidCell];
								}
							}
						}

						fluidFrame->c2n = n2c;
						fluidFrame->c2n.m[0][0] /= 0.5f * gridDimensions[0];
						fluidFrame->c2n.m[1][1] /= 0.5f * gridDimensions[1];
						fluidFrame->c2n.m[2][2] /= 0.5f * gridDimensions[2];

						for (int c = 0; c < 3; ++c) {
							fluidFrame->c2n.offs[c] /= gridDimensions[c];
						}

						fluidFrame->c2n.offs -= VUtils::Vector(0.5f,0.5f,1.0f);

					}
				}
			}

			deleteIAur(pAuraInterface);
		}
	}

	return fluidFrame;
}

string getDefaultMapping(const char *cachePath) {
	char buff[MAX_CHAN_MAP_LEN];
	if (1 == aurGenerateDefaultChannelMappings(buff, MAX_CHAN_MAP_LEN, cachePath)) {
		return string(buff);
	} else {
		return "";
	}
}


OP_ERROR SOP::PhxShaderCache::cookMySop(OP_Context &context)
{
	Log::getLog().info("%s cookMySop(%.3f)",
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
	evalString(path, "PhxShaderCache_cache_path", 0, t);
	if (path.equal("")) {
		return error();
	}

	m_serializedChannels.clear();

	if (!path.endsWith(".aur")) {
		int chanIndex = 0, isChannelVector3D;
		char chanName[MAX_CHAN_MAP_LEN];
		const char *cachePath = path.buffer();
		while(1 == aurGet3rdPartyChannelName(chanName, MAX_CHAN_MAP_LEN, &isChannelVector3D, cachePath, chanIndex++)) {
			m_serializedChannels.append(chanName);
		}
	
		if (!m_serializedChannels.size()) {
			addError(SOP_MESSAGE, (std::string("Did not load any channel names from file ") + cachePath).c_str());
			return error();
		} else {
			if (!this->gdp->setDetailAttributeS("vray_phx_channels", m_serializedChannels)) {
				Log::getLog().error("Failed to set channel names to geom detail");
			}
		}
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

		const bool flipAxis = evalInt("flip_yz", 0, 0.0f);
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


OP::VRayNode::PluginResult SOP::PhxShaderCache::asPluginDesc(Attrs::PluginDesc &pluginDesc, VRayExporter &exporter, ExportContext *parentContext)
{
	OP_Context &ctx = exporter.getContext();

	const float t = ctx.getTime();

	OP_Node     *vdbFileNode = nullptr;
	VRay::Plugin phxShaderCache;

	VRay::Transform nodeTm = VRayExporter::getObjTransform(parentContext->getTarget()->castToOBJNode(), ctx);

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
				Log::getLog().error("%s: gdp is not ready!",
									getName().buffer());
			}
			else {
				GA_ROAttributeRef ref_name = gdp->findStringTuple(GA_ATTRIB_PRIMITIVE, "name");
				const GA_ROHandleS hnd_name(ref_name.getAttribute());
				if (hnd_name.isInvalid()) {
					Log::getLog().error("%s: \"name\" attribute not found! Can't export fluid data!",
										getName().buffer());
				}
				else {
					int res[3];

					VRay::VUtils::ColorRefList vel;

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

								// phxTm matrix to convert from voxel space to object local space
								// Voxel space is defined to be the 2-radius cube from (-1,-1,-1) to (1,1,1) centered at (0,0,0)
								// Need to scale uniformly by 2 as for TexMayaFluid seems to span from (0,0,0) to (1,1,1)
								phxTm = VRayExporter::Matrix4ToTransform(m4);
								phxTm.offset.set(center.x(), center.y(), center.z());
								phxTm.matrix.v0.x *= 2.0f;
								phxTm.matrix.v1.y *= 2.0f;
								phxTm.matrix.v2.z *= 2.0f;

								// phxMatchTm matrix to convert from voxel space to world space
								// Needed for TexMayaFluidTransformed
								// Should match with transform for PhxShaderSim (?)
								VRay::Transform phxMatchTm;
								phxMatchTm.offset = nodeTm.matrix * phxTm.offset + nodeTm.offset;
								phxMatchTm.matrix = nodeTm.matrix * phxTm.matrix;

								Log::getLog().debug("Volume \"%s\": %i x %i x %i",
													texType.c_str(), res[0], res[1], res[2]);

								UT_VoxelArrayReadHandleF vh = vol->getVoxelHandle();

								const bool isVelX = (texType == "vel.x");
								const bool isVelY = (texType == "vel.y");
								const bool isVelZ = (texType == "vel.z");

								if (isVelX || isVelY || isVelZ) {
									if (NOT(vel.count())) {
										vel = VRay::VUtils::ColorRefList(voxCount);
									}
								}

								VRay::VUtils::FloatRefList values(voxCount);
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

								const std::string primPluginNamePrefix = texType + "|";

								Attrs::PluginDesc fluidTex(VRayExporter::getPluginName(conNode, primPluginNamePrefix), "TexMayaFluid");
								fluidTex.addAttribute(Attrs::PluginAttr("size_x", res[0]));
								fluidTex.addAttribute(Attrs::PluginAttr("size_y", res[1]));
								fluidTex.addAttribute(Attrs::PluginAttr("size_z", res[2]));
								fluidTex.addAttribute(Attrs::PluginAttr("values", values));

								Attrs::PluginDesc fluidTexTm(VRayExporter::getPluginName(conNode, primPluginNamePrefix+"Tm"), "TexMayaFluidTransformed");
								fluidTexTm.addAttribute(Attrs::PluginAttr("fluid_tex", exporter.exportPlugin(fluidTex)));
								fluidTexTm.addAttribute(Attrs::PluginAttr("fluid_value_scale", 1.0f));
								fluidTexTm.addAttribute(Attrs::PluginAttr("object_to_world", phxMatchTm));

								VRay::Plugin fluidTexPlugin = exporter.exportPlugin(fluidTexTm);

								if (texType == "density") {
									Attrs::PluginDesc fluidTexAlpha(VRayExporter::getPluginName(conNode, primPluginNamePrefix+"Alpha"), "PhxShaderTexAlpha");
									fluidTexAlpha.addAttribute(Attrs::PluginAttr("ttex", fluidTexPlugin));

									fluidTexPlugin = exporter.exportPlugin(fluidTexAlpha);
								}

								customFluidData[texType] = fluidTexPlugin;
							}
						}
					}

					if (vel.count()) {
						Attrs::PluginDesc velTexDesc(VRayExporter::getPluginName(conNode, "vel"), "TexMayaFluid");
						velTexDesc.addAttribute(Attrs::PluginAttr("size_x", res[0]));
						velTexDesc.addAttribute(Attrs::PluginAttr("size_y", res[1]));
						velTexDesc.addAttribute(Attrs::PluginAttr("size_z", res[2]));
						velTexDesc.addAttribute(Attrs::PluginAttr("color_values", vel));

						Attrs::PluginDesc velTexTmDesc(VRayExporter::getPluginName(conNode, "Vel@Tm@"), "TexMayaFluidTransformed");
						velTexTmDesc.addAttribute(Attrs::PluginAttr("fluid_tex", exporter.exportPlugin(velTexDesc)));
						velTexTmDesc.addAttribute(Attrs::PluginAttr("fluid_value_scale", 1.0f));

						VRay::Plugin velTmTex = exporter.exportPlugin(velTexTmDesc);

						velTmTex = exporter.exportPlugin(velTexTmDesc);

						customFluidData["velocity"] = velTmTex;
					}

					Attrs::PluginDesc phxShaderCacheDesc(VRayExporter::getPluginName(conNode), "PhxShaderCache");
					phxShaderCacheDesc.addAttribute(Attrs::PluginAttr("grid_size_x", (float)res[0]));
					phxShaderCacheDesc.addAttribute(Attrs::PluginAttr("grid_size_y", (float)res[1]));
					phxShaderCacheDesc.addAttribute(Attrs::PluginAttr("grid_size_z", (float)res[2]));
					exporter.setAttrsFromOpNodePrms(phxShaderCacheDesc, this, "PhxShaderCache_");

					// Skip "cache_path" exporting
					phxShaderCacheDesc.get("cache_path")->paramType = Attrs::PluginAttr::AttrTypeIgnore;

					phxShaderCache = exporter.exportPlugin(phxShaderCacheDesc);
				}
			}
		}
	}

	UT_String path;
	// NOTE: Path could be time dependent!
	evalString(path, "PhxShaderCache_cache_path", 0, t);
	if (path.equal("") && vdbFileNode) {
		vdbFileNode->evalString(path, "file", 0, t);
	}

	if (path.equal("") && NOT(phxShaderCache)) {
		Log::getLog().error("%s: \"Cache Path\" is not set!",
							getName().buffer());
		return OP::VRayNode::PluginResultError;
	}

	const auto evalTime = exporter.getContext().getTime();
	const auto evalThread = exporter.getContext().getThread();
	auto getParamIntValue = [&evalTime, &evalThread](const PRM_Parm * param) -> int32 {
		int32 val = 0;
		if (param) {
			param->getValue(evalTime, val, 0, evalThread);
		}
		return val;
	};


	// Export simulation
	//
	if (NOT(phxShaderCache)) {
		Attrs::PluginDesc phxShaderCacheDesc(VRayExporter::getPluginName(this, "Cache"), "PhxShaderCache");
		phxShaderCacheDesc.addAttribute(Attrs::PluginAttr("cache_path", path.buffer()));
		// channel mappings
		if (!path.endsWith(".aur")) {
			const int chCount = 9;
			static const char *chNames[chCount] = {"channel_smoke", "channel_temp", "channel_fuel", "channel_vel_x", "channel_vel_y", "channel_vel_z", "channel_red", "channel_green", "channel_blue"};
			static const int   chIDs[chCount] = {2, 1, 10, 4, 5, 6, 7, 8, 9};

			// will hold names so we can use pointers to them
			std::vector<UT_String> names;
			std::vector<int> ids;
			for (int c = 0; c < chCount; ++c) {
				const PRM_Parm * parameter = Parm::getParm(*this, chNames[c]);
				if (!parameter) {
					Log::getLog().error("Channel selector %s missing from UI", chNames[c]);
				} else {
					UT_String value(UT_String::ALWAYS_DEEP);
					this->evalString(value, parameter->getToken(), 0, 0.0f);
					if (value != "0") {
						names.push_back(value);
						ids.push_back(chIDs[c]);
					}
				}
			}

			const char * inputNames[chCount] = {0};
			for (int c = 0; c < names.size(); ++c) {
				inputNames[c] = names[c].c_str();
			}

			char usrchmap[MAX_CHAN_MAP_LEN] = {0,};
			if (1 == aurComposeChannelMappingsString(usrchmap, MAX_CHAN_MAP_LEN, ids.data(), const_cast<char * const *>(inputNames), names.size())) {
				phxShaderCacheDesc.addAttribute(Attrs::PluginAttr("usrchmap", usrchmap));
			}
		}

		exporter.setAttrsFromOpNodePrms(phxShaderCacheDesc, this, "");
		phxShaderCache = exporter.exportPlugin(phxShaderCacheDesc);
	}

	Attrs::PluginDesc phxShaderSimDesc(VRayExporter::getPluginName(this, "Sim"), "PhxShaderSim");
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

	enum RenderType {
		Volumetric  = 0,
		Volumetric_Geometry  = 1,
		Volumetric_Heat_Haze  = 2,
		Isosurface  = 3,
		Mesh  = 4,
	} rendMode;

	// renderMode
	const PRM_Parm *renderMode = Parm::getParm(*this, "renderMode");
	rendMode = static_cast<RenderType>(getParamIntValue(renderMode));
	phxShaderSimDesc.addAttribute(Attrs::PluginAttr("geommode", rendMode == Volumetric_Geometry || rendMode == Volumetric_Heat_Haze || rendMode == Isosurface));
	phxShaderSimDesc.addAttribute(Attrs::PluginAttr("mesher", rendMode == Mesh));
	phxShaderSimDesc.addAttribute(Attrs::PluginAttr("rendsolid", rendMode == Isosurface));
	phxShaderSimDesc.addAttribute(Attrs::PluginAttr("heathaze", rendMode == Volumetric_Heat_Haze));

	const auto primVal = getParamIntValue(Parm::getParm(*this, "pmprimary"));
	const bool enableProb = (exporter.isIPR() && primVal) || primVal == 2;
	phxShaderSimDesc.addAttribute(Attrs::PluginAttr("pmprimary", enableProb));

	const PRM_Parm *dynGeom = Parm::getParm(*this, "dynamic_geometry");
	const bool dynamic_geometry = getParamIntValue(dynGeom) == 1;

	exporter.setAttrsFromOpNodePrms(phxShaderSimDesc, this, "", true);
	VRay::Plugin phxShaderSim = exporter.exportPlugin(phxShaderSimDesc);
	if (phxShaderSim) {
		if (rendMode == Volumetric) {
			// merge all volumetrics
			exporter.phxAddSimumation(phxShaderSim);
		} else {
			const bool isMesh = rendMode == Mesh;

			const char *wrapperType = isMesh ? "PhxShaderSimMesh" : "PhxShaderSimGeom";
			Attrs::PluginDesc phxWrapper(VRayExporter::getPluginName(this, "", "Wrapper"), wrapperType);
			phxWrapper.add(Attrs::PluginAttr("phoenix_sim", phxShaderSim));
			VRay::Plugin phxWrapperPlugin = exporter.exportPlugin(phxWrapper);

			if (!isMesh) {
				// make static mesh that wraps the geom plugin
				Attrs::PluginDesc meshWrapper(VRayExporter::getPluginName(this, "", "Geom"), "GeomStaticMesh");
				meshWrapper.add(Attrs::PluginAttr("static_mesh", phxWrapperPlugin));
				meshWrapper.add(Attrs::PluginAttr("dynamic_geometry", dynamic_geometry));
			}
		}
	}

	// Plugin must be created here and do nothing after
	return OP::VRayNode::PluginResultSuccess;
}

#endif // CGR_HAS_AUR
