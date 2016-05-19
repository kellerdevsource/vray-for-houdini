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
		m_previewType(VUtils::SIMPLIFY_COMBINED),
		m_maxPreviewFaces(100),
		m_maxPreviewStrands(100),
		m_maxFacesPerVoxel(INT_MAX),
		m_exportColors(false),
		m_exportVelocity(false),
		m_velocityStart(0.f),
		m_velocityEnd(0.f),
		m_exportPCL(false),
		m_pointSize(0.f),
		m_applyTransform(false),
		m_previewLast(false)
	{ }

///	filepath to the .vrmesh file
	VUtils::CharString m_filename;

/// (SimplificationType enum) specifies how to do the simplification
/// default value: SIMPLIFY_COMBINED
	VUtils::SimplificationType m_previewType;

/// (int) max number of faces for preview geometry voxel
/// default value: 100
	int m_maxPreviewFaces;

/// (int) max number of strands for preview geometry voxel
/// default value: 100
	int m_maxPreviewStrands;

/// (int) max number of faces per voxel
/// default value: INT_MAX
	int m_maxFacesPerVoxel;

/// (bool) if true, export vertex color sets
/// default value: false
	int m_exportColors;

/// (bool) if true, export vertex velocity
/// this option is always false if exportAnimation is false
/// default value: false
/// vertex velocity is calulated as: (vert_pos(velocityEndTime) - vert_pos(velocityStartTime)) / (velocityEndTime - velocityStartTime)
	int m_exportVelocity;

/// (float) start time for vertex position sample
/// velocityStart should be in range [0,1)
	float m_velocityStart;

/// (float) end time for vertex position sample
/// velocityEnd should be in range (0,1]
	float m_velocityEnd;

/// (bool) if true, point cloud information will be computed and stored with each voxel in the file
/// default value: false
	int m_exportPCL;

/// (float) specifies the desired density for the points in the point cloud -
/// average area covered by one point
	float m_pointSize;

/// (bool) if true, export geometry in world space(optional)
/// this option is always true when exporting multiple objects to a single file
/// default value: false
/// cmd arg: -t
	int m_applyTransform;

/// (bool) if true, use last selected object as preview geometry(optional)
/// this option is always false if exporting single object per file
/// default value: false
/// cmd arg: -l
	int m_previewLast;
};


class VRayProxyExporter : public VUtils::MeshInterface
{
public:
	VRayProxyExporter(SOP_Node * const *nodes, int nodeCnt);
	~VRayProxyExporter();

	void                setParams(const VRayProxyExportOptions &params) { m_params = params; }
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
	VRayProxyExportOptions m_params;
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
