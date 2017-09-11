//
// Copyright (c) 2015-2017, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#include <boost/format.hpp>

#include "vfh_defines.h"
#include "vfh_typedefs.h"
#include "vfh_tex_utils.h"


#define CGR_DEBUG_RAMPS  0
#define CGR_MAX_NUM_POINTS 64


static boost::format FmtPos("%s#pos");
static boost::format FmtColor("%s#c");
static boost::format FmtValue("%s#value");
static boost::format FmtInterp("%s#interp");


struct MyPoint {
	float x;
	float y;
};


void VRayForHoudini::Texture::exportRampAttribute(VRayExporter &exporter, Attrs::PluginDesc &pluginDesc, OP_Node *op_node,
												  const std::string &rampAttrName,
												  const std::string &colAttrName, const std::string &posAttrName, const std::string &typesAttrName,
												  const bool asColor, const bool remapInterp)
{
	const fpreal &t = exporter.getContext().getTime();

	const std::string &pluginName = VRayExporter::getPluginName(op_node, rampAttrName);

	int nPoints = op_node->evalInt(rampAttrName.c_str(), 0, 0.0f);

#if CGR_DEBUG_RAMPS
	Log::getLog().info("Ramp points: %i",
			   nPoints);
#endif

	const bool needTypes = NOT(typesAttrName.empty());

	const std::string &prmPosName    = boost::str(FmtPos    % rampAttrName);
	const std::string &prmColName    = boost::str(FmtColor  % rampAttrName);
	const std::string &prmInterpName = boost::str(FmtInterp % rampAttrName);

	VRay::ValueList colorPlugins;
	VRay::ColorList colorList;
	VRay::FloatList positions;
	VRay::IntList   types;

	for(int i = 1; i <= nPoints; i++) {
		const float pos = (float)op_node->evalFloatInst(prmPosName.c_str(), &i, 0, t);

		const float colR = (float)op_node->evalFloatInst(prmColName.c_str(), &i, 0, t);
		const float colG = (float)op_node->evalFloatInst(prmColName.c_str(), &i, 1, t);
		const float colB = (float)op_node->evalFloatInst(prmColName.c_str(), &i, 2, t);

		int interp = op_node->evalIntInst(prmInterpName.c_str(), &i, 0, t);
		if (remapInterp) {
			interp = static_cast<int>(mapToVray(static_cast<HOU_InterpolationType>(interp)));
		}
#if CGR_DEBUG_RAMPS
		Log::getLog().info(" %.3f: Color(%.3f,%.3f,%.3f) [%i]",
				   pos, colR, colG, colB, interp);
#endif
		const std::string &colPluginName = boost::str(boost::format("%sPos%i") % pluginName % i);

		if (asColor) {
			colorList.push_back(VRay::Color(colR, colG, colB));
			positions.push_back(pos);
			if (needTypes) {
				types.push_back(interp);
			}
		}
		else {
			Attrs::PluginDesc colPluginDesc(colPluginName, "TexAColor");
			colPluginDesc.add(Attrs::PluginAttr("texture", Attrs::PluginAttr::AttrTypeAColor, colR, colG, colB, 1.0f));

			VRay::Plugin colPlugin = exporter.exportPlugin(colPluginDesc);
			if (colPlugin) {
				colorPlugins.push_back(VRay::Value(colPlugin));
				positions.push_back(pos);
				if (needTypes) {
					types.push_back(interp);
				}
			}
		}
	}

	if (asColor) {
		pluginDesc.add(Attrs::PluginAttr(colAttrName, colorList));
	}
	else {
		pluginDesc.add(Attrs::PluginAttr(colAttrName, colorPlugins));
	}
	pluginDesc.add(Attrs::PluginAttr(posAttrName, positions));
	if (needTypes) {
		pluginDesc.add(Attrs::PluginAttr(typesAttrName, types));
	}
}


