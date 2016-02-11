//
// Copyright (c) 2015, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#include "vfh_defines.h"
#include "cmd/vfh_cmd_vrayproxy.h"
#include "export/vfh_exporter.h"

#include <OP/OP_Director.h>
#include <OBJ/OBJ_Geometry.h>
#include <OBJ/OBJ_SubNet.h>
#include <SOP/SOP_Node.h>
#include <GU/GU_Detail.h>
#include <UT/UT_Version.h>

#include <vassert.h>
#include <uni.h>
#include <simplifier.h>
#include <voxelsubdivider.h>


using namespace VRayForHoudini;


enum ArgError {
	AE_ARG_NOT_FOUND = 1
};

enum DataError {
	DE_INCORRECT_GEOM = 1,
	DE_NO_RENDERABLE_GEOM = 2
};

struct VRayProxyParams
{
	VRayProxyParams():
		m_filepath(nullptr),
		m_filename(nullptr),
		m_newNodeName(nullptr)
	{ }

	VUtils::ErrorCode parse(const CMD_Args &args);
	const tchar* getFilename(const OBJ_Node &node) const;

///	(bool) export all selected geometry in single .vrmesh file(mandatory)
/// cmd arg: -m 0/1
/// 0 = export each object in separate file, 1 = export all objects in single file
	int m_exportAsSingle;

///	path to the folder where the .vrmesh file(s) will be saved(mandatory)
/// cmd arg: -d "path/to/file/"
	const tchar* m_filepath;

///	the name of the .vrmesh file(mandatory, if exportAsSingle is true)
/// if exportAsSingle is true file path is formed as "filepath/filename"
/// otherwise file path is formed as "filepath/objectName.vrmesh" and filename is ignored
/// cmd arg: -n "filename.vrmesh"
	const tchar* m_filename;

/// (bool) if true, export geometry in world space(optional)
/// this option is always true when exporting multiple objects to a single file
/// default value: false
/// cmd arg: -t
	int m_includeTransform;

/// (bool) if true, overwrite files having same file name(optional)
/// when false, will skip file export for corresponding objects
/// default value: false
/// cmd arg: -f
	int m_overwrite;

/// (bool) if true, use last selected object as preview geometry(optional)
/// this option is always false if exporting single object per file
/// default value: false
/// cmd arg: -l
	int m_previewLastSelected;

/// (bool) if true, export frame range(optional)
/// default value: false, in which case only current frame will be exported
/// cmd arg: -a animStartFrame animEndFrame
	int m_exportAnimation;

/// (int) start frame of the animation(mandatory, if exportAnimation is true)
/// cmd arg: -a animStartFrame animEndFrame
	long m_animStart;

/// (int) end frame of the animation(mandatory, if exportAnimation is true)
/// cmd arg: -a animStartFrame animEndFrame
	long m_animEnd;

/// (bool) if true, export vertex velocity(optional)
/// this option is always false if exportAnimation is false
/// default value: false
/// vertex velocity is calulated as: (vert_pos(velocityEndTime) - vert_pos(velocityStartTime)) / (velocityEndTime - velocityStartTime)
/// cmd arg: -v velocityStartTime velocityEndTime
	int m_exportVelocity;

/// (float) start time to sample vertex position (mandatory, if exportVelocity is true)
/// velocityStart should be in range [0,1)
/// cmd arg: -v velocityStartTime velocityEndTime
	float m_velocityStart;

/// (float) end time to sample vertex position (mandatory, if exportVelocity is true)
/// velocityEnd should be in range (0,1]
/// cmd arg: -v velocityStartTime velocityEndTime
	float m_velocityEnd;

/// (bool) if true, point cloud information will be computed and stored with each voxel in the file(optional)
/// default value: false
/// cmd arg: -P pointSize
	int m_exportPCL;

/// (float) specifies the desired density for the points in the point cloud -
/// average area covered by one point(mandatory, if exportPCL is true)
/// cmd arg: -P pointSize
	float m_pointSize;

/// (int) max number of faces for preview geometry voxel(optional)
/// default value: 100
/// cmd arg: -F maxPreviewFaces
	int maxPreviewFaces;

/// (int) max number of strands for preview geometry voxel(optional)
/// default value: 100
/// cmd arg: -H maxPreviewStrands
	int maxPreviewStrands;

/// (SimplificationType enum) specifies how to do the simplification(optional)
/// default value: SIMPLIFY_COMBINED
/// cmd arg: -T previewType
	VUtils::SimplificationType m_previewType;

/// (bool) if true, export vertex color sets(optional)
/// default value: false
/// cmd arg: -c
	int m_exportColors;

/// (bool) if true, ignore hidden and templated geometry(optional)
/// default value: false
/// cmd arg: -i
	int m_ignoreHidden;

/// (int) max number of faces per voxel(optional)
/// default value: INT_MAX
/// cmd arg: -x maxFacesPerVoxel
	int m_maxFacesPerVoxel;

// TODO: not implemented
/// (bool) if true, VRayProxy node for each exported .vrmesh file(optional)
/// default value: false
/// cmd arg: -A
	int m_autoCreateProxies;

// TODO: not implemented
/// (string) specifies name prefix for newly creaded VRayProxy nodes(optional)
/// node name is formed as "newNodeName_filename"
/// default value: ""
/// cmd arg: -N "nodePrefix"
	const tchar* m_newNodeName;

// TODO: not implemented
/// (bool) make backup file with selection(optional)
/// default value: false
/// cmd arg: -b
	int m_backup;
};

