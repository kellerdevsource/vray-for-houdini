//
// Copyright (c) 2015-2018, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#include "vop_MtlVRayMesh.h"

#include "vfh_vray.h"
#include "vfh_attr_utils.h"
#include "vfh_prm_templates.h"
#include "vfh_op_utils.h" 

using namespace VRayForHoudini;
using namespace VOP;

static const UT_String parmTokenVRayProxySop("vrayproxy_sop");
static const UT_String tokenError("ERROR");
static const UT_String shaderSetsToken("vfh_shader_sets");

/// Callback to clear cache for this node ("Reload Geometry" button in the GUI).
/// @param data Pointer to the node it was called on.
/// @param index The index of the menu entry.
/// @param t Current evaluation time.
/// @param tplate Pointer to the PRM_Template of the parameter it was triggered for.
/// @returns It should return 1 if you want the dialog to refresh
/// (ie if you changed any values) and 0 otherwise.
static int cbRebuildShaderSets(void *data, int index, fpreal t, const PRM_Template *tplate)
{
	MtlVRayMesh &self = *reinterpret_cast<MtlVRayMesh*>(data);
	self.setRebuildShaderSets(true);
	self.rebuildShaderSets();
	return 0;
}

PRM_Template* MtlVRayMesh::getPrmTemplate()
{
	static PRM_Template *myPrmList = nullptr;
	if (myPrmList)
		return myPrmList;

	myPrmList = Parm::getPrmTemplate("MtlVRayMesh");

	PRM_Template *prmIt = myPrmList;
	while (prmIt && prmIt->getType() != PRM_LIST_TERMINATOR) {
		if (vutils_strcmp(prmIt->getToken(), "force_update") == 0) {
			prmIt->setCallback(cbRebuildShaderSets);
			break;
		}
		prmIt++;
	}

	return myPrmList;
}

void MtlVRayMesh::setPluginType()
{
	pluginType = VRayPluginType::MATERIAL;
	pluginID = SL("MtlVRayMesh");
}

void MtlVRayMeshShaderSets::init(const OP_Node &self, const UT_String &filePath)
{
	using namespace VUtils;

	clear();

	if (!filePath.isstring())
		return;

	MeshFile *meshFile = newDefaultMeshFile(filePath.buffer());
	if (!meshFile) {
		Log::getLog().error("\"%s\": Can't open \"%s\"!",
		                    self.getFullPath().buffer(), filePath.buffer());
		return;
	}

	if (meshFile->init(filePath.buffer()).error()) {
		Log::getLog().error("\"%s\": Can't initialize \"%s\"!",
		                    self.getFullPath().buffer(), filePath.buffer());
	}
	else {
		DefaultMeshSetsData setsData;

		for (int i = meshFile->getNumVoxels() - 1; i >= 0; i--) {
			if (meshFile->getVoxelFlags(i) & MVF_PREVIEW_VOXEL) {
				MeshVoxel *voxel = meshFile->getVoxel(i);
				if (voxel) {
					MeshChannel *mayaInfoChannel = voxel->getChannel(MAYA_INFO_CHANNEL);
					if (mayaInfoChannel) {
						setsData.readFromBuffer(reinterpret_cast<char*>(mayaInfoChannel->data),
						                        mayaInfoChannel->elementSize * mayaInfoChannel->numElements);
					}

					meshFile->releaseVoxel(voxel);
				}

				break;
			}
		}

		for (int i = 0; i < setsData.getNumSets(MeshSetsData::meshSetType_shaderSet); ++i) {
			const char *setName = setsData.getSetName(MeshSetsData::meshSetType_shaderSet, i);

			addSetName(i, setName);
		}
	}

	deleteDefaultMeshFile(meshFile);
}

int MtlVRayMeshShaderSets::count() const
{
	vassert(indexToToken.size() == tokenToIndex.size());
	return indexToToken.size();
}

void MtlVRayMeshShaderSets::clear()
{
	indexToToken.clear();
	tokenToIndex.clear();
}

void MtlVRayMeshShaderSets::addSetName(int index, const char *setName)
{
	indexToToken[index] = UT_String(setName, true);
	tokenToIndex[setName] = index;
}

void MtlVRayMesh::setRebuildShaderSets(int value) const
{
	needRebuildShadersSets = value;
}

void MtlVRayMesh::rebuildShaderSets() const
{
	if (!needRebuildShadersSets)
		return;

	// On scene load VRayProxy::file parameter may not be yet available.
	// So, we'll do filepath check here.
	updateFilePath();

	shaderSets.clear();
	shaderSets.init(*this, filePath);

	setRebuildShaderSets(false);
}

void MtlVRayMesh::opChanged(OP_EventType reason, void *data)
{
	if (reason == OP_PARM_CHANGED) {
		const int parmIdx = reinterpret_cast<intptr_t>(data);
		if (parmIdx >= 0) {
			const PRM_ParmList *parmList = getParmList();
			vassert(parmList);

			const PRM_Parm *parm = parmList->getParmPtr(parmIdx);
			vassert(parm);

			if (parmTokenVRayProxySop.equal(parm->getToken())) {
				const int sopChanged = updateSopPath();

				setRebuildShaderSets(sopChanged);
			}
		}
	}

	VOP_Node::opChanged(reason, data);
}

const char* MtlVRayMesh::inputLabel(unsigned idx) const
{
	const IndexToToken::const_iterator it = shaderSets.indexToToken.find(idx);
	if (it != shaderSets.indexToToken.end())
		return it.value().buffer();
	return tokenError.buffer();
}

int MtlVRayMesh::getInputFromName(const UT_String &in) const
{
	return getInputFromNameSubclass(in);
}

