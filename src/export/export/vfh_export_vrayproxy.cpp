//
// Copyright (c) 2015-2017, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#include "vfh_export_vrayproxy.h"
#include "vfh_export_mesh.h"
#include "vfh_export_hair.h"

#include <ROP/ROP_Error.h>
#include <OBJ/OBJ_Node.h>
#include <FS/FS_Info.h>
#include <FS/FS_FileSystem.h>
#include <UT/UT_Assert.h>

#include <uni.h>
#include <voxelsubdivider.h>

using namespace VRayForHoudini;

GeometryDescription::GeometryDescription()
	: geometryType(geometryDescriptionUnknown)
	, opNode(nullptr)
	, transform(1)
	, bbox(0)
{}

VRayProxyExportOptions::VRayProxyExportOptions()
	: m_filepath(UT_String::ALWAYS_DEEP)
	, m_mkpath(true)
	, m_overwrite(false)
	, m_exportAsSingle(true)
	, m_exportAsAnimation(false)
	, m_animStart(0)
	, m_animEnd(0)
	, m_animFrames(0)
	, m_lastAsPreview(false)
	, m_applyTransform(false)
	, m_exportVelocity(false)
	, m_velocityStart(0.f)
	, m_velocityEnd(0.05f)
	, m_simplificationType(VUtils::SIMPLIFY_COMBINED)
	, m_maxPreviewFaces(100)
	, m_maxPreviewStrands(100)
	, m_maxFacesPerVoxel(0)
	, m_exportPCLs(false)
	, m_pointSize(0.5f)
{}

bool VRayProxyExportOptions::isAppendMode() const
{
	return m_exportAsAnimation &&
	       m_animFrames > 1 &&
	       m_context.getTime() > m_animStart;
}

UT_String VRayProxyExportOptions::getFilepath(const SOP_Node &sop) const
{
	UT_ASSERT(m_filepath.isstring());

	UT_String filepath = m_filepath;

	if (NOT(filepath.matchFileExtension(".vrmesh"))) {
		filepath += ".vrmesh";
	}

	if (NOT(m_exportAsSingle)) {
		UT_String soppathSuffix;
		sop.getFullPath(soppathSuffix);
		soppathSuffix.forceAlphaNumeric();
		soppathSuffix += ".vrmesh";
		filepath.replaceSuffix(".vrmesh", soppathSuffix);
	}

	return filepath;
}

const Attrs::PluginAttr& GeometryDescription::getAttr(const char *attrName) const
{
	const Attrs::PluginAttr *attr = pluginDesc.get(attrName);
	UT_ASSERT(attr);
	return *attr;
}

VRayProxyExporter::VRayProxyExporter(const VRayProxyExportOptions &options, const SOPList &sopList, ROP_Node *ropNode)
	: sopList(sopList)
	, m_rop(ropNode)
	, m_options(options)
	, m_previewVerts(nullptr)
	, m_previewFaces(nullptr)
	, m_previewStrandVerts(nullptr)
	, m_previewStrands(nullptr)
{}

VRayProxyExporter::~VRayProxyExporter()
{
	cleanup();
}

VRay::Plugin VRayExporterProxy::exportPlugin(const Attrs::PluginDesc &pluginDesc)
{
	if (pluginDesc.pluginID == "GeomStaticMesh") {
		GeometryDescription geomDesc;
		geomDesc.geometryType = GeometryDescription::geometryDescriptionMesh;
		geomDesc.pluginDesc = pluginDesc;
		geomDesc.opNode = objectExporter.getGenerator();

		data += geomDesc;
	}
	else if (pluginDesc.pluginID == "GeomMayaHair") {
		GeometryDescription geomDesc;
		geomDesc.geometryType = GeometryDescription::geometryDescriptionHair;
		geomDesc.pluginDesc = pluginDesc;
		geomDesc.opNode = objectExporter.getGenerator();

		data += geomDesc;
	}

	return VRay::Plugin();
}

VUtils::ErrorCode VRayProxyExporter::init()
{
	cleanup();

	const OP_Context &context(m_options.m_context);

	VRayOpContext ctx(context);
	if (m_options.m_exportVelocity) {
		ctx.setFrame(context.getFloatFrame() + m_options.m_velocityStart);
	}

	VRayProxyObjectExporter objectExporter(geometryDescriptions);
	objectExporter.setContext(ctx);
	objectExporter.getPluginExporter().setRopPtr(m_rop);

	for (int i = 0; i < sopList.size(); ++i) {
		SOP_Node *sopNode = sopList(i);
		if (sopNode) {
			OBJ_Node *objNode = CAST_OBJNODE(sopNode->getParentNetwork());
			if (objNode) {
				objectExporter.exportGeometry(*objNode, *sopNode);
			}
		}
	}

	preprocessDescriptions(ctx);

	VUtils::ErrorCode err;

	if (geometryDescriptions.count() == 0) {
		err.setError(__FUNCTION__, ROP_EXECUTE_ERROR, "No compatible geometry found!");
	}
	else {
		// +1 for preview voxel.
		m_voxels.resize(geometryDescriptions.count() + 1);
	}

	return err;
}