VUtils::ErrorCode VRayProxyParams::parse(const CMD_Args &args)
{
	VUtils::ErrorCode res;

	if ( NOT(args.found('m')) || NOT(args.found('d')) ) {
		res.setError(__FUNCTION__, AE_ARG_NOT_FOUND, "Invalid Usage\n");
		return res;
	}

	m_exportAsSingle = args.iargp('m', 0);

	if ( m_exportAsSingle && NOT(args.found('n')) ) {
		res.setError(__FUNCTION__, AE_ARG_NOT_FOUND, "Ivalid Usage\n");
		return res;
	}

	m_filepath = args.argp('d', 0);
	m_filename = (m_exportAsSingle)? args.argp('n', 0) : nullptr;

	m_includeTransform = m_exportAsSingle || args.found('t');
	m_overwrite = args.found('f');
	m_previewLastSelected = m_exportAsSingle && args.found('l');

	m_exportAnimation = args.found('a');
	m_animStart = (m_exportAnimation)? args.iargp('a', 0) : 0;
	m_animEnd = (m_exportAnimation)? args.iargp('a', 1) : 0;

	if (m_exportAnimation) {
		m_animStart = VUtils::Max(VUtils::Min(m_animStart, m_animEnd), 1l);
		m_animEnd = VUtils::Max(VUtils::Max(m_animStart, m_animEnd), 1l);
	}

	m_exportVelocity = m_exportAnimation && args.found('v');
	m_velocityStart = (m_exportVelocity)? args.fargp('v', 0) : 0;
	m_velocityEnd = (m_exportVelocity)? args.fargp('v', 1) : 0;

	if (m_exportVelocity) {
		m_velocityStart = VUtils::Min(m_velocityStart, m_velocityEnd);
		m_velocityEnd = VUtils::Max(m_velocityStart, m_velocityEnd);
	}

	m_exportPCL = args.found('P');
	m_pointSize = (m_exportPCL)? args.fargp('P', 0) : 0.f;

	maxPreviewFaces = (args.found('F'))? args.iargp('F', 0) : 100;
	maxPreviewStrands = (args.found('H'))? args.iargp('H', 0) : 100;

	int previewType = (args.found('T'))? args.iargp('T', 0) : -1;
	switch (previewType) {
	case 0:
		m_previewType = VUtils::SIMPLIFY_CLUSTERING;
		break;

	case 1:
		m_previewType = VUtils::SIMPLIFY_COMBINED;
		break;

	case 2:
		m_previewType = VUtils::SIMPLIFY_FACE_SAMPLING;
		break;

	case 3:
		m_previewType = VUtils::SIMPLIFY_EDGE_COLLAPSE;
		break;

	default:
		m_previewType = VUtils::SIMPLIFY_COMBINED;
	}

	m_exportColors = args.found('c');
	m_ignoreHidden = args.found('i');
	m_maxFacesPerVoxel = (args.found('x'))? args.iargp('x', 0) : INT_MAX;

	m_autoCreateProxies = args.found('A');
	m_newNodeName = (m_autoCreateProxies)? args.argp('N', 0) : nullptr;
	m_backup = (m_autoCreateProxies)? args.found('b') : 0;

	return res;
}

const tchar* VRayProxyParams::getFilename(const OBJ_Node& node) const
{
	vassert( m_filepath );

	VUtils::CharString filename(m_filepath);
	filename.append("/");

	if (m_exportAsSingle) {
		vassert( m_filename );
		filename.append(m_filename);
	} else {
		filename.append(node.getName().buffer());
		filename.append(".vrmesh");
	}

	return filename.ptr();
}

struct Logger {

	Logger(CMD_Args &args) : args(args)
	{ }

	std::ostream &out() { return args.out(); }
	std::ostream &err() { return args.err(); }

	void logError(const VUtils::ErrorCode &err)
	{
		const int size = COUNT_OF(err.errMsg->msg);
		tchar buffer[ size ];
		err.getErrorString(buffer, size);
		args.error(buffer);
	}

	CMD_Args &args;
};

struct GeometryDescription
{
	GeometryDescription(OBJ_Geometry &node):
		m_node(node)
	{ }
	~GeometryDescription()
	{ }

	const tchar *getVertsAttrName() const { return (m_isHair)? "hair_vertices" : "vertices"; }
	const tchar *getPrimAttrName() const { return (m_isHair)? "num_hair_vertices" : "faces"; }
	int hasValidData() const { return (m_description.contains( getVertsAttrName() ) && m_description.contains( getPrimAttrName() )); }

	Attrs::PluginAttr &getAttr(const tchar * attrName)
	{
		Attrs::PluginAttr *attr = m_description.get(attrName);
		vassert( attr );
		return *attr;
	}
	Attrs::PluginAttr &getVertAttr() { return getAttr(getVertsAttrName()); }
	Attrs::PluginAttr &getPrimAttr() { return getAttr(getPrimAttrName()); }

	void clearData()
	{
		m_isHair = false;
		m_description = Attrs::PluginDesc();
		// make identity
		m_transform.matrix.makeIdentity();
		m_transform.offset.makeZero();
		m_bbox.init();
	}

	OBJ_Geometry &m_node;
	bool m_isHair;
	Attrs::PluginDesc m_description;
	VRay::Transform m_transform;
	VUtils::Box m_bbox;

private:
	GeometryDescription();
	GeometryDescription &operator=(const GeometryDescription &other);
};

class MeshToVRayProxy : public VUtils::MeshInterface
{
public:
	MeshToVRayProxy(const VRayProxyParams &params, OBJ_Geometry * const *nodes, int nodeCnt);
	~MeshToVRayProxy()
	{ clearContextData(); }

	void setContext(const OP_Context& context, Logger &logger);
	void clearContextData();

	int getNumVoxels(void) VRAY_OVERRIDE { return voxels.size(); }
	uint32 getVoxelFlags(int i) VRAY_OVERRIDE;
	VUtils::Box getVoxelBBox(int i) VRAY_OVERRIDE;
	VUtils::MeshVoxel* getVoxel(int i, uint64 *memUsage = NULL) VRAY_OVERRIDE;
	void releaseVoxel(VUtils::MeshVoxel *voxel, uint64 *memUsage = NULL) VRAY_OVERRIDE;

private:
	MeshToVRayProxy():
		previewVerts(nullptr),
		previewFaces(nullptr),
		previewHairVerts(nullptr),
		previewStrands(nullptr)
	{ }

	VUtils::ErrorCode cacheDescriptionForContext(const OP_Context &context, GeometryDescription &geomDescr) const;
	VUtils::ErrorCode getDescriptionForContext(OP_Context &context, GeomExportParams &expParams, GeometryDescription& geomDescr) const;
	void getTransformForContext(OP_Context &context, OBJ_Geometry& node, VRay::Transform &transform) const;

	void buildGeometryVoxel(VUtils::MeshVoxel& voxel, GeometryDescription &meshDescr);
	void buildHairVoxel(VUtils::MeshVoxel& voxel, GeometryDescription &hairDescr);
	void buildPreviewVoxel(VUtils::MeshVoxel& voxel);

	void createMeshPreviewGeometry(VUtils::ObjectInfoChannelData &objInfo);
	void simplifyFaceSampling(int &numPreviewVerts, int &numPreviewFaces,
							  VUtils::ObjectInfoChannelData &objInfo);
	void simplifyMesh(int &numPreviewVerts, int &numPreviewFaces,
					  VUtils::ObjectInfoChannelData &objInfo);
	void createHairPreviewGeometry(VUtils::ObjectInfoChannelData &objInfo);
	void addObjectInfoForType(uint32 type, VUtils::ObjectInfoChannelData &objInfo);

	int getPreviewStartIdx() const;

private:
	VRayProxyParams params;

	std::vector<VUtils::MeshVoxel> voxels;
	std::vector<GeometryDescription> geomDescrList;

	VUtils::VertGeomData *previewVerts;
	VUtils::FaceTopoData *previewFaces;
	VUtils::VertGeomData *previewHairVerts;
	int *previewStrands;
};

MeshToVRayProxy::MeshToVRayProxy(const VRayProxyParams &params, OBJ_Geometry * const *nodes, int nodeCnt):
	previewVerts(nullptr),
	previewFaces(nullptr),
	previewHairVerts(nullptr),
	previewStrands(nullptr)
{
	vassert(nodeCnt > 0);
	vassert(nodes != nullptr);

	this->params = params;

	geomDescrList.reserve(nodeCnt);
	for (int i = 0; i < nodeCnt; ++i) {
		OBJ_Geometry *node = *(nodes + i);
		if (node) {
			geomDescrList.emplace_back(*node);
		}
	}

	voxels.resize(geomDescrList.size() + 1);
}

