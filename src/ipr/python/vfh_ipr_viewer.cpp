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

#include "vfh_defines.h"
#include "vfh_log.h"
#include "vfh_ipr_viewer.h"

#include <QtCore>

#include <UT/UT_WritePipe.h>

using namespace VRayForHoudini;

/// Write image as buckets.
#define USE_BUCKETS 1

/// A typedef for render elements array.
typedef std::vector<VRay::RenderElement> RenderElementsList;

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

struct TileImage {
	explicit TileImage(VRay::VRayImage *image=nullptr, const std::string &name="C")
		: image(image)
		, name(name)
		, x0(0)
		, x1(0)
		, y0(0)
		, y1(0)
	{}

	void freeMem() {
		FreePtr(image);
	}

	void setRegion(const VRay::ImageRegion &region) {
		x0 = region.getX();
		y0 = region.getY();

		// Those are inclusive.
		x1 = x0 + region.getWidth() - 1;
		y1 = y0 + region.getHeight() - 1;
	}

	VRay::VRayImage *image;
	std::string name;

	int x0;
	int x1;
	int y0;
	int y1;
};

/// A list of images planes.
typedef QList<TileImage> PlaneImages;
const int z = sizeof(PlaneImages);
/// A thread for writing into "imdisplay".
class ImdisplayThread
	: public QThread
{
public:
	ImdisplayThread() {}
	~ImdisplayThread() {
		close();
	}

	void init(int port) {
		static char buf[MAX_PATH] = "";
		vutils_sprintf_n(buf, COUNT_OF(buf), "imdisplay -p -k -f -s %i", port);

		/// TODO: Error checking.
		wp.open(buf);
	}

	void close() {
		if (wp.getFilePtr()) {
			TileHeader eof;
			eof.x0 = -2;
			writeHeader(eof);
		}

		wp.close();
	}

	void add(const PlaneImages &images) {
		// No need to write previous images.
		clear();

		mutex.lock();
		queue.enqueue(images);
		mutex.unlock();
	}

	void clear() {
		mutex.lock();
		queue.clear();
		mutex.unlock();
	}

	void setRendererOptions(VRay::VRayRenderer &renderer) {
		VRay::RendererOptions rendererOptions = renderer.getOptions();

		const VRay::RenderElements &reMan = renderer.getRenderElements();
		const RenderElementsList &reList = reMan.getAllByType(VRay::RenderElement::NONE);

		ImageHeader imageHeader;
		imageHeader.xres = rendererOptions.imageWidth;
		imageHeader.yres = rendererOptions.imageHeight;
		imageHeader.single_image_storage = 0;
		imageHeader.single_image_array_size = 4;
		imageHeader.multi_plane_count = 1 + reList.size();
		writeHeader(imageHeader);

		PlaneDefinition cPlaneDef;
		cPlaneDef.name_length = 1;
		cPlaneDef.data_format = 0;
		cPlaneDef.array_size = 4;
		writeHeader(cPlaneDef);
		write(1, sizeof(char), "C");

		for (const VRay::RenderElement &re : reList) {
			const std::string &reName = re.getName();
			const int planeNameSize = reName.size();

			PlaneDefinition rePlaneDef;
			rePlaneDef.name_length = planeNameSize;
			rePlaneDef.data_format = 0;
			rePlaneDef.array_size = 4;
			writeHeader(rePlaneDef);

			write(planeNameSize, sizeof(char), reName.c_str());
		}
	}

protected:
	void run() VRAY_OVERRIDE {
		while (true) {
			// NOTE: May be lock isEmpty() also
			if (!queue.isEmpty()) {
				mutex.lock();
				PlaneImages images = queue.dequeue();
				mutex.unlock();

				int imageIdx = 0;
				for (TileImage &image : images) {
					PlaneSelect planeHeader;
					planeHeader.plane_index = imageIdx;
					writeHeader(planeHeader);
#if USE_BUCKETS
					writeTileBuckets(image);
#else
					writeTile(image);
#endif
					imageIdx++;
				}
			}
		}
    }

private:
	/// Writes image tile to the pipe splitted into buckets.
	/// @param image Image data.
	void writeTileBuckets(TileImage &image) {
		static const int tileSize = 64;

		int width = 0;
		int height = 0;
		image.image->getSize(width, height);

		const int numY = height / 64 + (height % 64 ? 1 : 0);
		const int numX = width  / 64 + (width  % 64 ? 1 : 0);

		for (int m = 0; m < numY; ++m) {
			const int currentMRes = m * tileSize;
			for (int i = 0; i < numX; ++i) {
				const int currentIRes = i * tileSize;
				const int maxXRes = (currentIRes + tileSize - 1) < width  ? (currentIRes + tileSize - 1) : width  - 1;
				const int maxYRes = (currentMRes + tileSize - 1) < height ? (currentMRes + tileSize - 1) : height - 1;

				TileImage imageBucket;
				imageBucket.image = image.image->crop(currentIRes,
													  currentMRes,
													  maxXRes - currentIRes + 1,
													  maxYRes - currentMRes + 1);
				imageBucket.x0 = currentIRes;
				imageBucket.x1 = maxXRes;
				imageBucket.y0 = currentMRes;
				imageBucket.y1 = maxYRes;

				writeTile(imageBucket);
			}
		}
	}

	/// Writes image tile to the pipe.
	/// @param image Image data.
	void writeTile(TileImage &image) {
		TileHeader tileHeader;
		tileHeader.x0 = image.x0;
		tileHeader.x1 = image.x1;
		tileHeader.y0 = image.y0;
		tileHeader.y1 = image.y1;
		writeHeader(tileHeader);

		const int numPixels = image.image->getWidth() * image.image->getHeight();

		write(numPixels, sizeof(float) * 4, image.image->getPixelData());

		image.freeMem();
	}

	/// Write specified header to pipe.
	/// @param header Imdisplay header.
	template <typename HeaderType>
	void writeHeader(const HeaderType &header) {
		write(1, sizeof(HeaderType), &header);
	}

	/// Write to pipe.
	/// @param numElements Elements count.
	/// @param elementSize Element size.
	/// @param data Data pointer.
	void write(int numElements, int elementSize, const void *data) {
		if (!wp.getFilePtr()) {
			return;
		}

		if (fwrite(data, elementSize, numElements, wp.getFilePtr()) != numElements) {
			Log::getLog().error("Pipe write fail!");

			clear();
			close();
		}
	}

	/// Pipe to the imdisplay.
	UT_WritePipe wp;

	/// Image queue.
	QQueue<PlaneImages> queue;

	/// Queue lock.
	QMutex mutex;

	VfhDisableCopy(ImdisplayThread)
} imdisplayThread;

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

	TileImage rgbaImage(image);
	rgbaImage.setRegion(region);
	planes.append(rgbaImage);

	const VRay::RenderElements &reMan = renderer.getRenderElements();
	const RenderElementsList &reList = reMan.getAllByType(VRay::RenderElement::NONE);
	for (const VRay::RenderElement &re : reList) {
		TileImage renderElementImage(re.getImage(&region), re.getName());
		renderElementImage.setRegion(region);

		planes.append(renderElementImage);
	}

	imdisplayThread.add(planes);
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

void VRayForHoudini::closeImdisplay()
{
	Log::getLog().debug("closeImdisplay()");

	imdisplayThread.clear();
	imdisplayThread.close();
	imdisplayThread.terminate();
}

void VRayForHoudini::initImdisplay(VRay::VRayRenderer &renderer, int port)
{
	Log::getLog().debug("initImdisplay()");

	closeImdisplay();

	imdisplayThread.init(port);
	imdisplayThread.setRendererOptions(renderer);
	imdisplayThread.start(QThread::LowPriority);
}
