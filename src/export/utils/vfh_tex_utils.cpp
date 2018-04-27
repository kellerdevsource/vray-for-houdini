//
// Copyright (c) 2015-2018, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#include "vfh_defines.h"
#include "vfh_typedefs.h"
#include "vfh_tex_utils.h"

#define CGR_DEBUG_RAMPS  0
#define CGR_MAX_NUM_POINTS 64

struct MyPoint {
	float x;
	float y;
};

static const QString FmtPos(SL("%1#pos"));
static const QString FmtColor(SL("%1#c"));
static const QString FmtValue(SL("%1#value"));
static const QString FmtInterp(SL("%1#interp"));
static const QString FmtColorPluginName(SL("%1|%2"));

void VRayForHoudini::Texture::exportRampAttribute(VRayExporter &exporter, Attrs::PluginDesc &pluginDesc, OP_Node *op_node,
												  const QString &rampAttrName,
												  const QString &colAttrName, const QString &posAttrName, const QString &typesAttrName,
												  bool asColor, bool remapInterp)
{
	const fpreal &t = exporter.getContext().getTime();

	const QString &pluginName = VRayExporter::getPluginName(*op_node, rampAttrName);

	const int nPoints = op_node->evalInt(_toChar(rampAttrName), 0, 0.0f);

#if CGR_DEBUG_RAMPS
	Log::getLog().info("Ramp points: %i",
	                   nPoints);
#endif

	const bool needTypes = !typesAttrName.isEmpty();

	const QString &prmPosName = FmtPos.arg(rampAttrName);
	const QString &prmColName = FmtColor.arg(rampAttrName);
	const QString &prmInterpName = FmtInterp.arg(rampAttrName);

	VRay::ValueList colorPlugins;
	VRay::ColorList colorList;
	VRay::FloatList positions;
	VRay::IntList   types;

	for(int i = 1; i <= nPoints; i++) {
		const float pos = op_node->evalFloatInst(_toChar(prmPosName), &i, 0, t);

		const float colR = op_node->evalFloatInst(_toChar(prmColName), &i, 0, t);
		const float colG = op_node->evalFloatInst(_toChar(prmColName), &i, 1, t);
		const float colB = op_node->evalFloatInst(_toChar(prmColName), &i, 2, t);

		int interp = op_node->evalIntInst(_toChar(prmInterpName), &i, 0, t);
		if (remapInterp) {
			interp = static_cast<int>(mapToVray(static_cast<HOU_InterpolationType>(interp)));
		}
#if CGR_DEBUG_RAMPS
		Log::getLog().info(" %.3f: Color(%.3f,%.3f,%.3f) [%i]",
		                   pos, colR, colG, colB, interp);
#endif
		if (asColor) {
			colorList.push_back(VRay::Color(colR, colG, colB));
			positions.push_back(pos);
			if (needTypes) {
				types.push_back(interp);
			}
		}
		else {
			const QString colPluginName(FmtColorPluginName.arg(pluginName).arg(QString::number(i)));

			Attrs::PluginDesc colPluginDesc(colPluginName, SL("TexAColor"));
			colPluginDesc.add(Attrs::PluginAttr(SL("texture"), Attrs::PluginAttr::AttrTypeAColor, colR, colG, colB, 1.0f));

			VRay::Plugin colPlugin = exporter.exportPlugin(colPluginDesc);
			if (colPlugin.isNotEmpty()) {
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
										   const QString &curveAttrName,
										   VRay::IntList &interpolations, VRay::FloatList &positions, VRay::FloatList *values,
										   const bool needHandles, const bool remapInterp)
{
	const fpreal &t = exporter.getContext().getTime();

	const int numPoints = op_node->evalInt(_toChar(curveAttrName), 0, t);
	if (NOT(numPoints))
		return;

	MyPoint point[CGR_MAX_NUM_POINTS];

	const QString &prmPosName    = FmtPos.arg(curveAttrName);
	const QString &prmValName    = FmtValue.arg(curveAttrName);
	const QString &prmInterpName = FmtInterp.arg(curveAttrName);

	int p = 0;
	for(int i = 1; i <= numPoints; ++i, ++p) {
		const float pos = op_node->evalFloatInst(_toChar(prmPosName), &i, 0, t);
		const float val = op_node->evalFloatInst(_toChar(prmValName), &i, 0, t);
		int interp      = op_node->evalIntInst(_toChar(prmInterpName), &i, 0, t);

		if (!needHandles) {
			positions.push_back(pos);
			if (values) {
				values->push_back(val);
			}
			else {
				positions.push_back(val);
			}
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

	if (!needHandles) {
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