void MeshToVRayProxy::setContext(const OP_Context &context, Logger& logger)
{
	clearContextData();

	for (auto &geomDescr : geomDescrList){
		VUtils::ErrorCode err_code = cacheDescriptionForContext(context, geomDescr);
		if ( err_code.error() ){
			logger.logError(err_code);
		}
	}
}

void MeshToVRayProxy::clearContextData()
{
	for (auto &voxel : voxels) {
		voxel.freeMem();
	}

	for (auto &geomDecr : geomDescrList) {
		geomDecr.clearData();
	}

	if ( previewVerts )
		FreePtrArr(previewVerts);

	if ( previewFaces )
		FreePtrArr(previewFaces);

	if ( previewHairVerts )
		FreePtrArr(previewHairVerts);

	if ( previewStrands )
		FreePtrArr(previewStrands);
}

VUtils::ErrorCode MeshToVRayProxy::cacheDescriptionForContext(const OP_Context &context, GeometryDescription &geomDescr) const
{
	const UT_String& name = geomDescr.m_node.getName();

	VUtils::ErrorCode res;

	OP_Context cntx = context;
	//    advance current time with velocityStart and get mesh description for that time
	if (params.m_exportVelocity) {
		cntx.setFrame(context.getFloatFrame() + params.m_velocityStart);
	}

	if ( NOT(geomDescr.m_node.isObjectRenderable(cntx.getFloatFrame())) ) {
		res.setError(__FUNCTION__, DE_NO_RENDERABLE_GEOM, "No renderable geometry found for context #%0.3f!", context.getFloatFrame());
		return res;
	}

	GeomExportParams expParams;
	res = getDescriptionForContext(cntx, expParams, geomDescr);
	if (res.error()) {
		return res;
	}

	//    transform normals
	if (params.m_includeTransform) {
		if (geomDescr.m_description.contains("normals")) {
			VRay::VUtils::VectorRefList &normals = geomDescr.getAttr("normals").paramValue.valRawListVector;
			// calc nm(normal matrix) = transpose(inverse(matrix))
			VRay::Matrix nm = geomDescr.m_transform.matrix;
			nm = VRay::Matrix(nm[1]^nm[2], nm[2]^nm[0], nm[0]^nm[1]);
			for (int i = 0; i < normals.size(); ++i) {
				normals[i] = nm * normals[i];
			}
		}
	}

	VRay::VUtils::VectorRefList &verts = geomDescr.getVertAttr().paramValue.valRawListVector;

	//    calc bbox
	geomDescr.m_bbox.init();
	for (int i = 0; i < verts.size(); ++i) {
		geomDescr.m_bbox += VUtils::Vector(verts[i].x, verts[i].y, verts[i].z);
	}

	if (params.m_exportVelocity) {
		// add velocities to description
		VRay::VUtils::VectorRefList velocities(verts.size());
		for (int i = 0; i < velocities.size(); ++i) {
			velocities[i].set(0.f,0.f,0.f);
		}

		float velocityInterval = params.m_velocityEnd - params.m_velocityStart;
		if (velocityInterval > 1e-6f) {
			cntx.setFrame(context.getFloatFrame() + params.m_velocityEnd);

			GeomExportParams nextExpParams;
			nextExpParams.uvWeldThreshold = -1.f;
			nextExpParams.exportMtlIds = false;

			GeometryDescription nextDescr(geomDescr.m_node);
			VUtils::ErrorCode err_code = getDescriptionForContext(cntx, nextExpParams, nextDescr);

			if ( !err_code.error() ) {
				VRay::VUtils::VectorRefList &nextVerts = nextDescr.getVertAttr().paramValue.valRawListVector;
				// if no change in topology calc velocities
				if (verts.size() == nextVerts.size() ) {
					float dt = 1.f/velocityInterval;
					for (int i = 0; i < velocities.size(); ++i) {
						velocities[i] = (nextVerts[i] - verts[i]) * dt;
					}
				}
			}
		}

		geomDescr.m_description.addAttribute(Attrs::PluginAttr("velocities", velocities));
	}

	return res;
}

void MeshToVRayProxy::getTransformForContext(OP_Context &context, OBJ_Geometry &node, VRay::Transform &transform) const
{
	UT_Matrix4 mat;
	UT_Vector3 offs;
	node.getLocalToWorldTransform(context, mat);
	mat.getTranslates(offs);

	transform.matrix.setCol(0, VRay::Vector(mat(0,0), mat(0,1), mat(0,2)));
	transform.matrix.setCol(1, VRay::Vector(mat(1,0), mat(1,1), mat(1,2)));
	transform.matrix.setCol(2, VRay::Vector(mat(2,0), mat(2,1), mat(2,2)));
	transform.offset.set(offs(0), offs(1), offs(2));
}

VUtils::ErrorCode MeshToVRayProxy::getDescriptionForContext(OP_Context &context, GeomExportParams &expParams, GeometryDescription& geomDescr) const
{
	VUtils::ErrorCode res;

	//    query geometry for context
	SOP_Node *sop_node = geomDescr.m_node.getRenderSopPtr();
	if ( NOT(sop_node) ) {
		res.setError(__FUNCTION__, DE_NO_RENDERABLE_GEOM, "No renderable geometry found for context #%0.3f!", context.getFloatFrame());
		return res;
	}

	GU_DetailHandleAutoReadLock gdl(sop_node->getCookedGeoHandle(context));
	const GU_Detail *gdp = gdl.getGdp();

	// NOTE: Could happen, for example, with file node when file is missing
	if ( NOT(gdp) ) {
		res.setError(__FUNCTION__, DE_INCORRECT_GEOM, "Incorrect geometry detail for context #%0.3f!", context.getFloatFrame());
		return res;
	}

	GA_ROAttributeRef ref_guardhair(gdp->findAttribute(GA_ATTRIB_PRIMITIVE, "guardhair"));
	const GA_ROHandleI hnd_guardhair(ref_guardhair.getAttribute());

	GA_ROAttributeRef ref_hairid(gdp->findAttribute(GA_ATTRIB_PRIMITIVE, "hairid"));
	const GA_ROHandleI hnd_hairid(ref_hairid.getAttribute());

	geomDescr.m_isHair = (hnd_guardhair.isValid() && hnd_hairid .isValid());

	VRayExporter exporter(nullptr);
	if (  geomDescr.m_isHair ) {
		exporter.exportGeomMayaHairGeom(sop_node, gdp, geomDescr.m_description);

	} else {
		exporter.exportGeomStaticMeshDesc(*gdp, expParams, geomDescr.m_description);
	}

	if (NOT(geomDescr.hasValidData())) {
		res.setError(__FUNCTION__, DE_INCORRECT_GEOM, "No geometry found for context #%0.3f!", context.getFloatFrame());
		return res;
	}

	if (params.m_includeTransform) {
		VRay::VUtils::VectorRefList &verts = geomDescr.getVertAttr().paramValue.valRawListVector;
		getTransformForContext(context, geomDescr.m_node, geomDescr.m_transform);

		for (int i = 0; i < verts.size(); ++i) {
			verts[i] = geomDescr.m_transform.matrix * verts[i] + geomDescr.m_transform.offset;
		}

	} else {
		// make identity
		geomDescr.m_transform.matrix.makeIdentity();
		geomDescr.m_transform.offset.makeZero();
	}

	return res;
}



