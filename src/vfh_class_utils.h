//
// Copyright (c) 2015-2016, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#ifndef VRAY_FOR_HOUDINI_CLASS_UTILS_H
#define VRAY_FOR_HOUDINI_CLASS_UTILS_H

#include <OP/OP_Node.h>


namespace VRayForHoudini {


template<typename T>
OP_Node* VFH_VRAY_NODE_CREATOR(OP_Network *parent, const char *name, OP_Operator *entry) {
	T *t = new T(parent, name, entry);
	// Execute extra initialization
	t->init();
	return t;
}


#define VFH_OBJ_ADD_OPERATOR_AUTO(table, OpPluginType, OpClass) \
	OP_Operator *op##OpClass = new OP_Operator( \
		/* Internal name     */ "VRayNode" STRINGIZE(OpClass), \
		/* UI name           */ "V-Ray " STRINGIZE(OpClass), \
		/* How to create one */ VFH_VRAY_NODE_CREATOR<OBJ::OpClass>, \
		/* Parm definitions  */ OBJ::OpClass::GetPrmTemplate(), \
		/* Min # of inputs   */ 0, \
		/* Max # of inputs   */ 1 \
	); \
	op##OpClass->setIconName("ROP_vray"); \
	table->addOperator(op##OpClass);


#define VFH_SOP_ADD_OPERATOR(table, OpPluginType, OpClass, OpParmTemplate) \
	OP_Operator *op##OpClass = new OP_Operator( \
		/* Internal name     */ "VRayNode" STRINGIZE(OpClass), \
		/* UI name           */ "V-Ray " STRINGIZE(OpClass), \
		/* How to create one */ VFH_VRAY_NODE_CREATOR<SOP::OpClass>, \
		/* Parm definitions  */ OpParmTemplate, \
		/* Min # of inputs   */ 0, \
		/* Max # of inputs   */ 0 \
	); \
	op##OpClass->setIconName("ROP_vray"); \
	table->addOperator(op##OpClass);


#define VFH_SOP_ADD_OPERATOR_INPUTS(table, OpPluginType, OpClass, OpParmTemplate, MinInputs, MaxInputs) \
	OP_Operator *op##OpClass = new OP_Operator( \
		/* Internal name     */ "VRayNode" STRINGIZE(OpClass), \
		/* UI name           */ "V-Ray " STRINGIZE(OpClass), \
		/* How to create one */ VFH_VRAY_NODE_CREATOR<SOP::OpClass>, \
		/* Parm definitions  */ OpParmTemplate, \
		/* Min # of inputs   */ MinInputs, \
		/* Max # of inputs   */ MaxInputs \
	); \
	op##OpClass->setIconName("ROP_vray"); \
	table->addOperator(op##OpClass);


#define VFH_SOP_ADD_OPERATOR_AUTO(table, OpPluginType, OpClass) \
	OP_Operator *op##OpClass = new OP_Operator( \
		/* Internal name     */ "VRayNode" STRINGIZE(OpClass), \
		/* UI name           */ "V-Ray " STRINGIZE(OpClass), \
		/* How to create one */ VFH_VRAY_NODE_CREATOR<SOP::OpClass>, \
		/* Parm definitions  */ Parm::getPrmTemplate(STRINGIZE(OpClass)), \
		/* Min # of inputs   */ 0, \
		/* Max # of inputs   */ 0 \
	); \
	op##OpClass->setIconName("ROP_vray"); \
	table->addOperator(op##OpClass);


#define VFH_SOP_ADD_OPERATOR_AUTO_INPUTS(table, OpPluginType, OpClass, MinInputs, MaxInputs) \
	OP_Operator *op##OpClass = new OP_Operator( \
		/* Internal name     */ "VRayNode" STRINGIZE(OpClass), \
		/* UI name           */ "V-Ray " STRINGIZE(OpClass), \
		/* How to create one */ VFH_VRAY_NODE_CREATOR<SOP::OpClass>, \
		/* Parm definitions  */ Parm::getPrmTemplate(STRINGIZE(OpClass)), \
		/* Min # of inputs   */ MinInputs, \
		/* Max # of inputs   */ MaxInputs \
	); \
	op##OpClass->setIconName("ROP_vray"); \
	table->addOperator(op##OpClass);


#define VFH_SOP_ADD_OPERATOR_CUSTOM_ID_AUTO(table, OpPluginType, OpClass, PluginID) \
	OP_Operator *op##OpClass = new OP_Operator( \
		/* Internal name     */ "VRayNode" STRINGIZE(OpClass), \
		/* UI name           */ "V-Ray " STRINGIZE(OpClass), \
		/* How to create one */ VFH_VRAY_NODE_CREATOR<SOP::OpClass>, \
		/* Parm definitions  */ Parm::getPrmTemplate(PluginID), \
		/* Min # of inputs   */ 0, \
		/* Max # of inputs   */ 0 \
	); \
	op##OpClass->setIconName("ROP_vray"); \
	table->addOperator(op##OpClass);


#define VFH_VOP_ADD_OPERATOR_CUSTOM(table, OpPluginType, OpClass, OpParmTemplate) \
	VOP_Operator *vop##OpClass = new VOP_Operator( \
		/* Internal name     */ "VRayNode" STRINGIZE(OpClass), \
		/* UI name           */ "V-Ray " STRINGIZE(OpClass), \
		/* How to create one */ VFH_VRAY_NODE_CREATOR<VOP::OpClass>, \
		/* Parm definitions  */ OpParmTemplate, \
		/* Min # of inputs   */ 0, \
		/* Max # of inputs   */ VOP_VARIABLE_INOUT_MAX, \
		/* VOP network mask  */ "VRay", \
		/* Local variables   */ 0, \
		/* Special flags     */ OP_FLAG_UNORDERED \
	); \
	vop##OpClass->setIconName("ROP_vray"); \
	table->addOperator(vop##OpClass);


#define VFH_VOP_ADD_OPERATOR_BASE(table, OpPluginType, OpClass, OpPluginID, OpFlags) \
	VOP_Operator *vop##OpClass = new VOP_Operator( \
		/* Internal name     */ "VRayNode" STRINGIZE(OpClass), \
		/* UI name           */ "V-Ray " STRINGIZE(OpClass), \
		/* How to create one */ VFH_VRAY_NODE_CREATOR<VOP::OpClass>, \
		/* Parm definitions  */ Parm::getPrmTemplate(OpPluginID), \
		/* Min # of inputs   */ 0, \
		/* Max # of inputs   */ VOP_VARIABLE_INOUT_MAX, \
		/* VOP network mask  */ "VRay", \
		/* Local variables   */ 0, \
		/* Special flags     */ OpFlags \
	); \
	vop##OpClass->setIconName("ROP_vray"); \
	table->addOperator(vop##OpClass);


#define VFH_VOP_ADD_OPERATOR(table, OpPluginType, OpClass) \
	VFH_VOP_ADD_OPERATOR_BASE(table, OpPluginType, OpClass, STRINGIZE(OpClass), OP_FLAG_UNORDERED)

#define VFH_VOP_ADD_OPERATOR_OUTPUT(table, OpPluginType, OpClass) \
	VFH_VOP_ADD_OPERATOR_BASE(table, OpPluginType, OpClass, STRINGIZE(OpClass), OP_FLAG_OUTPUT)

#define VFH_VOP_ADD_OPERATOR_OUTPUT_CUSTOM_ID(table, OpPluginType, OpClass, OpPluginID) \
	VFH_VOP_ADD_OPERATOR_BASE(table, OpPluginType, OpClass, OpPluginID, OP_FLAG_UNORDERED)


} // namespace VRayForHoudini

#endif // VRAY_FOR_HOUDINI_CLASS_UTILS_H