void VRayForHoudini::Texture::getCurveData(VRayExporter &exporter, OP_Node *op_node,
										   const std::string &curveAttrName,
										   VRay::IntList &interpolations, VRay::FloatList &positions, VRay::FloatList *values,
										   const bool needHandles, const bool remapInterp)
{
	const fpreal &t = exporter.getContext().getTime();

	int numPoints = op_node->evalInt(curveAttrName.c_str(), 0, t);
	if (NOT(numPoints))
		return;

	MyPoint point[CGR_MAX_NUM_POINTS];

	const std::string &prmPosName    = boost::str(FmtPos    % curveAttrName);
	const std::string &prmValName    = boost::str(FmtValue  % curveAttrName);
	const std::string &prmInterpName = boost::str(FmtInterp % curveAttrName);

	int p = 0;
	for(int i = 1; i <= numPoints; ++i, ++p) {
		const float pos    = (float)op_node->evalFloatInst(prmPosName.c_str(),    &i, 0, t);
		const float val    = (float)op_node->evalFloatInst(prmValName.c_str(),    &i, 0, t);
		int interp =                op_node->evalIntInst(prmInterpName.c_str(), &i, 0, t);

		if (NOT(needHandles)) {
			positions.push_back(pos);
			if (values) values->push_back(val); else positions.push_back(val);
		}
		else {
			point[p].x = pos;
			point[p].y = val;
		}

		if (remapInterp) {
			interp = static_cast<int>(mapToVray(static_cast<HOU_InterpolationType>(interp)));
		}

		interpolations.push_back(interp);
	}

	if (NOT(needHandles)) {
		return;
	}

	float  deltaX[CGR_MAX_NUM_POINTS + 1];
	float  ySecon[CGR_MAX_NUM_POINTS];
	float  yPrim[CGR_MAX_NUM_POINTS];
	float  d[CGR_MAX_NUM_POINTS];
	float  w[CGR_MAX_NUM_POINTS];
	int    i;

	for(i = 1; i < numPoints; i++)
		deltaX[i] = point[i].x - point[i-1].x;
	deltaX[0] = deltaX[1];
	deltaX[numPoints] = deltaX[numPoints-1];
	for(i = 1; i < numPoints-1; i++) {
		d[i] = 2 * (point[i + 1].x - point[i - 1].x);
		w[i] = 6 * ((point[i + 1].y - point[i].y) / deltaX[i+1] - (point[i].y - point[i - 1].y) / deltaX[i]);
	}
	for(i = 1; i < numPoints-2; i++) {
		w[i + 1] -= w[i] * deltaX[i+1] / d[i];
		d[i + 1] -= deltaX[i+1] * deltaX[i+1] / d[i];
	}
	ySecon[0] = 0;
	ySecon[numPoints-1] = 0;
	for(i = numPoints - 2; i >= 1; i--)
		ySecon[i] = (w[i] - deltaX[i+1] * ySecon[i + 1]) / d[i];
	for(i = 0; i < numPoints-1; i++)
		yPrim[i] = (point[i+1].y - point[i].y) / deltaX[i+1] - (deltaX[i+1] / 6.0f) * (2 * ySecon[i] + ySecon[i+1]);
	yPrim[i] = (point[i].y - point[i-1].y) / deltaX[i] + (deltaX[i] / 6.0f) * ySecon[i-1];

	for(p = 0; p < numPoints; ++p) {
		const float &px = point[p].x;
		const float &py = point[p].y;

		const float h1x = -deltaX[p] / 3;
		const float h1y = -deltaX[p] * yPrim[p] / 3;

		const float h2x = deltaX[p+1] / 3;
		const float h2y = deltaX[p+1] * yPrim[p] / 3;

		positions.push_back(px);
		if (values) values->push_back(py); else positions.push_back(py);

		positions.push_back(h1x);
		if (values) values->push_back(h1y); else positions.push_back(h1y);

		positions.push_back(h2x);
		if (values) values->push_back(h2y); else positions.push_back(h2y);
	}
}
