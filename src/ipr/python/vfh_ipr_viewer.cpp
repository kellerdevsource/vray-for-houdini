//
// Copyright (c) 2015-2017, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text:
//  https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#include "vfh_ipr_viewer.h"

#include <UT/UT_WritePipe.h>

#include <QtCore>

#include <mutex>
#include <queue>

#include <stdio.h>

#ifndef _WIN32
inline FILE* _popen(const char* command, const char* type) {
	return popen(command, type);
}
#endif

struct ImageHeader {
	ImageHeader()
		: magic_number(('h' << 24) + ('M' << 16) + ('P' << 8) + '0')
		, xres(0)
		, yres(0)
		, single_image_storage(0)
		, single_image_array_size(0)
		, multi_plane_count(0)
	{}

	const int magic_number;
	int xres;
	int yres;

	/// Pixel format:
	///  0 = Floating point data
	///  1 = Unsigned char data
	///  2 = Unsigned short data
	///  4 = Unsigned int data
	int single_image_storage;

	/// Channles per pixel
	///  1 = A single channel image
	///  3 = RGB data
	///  4 = RGBA data
	int single_image_array_size;

	int multi_plane_count;

	int reserved[2];
};

struct PlaneDefinition {
	PlaneDefinition()
		: plane_number(0)
		, name_length(0)
		, data_format(0)
		, array_size(0)
	{}

	/// Sequentially increasing integer
	int plane_number;

	/// The length of the plane name
	int name_length;

	/// Format of the data
	int data_format;

	/// Array size of the data
	int array_size;

	int reserved[4];
};

struct PlaneSelect {
	PlaneSelect()
		: plane_marker(-1)
		, plane_index(0)
	{}

	const int plane_marker;

	int plane_index;

	int reserved[2];
};

struct TileHeader {
	TileHeader()
		: x0(0)
		, x1(0)
		, y0(0)
		, y1(0)
	{}
	int	x0;
	int x1;
	int	y0;
	int y1;
};

struct VRayIMDisplayImage {
	VRayIMDisplayImage()
		: name("C")
		, image(nullptr)
		, ownImage(true)
		, single(true)
		, x0(0)
		, x1(0)
		, y0(0)
		, y1(0)
	{}

	std::string name;
	VRay::VRayImage *image;
	int ownImage;

	int single;
	int x0;
	int x1;
	int y0;
	int y1;
};

typedef QList<VRayIMDisplayImage> VRayIMDisplayImages;
typedef std::vector<VRay::RenderElement> RenderElementsList;

class Tasks {
public:
	Tasks()
		: finalImage(false)
		, fd(nullptr)
	{}

	~Tasks() {
		transmitData();

		if (fd) {
			wp.close(false);
		}
	}

	void init(int port) {
		static char buf[MAX_PATH] = "";
		vutils_sprintf_n(buf, COUNT_OF(buf), "imdisplay -p -k -f -s %i", port);

		fd = wp.open(buf);
	}

	void setRendererOptions(VRay::VRayRenderer &renderer) {
		VRay::RendererOptions rendererOptions = renderer.getOptions();
		
		ImageHeader imageHeader;
		imageHeader.xres = rendererOptions.imageWidth;
		imageHeader.yres = rendererOptions.imageHeight;

		VRay::RenderElements reMan = renderer.getRenderElements();
		RenderElementsList reList = reMan.getAllByType(VRay::RenderElement::NONE);
		if (reList.size() < 2) {//channel count
			imageHeader.single_image_array_size = 4;
			imageHeader.single_image_storage = 0;
		}
		else {
			imageHeader.multi_plane_count = reList.size();
		}
		writeHeader(imageHeader);

		if (reList.size() > 1) {
			for (int imageIdx = 0; imageIdx < reList.size(); ++imageIdx) {
				//for (int k = 0; k < 2; k++) {
					//const VRayIMDisplayImage &vi = images[imageIdx];
					char tempArray[40] = { '\0' };
					const char *planeName = itoa(imageIdx, tempArray, 10);
					std::string temp = tempArray;
					const int   planeNameSize = temp.size();

					PlaneDefinition planesDef;
					planesDef.name_length = planeNameSize;
					planesDef.data_format = 0;
					planesDef.array_size = 4;
					writeHeader(planesDef);

					// Write plane name.
					if (fwrite(reinterpret_cast<const char*>(planeName), sizeof(char), planeNameSize, fd) != planeNameSize) {
						perror("Failed writing plane data, reason: ");
						Q_ASSERT(false);
					}
				//}
			}
		}

	}

	void registerTask(VRayIMDisplayImages &images, bool fnlImage = false) {
		if (images.empty()) {
			return;
		}

		if (images[0].single) {
			mtx.try_lock();
			cutUpSingleImage(images);
			mtx.unlock();
		}
		else {
			mtx.try_lock();
			imagesQueue.push(images);
			mtx.unlock();
		}
		finalImage = fnlImage||finalImage;
		transmitData();
	}

	void transmitData() {
		while (imagesQueue.size()>0) {
			mtx.try_lock();
			writeImages(imagesQueue.front());
			imagesQueue.pop();
			mtx.unlock();
		}
	}

private:
	template <typename HeaderType>
	void writeHeader(const HeaderType &header) {
		if (fwrite(reinterpret_cast<const char*>(&header), sizeof(HeaderType), 1, fd) != 1) {
			perror("Failed writing header data, reason: ");
			Q_ASSERT(false);
		}
	}

