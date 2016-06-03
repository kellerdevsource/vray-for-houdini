//
// Copyright (c) 2015-2016, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#ifndef VRAY_FOR_HOUDINI_EXPORT_VRAYPROXY_H
#define VRAY_FOR_HOUDINI_EXPORT_VRAYPROXY_H

#include "vfh_plugin_attrs.h"

#include <SOP/SOP_Node.h>

#include <mesh_file.h>
#include <simplifier.h>


namespace VRayForHoudini {

struct VRayProxyExportOptions
{
	VRayProxyExportOptions() :
		m_filepath(UT_String::ALWAYS_DEEP),
		m_mkpath(true),
		m_overwrite(false),
		m_exportAsSingle(true),
		m_exportAsAnimation(true),
		m_animStart(0),
		m_animEnd(0),
		m_animFrames(0),
		m_lastAsPreview(false),
		m_applyTransform(false),
		m_exportVelocity(false),
		m_velocityStart(0.f),
		m_velocityEnd(0.05f),
		m_simplificationType(VUtils::SIMPLIFY_COMBINED),
		m_maxPreviewFaces(100),
		m_maxPreviewStrands(100),
		m_maxFacesPerVoxel(0),
		m_exportPCLs(false),
		m_pointSize(0.5f)
	{ }

	~VRayProxyExportOptions()
	{ }

	inline UT_String getFilepath(const SOP_Node &sop) const;
	inline bool      isAppendMode() const;

///	filepath to the .vrmesh file
/// cmd arg: -n "path/to/filename.vrmesh" (mandatory)
	UT_String m_filepath;

/// (bool) if true, non-existing dirs in the filepath will be created
/// default value: true
/// cmd arg: -r (optional)
	int m_mkpath;

/// (bool) if true, existing file(s) will be overwritten
/// default value: false
/// cmd arg: -f (optional)
	int m_overwrite;

///	(bool) if true, export all geometry in a single .vrmesh file,
/// otherwise export each object in separate file
/// default value: true
/// cmd arg: -m (optional, if given m_exportAsSingle will be set to false)
	int m_exportAsSingle;

/// (bool) if true, export animation range in a single .vrmesh file
/// otherwise only current frame will be exported
/// default value: true
/// cmd arg: -a animStart animEnd (optional)
	int m_exportAsAnimation;

/// (fpreal) start time of the animation
/// default value: 0
/// cmd arg: -a animStart animEnd (mandatory, if m_exportAsAnimation is true)
	fpreal m_animStart;

/// (fpreal) end time of the animation
/// default value: 0
/// cmd arg: -a animStart animEnd (mandatory, if m_exportAsAnimation is true)
	fpreal m_animEnd;

/// (int) number of frames to be exported
/// this is calculated based on m_animStart m_animEnd values
/// default value: 0
	int m_animFrames;

/// (OP_Context) current time at which to export geometry geometry
/// default value: 0
	OP_Context m_context;

/// (bool) if true, use last object in list as preview geometry
/// this option is always false if exporting single object per file
/// default value: false
/// cmd arg: -l (optional)
	int m_lastAsPreview;

/// (bool) if true, export geometry in world space
/// default value: false
/// cmd arg: -t (optional)
	int m_applyTransform;

/// (bool) if true, export vertex velocity
/// this option is always false if exportAnimation is false
/// vertex velocity is calulated as: (vert_pos(velocityEndTime) - vert_pos(velocityStartTime)) / (velocityEndTime - velocityStartTime)
/// default value: false
/// cmd arg: -v velocityStart velocityEnd (optional)
	int m_exportVelocity;

/// (float) start time for vertex position sample
/// velocityStart should be in range [0,1)
/// default value: 0.f
/// cmd arg: -v velocityStart velocityEnd (mandatory, if exportVelocity is true)
	fpreal m_velocityStart;

/// (float) end time for vertex position sample
/// velocityEnd should be in range (0,1]
/// default value: 0.05f
/// cmd arg: -v velocityStart velocityEnd (mandatory, if exportVelocity is true)
	fpreal m_velocityEnd;

/// (SimplificationType enum) specifies how to do the simplification
/// default value: SIMPLIFY_COMBINED
/// cmd arg: -T previewType (optional)
	VUtils::SimplificationType m_simplificationType;

/// (int) max number of faces for preview geometry voxel
/// default value: 100
/// cmd arg: -F maxPreviewFaces (optional)
	int m_maxPreviewFaces;

/// (int) max number of strands for preview geometry voxel
/// default value: 100
/// cmd arg: -H maxPreviewStrands (optional)
	int m_maxPreviewStrands;

/// (int) max number of faces per voxel
/// if 0, assume 1 voxel per mesh
/// default value: 0
/// cmd arg: -X maxFacesPerVoxel (optional)
	int m_maxFacesPerVoxel;

/// (bool) if true, point cloud information will be computed and stored with each voxel in the file
/// default value: false
/// cmd arg: -P pointSize (optional)
	int m_exportPCLs;

/// (float) specifies the desired density for the points in the point cloud -
/// average area covered by one point
/// default value: 2.f
/// cmd arg: -P pointSize (optional)
	fpreal m_pointSize;
};


class VRayProxyExporter : public VUtils::MeshInterface
{
public:
	VRayProxyExporter(const VRayProxyExportOptions &options, SOP_Node * const *nodes, int nodeCnt);
	~VRayProxyExporter();

