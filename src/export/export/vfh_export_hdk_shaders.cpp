//
// Copyright (c) 2015-2017, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text:
//   https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#include "vfh_exporter.h"
#include "vfh_prm_templates.h"

using namespace VRayForHoudini;
using namespace Attrs;

static boost::format opNodeNameFmt("%s@%s");
static boost::format texUserNameFmt("%s|%s@%s");

static void getColor(OP_Node &opNode, const char *parmName, fpreal color[3], fpreal t)
{
	color[0] = opNode.evalFloat(parmName, 0, t);
	color[1] = opNode.evalFloat(parmName, 1, t);
	color[2] = opNode.evalFloat(parmName, 2, t);
}

static VRay::Plugin exportTexUserColor(VRayExporter &exporter, OP_Node &opNode, const char *parmName, int lookUpPriority=0)
{
	const fpreal t = exporter.getContext().getTime();

	UT_String nodePath;
	opNode.getFullPath(nodePath);

	fpreal defaultColor[3];
	getColor(opNode, parmName, defaultColor, t);

	Attrs::PluginDesc texUserColor(str(texUserNameFmt % "TexUserColor" % parmName % nodePath.buffer()),
								   "TexUserColor");

	// TODO: This may be a texture.
	texUserColor.add(PluginAttr("default_color", defaultColor[0], defaultColor[1], defaultColor[2], 1.0f));
	texUserColor.add(PluginAttr("user_attribute", parmName));
	texUserColor.add(PluginAttr("attribute_priority", lookUpPriority));

	return exporter.exportPlugin(texUserColor);
}

VRay::Plugin VRayExporter::exportPrincipledShader(OP_Node &opNode, ExportContext *parentContext)
{
	const fpreal t = getContext().getTime();

	UT_String nodePath;
	opNode.getFullPath(nodePath);

	Attrs::PluginDesc brdfVRayMtl(str(opNodeNameFmt % "BRDFVRayMtl" % nodePath.buffer()),
								  "BRDFVRayMtl");

	if (Parm::getParmInt(opNode, "basecolor_usePointColor", t)) {
		brdfVRayMtl.add(PluginAttr("diffuse", exportTexUserColor(*this, opNode, "basecolor", 0)));
	}
	else {
		fpreal color[3];
		getColor(opNode, "basecolor", color, t);

		brdfVRayMtl.add(PluginAttr("diffuse", color[0], color[1], color[2], 1.0f));
	}

	brdfVRayMtl.add(PluginAttr("roughness", opNode.evalFloat("rough", 0, t)));

	Attrs::PluginDesc mtlSingleBRDF(str(opNodeNameFmt % "MtlSingleBRDF" % nodePath.buffer()),
									"MtlSingleBRDF");
	mtlSingleBRDF.add(PluginAttr("brdf", exportPlugin(brdfVRayMtl)));

	return exportPlugin(mtlSingleBRDF);
}