void VRayProxyExporter::cleanup()
{
	for (VUtils::MeshVoxel &voxel : m_voxels) {
		voxel.freeMem();
	}

	if (m_previewVerts) {
		FreePtrArr(m_previewVerts);
	}

	if (m_previewFaces) {
		FreePtrArr(m_previewFaces);
	}

	if (m_previewStrandVerts) {
		FreePtrArr(m_previewStrandVerts);
	}

	if (m_previewStrands) {
		FreePtrArr(m_previewStrands);
	}
}

static VUtils::ErrorCode _doExport(VRayProxyExportOptions &options, const SOPList &sopList)
{
	VRayProxyExporter exporter(options, sopList, nullptr);

	VUtils::ErrorCode err = exporter.init();
	if (!err.error()) {
		err = exporter.doExportFrame();
	}

	return err;
}

VUtils::ErrorCode VRayProxyExporter::doExport(VRayProxyExportOptions &options, const SOPList &sopList)
{
	VUtils::ErrorCode err;

	if (NOT(options.m_filepath.isstring())) {
		err.setError(__FUNCTION__,
					 ROP_MISSING_FILE,
					 "Invalid file specified.");
		return err;
	}

	if (sopList.size() <= 0) {
		err.setError(__FUNCTION__,
					 ROP_BAD_CONTEXT,
					 "No geometry valid found.");
		return err;
	}

	UT_String dirpath;
	UT_String filename;
	options.m_filepath.splitPath(dirpath, filename);

	// create parent dirs if necessary
	FS_Info fsInfo(dirpath);
	if (NOT(fsInfo.exists())) {
		FS_FileSystem fsys;
		if (   NOT(options.m_mkpath)
			|| NOT(fsys.createDir(dirpath)))
		{
			err.setError(__FUNCTION__,
						 ROP_CREATE_DIRECTORY_FAIL,
						 "Failed to create parent directory.");
			return err;
		}
	}

	for (int f = 0; f < options.m_animFrames; ++f) {
		if (err.error()) {
			break;
		}

		options.m_context.setTime(options.m_animStart);
		options.m_context.setFrame(options.m_context.getFrame() + f);

		if (options.m_exportAsSingle) {
			err = _doExport(options, sopList);
		}
		else {
			for (int sopIdx = 0; sopIdx < sopList.size(); ++sopIdx) {
				SOPList singleItem;
				singleItem.append(sopList(sopIdx));

				err = _doExport(options, singleItem);
			}
		}
	}

	return err;
}

VUtils::ErrorCode VRayProxyExporter::doExportFrame()
{
	VUtils::ErrorCode err;

	SOP_Node *sopNode = CAST_SOPNODE(geometryDescriptions[0].opNode);

	UT_String filepath = m_options.getFilepath(*sopNode);
	bool isAppendMode = m_options.isAppendMode();

	FS_Info fsInfo(filepath);
	if (   fsInfo.fileExists()
		&& NOT(isAppendMode)
		&& NOT(m_options.m_overwrite)
		)
	{
		err.setError(__FUNCTION__,
					 ROP_FILE_EXISTS,
					 "File already exists: %s", filepath.buffer());
		return err;
	}

	VUtils::SubdivisionParams subdivParams;
	subdivParams.facesPerVoxel = (m_options.m_maxFacesPerVoxel > 0)? m_options.m_maxFacesPerVoxel : INT_MAX;

	return VUtils::subdivideMeshToFile(this,
									   filepath,
									   subdivParams,
									   NULL,
									   isAppendMode,
									   m_options.m_exportPCLs,
									   m_options.m_pointSize);
}

uint32 VRayProxyExporter::getVoxelFlags(int i)
{
	UT_ASSERT(i >= 0);
	UT_ASSERT(i < getNumVoxels());

	if (i == (getNumVoxels()-1)) {
		return MVF_PREVIEW_VOXEL;
	}

	return geometryDescriptions[i].geometryType == GeometryDescription::geometryDescriptionHair ? MVF_HAIR_GEOMETRY_VOXEL : MVF_GEOMETRY_VOXEL;
}

VUtils::Box VRayProxyExporter::getVoxelBBox(int i)
{
	UT_ASSERT(i >= 0);
	UT_ASSERT(i < getNumVoxels());

	if (i < (getNumVoxels()-1)) {
		return geometryDescriptions[i].bbox;
	}

	VUtils::Box previewBBox(0);
	for (int geomIdx = 0; geomIdx < geometryDescriptions.count(); ++geomIdx) {
		const GeometryDescription &geomDesc = geometryDescriptions[geomIdx];
		previewBBox += geomDesc.bbox;
	}

	return previewBBox;
}

VUtils::MeshVoxel* VRayProxyExporter::getVoxel(int i, uint64 *memUsage)
{
	UT_ASSERT(i >= 0);
	UT_ASSERT(i < getNumVoxels());

	VUtils::MeshVoxel &voxel = m_voxels[i];
	voxel.init();
	voxel.index = i;

	if (i == (getNumVoxels()-1)) {
		buildPreviewVoxel(voxel);
	}
	else if (geometryDescriptions[i].geometryType == GeometryDescription::geometryDescriptionHair) {
		buildHairVoxel(voxel, geometryDescriptions[i]);
	}
	else if (geometryDescriptions[i].geometryType == GeometryDescription::geometryDescriptionMesh) {
		buildMeshVoxel(voxel, geometryDescriptions[i]);
	}

	if (memUsage) {
		*memUsage = voxel.getMemUsage();
	}

	return &voxel;
}


