//
// Copyright (c) 2015-2017, Chaos Software Ltd
//
// V-Ray For Houdini Python IPR Module
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#include "vfh_defines.h"
#include "vfh_log.h"
#include "vfh_ipr_viewer.h"
#include "vfh_ipr_imdisplay_viewer.h"

#include <QtCore>

#include <UT/UT_WritePipe.h>

using namespace VRayForHoudini;

/// A typedef for render elements array.
typedef std::vector<VRay::RenderElement> RenderElementsList;

static ImdisplayThread imdisplayThread;

static void addImages(VRay::VRayRenderer &renderer, VRay::VRayImage *image, int x, int y)
{
	vassert(image);

	int width = 0;
	int height = 0;
	image->getSize(width, height);

	VRay::ImageRegion region(x, y);
	region.setWidth(width);
	region.setHeight(height);

	PlaneImages planes;

	TileImage *rgbaImage = new TileImage(image, "C");
	rgbaImage->setRegion(region);
	planes.append(rgbaImage);

	const VRay::RenderElements &reMan = renderer.getRenderElements();
	const RenderElementsList &reList = reMan.getAllByType(VRay::RenderElement::NONE);
	for (const VRay::RenderElement &re : reList) {
		TileImage *renderElementImage = new TileImage(re.getImage(&region), re.getName().c_str());
		renderElementImage->setRegion(region);

		planes.append(renderElementImage);
	}

	imdisplayThread.add(new TileImageMessage(planes));
}

void VRayForHoudini::onBucketReady(VRay::VRayRenderer &renderer, int x, int y, const char*, VRay::VRayImage *image, void*)
{
	addImages(renderer, image, x, y);
}

void VRayForHoudini::onRTImageUpdated(VRay::VRayRenderer &renderer, VRay::VRayImage *image, void*)
{
	addImages(renderer, image, 0, 0);
}

void VRayForHoudini::onImageReady(VRay::VRayRenderer &renderer, void*)
{
	addImages(renderer, renderer.getImage(), 0, 0);
}

ImdisplayThread & VRayForHoudini::getImdisplay()
{
	return imdisplayThread;
}

void VRayForHoudini::initImdisplay(VRay::VRayRenderer &renderer, const char *ropName)
{
	Log::getLog().debug("initImdisplay()");

	const VRay::RendererOptions &rendererOptions = renderer.getOptions();

	ImageHeaderMessage *imageHeaderMsg = new ImageHeaderMessage();
	imageHeaderMsg->imageWidth = rendererOptions.imageWidth;
	imageHeaderMsg->imageHeight = rendererOptions.imageHeight;
	imageHeaderMsg->planeNames.append("C");
	imageHeaderMsg->ropName = ropName;

	const VRay::RenderElements &reMan = renderer.getRenderElements();
	for (const VRay::RenderElement &re : reMan.getAllByType(VRay::RenderElement::NONE)) {
		imageHeaderMsg->planeNames.append(re.getName().c_str());
	}

	imdisplayThread.add(imageHeaderMsg);
}