uint32 MeshToVRayProxy::getVoxelFlags(int i)
{
	vassert( i >= 0 && i < getNumVoxels() );

	if ( i == (getNumVoxels()-1) ) {
		return MVF_PREVIEW_VOXEL;
	}

	const GeometryDescription &meshDescr = geomDescrList[i];
	return (meshDescr.m_isHair)? MVF_HAIR_GEOMETRY_VOXEL : MVF_GEOMETRY_VOXEL;
}

VUtils::Box MeshToVRayProxy::getVoxelBBox(int i)
{
	vassert( i >= 0 && i < getNumVoxels() );

	if ( i < (getNumVoxels()-1) ) {
		return geomDescrList[i].m_bbox;
	}

	VUtils::Box previewBBox;
	for (const auto &geomDecr : geomDescrList) {
		previewBBox += geomDecr.m_bbox;
	}

	return previewBBox;
}

VUtils::MeshVoxel* MeshToVRayProxy::getVoxel(int i, uint64 *memUsage)
{
	vassert( i >= 0 && i < getNumVoxels() );

	VUtils::MeshVoxel &voxel = voxels[i];
	voxel.init();
	voxel.index = i;

	if ( i == (getNumVoxels()-1) ) {
		buildPreviewVoxel(voxel);
	} else if (geomDescrList[i].m_isHair) {
		buildHairVoxel(voxel, geomDescrList[i]);
	} else {
		buildGeometryVoxel(voxel, geomDescrList[i]);
	}

	if (memUsage) {
		*memUsage = voxel.getMemUsage();
	}

	return &voxel;
}


void MeshToVRayProxy::releaseVoxel(VUtils::MeshVoxel *voxel, uint64 *memUsage)
{
	Log::getLog().info("Release voxel");

	if (voxel) {
		if (memUsage) {
			*memUsage = voxel->getMemUsage();
		}

		voxel->freeMem();
	}
}


OBJ_Geometry * isRenderableGeometry(const VRayProxyParams &params, OBJ_Node &node)
{
	OBJ_Geometry * geom = node.castToOBJGeometry();
	if ( NOT(geom) )
		return nullptr;

	//    query geometry for context
	SOP_Node *sop_node = node.getRenderSopPtr();
	if ( NOT(sop_node) )
		return nullptr;

	//    skip hidden and template geometry if ignoreHidden option is on
	if ( params.m_ignoreHidden && (NOT(geom->getVisible()) || sop_node->getTemplate()) )
		return nullptr;

	//    skip nodes for which ".vrmesh" file already exists when overwrite option is off
	const tchar* filename = params.getFilename(*geom);
	if ( VUtils::uniPathOrFileExists(filename) && NOT(params.m_overwrite))
		return nullptr;

	return geom;
}

OBJ_Geometry * isFurSubNet(OBJ_Node &node)
{
	OBJ_Geometry * geom  = nullptr;
	OBJ_SubNet * subnet = node.castToOBJSubNet();
	if ( NOT(subnet) )
		return geom;

	OP_Context context( CHgetEvalTime() );

	int nkids = subnet->getNchildren();
	for (int i = 0; i < nkids; ++i) {
		OBJ_Node * node = subnet->getChild(i)->castToOBJNode();
		if ( NOT(node) )
			continue;

		geom = node->castToOBJGeometry();
		if ( NOT(geom) || NOT(geom->getDisplay()) ) {
			geom = nullptr;
			continue;
		}

		//    query geometry for context
		SOP_Node *sop_node = geom->getRenderSopPtr();
		if ( NOT(sop_node) ) {
			geom = nullptr;
			continue;
		}

		GU_DetailHandleAutoReadLock gdl(sop_node->getCookedGeoHandle(context));
		const GU_Detail *gdp = gdl.getGdp();
		if ( NOT(gdp) ) {
			geom = nullptr;
			continue;
		}

		GA_ROAttributeRef ref_guardhair(gdp->findAttribute(GA_ATTRIB_PRIMITIVE, "guardhair"));
		const GA_ROHandleI hnd_guardhair(ref_guardhair.getAttribute());

		GA_ROAttributeRef ref_hairid(gdp->findAttribute(GA_ATTRIB_PRIMITIVE, "hairid"));
		const GA_ROHandleI hnd_hairid(ref_hairid.getAttribute());

		//        if attributes exist then this is hair geometry
		if ( hnd_guardhair.isValid() && hnd_hairid .isValid() ) {
			break;
		}

		geom = nullptr;
	}

	return geom;
}

void filterSubNetGeometry(const VRayProxyParams &params, OBJ_SubNet &subnet, std::vector<OBJ_Geometry *> &geometryNodes)
{
	int nkids = subnet.getNchildren();
	for (int i = 0; i < nkids; ++i) {
		OBJ_Node * node = subnet.getChild(i)->castToOBJNode();
		if ( NOT(node) || NOT(node->getDisplay()) )
			continue;

		OBJ_Geometry * geom = isRenderableGeometry(params, *node);
		if ( geom ) {
			geometryNodes.push_back(geom);
			continue;
		}

		geom = isFurSubNet(*node);
		if ( geom ) {
			SOP_Node *sop_node = geom->getRenderSopPtr();
			//    skip hidden and template geometry if ignoreHidden option is on
			if ( params.m_ignoreHidden && (NOT(geom->getVisible()) || sop_node->getTemplate()) )
				continue;

			//    skip nodes for which ".vrmesh" file already exists when overwrite option is off
			const tchar* filename = params.getFilename(*node);
			if ( VUtils::uniPathOrFileExists(filename) &&  NOT(params.m_overwrite) )
				continue;

			geometryNodes.push_back(geom);
			continue;
		}

		OBJ_SubNet * obj_subnet = node->castToOBJSubNet();
		if ( obj_subnet ) {
			filterSubNetGeometry(params, *obj_subnet, geometryNodes);
		}
	}
}