void VRayProxyExporter::releaseVoxel(VUtils::MeshVoxel *voxel, uint64 *memUsage)
{
	if (voxel) {
		if (memUsage) {
			*memUsage = voxel->getMemUsage();
		}

		voxel->freeMem();
	}
}


VUtils::ErrorCode VRayProxyExporter::preprocessDescriptions(const OP_Context &context)
{
	for (int geomIdx = 0; geomIdx < geometryDescriptions.count(); ++geomIdx) {
		GeometryDescription &geomDescr = geometryDescriptions[geomIdx];

		VUtils::ErrorCode res;

		OP_Context cntx = context;
		// advance current time with velocityStart and get mesh description for that time
		if (m_options.m_exportVelocity) {
			cntx.setFrame(context.getFloatFrame() + m_options.m_velocityStart);
		}

		OP_Network *parentNet = geomDescr.opNode->getParent();
		if (!parentNet) {
			res.setError(__FUNCTION__,
						 ROP_BAD_CONTEXT,
						 "No renderable geometry found for context #%0.3f!", context.getFloatFrame());
			return res;
		}

		OBJ_Node *parentOBJ = parentNet->castToOBJNode();
		if (!(parentOBJ && parentOBJ->isObjectRenderable(cntx.getFloatFrame()))) {
			res.setError(__FUNCTION__,
						 ROP_BAD_CONTEXT,
						 "No renderable geometry found for context #%0.3f!", context.getFloatFrame());
			return res;
		}

		// transform normals
		if (!m_options.m_applyTransform) {
			geomDescr.transform.makeIdentity();
		}
		else {
			getTransformForContext(context, geomDescr);

			if (geomDescr.pluginDesc.contains(geomDescr.getVertsAttrName())) {
				VRay::VUtils::VectorRefList &verts =
					const_cast<VRay::VUtils::VectorRefList&>(geomDescr.getVertAttr().paramValue.valRawListVector);
				for (int i = 0; i < verts.size(); ++i) {
					verts[i] = geomDescr.transform * verts[i];
				}
			}

			if (geomDescr.pluginDesc.contains("normals")) {
				VRay::VUtils::VectorRefList &normals =
					const_cast<VRay::VUtils::VectorRefList&>(geomDescr.getAttr("normals").paramValue.valRawListVector);

				VRay::Matrix nm = geomDescr.transform.matrix;
				nm.makeInverse();
				nm.makeTranspose();
				for (int i = 0; i < normals.size(); ++i) {
					normals[i] = nm * normals[i];
				}
			}
		}

		const VRay::VUtils::VectorRefList &verts = geomDescr.getVertAttr().paramValue.valRawListVector;

		// calc bbox
		geomDescr.bbox.init();
		for (int i = 0; i < verts.size(); ++i) {
			geomDescr.bbox += VUtils::Vector(verts[i].x, verts[i].y, verts[i].z);
		}
#if 0
		if (m_options.m_exportVelocity) {
			if (!geomDescr.pluginDesc.contains("velocities")) {
				// add velocities to description
				VRay::VUtils::VectorRefList velocities(verts.size());
				for (int i = 0; i < velocities.size(); ++i) {
					velocities[i].set(0.f,0.f,0.f);
				}

				const float velocityInterval = m_options.m_velocityEnd - m_options.m_velocityStart;
				if (velocityInterval > 1e-6f) {
					cntx.setFrame(context.getFloatFrame() + m_options.m_velocityEnd);

					GeometryDescription nextDescr(geomDescr.opNode);
					VUtils::ErrorCode err_code = getDescriptionForContext(cntx, nextDescr);

					if ( !err_code.error() ) {
						VRay::VUtils::VectorRefList &nextVerts =
							const_cast<VRay::VUtils::VectorRefList&>(nextDescr.getVertAttr().paramValue.valRawListVector);

						// if no change in topology calc velocities
						if (verts.size() == nextVerts.size() ) {
							const float dt = 1.f/velocityInterval;
							for (int i = 0; i < velocities.size(); ++i) {
								velocities[i] = (nextVerts[i] - verts[i]) * dt;
							}
						}
					}
				}

				geomDescr.pluginDesc.addAttribute(Attrs::PluginAttr("velocities", velocities));
			}
		}
#endif
	}

	return VUtils::ErrorCode();
}

void VRayProxyExporter::getTransformForContext(const OP_Context &context, GeometryDescription &geomDescr) const
{
	OP_Network *parentNet = geomDescr.opNode->getParent();
	if (!parentNet) {
		return;
	}

	OBJ_Node *parentOBJ = CAST_OBJNODE(parentNet);
	if (!parentOBJ) {
		return;
	}

	UT_Matrix4 mat;
	UT_Vector3 offs;
	parentOBJ->getLocalToWorldTransform(const_cast<OP_Context&>(context), mat);
	mat.getTranslates(offs);

	geomDescr.transform.matrix.setCol(0, VRay::Vector(mat(0,0), mat(0,1), mat(0,2)));
	geomDescr.transform.matrix.setCol(1, VRay::Vector(mat(1,0), mat(1,1), mat(1,2)));
	geomDescr.transform.matrix.setCol(2, VRay::Vector(mat(2,0), mat(2,1), mat(2,2)));
	geomDescr.transform.offset.set(offs(0), offs(1), offs(2));
}

