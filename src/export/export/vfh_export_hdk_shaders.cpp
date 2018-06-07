//
// Copyright (c) 2015-2018, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text:
//   https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#include "vfh_vray.h"
#include "vfh_exporter.h"
#include "vfh_prm_templates.h"

using namespace VRayForHoudini;
using namespace Attrs;

static VRay::Plugin exportTexUserColor(VRayExporter &exporter, const OP_Node &opNode, const char *parmName, int lookUpPriority=0)
{
	const fpreal t = exporter.getContext().getTime();

	const UT_StringHolder &nodePath = opNode.getFullPath();

	fpreal defaultColor[3];
	getParmFloat3(opNode, parmName, defaultColor, t);

	Attrs::PluginDesc texUserColor(SL("TexUserColor|") % parmName % SL("@") % nodePath.buffer(),
								   SL("TexUserColor"));

	// TODO: This may be a texture.
	texUserColor.add(SL("default_color"), defaultColor[0], defaultColor[1], defaultColor[2], 1.0f);
	texUserColor.add(SL("user_attribute"), parmName);
	texUserColor.add(SL("attribute_priority"), lookUpPriority);

	return exporter.exportPlugin(texUserColor);
}

VRay::Plugin VRayExporter::exportPrincipledShader(const OP_Node &opNode)
{
	const fpreal t = getContext().getTime();

	const UT_StringHolder &nodePath = opNode.getFullPath();

	Attrs::PluginDesc brdfVRayMtl(SL("BRDFVRayMtl@") % nodePath.buffer(),
								  SL("BRDFVRayMtl"));

	if (Parm::getParmInt(opNode, "basecolor_usePointColor", t)) {
		brdfVRayMtl.add(SL("diffuse"), exportTexUserColor(*this, opNode, "basecolor", 0));
	}
	else {
		fpreal color[3];
		getParmFloat3(opNode, "basecolor", color, t);

		brdfVRayMtl.add(SL("diffuse"), color[0], color[1], color[2], 1.0f);
	}

	brdfVRayMtl.add(SL("roughness"), opNode.evalFloat("rough", 0, t));

	Attrs::PluginDesc mtlSingleBRDF(SL("MtlSingleBRDF@") % nodePath.buffer(),
									SL("MtlSingleBRDF"));
	mtlSingleBRDF.add(SL("brdf"), exportPlugin(brdfVRayMtl));

	return exportPlugin(mtlSingleBRDF);
}