void filterGeometryFromSelection(const VRayProxyParams &params, std::vector<OBJ_Geometry*> &geometryNodes)
{
	OP_Director * opDirector = OPgetDirector();
	vassert( opDirector );

	OP_NodeList allSelected;
	opDirector->getPickedNodes(allSelected);
	geometryNodes.reserve(allSelected.size());

	for (OP_NodeList::const_iterator nodeIt = allSelected.begin(); nodeIt != allSelected.end(); ++nodeIt) {
		OP_Node *op_node = (*nodeIt);
		if (NOT(op_node)){
			continue;
		}

		OBJ_Node * node = op_node->castToOBJNode();
		if ( NOT(node) )
			continue;

		OBJ_Geometry * geom = isRenderableGeometry(params, *node);
		if ( geom ) {
			geometryNodes.push_back(geom);
			continue;
		}

		geom = isFurSubNet(*node);
		if ( geom ) {
			SOP_Node *sop_node = geom->getRenderSopPtr();
			//    skip hidden and template geometry if ignoreHidden option is on
			if ( params.m_ignoreHidden && (NOT(geom->getVisible()) || sop_node->getTemplate()) )
				continue;

			//    skip nodes for which ".vrmesh" file already exists when overwrite option is off
			const tchar* filename = params.getFilename(*node);
			if ( VUtils::uniPathOrFileExists(filename) &&  NOT(params.m_overwrite) )
				continue;

			geometryNodes.push_back(geom);
			continue;
		}

		OBJ_SubNet * obj_subnet = node->castToOBJSubNet();
		if ( obj_subnet ) {
			filterSubNetGeometry(params, *obj_subnet, geometryNodes);
		}
	}
}

void exportFrame(OP_Context& context,
				 const VRayProxyParams &params,
				 const std::vector<OBJ_Geometry*> &nodes,
				 Logger &logger)
{
	logger.out() << "Exporting frame " << context.getFrame() << "\n";

	int nNodeCnt = ((params.m_exportAsSingle)? nodes.size() : 1);
	for (int i = 0; i < nodes.size(); i += nNodeCnt) {

		MeshToVRayProxy meshExporter(params, nodes.data() + i, nNodeCnt);
		meshExporter.setContext(context, logger);

		const tchar* filename = params.getFilename( **(nodes.data() + i) );
		bool append = params.m_exportAnimation && (context.getFrame() > params.m_animStart);
		logger.out() << "Export frame: " << context.getFrame() << " filename: " << filename << " append to file: " << append << "\n";

		VUtils::SubdivisionParams subdivParams( params.m_maxFacesPerVoxel );
		VUtils::ErrorCode err_code = VUtils::subdivideMeshToFile(&meshExporter, filename, subdivParams, NULL, append, params.m_exportPCL, params.m_pointSize);

		if (err_code.error()) {
			logger.logError(err_code);
		}
	}
}

void CMD::vrayproxy(CMD_Args &args)
{
	Logger logger(args);
	VRayProxyParams params;
	VUtils::ErrorCode err_code = params.parse(args);
	if ( err_code.error() ) {
		logger.logError(err_code);
		return;
	}

	std::vector<OBJ_Geometry *> geomNodes;
	filterGeometryFromSelection(params, geomNodes);

	if ( NOT(geomNodes.size()) ) {
		logger.out() << "No geometry found! Skipping export!!!\n";
		return;
	}

	//    export frame range
	if ( params.m_exportAnimation ) {
		logger.out() << "Exporting animation range ("
					 << params.m_animStart << ", "<< params.m_animEnd << ")\n";

		for (long curFrame = params.m_animStart; curFrame <= params.m_animEnd; ++curFrame) {
			OP_Context context;
			context.setFrame(curFrame);

			exportFrame(context, params, geomNodes, logger);
		}
	} else {
		logger.out() << "Exporting single frame\n";

		OP_Context context;
		context.setTime( CHgetEvalTime() );

		exportFrame(context, params, geomNodes, logger);
	}

	logger.out() << "vrayproxy DONE\n";
}


void MeshToVRayProxy::buildGeometryVoxel(VUtils::MeshVoxel& voxel, GeometryDescription& meshDescr)
{
	int hasVerts = meshDescr.m_description.contains("vertices");
	int hasFaces = meshDescr.m_description.contains("faces");
	int hasMtlIDs = meshDescr.m_description.contains("face_mtlIDs");
	int hasNormals = meshDescr.m_description.contains("normals");
	int hasVelocities = meshDescr.m_description.contains("velocities");
	int nUVChannels = meshDescr.m_description.contains("map_channels");

	if (nUVChannels) {
		VRay::ValueList &map_channels = meshDescr.getAttr("map_channels").paramValue.valListValue;
		nUVChannels = map_channels.size();
	}

	voxel.numChannels = hasVerts + hasVelocities + hasFaces + hasMtlIDs + hasNormals * 2 + nUVChannels * 2;
	voxel.channels = new VUtils::MeshChannel[voxel.numChannels];

	int ch_idx = 0;

	//    init vertex channel
	if (hasVerts) {
		Log::getLog().info("buildGeomVoxel populate VERT_GEOM_CHANNEL");

		VRay::VUtils::VectorRefList &vertices = meshDescr.getAttr("vertices").paramValue.valRawListVector;

		VUtils::MeshChannel &verts_ch = voxel.channels[ ch_idx++ ];
		verts_ch.init( sizeof(VUtils::VertGeomData), vertices.size(), VERT_GEOM_CHANNEL, FACE_TOPO_CHANNEL, MF_VERT_CHANNEL, false);
		verts_ch.data = vertices.get();
	}
	//    init velocity channel
	if (hasVelocities) {
		Log::getLog().info("buildGeomVoxel populate VERT_VELOCITY_CHANNEL");

		VRay::VUtils::VectorRefList &velocities = meshDescr.getAttr("velocities").paramValue.valRawListVector;

		VUtils::MeshChannel &velocity_ch = voxel.channels[ ch_idx++ ];
		velocity_ch.init( sizeof(VUtils::VertGeomData), velocities.size(), VERT_VELOCITY_CHANNEL, FACE_TOPO_CHANNEL, MF_VERT_CHANNEL, false);
		velocity_ch.data = velocities.get();
	}
	//    init face channel
	if (hasFaces) {
		Log::getLog().info("buildGeomVoxel populate FACE_TOPO_CHANNEL");

		VRay::VUtils::IntRefList &faces = meshDescr.getAttr("faces").paramValue.valRawListInt;

		VUtils::MeshChannel &face_ch = voxel.channels[ ch_idx++ ];
		face_ch.init(sizeof(VUtils::FaceTopoData), faces.size() / 3, FACE_TOPO_CHANNEL, 0, MF_TOPO_CHANNEL, false);
		face_ch.data = faces.get();
	}
	//    init face info channel
	if (hasMtlIDs) {
		Log::getLog().info("buildGeomVoxel populate FACE_INFO_CHANNEL");

		VRay::VUtils::IntRefList &faceMtlIDs = meshDescr.getAttr("face_mtlIDs").paramValue.valRawListInt;

		VUtils::MeshChannel &faceinfo_ch = voxel.channels[ ch_idx++ ];
		faceinfo_ch.init(sizeof(VUtils::FaceInfoData), faceMtlIDs.size(), FACE_INFO_CHANNEL, 0, MF_FACE_CHANNEL, true);
		for(int i = 0; i < faceMtlIDs.size(); ++i) {
			((VUtils::FaceInfoData*)faceinfo_ch.data)[i].mtlID = faceMtlIDs[i];
		}
	}
	//    init normals channels
	if (hasNormals) {
		Log::getLog().info("buildGeomVoxel populate VERT_NORMAL_CHANNEL");

		VRay::VUtils::VectorRefList &normals = meshDescr.getAttr("normals").paramValue.valRawListVector;

		VUtils::MeshChannel &normal_ch = voxel.channels[ ch_idx++ ];
		normal_ch.init(sizeof(VUtils::VertGeomData), normals.count(), VERT_NORMAL_CHANNEL, VERT_NORMAL_TOPO_CHANNEL, MF_VERT_CHANNEL, false);
		normal_ch.data = normals.get();

		VRay::VUtils::IntRefList &facenormals = meshDescr.getAttr("faceNormals").paramValue.valRawListInt;

		VUtils::MeshChannel &facenormal_ch = voxel.channels[ ch_idx++ ];
		facenormal_ch.init(sizeof(VUtils::FaceTopoData), facenormals.count() / 3, VERT_NORMAL_TOPO_CHANNEL, 0, MF_TOPO_CHANNEL, false);
		facenormal_ch.data = facenormals.get();
	}

	//    TODO: need to check how to add color sets in houdini, mark and export them in separate channel
	//    init uv channels
	if (nUVChannels) {
		VRay::ValueList &mapChannels = meshDescr.getAttr("map_channels").paramValue.valListValue;

		for (int i = 0; i < nUVChannels; ++i) {
			VRay::ValueList &mapChannel = mapChannels[i].as<VRay::ValueList>();

			int uvchannel_idx = mapChannel[0].as<int>();
			VRay::VectorList &uv_verts = mapChannel[1].as<VRay::VectorList>();
			VRay::IntList &uv_faces = mapChannel[2].as<VRay::IntList>();

			Log::getLog().info("buildGeomVoxel populate VERT_TEX_CHANNEL %d", uvchannel_idx);

			VUtils::MeshChannel &uvverts_ch = voxel.channels[ ch_idx++ ];
			uvverts_ch.init(sizeof(VUtils::VertGeomData), uv_verts.size(),  VERT_TEX_CHANNEL0 + uvchannel_idx, VERT_TEX_TOPO_CHANNEL0 + uvchannel_idx, MF_VERT_CHANNEL, false);
			uvverts_ch.data = uv_verts.data();

			VUtils::MeshChannel &uvface_ch = voxel.channels[ ch_idx++ ];
			uvface_ch.init(sizeof(VUtils::FaceTopoData), uv_faces.size() / 3, VERT_TEX_TOPO_CHANNEL0 + uvchannel_idx, 0, MF_TOPO_CHANNEL, false);
			uvface_ch.data = uv_faces.data();
		}
	}

	vassert( voxel.numChannels == ch_idx );
}

