//
// Copyright (c) 2015-2017, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#include "vfh_export_hair.h"
#include "vfh_exporter.h"
#include "vfh_geoutils.h"

#include <GEO/GEO_PrimPoly.h>

using namespace VRayForHoudini;
using namespace VRayForHoudini::Attrs;

const char* const theHairParm = "geom_splines";
const char* const VFH_ATTRIB_INCANDESCENCE = "incandescence";
const char* const VFH_ATTRIB_TRANSPARENCY = "transparency";

HairPrimitiveExporter::HairPrimitiveExporter(OBJ_Node &obj, OP_Context &ctx, VRayExporter &exp, const GEOPrimList &primList)
	: PrimitiveExporter(obj, ctx, exp)
	, primList(primList)
{}

OP_Node* HairPrimitiveExporter::findPramOwnerForHairParms(OBJ_Node &objNode)
{
	if (objNode.getParmList() && objNode.getParmList()->getParmPtr(theHairParm)) {
		 return &objNode;
	}

	OP_Node *obj = objNode.getParent();
	if (obj &&
		obj->getOperator()->getName().contains("fur*") &&
		obj->getParmList()->getParmPtr(theHairParm) )
	{
		 return obj;
	}

	return nullptr;
}

bool HairPrimitiveExporter::asPluginDesc(const GU_Detail &gdp, Attrs::PluginDesc &pluginDesc)
{
	if (!primList.size()) {
		return false;
	}

	// collect strands
	VRay::VUtils::IntRefList strands( primList.size() );
	int nVerts = 0;
	int idx = 0;
	for (const GEO_Primitive *prim : primList) {
		const int nStarndVerts = prim->getVertexCount();
		strands[idx++] = nStarndVerts;
		nVerts += nStarndVerts;
	}

	// collect verts
	VRay::VUtils::VectorRefList verts(nVerts);
	GEOgetDataFromAttribute(gdp.getP(), primList, verts);

	pluginDesc.addAttribute(Attrs::PluginAttr("num_hair_vertices", strands));
	pluginDesc.addAttribute(Attrs::PluginAttr("hair_vertices", verts));

	OP_Node *prmOwner = findPramOwnerForHairParms(objNode);
	if (prmOwner) {
		// found parent node with hair rendering parameters set
		// so update plugin description with those parameters
		pluginExporter.setAttrsFromOpNodePrms(pluginDesc, prmOwner);
	}
	else {
		// no hair rendering parameters found - use defaults
		pluginDesc.addAttribute(Attrs::PluginAttr("geom_splines", true));
		pluginDesc.addAttribute(Attrs::PluginAttr("widths_in_pixels", false));
		pluginDesc.addAttribute(Attrs::PluginAttr("generate_w_coord", false));
		pluginDesc.addAttribute(Attrs::PluginAttr("use_global_hair_tree", true));
		pluginDesc.addAttribute(Attrs::PluginAttr("xgen_generated", false));
		pluginDesc.addAttribute(Attrs::PluginAttr("min_pixel_width", 0.f));
		pluginDesc.addAttribute(Attrs::PluginAttr("geom_tesselation_mult", 4.f));
	}

	// widths
	const GA_AttributeOwner vSearchOrder[] = {
		GA_ATTRIB_VERTEX,		// Unique vertex data
		GA_ATTRIB_POINT,		// Shared vertex data
	};

	GA_ROHandleF widthHdl = gdp.findAttribute(GEO_STD_ATTRIB_WIDTH,
											  vSearchOrder,
											  COUNT_OF(vSearchOrder));
	if (widthHdl.isInvalid()) {
		widthHdl = gdp.findAttribute(GEO_STD_ATTRIB_PSCALE,
									 vSearchOrder,
									 COUNT_OF(vSearchOrder));
	}

	if (widthHdl.isValid()) {
		VRay::VUtils::FloatRefList  widths(nVerts);
		GEOgetDataFromAttribute(widthHdl.getAttribute(), primList, widths);

		pluginDesc.addAttribute(Attrs::PluginAttr("widths", widths));
	}

	// colors
	GA_ROHandleV3 cdHdl = gdp.findAttribute(GEO_STD_ATTRIB_DIFFUSE,
											vSearchOrder,
											COUNT_OF(vSearchOrder));
	if (cdHdl.isValid()) {
		VRay::VUtils::ColorRefList colors(nVerts);
		GEOgetDataFromAttribute(cdHdl.getAttribute(), primList, colors);

		pluginDesc.addAttribute(Attrs::PluginAttr("colors", colors));
	}

	// transparency
	GA_ROHandleV3 transpHdl = gdp.findAttribute(VFH_ATTRIB_TRANSPARENCY,
												vSearchOrder,
												COUNT_OF(vSearchOrder));
	if (transpHdl.isValid()) {
		VRay::VUtils::ColorRefList transparency(nVerts);
		GEOgetDataFromAttribute(transpHdl.getAttribute(), primList, transparency);

		pluginDesc.addAttribute(Attrs::PluginAttr("transparency", transparency));
	}

	// incandescence
	GA_ROHandleV3 incdHdl = gdp.findAttribute(VFH_ATTRIB_INCANDESCENCE,
											  vSearchOrder,
											  COUNT_OF(vSearchOrder));
	if (incdHdl.isValid()) {
		VRay::VUtils::ColorRefList incandescence(nVerts);
		GEOgetDataFromAttribute(incdHdl.getAttribute(), primList, incandescence);

		pluginDesc.addAttribute(Attrs::PluginAttr("incandescence", incandescence));
	}

	// uvw
	VRay::VUtils::VectorRefList uvw(strands.size());
	GA_ROHandleV3 uvwHdl = gdp.findPrimitiveAttribute(GEO_STD_ATTRIB_TEXTURE);
	if (uvwHdl.isValid()) {
		GEOgetDataFromAttribute(uvwHdl.getAttribute(), primList, uvw);
	}
	else {
		std::memset(uvw.get(), 0, uvw.size() * sizeof(uvw[0]));
	}

	pluginDesc.addAttribute(Attrs::PluginAttr("strand_uvw", uvw));

	// add all additional V3 vertex/point attributes as map_channels
	GEOAttribList attrList;
	gdp.getAttributes().matchAttributes(GEOgetV3AttribFilter(),
										vSearchOrder,
										COUNT_OF(vSearchOrder),
										attrList);

	MapChannels mapChannels;
	for (const GA_Attribute *attr : attrList) {
		if (   attr
			&& attr->getName() != GEO_STD_ATTRIB_POSITION
			&& attr->getName() != GEO_STD_ATTRIB_WIDTH
			&& attr->getName() != GEO_STD_ATTRIB_PSCALE
			&& attr->getName() != GEO_STD_ATTRIB_DIFFUSE
			&& attr->getName() != VFH_ATTRIB_TRANSPARENCY
			&& attr->getName() != VFH_ATTRIB_INCANDESCENCE
			)
		{
			const std::string attrName = attr->getName().toStdString();
			if (!mapChannels.count(attrName)) {
				MapChannel &mapChannel = mapChannels[attrName];
				mapChannel.name = attrName;
				// assume we can use same count as for stands
				mapChannel.faces = strands;
				mapChannel.vertices = VRay::VUtils::VectorRefList(nVerts);
				GEOgetDataFromAttribute(attr, primList, mapChannel.vertices);
			}
		}
	}
	// NOTE: Channel index 0 is used for UVW coordinates if and _only_ if strand_uvw is not set.
	if (mapChannels.size()) {
		VRay::VUtils::ValueRefList map_channels(mapChannels.size());
		int channelIdx = 0;
		for (const auto &mc : mapChannels) {
			const MapChannel &mapChannel = mc.second;
			// Channel data
			VRay::VUtils::ValueRefList map_channel(4);
			map_channel[0].setDouble(channelIdx);
			map_channel[1].setListInt(mapChannel.faces);
			map_channel[2].setListVector(mapChannel.vertices);
			map_channel[3].setString(mapChannel.name.c_str());

			map_channels[channelIdx].setList(map_channel);
			++channelIdx;
		}

		pluginDesc.addAttribute(Attrs::PluginAttr("map_channels", map_channels));
	}

	return true;
}
