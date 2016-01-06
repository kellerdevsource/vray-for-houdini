//
// Copyright (c) 2015, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#ifndef VRAY_FOR_HOUDINI_EXPORT_CONTEXT_H
#define VRAY_FOR_HOUDINI_EXPORT_CONTEXT_H


#include "vfh_vray.h"

#include "OP/OP_Node.h"

#include <unordered_map>
#include <unordered_set>


namespace VRayForHoudini {


class VRayExporter;

class ECFnOBJNode;
class ECFnSHOPOverrides;

enum ContextType {
	CT_NULL = 0,
	CT_OBJ,
	CT_SOP,
	CT_SHOP,
	CT_VOP,
	CT_MAX
};


class ExportContext
{
public:
	ExportContext();
	ExportContext(ContextType type, VRayExporter &exporter, OP_Node &node);
	ExportContext(ContextType type, VRayExporter &exporter, OP_Node &node, ExportContext &parentContext);
	virtual ~ExportContext() { }

	virtual ContextType getType() const { return m_type; }
	virtual bool        hasType(ContextType type) const { return m_type == type; }

	VRayExporter*  getExporter() const { return m_exporter; }
	OP_Node*       getTarget() const { return m_target; }
	ExportContext* getParentContext() const { return m_parentContext; }

protected:
	ContextType    m_type;
	VRayExporter  *m_exporter;
	OP_Node       *m_target;
	ExportContext *m_parentContext;
};


enum MTLOverrideType {
	MTLO_NONE = 0,
	MTLO_OBJ,
	MTLO_GEO
};


class SHOPExportContext : public ExportContext
{
	friend class ECFnSHOPOverrides;
public:
	SHOPExportContext(VRayExporter &exporter, SHOP_Node &shopNode, ExportContext &parentContext);
	~SHOPExportContext() { }

private:
	typedef std::unordered_map< std::string, std::string > OverrideMap;
	typedef std::unordered_map< int, OverrideMap >         VOPOverrideMap;

	/// type of the mtl overrides for current context (object, shop), if any
	/// MTLO_NONE = no mtl overrides
	/// MTLO_OBJ = mtl overrides specified on the object node
	/// MTLO_GEO = per primitive mtl overrides stored as map channels on the geometry
	MTLOverrideType m_overrideType;
	/// table mapping <shop parm name> to <overriding (object parm name/map channel name)>
	OverrideMap m_shopOverrrides;
	/// table mapping <vop unique node id> to < table mapping <vop parm name> to <overriding shop parm name> >
	VOPOverrideMap m_vopOverrides;
};



class ECFnOBJNode
{
public:
	ECFnOBJNode(ExportContext *ctx);
	~ECFnOBJNode() { }

	bool             isValid() const;
	OBJ_Node*        getTargetNode() const;

private:
	ExportContext *m_context;
};


class ECFnSHOPOverrides
{
public:
	ECFnSHOPOverrides(ExportContext *ctx);
	~ECFnSHOPOverrides()
	{ }

	bool             isValid() const;

	void             initOverrides();
	SHOP_Node*       getTargetNode() const;
	OBJ_Node*        getObjectNode() const;
	bool             hasOverrides() const;
	bool             hasOverrides(VOP_Node &vopNode) const;
	MTLOverrideType  getOverrideType() const;
	bool             getOverrideName(VOP_Node &vopNode, const std::string &prmName, std::string &o_overrideName) const;

private:
	void initVOPOverrides();
	void initSHOPOverrides();

private:
	SHOPExportContext *m_context;
};


}

#endif // VRAY_FOR_HOUDINI_EXPORT_CONTEXT_H