	// Split a single image into tiles and adds them to the images to be written queue.
	void cutUpSingleImage(VRayIMDisplayImages &images) {
		const int tileSize = 64;
		int width, height;
		images[0].image->getSize(width, height);

		int numberOfTiles = ((width / 64) * (height / 64) + ((width % 64) ? 1 : 0) + ((height % 64) ? 1 : 0));

		for (int m = 0; m < ((height / 64) + ((height % 64) ? 1 : 0)); m++) {
			int currentMRes = m * tileSize;
			for (int i = 0; i < ((width / 64) + ((width % 64) ? 1 : 0)); i++) {
				int currentIRes = i * tileSize;
				VRayIMDisplayImages tempImages;
				for (int channel = 0; channel < images.count(); channel++) {
					VRayIMDisplayImage temp;

					int maxXRes = (currentIRes + tileSize - 1) < width ? (currentIRes + tileSize - 1) : width-1;
					int maxYRes = (currentMRes + tileSize - 1) < height ? (currentMRes + tileSize - 1) : height-1;

					temp.image = images[channel].image->crop(currentIRes, 
															currentMRes, 
															maxXRes - currentIRes+1,
															maxYRes - currentMRes+1);
					temp.x0 = currentIRes;
					temp.x1 = maxXRes;
					temp.y0 = currentMRes;
					temp.y1 = maxYRes;
					temp.single = false;

					tempImages.push_back(temp);
				}
				imagesQueue.push(tempImages);
			}
		}
	}

	void writeImages(VRayIMDisplayImages &images) {
		if (!images.count()) {
			return;
		}
		writeTiles(images);
	}

	void writeTiles(VRayIMDisplayImages &images) {
		if (!images[0].single) {
			for (int imageIdx = 0; imageIdx < images.count(); ++imageIdx) {
				const VRayIMDisplayImage &vi = images[imageIdx];
				if (images.count() > 1) {
					PlaneSelect planeHeader;
					planeHeader.plane_index = imageIdx;
					writeHeader(planeHeader);
				}

				TileHeader tileHeader;
				tileHeader.x0 = vi.x0;
				tileHeader.x1 = vi.x1;
				tileHeader.y0 = vi.y0;
				tileHeader.y1 = vi.y1;
				writeHeader(tileHeader);

				size_t numPixels = images[imageIdx].image->getWidth() * images[imageIdx].image->getHeight();
				size_t size = 0;
				if((size = fwrite(vi.image->getPixelData(), sizeof(float) * 4, numPixels, fd)) != numPixels) {
					perror("Failed writing image data, reason: ");
					Q_ASSERT(false);
				}
			}

			if (finalImage&&imagesQueue.size() == 1) {
				TileHeader eof;
				eof.x0 = -2;
				writeHeader(eof);
			}
		}
		else
			cutUpSingleImage(images);
	}

	bool finalImage;
	UT_WritePipe wp;
	FILE* fd;
	std::queue<VRayIMDisplayImages> imagesQueue;
	std::mutex mtx;
} tasks;

static void writeFinalRendererImages(VRay::VRayRenderer &renderer, VRay::VRayImage *beautyPass)
{
	VRayIMDisplayImages images;

	VRayIMDisplayImage beautyImage;
	beautyImage.name = "C";
	beautyImage.image = beautyPass->clone();
	beautyImage.ownImage = false;

	images += beautyImage;

	VRay::RenderElements reMan = renderer.getRenderElements();
	RenderElementsList reList = reMan.getAllByType(VRay::RenderElement::NONE);
	for (const VRay::RenderElement &re : reList) {
		VRayIMDisplayImage image;
		image.image = re.getImage();
		image.name = re.getName();

		images += image;
	}

	tasks.registerTask(images, true);
}

static void writeRendererImages(VRay::VRayRenderer &renderer, VRay::VRayImage *beautyPass)
{
	VRayIMDisplayImages images;

	VRayIMDisplayImage beautyImage;
	beautyImage.name = "C";
	beautyImage.image = beautyPass->clone();
	beautyImage.ownImage = false;

	images += beautyImage;

	VRay::RenderElements reMan = renderer.getRenderElements();
	RenderElementsList reList = reMan.getAllByType(VRay::RenderElement::NONE);
	for (const VRay::RenderElement &re : reList) {
		VRayIMDisplayImage image;
		image.image = re.getImage();
		image.name = re.getName();

		images += image;
	}

	tasks.registerTask(images);
}


void VRayForHoudini::initImdisplay(int port)
{
	tasks.init(port);
}

void VRayForHoudini::onRTImageUpdated(VRay::VRayRenderer &renderer, VRay::VRayImage *image, void*)
{
	VRay::VRayImage *rtImage = image->clone();
	if (rtImage) {
		writeRendererImages(renderer, rtImage);
		tasks.transmitData();
		delete rtImage;
	}
}

void VRayForHoudini::onImageReady(VRay::VRayRenderer &renderer, void*)
{
	VRay::VRayImage *finalImage = renderer.getImage();
	if (finalImage) {
		writeFinalRendererImages(renderer, finalImage);
		tasks.transmitData();
		delete finalImage;
	}
}

void VRayForHoudini::onBucketReady(VRay::VRayRenderer&, int x, int y, const char*, VRay::VRayImage* img, void*)
{
	int width, height;
	img->getSize(width, height);
	VRayIMDisplayImage bucketTileData;
	bucketTileData.image = img->clone();
	bucketTileData.x0 = x;
	bucketTileData.x1 = x + width - 1;
	bucketTileData.y0 = y;
	bucketTileData.y1 = y + height - 1;
	bucketTileData.single = false;

	VRayIMDisplayImages temporary;
	temporary.push_back(bucketTileData);

	tasks.registerTask(temporary);
}

void VRayForHoudini::onRenderStart(VRay::VRayRenderer &renderer, void*)
{
	tasks.setRendererOptions(renderer);
}
