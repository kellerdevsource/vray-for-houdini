//
// Copyright (c) 2015-2017, Chaos Software Ltd
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

}

#endif // VRAY_FOR_HOUDINI_EXPORT_CONTEXT_H
