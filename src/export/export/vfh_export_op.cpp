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
#include "vfh_attr_utils.h"

#include <COP2/COP2_Node.h>
#include <TIL/TIL_Raster.h>

using namespace VRayForHoudini;
using namespace Attrs;

/// Taken from: https://www.sidefx.com/docs/hdk/_h_d_k__data_flow__c_o_p.html
static TIL_Raster *getImageFromCop(COP2_Node &copNode, double time, const char *pname = "C")
{
	TIL_Raster *image = NULL;

	short key;
	const OP_ERROR err = copNode.open(key);

	if (err == UT_ERROR_NONE) {
		const TIL_Sequence *seq = copNode.getSequenceInfo();
		if (seq) {
			const TIL_Plane *plane = seq->getPlane(pname);

			int xres = 0;
			int yres = 0;
			seq->getRes(xres, yres);

			if (plane) {
				image = new TIL_Raster(PACK_RGBA, PXL_FLOAT32, xres, yres);

				if (seq->getImageIndex(time) == -1) {
					// out of frame range - black frame
					float black[4] = {0, 0, 0, 0};
					image->clearNormal(black);
				}
				else {
					OP_Context context(time);
					context.setXres(xres);
					context.setYres(yres);

					if (!copNode.cookToRaster(image, context, plane)) {
						delete image;
						image = NULL;
					}
				}
			}
		}
	}

	// Must be called even if open() failed.
	copNode.close(key);

	return image;
}

VRay::Plugin VRayExporter::exportCopNodeBitmapBuffer(COP2_Node &copNode)
{
	VRay::Plugin res;

	TIL_Raster *raster = getImageFromCop(copNode, getContext().getTime());
	if (raster) {
		const int numPixels = raster->getNumPixels();
		if (numPixels) {
			const int numComponents = 4;
			const int bytesPerComponent = 4;
			const int pixelFormat = 1; // Float RGBA

			const int numPixelBytes = numPixels * numComponents * bytesPerComponent;
			const int numInts = numPixelBytes / sizeof(int);

			VRay::VUtils::IntRefList pixels(numInts);
			vutils_memcpy(pixels.get(), raster->getPixels(), numPixelBytes);

			Attrs::PluginDesc rawBitmapBuffer(getPluginName(&copNode, "RawBitmapBuffer"), "RawBitmapBuffer");
			rawBitmapBuffer.addAttribute(PluginAttr("pixels", pixels));
			rawBitmapBuffer.addAttribute(PluginAttr("pixels_type", pixelFormat));
			rawBitmapBuffer.addAttribute(PluginAttr("width", raster->getXres()));
			rawBitmapBuffer.addAttribute(PluginAttr("height", raster->getYres()));

			res = exportPlugin(rawBitmapBuffer);
		}

		delete raster;
	}

	return res;
}

VRay::Plugin VRayExporter::exportCopNodeWithDefaultMapping(COP2_Node &copNode, DefaultMappingType mappingType)
{
	VRay::Plugin res;

	const VRay::Plugin bitmapBuffer = exportCopNodeBitmapBuffer(copNode);
	if (bitmapBuffer) {
		// Bitmap data needs flipping for some reason.
		VRay::Matrix uvwTm(1);
		uvwTm.v1.set(0.0f, -1.0f, 0.0f);

		VRay::Plugin uvwgen;

		switch (mappingType) {
			case defaultMappingChannel: {
				Attrs::PluginDesc uvwgenDesc(getPluginName(&copNode, "UVWGenChannel"),
											 "UVWGenChannel");
				uvwgenDesc.addAttribute(PluginAttr("uvw_channel", 0));
				uvwgenDesc.addAttribute(PluginAttr("uvw_transform", VRay::Transform(uvwTm, VRay::Vector(0.0f, 0.0f, 0.0f))));

				uvwgen = exportPlugin(uvwgenDesc);
				break;
			}
			case defaultMappingChannelName: {
				Attrs::PluginDesc uvwgenDesc(getPluginName(&copNode, "UVWGenMayaPlace2dTexture"),
											 "UVWGenMayaPlace2dTexture");
				uvwgenDesc.addAttribute(PluginAttr("uv_set_name", "uv"));
				uvwgenDesc.addAttribute(PluginAttr("mirror_v", true));

				uvwgen = exportPlugin(uvwgenDesc);
				break;
			}
			case defaultMappingSpherical: {
				Attrs::PluginDesc uvwgenDesc(getPluginName(&copNode, "UVWGenEnvironment"),
											 "UVWGenEnvironment");
				uvwgenDesc.addAttribute(PluginAttr("mapping_type", "spherical"));
				uvwgenDesc.addAttribute(PluginAttr("uvw_matrix", uvwTm));

				uvwgen = exportPlugin(uvwgenDesc);
				break;
			}
			default:
				break;
		}

		if (uvwgen) {
			Attrs::PluginDesc texBitmap(getPluginName(&copNode, "TexBitmap"), "TexBitmap");
			texBitmap.addAttribute(PluginAttr("bitmap", bitmapBuffer));
			texBitmap.addAttribute(PluginAttr("uvwgen", uvwgen));

			res = exportPlugin(texBitmap);
		}
	}

	return res;
}

VRay::Plugin VRayExporter::exportCopNode(COP2_Node &copNode)
{
	return exportCopNodeWithDefaultMapping(copNode, defaultMappingChannel);
}

VRay::Plugin VRayExporter::exportOpPath(const UT_String &path)
{
	VRay::Plugin res;

	if (path.startsWith(OPREF_PREFIX)) {
		OP_Node *opNode = getOpNodeFromPath(path, getContext().getTime());
		if (opNode) {
			COP2_Node *copNode = opNode->castToCOP2Node();
			VOP_Node *vopNode = opNode->castToVOPNode();
			if (copNode) {
				res = exportCopNode(*copNode);
			}
			else if (vopNode) {
				res = exportVop(vopNode);
			}
		}
	}

	return res;
}
