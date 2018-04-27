//
// Copyright (c) 2015-2018, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#include "sop_GeomPlane.h"

using namespace VRayForHoudini;
using namespace SOP;

GeomPlane::GeomPlane(OP_Network *parent, const char *name, OP_Operator *entry)
	: NodePackedBase("GeomPlaneRef", parent, name, entry)
{}

void GeomPlane::setPluginType()
{
	pluginType = VRayPluginType::GEOMETRY;
	pluginID   = "GeomPlane";
}