void MeshToVRayProxy::buildHairVoxel(VUtils::MeshVoxel& voxel, GeometryDescription &hairDescr)
{
	int hasVerts = hairDescr.m_description.contains("hair_vertices");
	int hasStrands = hairDescr.m_description.contains("num_hair_vertices");
	int hasWidths = hairDescr.m_description.contains("widths");
	int hasVelocities = hairDescr.m_description.contains("velocities");

	voxel.numChannels = hasVerts + hasStrands + hasVelocities + hasWidths;
	voxel.channels = new VUtils::MeshChannel[voxel.numChannels];
	int ch_idx = 0;

	if ( hasVerts ) {
		Log::getLog().info("buildHairVoxel populate HAIR_VERT_CHANNEL");
		VRay::VUtils::VectorRefList &vertices = hairDescr.getAttr("hair_vertices").paramValue.valRawListVector;

		VUtils::MeshChannel &vertices_ch = voxel.channels[ ch_idx++ ];
		vertices_ch.init( sizeof(VUtils::VertGeomData), vertices.size(), HAIR_VERT_CHANNEL, HAIR_NUM_VERT_CHANNEL, MF_VERT_CHANNEL, false);
		vertices_ch.data = vertices.get();
	}

	if ( hasStrands ) {
		Log::getLog().info("buildHairVoxel populate HAIR_NUM_VERT_CHANNEL");
		VRay::VUtils::IntRefList &strands = hairDescr.getAttr("num_hair_vertices").paramValue.valRawListInt;

		VUtils::MeshChannel &strands_ch = voxel.channels[ ch_idx++ ];
		strands_ch.init( sizeof(int), strands.size(), HAIR_NUM_VERT_CHANNEL, 0, MF_NUM_VERT_CHANNEL, false);
		strands_ch.data = strands.get();
	}

	if ( hasWidths ) {
		Log::getLog().info("buildHairVoxel populate HAIR_WIDTH_CHANNEL");
		VRay::VUtils::FloatRefList &widths = hairDescr.getAttr("widths").paramValue.valRawListFloat;

		VUtils::MeshChannel &width_ch = voxel.channels[ ch_idx++ ];
		width_ch.init( sizeof(float), widths.size(), HAIR_WIDTH_CHANNEL, HAIR_NUM_VERT_CHANNEL, MF_VERT_CHANNEL, false);
		width_ch.data = widths.get();
	}

	if ( hasVelocities ) {
		Log::getLog().info("buildHairVoxel populate HAIR_VELOCITY_CHANNEL");
		VRay::VUtils::VectorRefList &velocities = hairDescr.getAttr("velocities").paramValue.valRawListVector;

		VUtils::MeshChannel &velocities_ch = voxel.channels[ ch_idx++ ];
		velocities_ch.init( sizeof(VUtils::VertGeomData), velocities.size(), HAIR_VELOCITY_CHANNEL, HAIR_NUM_VERT_CHANNEL, MF_VERT_CHANNEL, false);
		velocities_ch.data = velocities.get();
	}

	vassert( voxel.numChannels == ch_idx );
}