void VRayProxyExporter::buildMeshVoxel(VUtils::MeshVoxel &voxel, GeometryDescription &meshDescr)
{
	const int hasVerts = meshDescr.pluginDesc.contains("vertices");
	const int hasFaces = meshDescr.pluginDesc.contains("faces");
	const int hasMtlIDs = meshDescr.pluginDesc.contains("face_mtlIDs");
	const int hasNormals = meshDescr.pluginDesc.contains("normals");
	const int hasVelocities = meshDescr.pluginDesc.contains("velocities");
	int nUVChannels = meshDescr.pluginDesc.contains("map_channels");

	if (nUVChannels) {
		const VRay::VUtils::ValueRefList &map_channels = meshDescr.getAttr("map_channels").paramValue.valRawListValue;
		nUVChannels = map_channels.size();
	}

	voxel.numChannels = hasVerts + hasVelocities + hasFaces + hasMtlIDs + hasNormals * 2 + nUVChannels * 2;
	voxel.channels = new VUtils::MeshChannel[voxel.numChannels];

	int ch_idx = 0;

	// init vertex channel
	if (hasVerts) {
		Log::getLog().info("buildGeomVoxel populate VERT_GEOM_CHANNEL");

		const VRay::VUtils::VectorRefList &vertices = meshDescr.getAttr("vertices").paramValue.valRawListVector;

		VUtils::MeshChannel &verts_ch = voxel.channels[ ch_idx++ ];
		verts_ch.init( sizeof(VUtils::VertGeomData), vertices.size(), VERT_GEOM_CHANNEL, FACE_TOPO_CHANNEL, MF_VERT_CHANNEL, false);
		verts_ch.data = const_cast<VRay::Vector*>(vertices.get());
	}
	// init velocity channel
	if (hasVelocities) {
		Log::getLog().info("buildGeomVoxel populate VERT_VELOCITY_CHANNEL");

		const VRay::VUtils::VectorRefList &velocities = meshDescr.getAttr("velocities").paramValue.valRawListVector;

		VUtils::MeshChannel &velocity_ch = voxel.channels[ ch_idx++ ];
		velocity_ch.init( sizeof(VUtils::VertGeomData), velocities.size(), VERT_VELOCITY_CHANNEL, FACE_TOPO_CHANNEL, MF_VERT_CHANNEL, false);
		velocity_ch.data = const_cast<VRay::Vector*>(velocities.get());
	}
	// init face channel
	if (hasFaces) {
		Log::getLog().info("buildGeomVoxel populate FACE_TOPO_CHANNEL");

		const VRay::VUtils::IntRefList &faces = meshDescr.getAttr("faces").paramValue.valRawListInt;

		VUtils::MeshChannel &face_ch = voxel.channels[ ch_idx++ ];
		face_ch.init(sizeof(VUtils::FaceTopoData), faces.size() / 3, FACE_TOPO_CHANNEL, 0, MF_TOPO_CHANNEL, false);
		face_ch.data = const_cast<int*>(faces.get());
	}
	// init face info channel
	if (hasMtlIDs) {
		Log::getLog().info("buildGeomVoxel populate FACE_INFO_CHANNEL");

		const VRay::VUtils::IntRefList &faceMtlIDs = meshDescr.getAttr("face_mtlIDs").paramValue.valRawListInt;

		VUtils::MeshChannel &faceinfo_ch = voxel.channels[ ch_idx++ ];
		faceinfo_ch.init(sizeof(VUtils::FaceInfoData), faceMtlIDs.size(), FACE_INFO_CHANNEL, 0, MF_FACE_CHANNEL, true);
		for(int i = 0; i < faceMtlIDs.size(); ++i) {
			static_cast<VUtils::FaceInfoData*>(faceinfo_ch.data)[i].mtlID = faceMtlIDs[i];
		}
	}
	// init normals channels
	if (hasNormals) {
		Log::getLog().info("buildGeomVoxel populate VERT_NORMAL_CHANNEL");

		const VRay::VUtils::VectorRefList &normals = meshDescr.getAttr("normals").paramValue.valRawListVector;

		VUtils::MeshChannel &normal_ch = voxel.channels[ ch_idx++ ];
		normal_ch.init(sizeof(VUtils::VertGeomData), normals.size(), VERT_NORMAL_CHANNEL, VERT_NORMAL_TOPO_CHANNEL, MF_VERT_CHANNEL, false);
		normal_ch.data = const_cast<VRay::Vector*>(normals.get());

		const VRay::VUtils::IntRefList &facenormals = meshDescr.getAttr("faceNormals").paramValue.valRawListInt;

		VUtils::MeshChannel &facenormal_ch = voxel.channels[ ch_idx++ ];
		facenormal_ch.init(sizeof(VUtils::FaceTopoData), facenormals.size() / 3, VERT_NORMAL_TOPO_CHANNEL, 0, MF_TOPO_CHANNEL, false);
		facenormal_ch.data = const_cast<int*>(facenormals.get());
	}

	// init uv channels
	if (nUVChannels) {
		VRay::VUtils::ValueRefList mapChannels = meshDescr.getAttr("map_channels").paramValue.valRawListValue;

		for (int i = 0; i < nUVChannels; ++i) {
			VRay::VUtils::ValueRefList mapChannel = mapChannels[i].getList();

			int uvchannel_idx = static_cast< int >(mapChannel[0].getDouble());
			const VRay::VUtils::VectorRefList &uv_verts = mapChannel[1].getListVector();
			const VRay::VUtils::IntRefList &uv_faces = mapChannel[2].getListInt();

			Log::getLog().info("buildGeomVoxel populate VERT_TEX_CHANNEL %d", uvchannel_idx);

			VUtils::MeshChannel &uvverts_ch = voxel.channels[ ch_idx++ ];
			uvverts_ch.init(sizeof(VUtils::VertGeomData), uv_verts.size(),  VERT_TEX_CHANNEL0 + uvchannel_idx, VERT_TEX_TOPO_CHANNEL0 + uvchannel_idx, MF_VERT_CHANNEL, false);
			uvverts_ch.data = const_cast<VRay::Vector*>(uv_verts.get());

			VUtils::MeshChannel &uvface_ch = voxel.channels[ ch_idx++ ];
			uvface_ch.init(sizeof(VUtils::FaceTopoData), uv_faces.size() / 3, VERT_TEX_TOPO_CHANNEL0 + uvchannel_idx, 0, MF_TOPO_CHANNEL, false);
			uvface_ch.data = const_cast<int*>(uv_faces.get());
		}
	}

	UT_ASSERT( voxel.numChannels == ch_idx );
}


