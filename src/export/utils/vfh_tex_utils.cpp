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

static const int CGR_MAX_NUM_POINTS = 64;

struct MyPoint {
	float x = 0.0f;
	float y = 0.0f;
};

static const QString FmtPos(SL("%1#pos"));
static const QString FmtColor(SL("%1#c"));
static const QString FmtValue(SL("%1#value"));
static const QString FmtInterp(SL("%1#interp"));

static const QString FmtPosAttrName(SL("%1%2pos"));
static const QString FmtColorAttrName(SL("%1%2c"));
static const QString FmtValueAttrName(SL("%1%2value"));
static const QString FmtInterpAttrName(SL("%1%2interp"));

static const QString FmtColorPluginName(SL("%1|%2"));

void VRayForHoudini::Texture::exportRampAttribute(VRayExporter &exporter,
                                                  Attrs::PluginDesc &pluginDesc,
                                                  OP_Node &opNode,
                                                  const QString &rampAttrName,
                                                  const QString &colAttrName,
                                                  const QString &posAttrName,
                                                  const QString &typesAttrName,
                                                  bool asColor,
                                                  bool remapInterp)
{
	const fpreal t = exporter.getContext().getTime();

	const QString &pluginName = VRayExporter::getPluginName(opNode, rampAttrName);

	const int nPoints = opNode.evalInt(qPrintable(rampAttrName), 0, 0.0f);

	const bool needTypes = !typesAttrName.isEmpty();

	const QString &prmPosName = FmtPos.arg(rampAttrName);
	const QString &prmColName = FmtColor.arg(rampAttrName);
	const QString &prmInterpName = FmtInterp.arg(rampAttrName);

	VRay::VUtils::ValueRefList colorPlugins;
	VRay::VUtils::ColorRefList colorList;

	if (asColor) {
		colorList = VRay::VUtils::ColorRefList(nPoints);
	}
	else {
		colorPlugins = VRay::VUtils::ValueRefList(nPoints);
	}

	VRay::VUtils::FloatRefList positions(nPoints);

	VRay::VUtils::IntRefList types;
	if (needTypes) {
		types = VRay::VUtils::IntRefList(nPoints);
	}

	bool isPosAnimated = false;
	bool isColAnimated = false;
	bool isInterpAnimated = false;

	for(int i = 1; i <= nPoints; i++) {
		const int pntIdx = i - 1;

		const float pos = opNode.evalFloatInst(qPrintable(prmPosName), &i, 0, t);

		const float colR = opNode.evalFloatInst(qPrintable(prmColName), &i, 0, t);
		const float colG = opNode.evalFloatInst(qPrintable(prmColName), &i, 1, t);
		const float colB = opNode.evalFloatInst(qPrintable(prmColName), &i, 2, t);

		const QString posAttrInstName = FmtPosAttrName.arg(rampAttrName).arg(QString::number(i));
		const QString colAttrInstName = FmtColorAttrName.arg(rampAttrName).arg(QString::number(i));
		const QString interpAttrInstName = FmtInterpAttrName.arg(rampAttrName).arg(QString::number(i));

		isPosAnimated |= opNode.isParmTimeDependent(qPrintable(posAttrInstName));
		isColAnimated |= opNode.isParmTimeDependent(qPrintable(colAttrInstName));
		isInterpAnimated |= opNode.isParmTimeDependent(qPrintable(interpAttrInstName));

		int interp = opNode.evalIntInst(qPrintable(prmInterpName), &i, 0, t);
		if (remapInterp) {
			interp = static_cast<int>(mapToVray(static_cast<HOU_InterpolationType>(interp)));
		}

		if (asColor) {
			colorList[pntIdx] = VRay::Color(colR, colG, colB);
			positions[pntIdx] = pos;
			if (needTypes) {
				types[pntIdx] = interp;
			}
		}
		else {
			const QString colPluginName(FmtColorPluginName.arg(pluginName).arg(QString::number(i)));

			Attrs::PluginDesc colPluginDesc(colPluginName,
			                                SL("TexAColor"));
			colPluginDesc.add(SL("texture"), colR, colG, colB, 1.0f, isColAnimated);

			const VRay::Plugin colPlugin = exporter.exportPlugin(colPluginDesc);
			vassert(colPlugin.isNotEmpty());

			colorPlugins[pntIdx].setPlugin(colPlugin);
			positions[pntIdx] = pos;
			if (needTypes) {
				types[pntIdx] = interp;
			}
		}
	}

	if (asColor) {
		pluginDesc.add(colAttrName, colorList, isColAnimated);
	}
	else {
		pluginDesc.add(colAttrName, colorPlugins);
	}

	pluginDesc.add(posAttrName, positions, isPosAnimated);

	if (needTypes) {
		pluginDesc.add(typesAttrName, types, isInterpAnimated);
	}
}

