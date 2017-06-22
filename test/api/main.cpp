/// Based on: houdini/public/deepmplay/README.html

#include <vraysdk.hpp>

#include <QtWidgets>
#include <QtCore>

typedef std::vector<VRay::RenderElement> RenderElementsList;

static QMutex callbackMtx;

#pragma pack(push, 1)

struct ImageHeader {
	ImageHeader()
		: magic_number(('h'<<24)+('M'<<16)+('P'<< 8)+'0')
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

	///< Sequentially increasing integer
	int plane_number;

	///< The length of the plane name
	int name_length;

	///< Format of the data
	int data_format;

	///< Array size of the data
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

/// NOTE: The tile coordinates are specified as inclusive coordinates.
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

#pragma pack(pop)

struct VRayIMDisplayImage {
	VRayIMDisplayImage()
		: name("C")
		, image(nullptr)
		, ownImage(true)
	{}

	std::string name;
	VRay::VRayImage *image;
	int ownImage;
};

typedef QList<VRayIMDisplayImage> VRayIMDisplayImages;

struct IMDisplay {
	IMDisplay() {
		const QString programPath = "C:/Program Files/Side Effects Software/Houdini 16.0.600/bin/imdisplay.exe";

		QStringList arguments;

		// The -p option will cause imdisplay.exe to read an image from standard in.
		arguments << "-p";

		// When using the -p option, the -k option will cause imdisplay
		// to keep reading image data after the entire image has been read.
		arguments << "-k";

		// The -f option will flip the image vertically for display
		arguments << "-f";

		// [hostname:]port will connect to the mplay process which is listening
		// on the given host/port.
		arguments << "-s" << "62218";

		proc.start(programPath, arguments);
		proc.waitForStarted();
	}

	~IMDisplay() {
		if (proc.state() != QProcess::Running)
			return;
		proc.waitForBytesWritten();
		proc.closeWriteChannel();
		proc.close();
	}

	template <typename HeaderType>
	void writeHeader(const HeaderType &header) {
		proc.write(reinterpret_cast<const char*>(&header), sizeof(HeaderType));
		proc.waitForBytesWritten();
	}

	void writeImages(const VRayIMDisplayImages &images) {
		if (!images.count()) {
			return;
		}

		// All images are of the same size / format
		VRay::VRayImage *firstImage = images[0].image;

		int width = 0;
		int height = 0;
		firstImage->getSize(width, height);

		const int numPixels = width * height;
		const int numPixelBytes = numPixels * sizeof(float) * 4;

		ImageHeader imageHeader;
		imageHeader.xres = width;
		imageHeader.yres = height;
		imageHeader.multi_plane_count = images.count();
		writeHeader(imageHeader);

		for (int imageIdx = 0; imageIdx < images.count(); ++imageIdx) {
			const VRayIMDisplayImage &vi = images[imageIdx];
			const char *planeName = vi.name.c_str();
			const int   planeNameSize = vi.name.length();

			PlaneDefinition planesDef;
			planesDef.name_length = planeNameSize;
			planesDef.data_format = 0;
			planesDef.array_size = 4;
			writeHeader(planesDef);

			// Write plane name.
			proc.write(reinterpret_cast<const char*>(planeName), planeNameSize);
			proc.waitForBytesWritten();
		}

		for (int imageIdx = 0; imageIdx < images.count(); ++imageIdx) {
			const VRayIMDisplayImage &vi = images[imageIdx];

			PlaneSelect planeHeader;
			planeHeader.plane_index = imageIdx;
			writeHeader(planeHeader);

			TileHeader tileHeader;
			tileHeader.x1 = width - 1;
			tileHeader.y1 = height - 1;
			writeHeader(tileHeader);

			proc.write(reinterpret_cast<const char*>(vi.image->getPixelData()), numPixelBytes);
			proc.waitForBytesWritten();
		}

		TileHeader eof;
		eof.x0 = -2;
		writeHeader(eof);
	}

private:
	QProcess proc;

	Q_DISABLE_COPY(IMDisplay)
};

struct CallbackData {
	CallbackData() {}

	IMDisplay imdisplay;

private:
    Q_DISABLE_COPY(CallbackData)
};

static void writeRendererImages(VRay::VRayRenderer &renderer, VRay::VRayImage *beautyPass)
{
	VRayIMDisplayImages images;

	VRayIMDisplayImage beautyImage;
	beautyImage.name = "C";
	beautyImage.image = beautyPass;
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

	IMDisplay imd;
	imd.writeImages(images);

	for (VRayIMDisplayImage &vi : images) {
		if (vi.ownImage) {
			delete vi.image;
		}
	}
}

static void onRTImageUpdated(VRay::VRayRenderer &renderer, VRay::VRayImage *image, void*)
{
	if (!callbackMtx.tryLock())
		return;

	VRay::VRayImage *rtImage = image->clone();
	if (rtImage) {
		writeRendererImages(renderer, rtImage);
		delete rtImage;
	}

	callbackMtx.unlock();
}

static void onImageReady(VRay::VRayRenderer &renderer, void*) 
{
	if (!callbackMtx.tryLock())
		return;

	VRay::VRayImage *finalImage = renderer.getImage();
	if (finalImage) {
		writeRendererImages(renderer, finalImage);
		delete finalImage;
	}

	callbackMtx.unlock();
}

static void renderScene(const char *filepath)
{
	VRay::RendererOptions options;
	options.renderMode = VRay::RendererOptions::RENDER_MODE_RT_CPU;
	options.keepRTRunning = true;
	options.imageWidth  = 800;
	options.imageHeight = 600;
	options.rtNoiseThreshold = 0.5f;
	options.showFrameBuffer = false;

	VRay::VRayRenderer vray(options);
	vray.setOnImageReady(onImageReady);
	vray.setOnRTImageUpdated(onRTImageUpdated);

	if (vray.load(filepath) == 0) {
		vray.start();
		vray.waitForImageReady();
	}
}

int main(int argc, char *argv[])
{
	VRay::VRayInit vrayInit(false);

	if (argc == 2) {
		const char *sceneFilePath = argv[1];
		renderScene(sceneFilePath);
	}

	return 0;
}