void VRayProxyExporter::buildHairVoxel(VUtils::MeshVoxel &voxel, GeometryDescription &hairDescr)
{
	const int hasVerts = hairDescr.pluginDesc.contains("hair_vertices");
	const int hasStrands = hairDescr.pluginDesc.contains("num_hair_vertices");
	const int hasWidths = hairDescr.pluginDesc.contains("widths");
	const int hasVelocities = hairDescr.pluginDesc.contains("velocities");

	voxel.numChannels = hasVerts + hasStrands + hasVelocities + hasWidths;
	voxel.channels = new VUtils::MeshChannel[voxel.numChannels];
	int ch_idx = 0;

	if (hasVerts) {
		Log::getLog().info("buildHairVoxel populate HAIR_VERT_CHANNEL");
		const VRay::VUtils::VectorRefList &vertices = hairDescr.getAttr("hair_vertices").paramValue.valRawListVector;

		VUtils::MeshChannel &vertices_ch = voxel.channels[ ch_idx++ ];
		vertices_ch.init( sizeof(VUtils::VertGeomData), vertices.size(), HAIR_VERT_CHANNEL, HAIR_NUM_VERT_CHANNEL, MF_VERT_CHANNEL, false);
		vertices_ch.data = const_cast<VRay::Vector*>(vertices.get());
	}

	if (hasStrands) {
		Log::getLog().info("buildHairVoxel populate HAIR_NUM_VERT_CHANNEL");
		const VRay::VUtils::IntRefList &strands = hairDescr.getAttr("num_hair_vertices").paramValue.valRawListInt;

		VUtils::MeshChannel &strands_ch = voxel.channels[ ch_idx++ ];
		strands_ch.init( sizeof(int), strands.size(), HAIR_NUM_VERT_CHANNEL, 0, MF_NUM_VERT_CHANNEL, false);
		strands_ch.data = const_cast<int*>(strands.get());
	}

	if (hasWidths) {
		Log::getLog().info("buildHairVoxel populate HAIR_WIDTH_CHANNEL");
		const VRay::VUtils::FloatRefList &widths = hairDescr.getAttr("widths").paramValue.valRawListFloat;

		VUtils::MeshChannel &width_ch = voxel.channels[ ch_idx++ ];
		width_ch.init( sizeof(float), widths.size(), HAIR_WIDTH_CHANNEL, HAIR_NUM_VERT_CHANNEL, MF_VERT_CHANNEL, false);
		width_ch.data = const_cast<float*>(widths.get());
	}

	if (hasVelocities) {
		Log::getLog().info("buildHairVoxel populate HAIR_VELOCITY_CHANNEL");
		const VRay::VUtils::VectorRefList &velocities = hairDescr.getAttr("velocities").paramValue.valRawListVector;

		VUtils::MeshChannel &velocities_ch = voxel.channels[ ch_idx++ ];
		velocities_ch.init( sizeof(VUtils::VertGeomData), velocities.size(), HAIR_VELOCITY_CHANNEL, HAIR_NUM_VERT_CHANNEL, MF_VERT_CHANNEL, false);
		velocities_ch.data = const_cast<VRay::Vector*>(velocities.get());
	}

	UT_ASSERT( voxel.numChannels == ch_idx );
}


