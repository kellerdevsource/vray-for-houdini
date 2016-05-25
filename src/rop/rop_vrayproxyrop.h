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


namespace VRayForHoudini {

class VRayProxyExportOptions;


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

protected:
	VRayProxyROP(OP_Network *parent, const char *name, OP_Operator *op);
	virtual ~VRayProxyROP();

	virtual int             startRender(int nframes, fpreal tstart, fpreal tend) VRAY_OVERRIDE;
	virtual ROP_RENDER_CODE renderFrame(fpreal time, UT_Interrupt *boss = 0) VRAY_OVERRIDE;
	virtual ROP_RENDER_CODE endRender() VRAY_OVERRIDE;

private:
	enum ROPError {
		ROP_ERR_NO_ERR = 0,
		ROP_ERR_INVALID_PARM,
		ROP_ERR_INVALID_FILE,
	};

	int getSOPList(fpreal time, UT_ValArray<SOP_Node *> &sopList);
	ROPError getExportOptions(fpreal time, VRayProxyExportOptions &options) const;

private:
	int         m_nframes;
	fpreal      m_tstart;
	fpreal      m_tend;
	std::shared_ptr< VRayProxyExportOptions > m_options;
};

} // namespace VRayForHoudini

#endif // VRAY_FOR_HOUDINI_ROP_NODE_VRAYPROXYROP_H