void MeshToVRayProxy::buildPreviewVoxel(VUtils::MeshVoxel& voxel)
{
	VUtils::ObjectInfoChannelData meshObjInfo;
	createMeshPreviewGeometry(meshObjInfo);

	VUtils::ObjectInfoChannelData hairObjInfo;
	createHairPreviewGeometry(hairObjInfo);

	//    init uv sets data
	VUtils::DefaultMeshSetsData setsInfo;
	int nTotalUVChannels = 0;
	for (auto &meshDescr : geomDescrList) {
		if ( meshDescr.m_description.contains("map_channel_names") ) {
			const VRay::ValueList &mapChannelNames = meshDescr.getAttr("map_channel_names").paramValue.valListValue;
			nTotalUVChannels += mapChannelNames.size();
		}
	}

	setsInfo.setNumSets(VUtils::MeshSetsData::meshSetType_uvSet, nTotalUVChannels);

	int offset = 0;
	for (auto &meshDescr : geomDescrList) {
		if (meshDescr.m_description.contains("map_channel_names")) {
			VRay::ValueList &mapChannelNames = meshDescr.getAttr("map_channel_names").paramValue.valListValue;

			for (auto &value : mapChannelNames) {
				std::string &channelName = value.as<std::string>();
				setsInfo.setSetName(VUtils::MeshSetsData::meshSetType_uvSet, offset++, channelName.c_str());
			}
		}
	}

	int numMeshObj = meshObjInfo.getNumObjectInfos();
	int numHairObj = hairObjInfo.getNumObjectInfos();
	int numUVSets = setsInfo.getNumSets(VUtils::MeshSetsData::meshSetType_uvSet);

	voxel.numChannels = ((bool)numMeshObj)*3 + ((bool)numHairObj)*3 + (bool)numUVSets;
	voxel.channels = new VUtils::MeshChannel[voxel.numChannels];

	int ch_idx = 0;
	if ( numMeshObj ) {
		//    vertex channel
		Log::getLog().info("buildPreviewVoxel populate VERT_GEOM_CHANNEL");

		VUtils::MeshChannel &verts_ch = voxel.channels[ ch_idx++ ];
		verts_ch.init( sizeof(VUtils::VertGeomData), meshObjInfo.getTotalPreviewVertices(), VERT_GEOM_CHANNEL, FACE_TOPO_CHANNEL, MF_VERT_CHANNEL, false);
		verts_ch.data = previewVerts;

		//    face channel
		Log::getLog().info("buildPreviewVoxel populate FACE_TOPO_CHANNEL");

		VUtils::MeshChannel &face_ch = voxel.channels[ ch_idx++ ];
		face_ch.init( sizeof(VUtils::FaceTopoData), meshObjInfo.getTotalPreviewElements(), FACE_TOPO_CHANNEL, 0, MF_TOPO_CHANNEL, false);
		face_ch.data = previewFaces;

		//    obj info channel
		Log::getLog().info("buildPreviewVoxel populate OBJECT_INFO_CHANNEL");

		int bufLen = 0;
		meshObjInfo.writeToBuffer(NULL, bufLen);

		VUtils::MeshChannel &info_ch = voxel.channels[ ch_idx++ ];
		info_ch.init(bufLen, 1, OBJECT_INFO_CHANNEL, 0, MF_INFO_CHANNEL);
		meshObjInfo.writeToBuffer(info_ch.data, bufLen);
	}

	if (numHairObj) {
		//    vertex channel
		Log::getLog().info("buildPreviewVoxel populate HAIR_VERT_CHANNEL");

		VUtils::MeshChannel &verts_ch = voxel.channels[ ch_idx++ ];
		verts_ch.init( sizeof(VUtils::VertGeomData), hairObjInfo.getTotalPreviewVertices(), HAIR_VERT_CHANNEL, HAIR_NUM_VERT_CHANNEL, MF_VERT_CHANNEL, false);
		verts_ch.data = previewHairVerts;

		//    strand channel
		Log::getLog().info("buildPreviewVoxel populate HAIR_NUM_VERT_CHANNEL");

		VUtils::MeshChannel &strands_ch = voxel.channels[ ch_idx++ ];
		strands_ch.init( sizeof(int), hairObjInfo.getTotalPreviewElements(), HAIR_NUM_VERT_CHANNEL, 0, MF_NUM_VERT_CHANNEL, false);
		strands_ch.data = previewStrands;

		//    obj info channel
		Log::getLog().info("buildPreviewVoxel populate HAIR_OBJECT_INFO_CHANNEL");

		int bufLen = 0;
		hairObjInfo.writeToBuffer(NULL, bufLen);

		VUtils::MeshChannel &info_ch = voxel.channels[ ch_idx++ ];
		info_ch.init(bufLen, 1, HAIR_OBJECT_INFO_CHANNEL, 0, MF_INFO_CHANNEL);
		hairObjInfo.writeToBuffer(info_ch.data, bufLen);
	}

	if (numUVSets) {
		//    uv sets channel
		Log::getLog().info("buildPreviewVoxel populate MAYA_INFO_CHANNEL");
		int bufLen = 0;
		setsInfo.writeToBuffer(NULL, bufLen);

		VUtils::MeshChannel &info_ch = voxel.channels[ ch_idx++ ];
		info_ch.init(bufLen, 1, MAYA_INFO_CHANNEL, 0, MF_MAYA_INFO_CHANNEL);
		setsInfo.writeToBuffer(info_ch.data, bufLen);
	}

	vassert( voxel.numChannels == ch_idx );
}

void MeshToVRayProxy::createHairPreviewGeometry(VUtils::ObjectInfoChannelData &objInfo)
{
	addObjectInfoForType(MVF_HAIR_GEOMETRY_VOXEL, objInfo);

	int nTotalStrands = 0;
	int maxVertsPerStrand = 0;
	int previewStartIdx = getPreviewStartIdx();

//	calc total number of strands for hairs that will be preview
	for (int j = previewStartIdx; j < geomDescrList.size(); ++j) {
//		skip geometry other than hair
		if (NOT(getVoxelFlags(j) & MVF_HAIR_GEOMETRY_VOXEL)) {
			continue;
		}

		GeometryDescription &meshDescr =  geomDescrList[j];
		if (NOT(meshDescr.hasValidData())) {
			continue;
		}

		VRay::VUtils::IntRefList &strands = meshDescr.getAttr("num_hair_vertices").paramValue.valRawListInt;
		for (int i = 0; i < strands.count(); ++i) {
			maxVertsPerStrand = VUtils::Max(maxVertsPerStrand, strands[i]);
		}

		nTotalStrands += strands.count();
	}

	//    build hair preview geometry by skipping strands
	int numPreviewVerts = 0;
	int numPreviewStrands = 0;
	const int maxPreviewStrands = VUtils::Min(params.maxPreviewStrands, nTotalStrands);
	if (maxPreviewStrands > 0 && nTotalStrands > 0) {
		previewHairVerts = new VUtils::VertGeomData[ maxPreviewStrands * maxVertsPerStrand ];
		previewStrands = new int[ maxPreviewStrands ];

		const float nStrands = static_cast<float>(maxPreviewStrands) / nTotalStrands;
		for (int j = previewStartIdx; j < geomDescrList.size(); ++j) {
	//		skip geometry other than hair
			if (NOT(getVoxelFlags(j) & MVF_HAIR_GEOMETRY_VOXEL)) {
				continue;
			}

			GeometryDescription &meshDescr =  geomDescrList[j];
			if (NOT(meshDescr.hasValidData())) {
				continue;
			}

			VRay::VUtils::IntRefList &strands = meshDescr.getAttr("num_hair_vertices").paramValue.valRawListInt;
			VRay::VUtils::VectorRefList &vertices = meshDescr.getAttr("hair_vertices").paramValue.valRawListVector;

			int numStrandsPrevObj = numPreviewStrands;

			int voffset = 0;
			for(int i = 0; i < strands.count(); ++i) {
				int p0 =  i * nStrands;
				int p1 = (i+1) * nStrands;

				if (p0 != p1 && numPreviewStrands < maxPreviewStrands) {
					previewStrands[ numPreviewStrands++ ] = strands[i];
					for(int k = 0; k < strands[i]; ++k) {
						VRay::Vector &v = vertices[voffset + k];
						previewHairVerts[ numPreviewVerts++ ].set(v.x, v.y, v.z);
					}

					if (numPreviewStrands >= maxPreviewStrands)
						break;
				}

				voffset += strands[i];
			}

			objInfo.setElementRange(j, numStrandsPrevObj, numPreviewStrands);
			objInfo.addVoxelData(j, vertices.count(), strands.count());
		}
	}

	objInfo.setTotalPreviewVertices(numPreviewVerts);
	objInfo.setTotalPreviewElements(numPreviewStrands);

	vassert( numPreviewVerts <= maxPreviewStrands * maxVertsPerStrand );
	vassert( numPreviewStrands <= maxPreviewStrands );
}