void VRayProxyExporter::buildPreviewVoxel(VUtils::MeshVoxel &voxel)
{
	VUtils::ObjectInfoChannelData meshObjInfo;
	createMeshPreviewGeometry(meshObjInfo);

	VUtils::ObjectInfoChannelData hairObjInfo;
	createHairPreviewGeometry(hairObjInfo);

	// init uv sets data
	VUtils::DefaultMeshSetsData setsInfo;
	int nTotalUVChannels = 0;
	for (int geomIdx = 0; geomIdx < geometryDescriptions.count(); ++geomIdx) {
		const GeometryDescription &geomDesc = geometryDescriptions[geomIdx];

		if (geomDesc.pluginDesc.contains("map_channels_names")) {
			const VRay::VUtils::ValueRefList &mapChannelNames = geomDesc.getAttr("map_channels_names").paramValue.valRawListValue;
			nTotalUVChannels += mapChannelNames.size();
		}
	}

	setsInfo.setNumSets(VUtils::MeshSetsData::meshSetType_uvSet, nTotalUVChannels);

	int offset = 0;
	for (int geomIdx = 0; geomIdx < geometryDescriptions.count(); ++geomIdx) {
		const GeometryDescription &geomDesc = geometryDescriptions[geomIdx];
		if (geomDesc.pluginDesc.contains("map_channels_names")) {
			VRay::VUtils::ValueRefList mapChannelNames = geomDesc.getAttr("map_channels_names").paramValue.valRawListValue;

			for (int i = 0; i < mapChannelNames.size(); ++i) {
				VRay::VUtils::CharString channelName = mapChannelNames[i].getString();
				setsInfo.setSetName(VUtils::MeshSetsData::meshSetType_uvSet, offset++, channelName.ptr());
			}
		}
	}

	const int numMeshObj = meshObjInfo.getNumObjectInfos();
	const int numHairObj = hairObjInfo.getNumObjectInfos();
	const int numUVSets = setsInfo.getNumSets(VUtils::MeshSetsData::meshSetType_uvSet);

	voxel.numChannels = ((bool)numMeshObj)*3 + ((bool)numHairObj)*3 + (bool)numUVSets;
	voxel.channels = new VUtils::MeshChannel[voxel.numChannels];

	int ch_idx = 0;
	if ( numMeshObj ) {
		// vertex channel
		Log::getLog().info("buildPreviewVoxel populate VERT_GEOM_CHANNEL");

		VUtils::MeshChannel &verts_ch = voxel.channels[ ch_idx++ ];
		verts_ch.init( sizeof(VUtils::VertGeomData), meshObjInfo.getTotalPreviewVertices(), VERT_GEOM_CHANNEL, FACE_TOPO_CHANNEL, MF_VERT_CHANNEL, false);
		verts_ch.data = m_previewVerts;

		// face channel
		Log::getLog().info("buildPreviewVoxel populate FACE_TOPO_CHANNEL");

		VUtils::MeshChannel &face_ch = voxel.channels[ ch_idx++ ];
		face_ch.init( sizeof(VUtils::FaceTopoData), meshObjInfo.getTotalPreviewElements(), FACE_TOPO_CHANNEL, 0, MF_TOPO_CHANNEL, false);
		face_ch.data = m_previewFaces;

		// obj info channel
		Log::getLog().info("buildPreviewVoxel populate OBJECT_INFO_CHANNEL");

		int bufLen = 0;
		meshObjInfo.writeToBuffer(NULL, bufLen);

		VUtils::MeshChannel &info_ch = voxel.channels[ ch_idx++ ];
		info_ch.init(bufLen, 1, OBJECT_INFO_CHANNEL, 0, MF_INFO_CHANNEL);
		meshObjInfo.writeToBuffer(info_ch.data, bufLen);
	}

	if (numHairObj) {
		// vertex channel
		Log::getLog().info("buildPreviewVoxel populate HAIR_VERT_CHANNEL");

		VUtils::MeshChannel &verts_ch = voxel.channels[ ch_idx++ ];
		verts_ch.init( sizeof(VUtils::VertGeomData), hairObjInfo.getTotalPreviewVertices(), HAIR_VERT_CHANNEL, HAIR_NUM_VERT_CHANNEL, MF_VERT_CHANNEL, false);
		verts_ch.data = m_previewStrandVerts;

		// strand channel
		Log::getLog().info("buildPreviewVoxel populate HAIR_NUM_VERT_CHANNEL");

		VUtils::MeshChannel &strands_ch = voxel.channels[ ch_idx++ ];
		strands_ch.init( sizeof(int), hairObjInfo.getTotalPreviewElements(), HAIR_NUM_VERT_CHANNEL, 0, MF_NUM_VERT_CHANNEL, false);
		strands_ch.data = m_previewStrands;

		// obj info channel
		Log::getLog().info("buildPreviewVoxel populate HAIR_OBJECT_INFO_CHANNEL");

		int bufLen = 0;
		hairObjInfo.writeToBuffer(NULL, bufLen);

		VUtils::MeshChannel &info_ch = voxel.channels[ ch_idx++ ];
		info_ch.init(bufLen, 1, HAIR_OBJECT_INFO_CHANNEL, 0, MF_INFO_CHANNEL);
		hairObjInfo.writeToBuffer(info_ch.data, bufLen);
	}

	if (numUVSets) {
		// uv sets channel
		Log::getLog().info("buildPreviewVoxel populate MAYA_INFO_CHANNEL");
		int bufLen = 0;
		setsInfo.writeToBuffer(NULL, bufLen);

		VUtils::MeshChannel &info_ch = voxel.channels[ ch_idx++ ];
		info_ch.init(bufLen, 1, MAYA_INFO_CHANNEL, 0, MF_MAYA_INFO_CHANNEL);
		setsInfo.writeToBuffer(info_ch.data, bufLen);
	}

	UT_ASSERT( voxel.numChannels == ch_idx );
}


