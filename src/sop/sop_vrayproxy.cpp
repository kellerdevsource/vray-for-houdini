//
// Copyright (c) 2015-2018, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#include "sop_vrayproxy.h"
#include "gu_vrayproxyref.h"

#include "vfh_vrayproxyutils.h"
#include "vfh_prm_templates.h"

#include <parse.h>
#include <string_builder.h>

using namespace VRayForHoudini;
using namespace SOP;

/// Callback to clear cache for this node ("Reload Geometry" button in the GUI).
/// @param data Pointer to the node it was called on.
/// @param index The index of the menu entry.
/// @param t Current evaluation time.
/// @param tplate Pointer to the PRM_Template of the parameter it was triggered for.
/// @return It should return 1 if you want the dialog to refresh
/// (ie if you changed any values) and 0 otherwise.
static int cbClearCache(void *data, int index, fpreal t, const PRM_Template *tplate)
{
	OP_Node *node = reinterpret_cast<OP_Node*>(data);

	UT_String filepath;
	node->evalString(filepath, "file", 0, t);

	if (filepath.isstring()) {
#if 0
		clearVRayProxyCache(filepath);
#endif
	}

	return 0;
}

PRM_Template* VRayProxy::getPrmTemplate()
{
	static PRM_Template* myPrmList = nullptr;
	if (myPrmList) {
		return myPrmList;
	}

	myPrmList = Parm::getPrmTemplate("GeomMeshFile");

	PRM_Template* prmIt = myPrmList;
	while (prmIt && prmIt->getType() != PRM_LIST_TERMINATOR) {
		if (vutils_strcmp(prmIt->getToken(), "reload") == 0) {
			prmIt->setCallback(cbClearCache);
			break;
		}
		prmIt++;
	}

	return myPrmList;
}

VRayProxy::VRayProxy(OP_Network *parent, const char *name, OP_Operator *entry)
	: NodePackedBase("VRayProxyRef", parent, name, entry)
{
	// This indicates that this SOP manually manages its data IDs,
	// so that Houdini can identify what attributes may have changed,
	// e.g. to reduce work for the viewport, or other SOPs that
	// check whether data IDs have changed.
	// By default, (i.e. if this msg weren't here), all data IDs
	// would be bumped after the SOP cook, to indicate that
	// everything might have changed.
	// If some data IDs don't get bumped properly, the viewport
	// may not update, or SOPs that check data IDs
	// may not cook correctly, so be *very* careful!
	// XXX: Is this still required?
	// mySopFlags.setManagesDataIDs(true);
}

VRayProxy::~VRayProxy()
{}

void VRayProxy::setPluginType()
{
	pluginType = VRayPluginType::GEOMETRY;
	pluginID = SL("GeomMeshFile");
}

static int getNumVRayProxyFrames(OP_Node &owner, const UT_String &filePath)
{
	using namespace VUtils;

	MeshFile *meshFile = newDefaultMeshFile(filePath);
	if (!meshFile) {
		Log::getLog().error("\"%s\": Can't open \"%s\"!",
		                    owner.getFullPath().buffer(), filePath);
		return 0;
	}

	int numFrames = 0;

	const ErrorCode res = meshFile->init(filePath);
	if (res.error()) {
		Log::getLog().error("\"%s\": Can't initialize \"%s\" [%s]!",
		                    owner.getFullPath().buffer(), filePath, res.getErrorString().ptrOrValue("UNKNOWN"));
	}
	else {
		numFrames = meshFile->getNumFrames();
	}

	deleteDefaultMeshFile(meshFile);

	return numFrames;
}

void VRayProxy::setTimeDependent()
{
	int numFrames = 0;

	UT_String filePath;
	evalString(filePath, "file", 0, 0.0);
	if (filePath.isstring()) {
		numFrames = getNumVRayProxyFrames(*this, filePath);
	}

	if (numFrames <= 1) {
		flags().setTimeDep(false);
	}
	else {
		const VUtils::MeshFileAnimType::Enum animType =
			static_cast<VUtils::MeshFileAnimType::Enum>(evalInt("anim_type", 0, 0.0));

		flags().setTimeDep(animType != VUtils::MeshFileAnimType::Still);
	}
}

PrimWithOptions& VRayProxy::createPrimitive(const QString &name)
{
	PrimWithOptions &prim = prims[name];

	prim.prim = GU_PrimPacked::build(*gdp, m_primType);
	vassert(prim.prim);

	// Set the location of the packed primitive point.
	const UT_Vector3 pivot(0.0, 0.0, 0.0);
	prim.prim->setPivot(pivot);

	gdp->setPos3(prim.prim->getPointOffset(0), pivot);

	return prim;
}

