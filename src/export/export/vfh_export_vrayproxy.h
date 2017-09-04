//
// Copyright (c) 2015-2017, Chaos Software Ltd
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
#include "vfh_plugin_attrs.h"

#include <SOP/SOP_Node.h>

#include <mesh_file.h>
#include <simplifier.h>

namespace VRayForHoudini {

/// SOP node array type.
typedef UT_ValArray<SOP_Node*> SOPList;

/// GeometryDescription wraps and caches geometry data for a given sop node
/// @note this is a helper data structure and should only be visible to VRayProxyExporter
class VRayExporterProxy;
struct GeometryDescription {
	friend class VRayExporterProxy;

	enum GeometryDescriptionType {
		geometryDescriptionUnknown = 0,
		geometryDescriptionMesh,
		geometryDescriptionHair,
	};

	GeometryDescription();

	/// Accessor for a plugin attribute
	/// @param attrName[in] - attribute name. Note this name must exist on the plugin description
	/// @retval the plugin attribute
	const Attrs::PluginAttr &getAttr(const char *attrName) const;

	/// Accessor for the attribute name holding the vertices
	/// based on whether this objects represents hair or mesh geometry
	/// @retval attribute name string
	const char *getVertsAttrName() const {
		switch (geometryType) {
			case geometryDescriptionMesh: return "vertices";
			case geometryDescriptionHair: return "hair_vertices";
			default: return "NULL";
		}
	}

	/// Accessor for the attribute name holding the faces
	/// based on whether this objects represents hair or mesh geometry
	/// @retval attribute name string
	const char *getPrimAttrName() const {
		switch (geometryType) {
			case geometryDescriptionMesh: return "faces";
			case geometryDescriptionHair: return "num_hair_vertices";
			default: return "NULL";
		}
	}

	/// Accessor for the vertices plugin attribute
	/// @retval the plugin attribute
	const Attrs::PluginAttr &getVertAttr() const { return getAttr(getVertsAttrName()); }

	/// Accessor for the faces plugin attribute
	/// @retval the plugin attribute
	const Attrs::PluginAttr &getPrimAttr() const { return getAttr(getPrimAttrName()); }

	/// Returns plugin geometry type.
	GeometryDescriptionType getGeometryType() const { return geometryType; }

	/// Geometry type. Hair or mesh for now.
	GeometryDescriptionType geometryType;

	/// Node this geometry comes from.
	OP_Node *opNode;

	/// Cached transform of the geometry
	VRay::Transform transform;

	/// Cached bounding box of the geometry
	VUtils::Box bbox;

	/// Cached plugin description of the geometry.
	Attrs::PluginDesc pluginDesc;
};

typedef VUtils::Table<GeometryDescription, -1> GeometryDescriptions;

class VRayExporterProxy
	: public VRayExporter
{
public:
	VRayExporterProxy(ObjectExporter &objectExporter, GeometryDescriptions &data)
		: VRayExporter(nullptr)
		, objectExporter(objectExporter)
		, data(data)
	{}

	VRay::Plugin exportPlugin(const Attrs::PluginDesc &pluginDesc) VRAY_OVERRIDE;

private:
	ObjectExporter &objectExporter;

	GeometryDescriptions &data;

	VUTILS_DISABLE_COPY(VRayExporterProxy);
};

class VRayProxyObjectExporter
	: public ObjectExporter
{
public:
	explicit VRayProxyObjectExporter(GeometryDescriptions &data)
		: ObjectExporter(pluginExporter)
		, pluginExporter(*this, data)
	{}

	void setContext(const VRayOpContext &value) { pluginExporter.setContext(value); }
	
	/// Get the underlying pluginExporter
	VRayExporterProxy &getPluginExporter() { return pluginExporter; }
private:
	VRayExporterProxy pluginExporter;
};

/// VRayProxyExportOptions wraps all options necessary to export .vrmesh file(s).
/// "vrayproxy" hscript cmd and "V-Ray Proxy ROP" create and configure an instance
/// of this data structure and pass it to VRayProxyExporter
struct VRayProxyExportOptions {
	VRayProxyExportOptions();

	/// Return the .vrmesh filepath and adjust the filename if needed
	/// 1. append the sop path to the filename if separate objects
	///    should be exported into separate files (m_exportAsSingle == false)
	/// 2. append .vrmesh extension if missing
	/// @param sop[in] - the sop node being processed
	/// @retval filepath
	inline UT_String getFilepath(const SOP_Node &sop) const;

	/// Easy accessor to check if new content should be appended to the .vrmesh file
	/// @retval false if we are exporting single frame or the first frame of an animation
	///         true otherwise
	inline bool isAppendMode() const;