void VRayProxyExporter::createHairPreviewGeometry(VUtils::ObjectInfoChannelData &objInfo)
{
	addObjectInfoForType(MVF_HAIR_GEOMETRY_VOXEL, objInfo);

	int nTotalStrands = 0;
	int maxVertsPerStrand = 0;
	const int previewStartIdx = getPreviewStartIdx();

	// calc total number of strands for hairs that will be preview
	for (int j = previewStartIdx; j < geometryDescriptions.count(); ++j) {
		// skip geometry other than hair
		if (NOT(getVoxelFlags(j) & MVF_HAIR_GEOMETRY_VOXEL)) {
			continue;
		}

		const GeometryDescription &geomDesc = geometryDescriptions[j];
		if (geomDesc.geometryType == GeometryDescription::geometryDescriptionHair) {
			const VRay::VUtils::IntRefList &strands = geomDesc.getAttr("num_hair_vertices").paramValue.valRawListInt;
			for (int i = 0; i < strands.count(); ++i) {
				maxVertsPerStrand = std::max(maxVertsPerStrand, strands[i]);
			}

			nTotalStrands += strands.count();
		}
	}

	// build hair preview geometry by skipping strands
	int numPreviewVerts = 0;
	int numPreviewStrands = 0;
	const int maxPreviewStrands = std::min(m_options.m_maxPreviewStrands, nTotalStrands);
	if (maxPreviewStrands > 0 && nTotalStrands > 0) {
		m_previewStrandVerts = new VUtils::VertGeomData[ maxPreviewStrands * maxVertsPerStrand ];
		m_previewStrands = new int[ maxPreviewStrands ];

		const float nStrands = static_cast<float>(maxPreviewStrands) / nTotalStrands;
		for (int j = previewStartIdx; j < geometryDescriptions.count(); ++j) {
			// skip geometry other than hair
			if (NOT(getVoxelFlags(j) & MVF_HAIR_GEOMETRY_VOXEL)) {
				continue;
			}

			const GeometryDescription &geomDesc = geometryDescriptions[j];
			if (geomDesc.geometryType == GeometryDescription::geometryDescriptionHair) {
				const VRay::VUtils::IntRefList &strands = geomDesc.getAttr("num_hair_vertices").paramValue.valRawListInt;
				const VRay::VUtils::VectorRefList &vertices = geomDesc.getAttr("hair_vertices").paramValue.valRawListVector;

				const int numStrandsPrevObj = numPreviewStrands;

				int voffset = 0;
				for(int i = 0; i < strands.count(); ++i) {
					const int p0 =  i * nStrands;
					const int p1 = (i+1) * nStrands;

					if (p0 != p1 && numPreviewStrands < maxPreviewStrands) {
						m_previewStrands[ numPreviewStrands++ ] = strands[i];
						for(int k = 0; k < strands[i]; ++k) {
							const VRay::Vector &v = vertices[voffset + k];
							m_previewStrandVerts[ numPreviewVerts++ ].set(v.x, v.y, v.z);
						}

						if (numPreviewStrands >= maxPreviewStrands) {
							break;
						}
					}

					voffset += strands[i];
				}

				objInfo.setElementRange(j, numStrandsPrevObj, numPreviewStrands);
				objInfo.addVoxelData(j, vertices.count(), strands.count());
			}
		}
	}

	objInfo.setTotalPreviewVertices(numPreviewVerts);
	objInfo.setTotalPreviewElements(numPreviewStrands);

	UT_ASSERT( numPreviewVerts <= maxPreviewStrands * maxVertsPerStrand );
	UT_ASSERT( numPreviewStrands <= maxPreviewStrands );
}


void VRayProxyExporter::createMeshPreviewGeometry(VUtils::ObjectInfoChannelData &objInfo)
{
	addObjectInfoForType(MVF_GEOMETRY_VOXEL, objInfo);

	int numPreviewVerts = 0;
	int numPreviewFaces = 0;

	if (m_options.m_simplificationType == VUtils::SIMPLIFY_FACE_SAMPLING) {
		simplifyFaceSampling(numPreviewVerts, numPreviewFaces, objInfo);
	} else {
		simplifyMesh(numPreviewVerts, numPreviewFaces, objInfo);
	}

	objInfo.setTotalPreviewVertices(numPreviewVerts);
	objInfo.setTotalPreviewElements(numPreviewFaces);
}