void VRayProxy::enumObjectInfo(const VUtils::ObjectInfoChannelData &chanData, int channelID)
{
	using namespace VUtils;

	const int objectType = int(objectInfoIdToObjectType(channelID));

	for (int i = 0; i < chanData.getNumObjectInfos(); ++i) {
		const ObjectInfo &objectInfo = chanData[i];

		const CharString objectPath = objectInfo.name;

		PrimWithOptions &prim = createPrimitive(objectPath.ptr());
		prim.options.setOptionS("object_path", objectPath.ptr());
		prim.options.setOptionI("object_id", objectInfo.id);
		prim.options.setOptionI("object_type", objectType);

		GA_PrimitiveGroup *primGroup = gdp->newPrimitiveGroup(objectPath.ptr());
		vassert(primGroup);
		primGroup->add(prim.prim);
	}
}

void VRayProxy::enumMeshFile(const char *filePath)
{
	using namespace VUtils;

	if (!UTisstring(filePath))
		return;

	MeshFile *meshFile = newDefaultMeshFile(filePath);
	if (!meshFile) {
		Log::getLog().error("\"%s\": Can't open \"%s\"!",
		                    getFullPath().buffer(), filePath);
		return;
	}

	const ErrorCode res = meshFile->init(filePath);
	if (res.error()) {
		Log::getLog().error("\"%s\": Can't initialize \"%s\" [%s]!",
		                    getFullPath().buffer(), filePath, res.getErrorString().ptrOrValue("UNKNOWN"));
	}
	else {
		meshFile->setNumPreviewFaces(0);
		meshFile->setNumPreviewHairs(0);
		meshFile->setNumPreviewParticles(0);
		meshFile->setCurrentFrame(0);

		for (int i = meshFile->getNumVoxels() - 1; i >= 0; i--) {
			if (meshFile->getVoxelFlags(i) & MVF_PREVIEW_VOXEL) {
				MeshVoxel *voxel = meshFile->getVoxel(i);
				if (voxel) {
					static const int objectInfoChannelTypes[] = {
						OBJECT_INFO_CHANNEL,
						HAIR_OBJECT_INFO_CHANNEL,
						PARTICLE_OBJECT_INFO_CHANNEL,
					};

					for (int channelId : objectInfoChannelTypes) {
						ObjectInfoChannelData objectInfo;
						if (readObjectInfoChannelData(voxel, objectInfo, channelId)) {
							enumObjectInfo(objectInfo, channelId);
						}
					}

					meshFile->releaseVoxel(voxel);
				}

				break;
			}
		}
	}

	deleteDefaultMeshFile(meshFile);
}

void VRayProxy::getCreatePrimitive()
{
	prims.clear();

	gdp->stashAll();

	UT_String filePath;
	evalString(filePath, "file", 0, 0.0);
	if (filePath.isstring()) {
		enumMeshFile(filePath.buffer());

		// If OBJECT_INFO_CHANNEL was not found - add a single primitive
		// for the whole *.vrmesh file.
		if (prims.empty()) {
			createPrimitive(SL("vrmesh"));
		}
	}

	gdp->destroyStashed();
}

void VRayProxy::updatePrimitiveFromOptions(const OP_Options &options)
{
	for (PrimWithOptions &prim : prims) {
		OP_Options primOptions;
		primOptions.merge(options);
		primOptions.merge(prim.options);

		GU_PackedImpl *primImpl = prim.prim->implementation();
		if (primImpl) {
#ifdef HDK_16_5
			primImpl->update(prim.prim, primOptions);
#else
			primImpl->update(primOptions);
#endif
		}
	}
}

void VRayProxy::updatePrimitive(const OP_Context &context)
{
	const fpreal t = context.getTime();

	// Set the options on the primitive
	OP_Options primOptions;
	for (int i = 0; i < getParmList()->getEntries(); ++i) {
		const PRM_Parm &prm = getParm(i);
		primOptions.setOptionFromTemplate(this, prm, *prm.getTemplatePtr(), t);
	}

	primOptions.setOptionI("preview_type", evalInt("preview_type", 0, 0.0));
	primOptions.setOptionF("current_frame", flags().getTimeDep() ? context.getFloatFrame() : 0.0f);

	/* alembic_layers */ {
		const int numFiles = evalInt("alembic_layers", 0, 0.0);
		const int numFilesOffset = getParm("alembic_layers").getMultiStartOffset();

		UT_StringArray layerFiles;
		for (int i = 0; i < numFiles; ++i) {
			const int multiIdx = numFilesOffset + i;

			UT_String layerPath;
			evalStringInst("alembic_layer#", &multiIdx, layerPath, 0, t);

			if (layerPath.isstring()) {
				layerFiles.append(layerPath);
			}
		}

		primOptions.setOptionSArray("alembic_layers", layerFiles);
	}

	updatePrimitiveFromOptions(primOptions);
}
