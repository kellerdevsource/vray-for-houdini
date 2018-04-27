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

#include <QFile>

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

/// Flips raster in V.
/// @param raster Raster raw pixel data.
/// @param w Image width in pixels.
/// @param h Image height in pixels.
static void flipRasterU(void *raster, int w, int h)
{
	struct MyPixel {
		float r;
		float g;
		float b;
		float a;
	};

	MyPixel *pixels = reinterpret_cast<MyPixel*>(raster);

	const int halfH = VUtils::fast_ceil(float(h) / 2.0f);

	const int rowItems = w;
	const int rowBytes = rowItems * sizeof(MyPixel);

	MyPixel *rowBuf = new MyPixel[rowItems];

	for (int i = 0; i < halfH; ++i) {
		MyPixel *toRow   = pixels + i       * rowItems;
		MyPixel *fromRow = pixels + (h-i-1) * rowItems;

		vutils_memcpy(rowBuf,  toRow,   rowBytes);
		vutils_memcpy(toRow,   fromRow, rowBytes);
		vutils_memcpy(fromRow, rowBuf,  rowBytes);
	}

	FreePtrArr(rowBuf);
}

int VRayExporter::fillCopNodeBitmapBuffer(COP2_Node &copNode, Attrs::PluginDesc &rawBitmapBuffer)
{
	QScopedPointer<TIL_Raster> raster(getImageFromCop(copNode, getContext().getTime()));
	if (!raster)
		return 0;

	const int numPixels = raster->getNumPixels();
	if (!numPixels)
		return 0;

	const int numComponents = 4;
	const int bytesPerComponent = 4;
	const int pixelFormat = 1; // Float RGBA

	const int numPixelBytes = numPixels * numComponents * bytesPerComponent;
	const int numInts = numPixelBytes / sizeof(int);

	const int w = raster->getXres();
	const int h = raster->getYres();

	// Flip image V in-place.
	flipRasterU(raster->getPixels(), w, h);

	VRay::VUtils::IntRefList pixels(numInts);
	vutils_memcpy(pixels.get(), raster->getPixels(), numPixelBytes);

	const PXL_ColorSpace rasterColorSpace = raster->getColorSpace();

	// Only valid for PXL_CS_LINEAR, PXL_CS_GAMMA2_2, and PXL_CS_CUSTOM_GAMMA.
	const fpreal rasterGamma = raster->getColorSpaceGamma();

	BitmapBufferColorSpace bitmapBufferColorSpace;
	fpreal bitmapBufferGamma;

	switch (rasterColorSpace) {
		case PXL_CS_LINEAR: {
			bitmapBufferColorSpace = bitmapBufferColorSpaceLinear;
			bitmapBufferGamma = rasterGamma;
			break;
		}
		case PXL_CS_GAMMA2_2:
		case PXL_CS_CUSTOM_GAMMA: {
			bitmapBufferColorSpace = bitmapBufferColorGammaCorrected;
			bitmapBufferGamma = rasterGamma;
			break;
		}
		case PXL_CS_SRGB: {
			bitmapBufferColorSpace = bitmapBufferColorSRGB;
			bitmapBufferGamma = 1.0;
			break;
		}
		case PXL_CS_UNKNOWN:
		case PXL_CS_OCIO:
		case PXL_CS_REC709:
		default: {
			bitmapBufferColorSpace = bitmapBufferColorSpaceLinear;
			bitmapBufferGamma = 1.0;
			break;
		}
	}

	rawBitmapBuffer.add(PluginAttr(SL("pixels"), pixels));
	rawBitmapBuffer.add(PluginAttr(SL("pixels_type"), pixelFormat));
	rawBitmapBuffer.add(PluginAttr(SL("width"), w));
	rawBitmapBuffer.add(PluginAttr(SL("height"), h));
	rawBitmapBuffer.add(PluginAttr(SL("color_space"), bitmapBufferColorSpace));
	rawBitmapBuffer.add(PluginAttr(SL("gamma"), bitmapBufferGamma));

	return 1;
}

VRay::Plugin VRayExporter::exportCopNodeBitmapBuffer(COP2_Node &copNode)
{
	VRay::Plugin res;

	Attrs::PluginDesc rawBitmapBuffer(getPluginName(copNode, SL("RawBitmapBuffer")),
	                                  SL("RawBitmapBuffer"));
	if (fillCopNodeBitmapBuffer(copNode, rawBitmapBuffer)) {
		res = exportPlugin(rawBitmapBuffer);
	}

	return res;
}

void VRayExporter::fillDefaultMappingDesc(DefaultMappingType mappingType, Attrs::PluginDesc &uvwgenDesc)
{
	switch (mappingType) {
		case defaultMappingChannel: {
			uvwgenDesc.pluginID = SL("UVWGenChannel");
			uvwgenDesc.add(PluginAttr(SL("uvw_channel"), 0));
			break;
		}
		case defaultMappingChannelName: {
			uvwgenDesc.pluginID = SL("UVWGenMayaPlace2dTexture");
			uvwgenDesc.add(PluginAttr(SL("uv_set_name"), SL("uv")));
			break;
		}
		case defaultMappingSpherical: {
			VRay::Matrix uvwTm(1);
			VUtils::swap(uvwTm[1], uvwTm[2]);
			uvwTm[2].y = -uvwTm[2].y;

			uvwgenDesc.pluginID = SL("UVWGenEnvironment");
			uvwgenDesc.add(PluginAttr(SL("mapping_type"), SL("spherical")));
			uvwgenDesc.add(PluginAttr(SL("uvw_matrix"), uvwTm));
			break;
		}
		case defaultMappingTriPlanar: {
			uvwgenDesc.pluginID = SL("UVWGenProjection");
			uvwgenDesc.add(PluginAttr(SL("type"), 6));
			uvwgenDesc.add(PluginAttr(SL("object_space"), true));
			break;
		}
		default:
			break;
	}
}