	VUtils::ErrorCode   init();
	void                cleanup();
	VUtils::ErrorCode   doExportFrame();

	int                 getNumVoxels(void) VRAY_OVERRIDE { return m_voxels.size(); }
	uint32              getVoxelFlags(int i) VRAY_OVERRIDE;
	VUtils::Box         getVoxelBBox(int i) VRAY_OVERRIDE;
	VUtils::MeshVoxel * getVoxel(int i, uint64 *memUsage = NULL) VRAY_OVERRIDE;
	void                releaseVoxel(VUtils::MeshVoxel *voxel, uint64 *memUsage = NULL) VRAY_OVERRIDE;

private:
	struct GeometryDescription
	{
		GeometryDescription(SOP_Node &node) : m_node(node) { }
		~GeometryDescription() { }

		const tchar *getVertsAttrName() const { return (m_isHair)? "hair_vertices" : "vertices"; }
		const tchar *getPrimAttrName() const { return (m_isHair)? "num_hair_vertices" : "faces"; }
		int hasValidData() const;
		void clearData();

		Attrs::PluginAttr &getAttr(const tchar *attrName);
		Attrs::PluginAttr &getVertAttr() { return getAttr(getVertsAttrName()); }
		Attrs::PluginAttr &getPrimAttr() { return getAttr(getPrimAttrName()); }

		SOP_Node &m_node;
		bool m_isHair;
		Attrs::PluginDesc m_description;
		VRay::Transform m_transform;
		VUtils::Box m_bbox;

	private:
		GeometryDescription();
		GeometryDescription &operator=(const GeometryDescription &other);
	};

private:
	VRayProxyExporter();

	VUtils::ErrorCode cacheDescriptionForContext(const OP_Context &context, GeometryDescription &geomDescr);
	VUtils::ErrorCode getDescriptionForContext(OP_Context &context, GeometryDescription &geomDescr);
	void              getTransformForContext(OP_Context &context, GeometryDescription &geomDescr) const;

	void buildMeshVoxel(VUtils::MeshVoxel &voxel, GeometryDescription &meshDescr);
	void buildHairVoxel(VUtils::MeshVoxel &voxel, GeometryDescription &hairDescr);
	void buildPreviewVoxel(VUtils::MeshVoxel &voxel);

	void createMeshPreviewGeometry(VUtils::ObjectInfoChannelData &objInfo);
	void simplifyFaceSampling(int &numPreviewVerts, int &numPreviewFaces,
							  VUtils::ObjectInfoChannelData &objInfo);
	void simplifyMesh(int &numPreviewVerts, int &numPreviewFaces,
					  VUtils::ObjectInfoChannelData &objInfo);
	void createHairPreviewGeometry(VUtils::ObjectInfoChannelData &objInfo);
	void addObjectInfoForType(uint32 type, VUtils::ObjectInfoChannelData &objInfo);

	int  getPreviewStartIdx() const;

private:
	const VRayProxyExportOptions &m_options;

	std::vector<VUtils::MeshVoxel>   m_voxels;
	std::vector<GeometryDescription> m_geomDescrList;

	VUtils::VertGeomData *m_previewVerts;
	VUtils::FaceTopoData *m_previewFaces;
	VUtils::VertGeomData *m_previewStrandVerts;
	int                  *m_previewStrands;
};

}

#endif // VRAY_FOR_HOUDINI_EXPORT_VRAYPROXY_H
