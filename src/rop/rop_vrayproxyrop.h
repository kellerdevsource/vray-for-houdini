//
// Copyright (c) 2015-2016, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#ifndef VRAY_FOR_HOUDINI_ROP_NODE_VRAYPROXYROP_H
#define VRAY_FOR_HOUDINI_ROP_NODE_VRAYPROXYROP_H

#include "vfh_vray.h"
#include <ROP/ROP_Node.h>


class VRayProxyExportOptions;


namespace VRayForHoudini {

class VRayProxyROP:
		public ROP_Node
{
public:
	static PRM_Template *      getMyPrmTemplate();
	static OP_TemplatePair *   getTemplatePair();
	static OP_VariablePair *   getVariablePair();

	static void                register_ropoperator(OP_OperatorTable *table);
	static void                register_sopoperator(OP_OperatorTable *table);
	static OP_Node *           creator(OP_Network *parent, const char *name, OP_Operator *op);

public:
//	virtual bool              hasImageOutput() VRAY_OVERRIDE { return false; }

protected:
	VRayProxyROP(OP_Network *parent, const char *name, OP_Operator *op);
	virtual ~VRayProxyROP();

	virtual int             startRender(int nframes, fpreal tstart, fpreal tend) VRAY_OVERRIDE;
	virtual ROP_RENDER_CODE renderFrame(fpreal time, UT_Interrupt *boss = 0) VRAY_OVERRIDE;
	virtual ROP_RENDER_CODE endRender() VRAY_OVERRIDE;

private:
	typedef UT_ValArray<OP_Node *> OP_SOPList;

	int getSOPList(OP_SOPList &sopList) const;
	int getExportOptions(VRayProxyExportOptions &options) const;

private:
	fpreal m_tend;
	int    m_nframes;
};

} // namespace VRayForHoudini

#endif // VRAY_FOR_HOUDINI_ROP_NODE_VRAYPROXYROP_H