void VRayForHoudini::Texture::getCurveData(VRayExporter &exporter, OP_Node *op_node,
                                           const QString &curveAttrName,
                                           VRay::VUtils::IntRefList &interpolations,
                                           VRay::VUtils::FloatRefList &positions,
                                           VRay::VUtils::FloatRefList &values,
                                           bool needValues,
                                           bool needHandles,
                                           bool remapInterp)
{
	const fpreal &t = exporter.getContext().getTime();

	const int numPoints = op_node->evalInt(qPrintable(curveAttrName), 0, t);
	if (!numPoints)
		return;

	interpolations = VRay::VUtils::IntRefList(numPoints);

	if (!needHandles) {
		if (needValues) {
			positions = VRay::VUtils::FloatRefList(numPoints);
			values = VRay::VUtils::FloatRefList(numPoints);
		}
		else {
			// List will cover point and value.
			positions = VRay::VUtils::FloatRefList(numPoints * 2);
		}
	}

	MyPoint point[CGR_MAX_NUM_POINTS];

	const QString &prmPosName    = FmtPos.arg(curveAttrName);
	const QString &prmValName    = FmtValue.arg(curveAttrName);
	const QString &prmInterpName = FmtInterp.arg(curveAttrName);

	const char *prmPosPtr = qPrintable(prmPosName);
	const char *prmValNamePtr = qPrintable(prmValName);
	const char *prmInterpNamePtr = qPrintable(prmInterpName);

	int p = 0;
	int posIdx = 0;

	for(int i = 1; i <= numPoints; ++i, ++p) {
		const float pos = op_node->evalFloatInst(prmPosPtr, &i, 0, t);
		const float val = op_node->evalFloatInst(prmValNamePtr, &i, 0, t);
		int interp      = op_node->evalIntInst(prmInterpNamePtr, &i, 0, t);

		if (!needHandles) {
			positions[posIdx++] = pos;
			if (needValues) {
				values[p] = val;
			}
			else {
				positions[posIdx++] = val;
			}
		}
		else {
			point[p].x = pos;
			point[p].y = val;
		}

		if (remapInterp) {
			interp = static_cast<int>(mapToVray(static_cast<HOU_InterpolationType>(interp)));
		}

		interpolations[p] = interp;
	}

	if (!needHandles)
		return;

	float  deltaX[CGR_MAX_NUM_POINTS + 1];
	float  ySecon[CGR_MAX_NUM_POINTS];
	float  yPrim[CGR_MAX_NUM_POINTS];
	float  d[CGR_MAX_NUM_POINTS];
	float  w[CGR_MAX_NUM_POINTS];
	int    i;

	for (i = 1; i < numPoints; i++) {
		deltaX[i] = point[i].x - point[i - 1].x;
	}

	deltaX[0] = deltaX[1];
	deltaX[numPoints] = deltaX[numPoints - 1];

	for (i = 1; i < numPoints - 1; i++) {
		d[i] = 2 * (point[i + 1].x - point[i - 1].x);
		w[i] = 6 * ((point[i + 1].y - point[i].y) / deltaX[i + 1] - (point[i].y - point[i - 1].y) / deltaX[i]);
	}

	for (i = 1; i < numPoints - 2; i++) {
		w[i + 1] -= w[i] * deltaX[i + 1] / d[i];
		d[i + 1] -= deltaX[i + 1] * deltaX[i + 1] / d[i];
	}

	ySecon[0] = 0;
	ySecon[numPoints - 1] = 0;

	for (i = numPoints - 2; i >= 1; i--)
		ySecon[i] = (w[i] - deltaX[i + 1] * ySecon[i + 1]) / d[i];

	for (i = 0; i < numPoints - 1; i++)
		yPrim[i] = (point[i + 1].y - point[i].y) / deltaX[i + 1] - (deltaX[i + 1] / 6.0f) * (
			           2 * ySecon[i] + ySecon[i + 1]);
	yPrim[i] = (point[i].y - point[i - 1].y) / deltaX[i] + (deltaX[i] / 6.0f) * ySecon[i - 1];

	if (needValues) {
		positions = VRay::VUtils::FloatRefList(numPoints * 3);
		values = VRay::VUtils::FloatRefList(numPoints * 3);
	}
	else {
		// List will cover point and value.
		positions = VRay::VUtils::FloatRefList(numPoints * 6);
	}

	posIdx = 0;
	int valIdx = 0;

	for (p = 0; p < numPoints; ++p) {
		const float &px = point[p].x;
		const float &py = point[p].y;

		const float h1x = -deltaX[p] / 3;
		const float h1y = -deltaX[p] * yPrim[p] / 3;

		const float h2x = deltaX[p + 1] / 3;
		const float h2y = deltaX[p + 1] * yPrim[p] / 3;

		positions[posIdx++] = px;
		if (needValues)
			values[valIdx++] = py;
		else
			positions[posIdx++] = py;

		positions[posIdx++] = h1x;
		if (needValues)
			values[valIdx++] = h1y;
		else
			positions[posIdx++] = h1y;

		positions[posIdx++] = h2x;
		if (needValues)
			values[valIdx++] = h2y;
		else
			positions[posIdx++] = h2y;
	}
}