void MtlVRayMesh::getInputNameSubclass(UT_String &in, int idx) const
{
	const IndexToToken::const_iterator it = shaderSets.indexToToken.find(idx);
	if (it != shaderSets.indexToToken.end()) {
		in = it.value();
	}
	else {
		in = tokenError;
	}
}

int MtlVRayMesh::getInputFromNameSubclass(const UT_String &in) const
{
	const TokenToIndex::const_iterator it = shaderSets.tokenToIndex.find(in.buffer());
	if (it != shaderSets.tokenToIndex.end())
		return it.value();
	return -1;
}

void MtlVRayMesh::getInputTypeInfoSubclass(VOP_TypeInfo &type_info, int)
{
	type_info.setType(VOP_SURFACE_SHADER);
}

void MtlVRayMesh::getAllowedInputTypeInfosSubclass(unsigned idx, VOP_VopTypeInfoArray &type_infos)
{
	VOP_TypeInfo vopTypeInfo;
	getInputTypeInfoSubclass(vopTypeInfo, idx);

	if (vopTypeInfo == VOP_TypeInfo(VOP_SURFACE_SHADER)) {
		type_infos.append(VOP_TypeInfo(VOP_TYPE_BSDF));
	}
}

void MtlVRayMesh::getAllowedInputTypesSubclass(unsigned idx, VOP_VopTypeArray &voptypes)
{
	VOP_TypeInfo vopTypeInfo;
	getInputTypeInfoSubclass(vopTypeInfo, idx);

	if (vopTypeInfo == VOP_TypeInfo(VOP_SURFACE_SHADER)) {
		voptypes.append(VOP_TYPE_BSDF);
	}
}

bool MtlVRayMesh::willAutoconvertInputType(int)
{
	return true;
}

int MtlVRayMesh::updateSopPath() const
{
	UT_String currentSopPath;
	evalString(currentSopPath, parmTokenVRayProxySop.buffer(), 0, 0.0);

	if (currentSopPath != sopPath) {
		sopPath.adopt(currentSopPath);
		return true;
	}

	return false;
}

int MtlVRayMesh::updateFilePath() const
{
	UT_String currentFilePath;

	const OP_Node *vrayProxy = getOpNodeFromPath(*this, sopPath.buffer());
	if (vrayProxy) {
		vrayProxy->evalString(currentFilePath, "file", 0, 0.0);
	}

	if (currentFilePath != filePath) {
		filePath.adopt(currentFilePath);
		return true;
	}

	return false;
}

unsigned MtlVRayMesh::getNumVisibleInputs() const
{
	return orderedInputs();
}

unsigned MtlVRayMesh::orderedInputs() const
{
	rebuildShaderSets();
	return shaderSets.count();
}

OP_ERROR MtlVRayMesh::saveIntrinsic(std::ostream &os, const OP_SaveFlags &sflags)
{
	const int mtlsCount = shaderSets.count();

	os << shaderSetsToken << vfhStreamSaveSeparator;
	os << sopPath << vfhStreamSaveSeparator;
	os << filePath << vfhStreamSaveSeparator;

	os << mtlsCount << vfhStreamSaveSeparator;

	for (int i = 0; i < mtlsCount; ++i) {
		const IndexToToken::const_iterator it = shaderSets.indexToToken.find(i);
		vassert(it != shaderSets.indexToToken.end());

		os << it.value() << vfhStreamSaveSeparator;
	}

	return VOP_Node::saveIntrinsic(os, sflags);
}

bool MtlVRayMesh::loadPacket(UT_IStream &is, const char *token, const char *path)
{
	if (VOP_Node::loadPacket(is, token, path))
		return true;

	if (shaderSetsToken.equal(token)) {
		shaderSets.clear();

		is.read(sopPath);
		is.read(filePath);

		int mtlsCount = 0;
		is.read(&mtlsCount);

		for (int i = 0; i < mtlsCount; ++i) {
			UT_String setName;
			is.read(setName);

			shaderSets.addSetName(i, setName);
		}

		const int isDataLoaded = sopPath.isstring() && filePath.isstring() && mtlsCount;
		setRebuildShaderSets(!isDataLoaded);

		return true;
	}

	return false;
}

OP::VRayNode::PluginResult MtlVRayMesh::asPluginDesc(Attrs::PluginDesc &pluginDesc, VRayExporter &exporter)
{
	// The real plugin ID is MtlMulti.
	pluginDesc.pluginID = SL("MtlMulti");

	ShaderExporter &shaderExporter = exporter.getShaderExporter();

	const int mtlsCount = shaderSets.count();

	Attrs::QValueList mtlsList(mtlsCount);
	Attrs::QCharStringList namesList(mtlsCount);

	for (int i = 0; i < mtlsCount; ++i) {
		const IndexToToken::const_iterator it = shaderSets.indexToToken.find(i);
		vassert(it != shaderSets.indexToToken.end());

		const UT_String &mtlSockName = it.value();

		const VRay::Plugin mtlPlugin = shaderExporter.exportConnectedSocket(*this, mtlSockName);
		if (mtlPlugin.isEmpty()) {
			Log::getLog().error("\"%s\": Failed to export node connected to \"%s\"!",
			                    getFullPath().buffer(), mtlSockName.buffer());
		}
		else {
			mtlsList.append(VRay::VUtils::Value(mtlPlugin));
			namesList.append(mtlSockName.buffer());
		}
	}

	if (mtlsList.empty())
		return PluginResultError;

	pluginDesc.add(SL("mtls_list"), mtlsList);
	pluginDesc.add(SL("shader_sets_list"), namesList);

	return PluginResultContinue;
}