VRay::Plugin VRayExporter::exportCopNodeWithDefaultMapping(COP2_Node &copNode, DefaultMappingType mappingType)
{
	VRay::Plugin res;

	const VRay::Plugin bitmapBuffer = exportCopNodeBitmapBuffer(copNode);
	if (bitmapBuffer.isNotEmpty()) {
		Attrs::PluginDesc uvwgenDesc;
		uvwgenDesc.pluginName = SL("DefaultMapping|") % bitmapBuffer.getName(),
		fillDefaultMappingDesc(mappingType, uvwgenDesc);

		VRay::Plugin uvwgen;

		switch (mappingType) {
			case defaultMappingChannel: {
				uvwgenDesc.pluginName = getPluginName(copNode, SL("UVWGenChannel"));
				uvwgen = exportPlugin(uvwgenDesc);
				break;
			}
			case defaultMappingChannelName: {
				uvwgenDesc.pluginName = getPluginName(copNode, SL("UVWGenMayaPlace2dTexture"));
				uvwgen = exportPlugin(uvwgenDesc);
				break;
			}
			case defaultMappingSpherical: {
				uvwgenDesc.pluginName = getPluginName(copNode, SL("UVWGenEnvironment"));
				uvwgen = exportPlugin(uvwgenDesc);
				break;
			}
			default:
				break;
		}

		if (uvwgen.isNotEmpty()) {
			Attrs::PluginDesc texBitmapDesc(getPluginName(copNode, SL("TexBitmap")),
											SL("TexBitmap"));
			texBitmapDesc.add(PluginAttr(SL("bitmap"), bitmapBuffer));
			texBitmapDesc.add(PluginAttr(SL("uvwgen"), uvwgen));

			res = exportPlugin(texBitmapDesc);
		}
	}

	return res;
}

VRay::Plugin VRayExporter::exportFileTextureBitmapBuffer(const UT_String &filePath, BitmapBufferColorSpace colorSpace)
{
	Attrs::PluginDesc bitmapBufferDesc(SL("BitmapBuffer|") % QString::number(VUtils::hashlittle(filePath.buffer(), filePath.length())),
									   SL("BitmapBuffer"));

	bitmapBufferDesc.add(PluginAttr(SL("color_space"), colorSpace));
	bitmapBufferDesc.add(PluginAttr(SL("file"), filePath));

	return exportPlugin(bitmapBufferDesc);
}

VRay::Plugin VRayExporter::exportFileTextureWithDefaultMapping(const UT_String &filePath, DefaultMappingType mappingType, BitmapBufferColorSpace colorSpace)
{
	VRay::Plugin res;

	const VRay::Plugin bitmapBuffer = exportFileTextureBitmapBuffer(filePath, colorSpace);
	if (bitmapBuffer.isNotEmpty()) {
		Attrs::PluginDesc uvwgenDesc;
		uvwgenDesc.pluginName = SL("DefaultMapping|") % bitmapBuffer.getName(),
		fillDefaultMappingDesc(mappingType, uvwgenDesc);

		const VRay::Plugin uvwgen = exportPlugin(uvwgenDesc);
		if (uvwgen.isNotEmpty()) {
			Attrs::PluginDesc texBitmapDesc(SL("TexBitmap|") % bitmapBuffer.getName(),
											SL("TexBitmap"));
			texBitmapDesc.add(PluginAttr(SL("bitmap"), bitmapBuffer));
			texBitmapDesc.add(PluginAttr(SL("uvwgen"), uvwgen));

			res = exportPlugin(texBitmapDesc);
		}
	}

	return res;
}

VRay::Plugin VRayExporter::exportNodeFromPathWithDefaultMapping(const UT_String &path, DefaultMappingType mappingType, BitmapBufferColorSpace colorSpace)
{
	VRay::Plugin res;

	if (path.startsWith(OPREF_PREFIX)) {
		OP_Node *opNode = OPgetDirector()->findNode(path);
		if (opNode) {
			COP2_Node *copNode = opNode->castToCOP2Node();
			VOP_Node *vopNode = opNode->castToVOPNode();
			if (copNode) {
				res = exportCopNodeWithDefaultMapping(*copNode, mappingType);
			}
			else if (vopNode) {
				res = exportVop(vopNode);
			}
		}
	}
	else {
		QFile fileChecker(path.buffer());
		if (fileChecker.exists()) {
			res = exportFileTextureWithDefaultMapping(path, mappingType, colorSpace);
		}
	}

	return res;
}

VRay::Plugin VRayExporter::exportNodeFromPath(const UT_String &path)
{
	return exportNodeFromPathWithDefaultMapping(path, defaultMappingTriPlanar, bitmapBufferColorSpaceLinear);
}
