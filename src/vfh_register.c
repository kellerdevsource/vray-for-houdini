//
// Copyright (c) 2015-2016, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//


// For newShopOperator()
#include <UT/UT_DSOVersion.h>
#include <OP/OP_OperatorTable.h>
#include <ROP/ROP_Node.h>
#include <vraysdk.hpp>

class MyROP:public ROP_Node 
{
public:
	MyROP(OP_Network *parent, const char *name, OP_Operator *entry):ROP_Node(parent, name, entry) {}
};

OP_Node* VFH_VRAY_NODE_CREATOR(OP_Network *parent, const char *name, OP_Operator *entry) {
	auto *t = new MyROP(parent, name, entry);
	return t;
}


void newDriverOperator(OP_OperatorTable *table) {
	VRay::VRayInit a("D:/libs/vray_for_houdini_sdk/appsdk/appsdk20161115/bin/VRaySDKLibrary.dll");
	OP_Operator* op = new OP_Operator("myop", "My OP", &VFH_VRAY_NODE_CREATOR, (PRM_Template*)nullptr, 0);
	table->addOperator(op);
}