	/// filepath to the .vrmesh file
	/// cmd arg: -n "path/to/filename.vrmesh" (mandatory)
	UT_String m_filepath;

	/// (bool) if true, non-existing dirs in the filepath will be created
	/// default value: true
	/// cmd arg: -c (optional)
	int m_mkpath;

	/// (bool) if true, existing file(s) will be overwritten
	/// default value: false
	/// cmd arg: -f (optional)
	int m_overwrite;

	/// (bool) if true, export all geometry in a single .vrmesh file,
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
	/// default value: 0.5f
	/// cmd arg: -P pointSize (optional)
	fpreal m_pointSize;
};

/// VRayProxyExporter wraps the geometry export of a single frame into a single .vrmesh file
class VRayProxyExporter:
		public VUtils::MeshInterface
{
public:
	/// Wrap the actual export of .vrmesh file(s) besed on given geometry and export options
	/// @param options[in] - options used to configure how geometry should be exported
	/// @param sopList[in] - list of geometry sop nodes. Note that options.m_context would be modified
	///                       if we are exporting animation
	/// @retval error code - use ErrorCode::error() to check for errors
	///                      and ErrorCode::getErrorString() to get the error message
	static VUtils::ErrorCode doExport(VRayProxyExportOptions &options, const SOPList &sopList);

	/// Constructor
	/// @note at this point m_geomDescrList is only partially initilized and the number of voxels is determined
	/// @param options[in] - options used to configure how geometry should be exported
	/// @param nodes[in] - pointer to an element of a consecutive list of geometry sop nodes
	///                    nodes should be a valid pointer
	/// @param nodeCnt[in] - number of nodes that should be taken from the list
	///                      nodeCnt should be > 0
	VRayProxyExporter(const VRayProxyExportOptions &options, const SOPList &sopList, ROP_Node *ropNode);
	~VRayProxyExporter();

	/// Initilize the exporter for the current time (based on what is set in m_options.m_context)
	/// by caching out geometry data for all objects in m_geomDescrList
	/// @note this should be called for every animation frame before doExportFrame()
	/// @retval error code - use ErrorCode::error() to check for errors
	///                      and ErrorCode::getErrorString() to get the error message
	VUtils::ErrorCode init();

	/// Cleanup cached data for current time
	void cleanup();

	/// Export cached data for the current time to .vrmesh file
	/// @retval error code - use ErrorCode::error() to check for errors
	///                      and ErrorCode::getErrorString() to get the error message
	VUtils::ErrorCode doExportFrame();

	/// Follows implementation of VUtils::MeshInterface API
	///
	/// Get number of voxels
	/// @retval number of volxels
	int getNumVoxels(void) VRAY_OVERRIDE { return m_voxels.size(); }

	/// Get voxel flags
	/// @param i[in] - voxel index
	/// @retval voxel flags is combination of the voxel type and MVF_INSTANCE_VOXEL | MVF_HIDDEN_VOXEL
	///         where voxel type can be one of the following
	///         { MVF_PREVIEW_VOXEL, MVF_GEOMETRY_VOXEL, MVF_HAIR_GEOMETRY_VOXEL, MVF_PARTICLE_GEOMETRY_VOXEL}
	uint32 getVoxelFlags(int i) VRAY_OVERRIDE;

	/// Get voxel bbox
	/// @param i[in] - voxel index
	/// @retval voxel bbox
	VUtils::Box getVoxelBBox(int i) VRAY_OVERRIDE;

	/// Get voxel data - the voxel with the largest index is the preview one
	/// @param i[in] - voxel index
	/// @param memUsage[in/out] - if not NULL voxel memory usage will output here
	/// @retval voxel data
	VUtils::MeshVoxel* getVoxel(int i, uint64 *memUsage = NULL) VRAY_OVERRIDE;

	/// Free voxel data
	/// @param voxel[in] - voxel which data should be freed
	/// @param memUsage[in/out] - if not NULL voxel memory usage will output here
	void releaseVoxel(VUtils::MeshVoxel *voxel, uint64 *memUsage = NULL) VRAY_OVERRIDE;

	VUtils::ErrorCode preprocessDescriptions(const OP_Context &context);

	/// Sample geometry transform at the given time and return the result in geomDescr
	/// @note this is called from cacheDescriptionForContext()
	/// @param context[in] - sample time
	/// @param geomDescr[out] - geometry description
	void getTransformForContext(const OP_Context &context, GeometryDescription &geomDescr) const;

	/// Fill mesh voxel data from geometry description
	/// @note this is called from getVoxel()
	/// @param voxel[out] - voxel data
	/// @param meshDescr[in] - geometry description
	void buildMeshVoxel(VUtils::MeshVoxel &voxel, GeometryDescription &meshDescr);

	/// Fill hair voxel data from geometry description
	/// @note this is called from getVoxel()
	/// @param voxel[out] - voxel data
	/// @param hairDescr[in] - geometry description
	void buildHairVoxel(VUtils::MeshVoxel &voxel, GeometryDescription &hairDescr);

	/// Create and fill preview voxel data
	/// @note this is called from getVoxel()
	/// @param voxel[out] - voxel data
	/// @param hairDescr[in] - geometry description
	void buildPreviewVoxel(VUtils::MeshVoxel &voxel);

	/// Create mesh preview geometry
	/// and save it in m_previewVerts, m_previewFaces
	/// fill out data that should be written to OBJECT_INFO_CHANNEL of the preview voxel
	/// @note this is called from buildPreviewVoxel()
	/// @param objInfo[out] - data that will be written to OBJECT_INFO_CHANNEL of the preview voxel
	void createMeshPreviewGeometry(VUtils::ObjectInfoChannelData &objInfo);

	/// Create mesh preview geometry by sampling faces
	/// and save it in m_previewVerts, m_previewFaces
	/// fill out data that should be written to OBJECT_INFO_CHANNEL of the preview voxel
	/// @note this is called from createMeshPreviewGeometry() based on m_options.m_simplificationType
	/// @param numPreviewVerts[out] - number of preview vertices
	/// @param numPreviewVerts[out] - number of preview faces
	/// @param objInfo[out] - data that will be written to OBJECT_INFO_CHANNEL of the preview voxel
	void simplifyFaceSampling(int &numPreviewVerts, int &numPreviewFaces,
							  VUtils::ObjectInfoChannelData &objInfo);
	/// Create mesh preview geometry
	/// and saves it in m_previewVerts, m_previewFaces
	/// fill out data that should be written to OBJECT_INFO_CHANNEL of the preview voxel
	/// @note this is called from createMeshPreviewGeometry() based on m_options.m_simplificationTy
	/// @param numPreviewVerts[out] - number of preview vertices
	/// @param numPreviewVerts[out] - number of preview faces
	/// @param objInfo[out] - data that will be written to OBJECT_INFO_CHANNEL of the preview voxel
	void simplifyMesh(int &numPreviewVerts, int &numPreviewFaces,
					  VUtils::ObjectInfoChannelData &objInfo);

	/// Create hair preview geometry by sampling strands
	/// and save it in m_previewStrandVerts, m_previewStrands
	/// fill out data that should be written to HAIR_OBJECT_INFO_CHANNEL of the preview voxel
	/// @note this is called from buildPreviewVoxel()
	/// @param objInfo[out] - data that will be written to HAIR_OBJECT_INFO_CHANNEL of the preview voxel
	void createHairPreviewGeometry(VUtils::ObjectInfoChannelData &objInfo);

	/// Helper function to initilize the object info data based for the given voxel type
	/// @note this is called from createMeshPreviewGeometry() and createHairPreviewGeometry()
	/// @param type[in] - voxel type can be either MVF_GEOMETRY_VOXEL or MVF_HAIR_GEOMETRY_VOXEL
	/// @param objInfo[out] - data that should be written to OBJECT_INFO_CHANNEL or HAIR_OBJECT_INFO_CHANNEL
	///                       of the preview voxel depending on the type
	void addObjectInfoForType(uint32 type, VUtils::ObjectInfoChannelData &objInfo);

	/// Helper function to get the index of the first geometry description
	/// that should be included in the preview voxel
	/// (all consequent ones are also included)
	/// @retval index of the first geometry description
	int getPreviewStartIdx() const;

private:
	/// Input SOP list.
	const SOPList &sopList;
	ROP_Node *m_rop;

	/// SOP geometry descriptions to make vrmesh from.
	GeometryDescriptions geometryDescriptions;

	const VRayProxyExportOptions &m_options; ///< export options

	std::vector<VUtils::MeshVoxel>   m_voxels; ///< voxel array - last voxel is the preview one

	VUtils::VertGeomData            *m_previewVerts; ///< vertices of preview mesh geometry
	VUtils::FaceTopoData            *m_previewFaces; ///< faces of preview mesh geometry
	VUtils::VertGeomData            *m_previewStrandVerts; ///< vertices of preview hair geometry
	int                             *m_previewStrands; ///< faces of preview hair geometry

	VUTILS_DISABLE_COPY(VRayProxyExporter)
};

} // namespace VRayForHoudini

#endif // VRAY_FOR_HOUDINI_EXPORT_VRAYPROXY_H
