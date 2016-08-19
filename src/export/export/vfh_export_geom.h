//
// Copyright (c) 2015-2016, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#ifndef VRAY_FOR_HOUDINI_EXPORT_GEOM_H
#define VRAY_FOR_HOUDINI_EXPORT_GEOM_H

#include "vfh_vray.h"
#include "vfh_exporter.h"
#include "vfh_material_override.h"
#include "vfh_export_primitive.h"

#include <OBJ/OBJ_Geometry.h>
#include <GU/GU_PrimPacked.h>

#include <list>
#include <unordered_map>


namespace VRayForHoudini {

class GeometryExporter
{
public:
	GeometryExporter(OBJ_Geometry &node, VRayExporter &pluginExporter);
	~GeometryExporter() { }

	int                 isNodeVisible() const;
	int                 isNodeMatte() const;
	int                 isNodePhantom() const;

	GeometryExporter&    setExportGeometry(bool val) { m_exportGeometry = val; return *this; }
	bool                 hasSubdivApplied() const;
	void                 cleanup();
	int                  exportNodes();
	int                  getNumPluginDesc() const;
	Attrs::PluginDesc&   getPluginDescAt(int idx);

private:
	int              exportVRaySOP(SOP_Node &sop, PluginDescList &pluginList);
	int              exportHair(SOP_Node &sop, GU_DetailHandleAutoReadLock &gdl, PluginDescList &pluginList);
	int              exportDetail(SOP_Node &sop, GU_DetailHandleAutoReadLock &gdl, PluginDescList &pluginList);
	int              exportPolyMesh(SOP_Node &sop, const GU_Detail &gdp, PluginDescList &pluginList);

	int              exportPacked(SOP_Node &sop, const GU_PrimPacked &prim, PluginDescList &pluginList);
	uint             getPrimPackedID(const GU_PrimPacked &prim);
	int              exportPrimPacked(SOP_Node &sop, const GU_PrimPacked &prim, PluginDescList &pluginList);
	int              exportAlembicRef(SOP_Node &sop, const GU_PrimPacked &prim, PluginDescList &pluginList);
	int              exportVRayProxyRef(SOP_Node &sop, const GU_PrimPacked &prim, PluginDescList &pluginList);
	int              exportPackedDisk(SOP_Node &sop, const GU_PrimPacked &prim, PluginDescList &pluginList);
	int              exportPackedGeometry(SOP_Node &sop, const GU_PrimPacked &prim, PluginDescList &pluginList);

	VRay::Plugin     exportMaterial();
	int getSHOPOverridesAsUserAttributes(UT_String& userAttrs) const;

private:
	OBJ_Geometry &m_objNode;
	OP_Context   &m_context;
	VRayExporter &m_pluginExporter;

	bool                 m_exportGeometry;
	uint                 m_myDetailID;
	DetailToPluginDesc   m_detailToPluginDesc;
	SHOPList             m_shopList;
};


} // namespace VRayForHoudini

#endif // VRAY_FOR_HOUDINI_EXPORT_GEOM_H