void VRayProxyExporter::simplifyFaceSampling(int &numPreviewVerts, int &numPreviewFaces,
										   VUtils::ObjectInfoChannelData &objInfo)
{
	numPreviewVerts = 0;
	numPreviewFaces = 0;
	int nTotalFaces = 0;
	const int previewStartIdx = getPreviewStartIdx();
	for (int j = previewStartIdx; j < geometryDescriptions.count(); ++j) {
		// skip other geometry than mesh
		if (NOT(getVoxelFlags(j) & MVF_GEOMETRY_VOXEL)) {
			continue;
		}

		const GeometryDescription &geomDesc = geometryDescriptions[j];
		if (geomDesc.geometryType == GeometryDescription::geometryDescriptionMesh) {
			const VRay::VUtils::IntRefList &faces = geomDesc.getAttr("faces").paramValue.valRawListInt;
			nTotalFaces += faces.count() / 3;
		}
	}

	const int maxPreviewFaces = std::min(m_options.m_maxPreviewFaces, nTotalFaces);
	if (maxPreviewFaces > 0 && nTotalFaces > 0) {
		m_previewVerts = new VUtils::VertGeomData[ maxPreviewFaces * 3 ];
		m_previewFaces = new VUtils::FaceTopoData[ maxPreviewFaces ];

		const float nFaces = static_cast<float>(maxPreviewFaces) / nTotalFaces;
		for (int j = previewStartIdx; j < geometryDescriptions.count(); ++j) {
			// skip other geometry than mesh
			if (NOT(getVoxelFlags(j) & MVF_GEOMETRY_VOXEL)) {
				continue;
			}

			const GeometryDescription &geomDesc = geometryDescriptions[j];
			if (geomDesc.geometryType == GeometryDescription::geometryDescriptionMesh) {
				const VRay::VUtils::VectorRefList &vertices = geomDesc.getAttr("vertices").paramValue.valRawListVector;
				const VRay::VUtils::IntRefList &faces = geomDesc.getAttr("faces").paramValue.valRawListInt;

				int numFacesPrevObj = numPreviewFaces;

				for(int i = 0; i < faces.count() / 3; ++i) {
					const int p0 =  i * nFaces;
					const int p1 = (i+1) * nFaces;
					if (p0 != p1 && numPreviewFaces < maxPreviewFaces) {
						for(int k = 0; k < 3; ++k) {
							const VRay::Vector &v = vertices[faces[i*3+k]];
							m_previewFaces[numPreviewFaces].v[k] = numPreviewVerts;
							m_previewVerts[numPreviewVerts++].set(v.x, v.y, v.z);
						}

						numPreviewFaces++;
						if (numPreviewFaces >= maxPreviewFaces)
							break;
					}
				}

				objInfo.setElementRange(j, numFacesPrevObj, numPreviewFaces);
				objInfo.addVoxelData(j, vertices.count(), faces.count());
			}
		}
	}

	UT_ASSERT( numPreviewFaces <= maxPreviewFaces );
	UT_ASSERT( numPreviewVerts <= (maxPreviewFaces * 3) );
}


void VRayProxyExporter::simplifyMesh(int &numPreviewVerts, int &numPreviewFaces,
								   VUtils::ObjectInfoChannelData &objInfo)
{
	numPreviewVerts = 0;
	numPreviewFaces = 0;
	int nTotalFaces = 0;
	const int previewStartIdx = getPreviewStartIdx();
	VUtils::SimplifierMesh mesh;
	for (int j = previewStartIdx; j < geometryDescriptions.count(); ++j) {
		// skip other than mesh geometry
		if (NOT(getVoxelFlags(j) & MVF_GEOMETRY_VOXEL)) {
			continue;
		}

		const GeometryDescription &geomDesc = geometryDescriptions[j];
		if (geomDesc.geometryType == GeometryDescription::geometryDescriptionMesh) {
			const VRay::VUtils::VectorRefList &vertices = geomDesc.getAttr("vertices").paramValue.valRawListVector;
			const VRay::VUtils::IntRefList &faces = geomDesc.getAttr("faces").paramValue.valRawListInt;

			objInfo.addVoxelData(j, vertices.count(), faces.count());

			const int vertOffset = mesh.countVertices();
			for (int i = 0; i != vertices.size(); ++i) {
				mesh.addVertex(VUtils::Vector(vertices[i].x, vertices[i].y, vertices[i].z));
			}

			for (int i = 0; i != faces.size();) {
				int nullValues[3] = {-1,-1,-1};
				int verts[3];
				for(int p = 0 ; p < 3 ; ++p)
					verts[p] = faces[i++];

				mesh.addFace(verts, nullValues, nullValues, vertOffset);
				++nTotalFaces;
			}
		}
	}

	const int maxPreviewFaces = VUtils::Min(m_options.m_maxPreviewFaces, nTotalFaces);
	if (maxPreviewFaces > 0) {
		VUtils::simplifyMesh(mesh, m_options.m_simplificationType, maxPreviewFaces);
		mesh.exportMesh(m_previewVerts, m_previewFaces, numPreviewVerts, numPreviewFaces);
	}
}


void VRayProxyExporter::addObjectInfoForType(uint32 type, VUtils::ObjectInfoChannelData &objInfo)
{
	objInfo.setTotalVoxelCount(m_voxels.size());
	for (int j = 0; j < geometryDescriptions.count(); ++j) {
		// skip other geometry
		if (NOT(getVoxelFlags(j) & type)) {
			continue;
		}

		const GeometryDescription &meshDescr = geometryDescriptions[j];
		UT_ASSERT(meshDescr.opNode);

		UT_String fullName;
		meshDescr.opNode->getFullPath(fullName);

		VUtils::ObjectInfo info(meshDescr.opNode->getUniqueId(), fullName);
		info.setVoxelRange(j, j+1);
		objInfo.addObjectInfo(info);
	}
}


int VRayProxyExporter::getPreviewStartIdx() const
{
	if (m_options.m_lastAsPreview) {
		return std::max(static_cast<int>(geometryDescriptions.count() - 1), 0);
	}
	return 0;
}