void MeshToVRayProxy::createMeshPreviewGeometry(VUtils::ObjectInfoChannelData &objInfo)
{
	addObjectInfoForType(MVF_GEOMETRY_VOXEL, objInfo);

	int numPreviewVerts = 0;
	int numPreviewFaces = 0;

	if (params.m_previewType == VUtils::SIMPLIFY_FACE_SAMPLING) {
		simplifyFaceSampling(numPreviewVerts, numPreviewFaces, objInfo);
	} else {
		simplifyMesh(numPreviewVerts, numPreviewFaces, objInfo);
	}

	objInfo.setTotalPreviewVertices(numPreviewVerts);
	objInfo.setTotalPreviewElements(numPreviewFaces);
}

void MeshToVRayProxy::simplifyFaceSampling(int &numPreviewVerts, int &numPreviewFaces,
										   VUtils::ObjectInfoChannelData &objInfo)
{
	numPreviewVerts = 0;
	numPreviewFaces = 0;
	int nTotalFaces = 0;
	int previewStartIdx = getPreviewStartIdx();
	for (int j = previewStartIdx; j < geomDescrList.size(); ++j) {
//			skip other geometry than mesh
		if (NOT(getVoxelFlags(j) & MVF_GEOMETRY_VOXEL)) {
			continue;
		}

		GeometryDescription &meshDescr =  geomDescrList[j];
		if (NOT(meshDescr.hasValidData())) {
			continue;
		}

		VRay::VUtils::IntRefList &faces = meshDescr.getAttr("faces").paramValue.valRawListInt;
		nTotalFaces += faces.count() / 3;
	}

	int maxPreviewFaces = VUtils::Min(params.maxPreviewFaces, nTotalFaces);
	if (maxPreviewFaces > 0 && nTotalFaces > 0) {
		previewVerts = new VUtils::VertGeomData[ maxPreviewFaces * 3 ];
		previewFaces = new VUtils::FaceTopoData[ maxPreviewFaces ];

		const float nFaces = static_cast<float>(maxPreviewFaces) / nTotalFaces;
		for (int j = previewStartIdx; j < geomDescrList.size(); ++j) {
//			skip other geometry than mesh
			if (NOT(getVoxelFlags(j) & MVF_GEOMETRY_VOXEL)) {
				continue;
			}

			GeometryDescription &meshDescr =  geomDescrList[j];
			if (NOT(meshDescr.hasValidData())) {
				continue;
			}

			VRay::VUtils::VectorRefList &vertices = meshDescr.getAttr("vertices").paramValue.valRawListVector;
			VRay::VUtils::IntRefList &faces = meshDescr.getAttr("faces").paramValue.valRawListInt;

			int numFacesPrevObj = numPreviewFaces;

			for(int i = 0; i < faces.count() / 3; ++i) {
				int p0 =  i * nFaces;
				int p1 = (i+1) * nFaces;
				if (p0 != p1 && numPreviewFaces < maxPreviewFaces) {
					for(int k = 0; k < 3; ++k) {
						VRay::Vector &v = vertices[faces[i*3+k]];
						previewFaces[numPreviewFaces].v[k] = numPreviewVerts;
						previewVerts[numPreviewVerts++].set(v.x, v.y, v.z);
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

	vassert( numPreviewFaces <= maxPreviewFaces );
	vassert( numPreviewVerts <= (maxPreviewFaces * 3) );
}

void MeshToVRayProxy::simplifyMesh(int &numPreviewVerts, int &numPreviewFaces,
								   VUtils::ObjectInfoChannelData &objInfo)
{
	numPreviewVerts = 0;
	numPreviewFaces = 0;
	int nTotalFaces = 0;
	int previewStartIdx = getPreviewStartIdx();
	VUtils::SimplifierMesh mesh;
	for (int j = previewStartIdx; j < geomDescrList.size(); ++j) {
//			skip other than mesh geometry
		if (NOT(getVoxelFlags(j) & MVF_GEOMETRY_VOXEL)) {
			continue;
		}

		GeometryDescription &meshDescr =  geomDescrList[j];
		if (NOT(meshDescr.hasValidData())) {
			continue;
		}

		VRay::VUtils::VectorRefList &vertices = meshDescr.getAttr("vertices").paramValue.valRawListVector;
		VRay::VUtils::IntRefList &faces = meshDescr.getAttr("faces").paramValue.valRawListInt;

		objInfo.addVoxelData(j, vertices.count(), faces.count());

		int vertOffset = mesh.countVertices();
		for (int i = 0; i != vertices.size(); ++i)
			mesh.addVertex(VUtils::Vector(vertices[i].x, vertices[i].y, vertices[i].z));

		for (int i = 0; i != faces.size();) {
			int nullValues[3] = {-1,-1,-1};
			int verts[3];
			for(int p = 0 ; p < 3 ; ++p)
				verts[p] = faces[i++];

			mesh.addFace(verts, nullValues, nullValues, vertOffset);
			++nTotalFaces;
		}
	}

	int maxPreviewFaces = VUtils::Min(params.maxPreviewFaces, nTotalFaces);
	if (maxPreviewFaces > 0) {
		VUtils::simplifyMesh(mesh, params.m_previewType, maxPreviewFaces);
		mesh.exportMesh(previewVerts, previewFaces, numPreviewVerts, numPreviewFaces);
	}
}

void MeshToVRayProxy::addObjectInfoForType(uint32 type, VUtils::ObjectInfoChannelData &objInfo)
{
	objInfo.setTotalVoxelCount(voxels.size());
	for (int j = 0; j < geomDescrList.size(); ++j) {
//		skip other geometry
		if (NOT(getVoxelFlags(j) & type)) {
			continue;
		}

		UT_String fullName;
		GeometryDescription &meshDescr =  geomDescrList[j];
		meshDescr.m_node.getFullPath(fullName);

		VUtils::ObjectInfo info(meshDescr.m_node.getUniqueId(), fullName);
		info.setVoxelRange(j, j+1);
		objInfo.addObjectInfo(info);
	}
}

int MeshToVRayProxy::getPreviewStartIdx() const
{
	if (params.m_previewLastSelected) {
		return std::max(static_cast<int>(geomDescrList.size() - 1), 0);
	}
	return 0;
}
