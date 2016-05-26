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

#include "vfh_exporter.h"
#include <SOP/SOP_Node.h>
#include <simplifier.h>


namespace VRayForHoudini {

struct VRayProxyExportOptions
{
	VRayProxyExportOptions() :
		m_filepath(UT_String::ALWAYS_DEEP),
		m_exportAsSingle(true),
		m_animation(false),
		m_animStart(0),
		m_animEnd(0),
		m_animtime(0),
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

	inline bool              isSequentialFrame() const { return (m_animation && m_animtime > 0); }
	VRayProxyExportOptions & extendFilepath(const SOP_Node &sop);

///	filepath to the .vrmesh file
/// cmd arg: -n "path/to/filename.vrmesh" (mandatory)
	UT_String m_filepath;

///	(bool) export all geometry in single .vrmesh file
/// 0 = export each object in separate file, 1 = export all objects in single file
/// default value: true
/// cmd arg: -m 0/1 (optional)
	int m_exportAsSingle;

/// (bool) if true - export frame range
/// if false - only current frame will be exported
/// default value: false
/// cmd arg: -a animStart animEnd (optional)
	int m_animation;

/// (int) start frame of the animation
/// default value: 0
/// cmd arg: -a animStart animEnd (mandatory, if animation is true)
	int m_animStart;

/// (int) end frame of the animation
/// default value: 0
/// cmd arg: -a animStart animEnd (mandatory, if animation is true)
	int m_animEnd;

/// (float) current time at which geometry is being exported
/// if 0, it means this will be the first frame saved in file
/// if > 0, it means geometry data will be appended to file
/// it is always 0, if exporting each frame in different file
/// default value: 0
	fpreal m_animtime;

/// (bool) if true, use last selected object as preview geometry
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
/// cmd arg: -x maxFacesPerVoxel (optional)
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
	VRayProxyExporter(SOP_Node * const *nodes, int nodeCnt);
	~VRayProxyExporter();

	void                setOptions(const VRayProxyExportOptions &options) { m_options = options; }
	VUtils::ErrorCode   setContext(const OP_Context &context);
	void                clearContextData();
	VUtils::ErrorCode   doExport();

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
	void getTransformForContext(OP_Context &context, GeometryDescription &geomDescr) const;

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

	int getPreviewStartIdx() const;

private:
	VRayProxyExportOptions m_options;
	VRayExporter         m_exporter;

	std::vector<VUtils::MeshVoxel>   m_voxels;
	std::vector<GeometryDescription> m_geomDescrList;

	VUtils::VertGeomData *m_previewVerts;
	VUtils::FaceTopoData *m_previewFaces;
	VUtils::VertGeomData *m_previewHairVerts;
	int                  *m_previewStrands;
};

}

#endif // VRAY_FOR_HOUDINI_EXPORT_VRAYPROXY_H
