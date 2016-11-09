//
// Copyright (c) 2015-2016, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#ifndef VRAY_FOR_HOUDINI_EXPORT_PRIMITIVE_H
#define VRAY_FOR_HOUDINI_EXPORT_PRIMITIVE_H

#include "vfh_vray.h"
#include "vfh_exporter.h"

#include <OBJ/OBJ_Geometry.h>
#include <GU/GU_PrimPacked.h>

namespace VRayForHoudini {

typedef std::vector< VRay::Plugin > PluginList;
typedef std::list< Attrs::PluginDesc > PluginDescList;
typedef std::unordered_map< uint, PluginDescList > DetailToPluginDesc;

class PrimitiveExporter {
public:
	PrimitiveExporter(OBJ_Node &obj, OP_Context &ctx, VRayExporter &exp): m_object(obj), m_context(ctx), m_exporter(exp) {}

	virtual void exportPrimitives(const GU_Detail &detail, PluginDescList &plugins) = 0;
	virtual ~PrimitiveExporter() {}
protected:
	OBJ_Node     &m_object;
	OP_Context   &m_context;
	VRayExporter &m_exporter;
};

typedef std::shared_ptr<PrimitiveExporter> PrimitiveExporterPtr;

#ifdef CGR_HAS_AUR
class VolumeExporter: public PrimitiveExporter {
public:
	VolumeExporter(OBJ_Node &obj, OP_Context &ctx, VRayExporter &exp): PrimitiveExporter(obj, ctx, exp) {};

	virtual void exportPrimitives(const GU_Detail &detail, PluginDescList &plugins) VRAY_OVERRIDE;
protected:
	void exportCache(const GA_Primitive &prim);
	void exportSim(SHOP_Node *shop, const Attrs::PluginAttrs &overrideAttrs, const std::string &cacheName);
};

// this will export houdini volumes
class HoudiniVolumeExporter: public VolumeExporter {
public:
	HoudiniVolumeExporter(OBJ_Node &obj, OP_Context &ctx, VRayExporter &exp): VolumeExporter(obj, ctx, exp) {};

	virtual void exportPrimitives(const GU_Detail &detail, PluginDescList &plugins) VRAY_OVERRIDE;
private:

};
#endif // CGR_HAS_AUR

} // namespace VRayForHoudini

#endif // VRAY_FOR_HOUDINI_EXPORT_PRIMITIVE_H



