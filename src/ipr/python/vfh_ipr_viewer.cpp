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

/// Write image as buckets.
#define USE_BUCKETS 1

/// A typedef for render elements array.
typedef std::vector<VRay::RenderElement> RenderElementsList;
static ImdisplayThread imdisplayThread;

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
	explicit TileImage(VRay::VRayImage *image=nullptr, const QString &name="C")
		: image(image)
		, name(name)
		, x0(0)
		, x1(0)
		, y0(0)
		, y1(0)
	{}

	~TileImage() {
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
	QString name;

	int x0;
	int x1;
	int y0;
	int y1;
};

/// A list of images planes.
typedef QList<TileImage*> PlaneImages;

/// Queue message base.
struct TileQueueMessage {
	enum TileQueueMessageType {
		messageTypeNone = -1,
		messageTypeImageHeader = 0,
		messageTypeImageTiles,
	};

	virtual ~TileQueueMessage() {}

	/// Returns message type.
	virtual TileQueueMessageType type() const = 0;
};

/// Image header message.
struct ImageHeaderMessage
	: TileQueueMessage
{
	ImageHeaderMessage()
		: imageWidth(0)
		, imageHeight(0)
	{}

	TileQueueMessageType type() const VRAY_OVERRIDE { return messageTypeImageHeader; }

	int imageWidth;
	int imageHeight;
	QList<QString> planeNames;
};

/// Image planes message.
struct TileImageMessage
	: TileQueueMessage
{
	explicit TileImageMessage(const PlaneImages &images)
		: images(images)
	{}

	virtual ~TileImageMessage() {
		for (const TileImage *image : images) {
			delete image;
		}
	}

	TileQueueMessageType type() const VRAY_OVERRIDE { return messageTypeImageTiles; }

	PlaneImages images;
};


ImdisplayThread::ImdisplayThread()
	: port(0)
	, onStop([]() {})
{
	setObjectName("ImdisplayPipe");
	setTerminationEnabled();

	// TODO: utilize finished() / terminated() signals to clean-up left data.
}

ImdisplayThread::~ImdisplayThread() {
	terminate();
}

void ImdisplayThread::init() {
	arguments.clear();

	// The -p option will cause imdisplay.exe to read an image from standard in.
	arguments << "-p";

	// When using the -p option, the -k option will cause imdisplay
	// to keep reading image data after the entire image has been read.
	arguments << "-k";

	// The -f option will flip the image vertically for display
	arguments << "-f";

	// [hostname:]port will connect to the mplay process which is listening
	// on the given host/port.
	arguments << "-s" << QString::number(port);
}

void ImdisplayThread::restart() {
	clear();
	terminate();
	wait();
	start(LowPriority);
}

void ImdisplayThread::setOnStopCallback(std::function<void()> cb) {
	onStop = cb;
}

void ImdisplayThread::add(TileQueueMessage *msg) {
	QMutexLocker locker(&mutex);

	if (msg->type() == TileQueueMessage::TileQueueMessageType::messageTypeImageHeader) {
		/// Clear the queue if new resolution comes
		for (TileQueueMessage *message : queue) {
			delete message;
		}
		queue.clear();
	}
	else {
		/// Remove previous messages of the same type.
		QList<int> indexesToRemove;
		int indexToRemove = 0;
		for (const TileQueueMessage *queueMsg : queue) {
			if (queueMsg->type() == msg->type()) {
				indexesToRemove.append(indexToRemove);
			}
			indexToRemove++;
		}

		for (const int index : indexesToRemove) {
			TileQueueMessage *queueMsg = queue[index];
			queue.removeAt(index);
			delete queueMsg;
		}
	}

	queue.enqueue(msg);
}

void ImdisplayThread::clear() {
	QMutexLocker locker(&mutex);
	for (const TileQueueMessage *msg : queue) {
		delete msg;
	}
	queue.clear();
}

/// Set imdisplay port.
/// @param value Port.
void ImdisplayThread::setPort(int value) {
	port = value;
}

/// Returns current imdisplay port.
int ImdisplayThread::getPort() const {
	return port;
}

void ImdisplayThread::onPipeClose() {
	terminate();
	onStop();
}

void ImdisplayThread::onPipeError(QProcess::ProcessError error) {
	terminate();
	onStop();
}

void ImdisplayThread::onPipeStateChange(QProcess::ProcessState newState) {
	if (newState == QProcess::ProcessState::NotRunning) {
		terminate();
		onStop();
	}
}


void ImdisplayThread::run() {
	/// Pipe to the imdisplay.
	QProcess pipe;
	pipe.start("imdisplay", arguments);

	connect(&pipe, &QProcess::aboutToClose, this, &ImdisplayThread::onPipeClose);
	connect(&pipe, &QProcess::errorOccurred, this, &ImdisplayThread::onPipeError);
	connect(&pipe, &QProcess::stateChanged, this, &ImdisplayThread::onPipeStateChange);
	connect(&pipe, &QProcess::readChannelFinished, this, &ImdisplayThread::onPipeClose);
	
	if (!pipe.waitForStarted()) {
		return;
	}

	while (true) {
		if (queue.isEmpty())
			continue;
		if (!pipe.isOpen())
			break;

		mutex.lock();
		TileQueueMessage *msg = queue.dequeue();
		mutex.unlock();

		switch (msg->type()) {
			case TileQueueMessage::messageTypeImageHeader: {
				processImageHeaderMessage(*this, pipe, static_cast<ImageHeaderMessage&>(*msg));
				break;
			}
			case TileQueueMessage::messageTypeImageTiles: {
				processTileMessage(*this, pipe, static_cast<TileImageMessage&>(*msg));
				break;
			}
			default: {
				break;
			}
		}


		delete msg;
	}

	disconnect(&pipe, 0, 0, 0);
}


void ImdisplayThread::processImageHeaderMessage(QThread &worker, QProcess &pipe, ImageHeaderMessage &msg) {
	ImageHeader imageHeader;
	imageHeader.xres = msg.imageWidth;
	imageHeader.yres = msg.imageHeight;
	imageHeader.single_image_storage = 0;
	imageHeader.single_image_array_size = 4;
	imageHeader.multi_plane_count = msg.planeNames.count();
	writeHeader(worker, pipe, imageHeader);

	for (const QString &planeName : msg.planeNames) {
		const int planeNameSize = planeName.length();

		PlaneDefinition rePlaneDef;
		rePlaneDef.name_length = planeNameSize;
		rePlaneDef.data_format = 0;
		rePlaneDef.array_size = 4;
		writeHeader(worker, pipe, rePlaneDef);

		write(worker, pipe, planeNameSize, sizeof(char), planeName.toLocal8Bit().constData());
	}
}

void ImdisplayThread::processTileMessage(QThread &worker, QProcess &pipe, TileImageMessage &msg) {
	int imageIdx = 0;
	for (const TileImage *image : msg.images) {
		PlaneSelect planeHeader;
		planeHeader.plane_index = imageIdx;
		writeHeader(worker, pipe, planeHeader);
#if USE_BUCKETS
		writeTileBuckets(worker, pipe, *image);
#else
		writeTile(worker, pipe, *image);
#endif
		imageIdx++;
	}
}

/// Writes image tile to the pipe splitted into buckets.
/// @param pipe Process pipe.
/// @param image Image data.
void ImdisplayThread::writeTileBuckets(QThread &worker, QProcess &pipe, const TileImage &image) {
	static const int tileSize = 64;

	int width = 0;
	int height = 0;
	image.image->getSize(width, height);

	const int numY = height / tileSize + (height % tileSize ? 1 : 0);
	const int numX = width  / tileSize + (width  % tileSize ? 1 : 0);

	for (int m = 0; m < numY; ++m) {
		const int currentMRes = m * tileSize;
		for (int i = 0; i < numX; ++i) {
			const int currentIRes = i * tileSize;
			const int maxXRes = (currentIRes + tileSize - 1) < width  ? (currentIRes + tileSize - 1) : width  - 1;
			const int maxYRes = (currentMRes + tileSize - 1) < height ? (currentMRes + tileSize - 1) : height - 1;

			VRay::VRayImage *cropImage = image.image->crop(currentIRes,
														   currentMRes,
														   maxXRes - currentIRes + 1,
														   maxYRes - currentMRes + 1);

			TileImage *imageBucket = new TileImage(cropImage, image.name);
			imageBucket->x0 = currentIRes;
			imageBucket->x1 = maxXRes;
			imageBucket->y0 = currentMRes;
			imageBucket->y1 = maxYRes;

			writeTile(worker, pipe, *imageBucket);

			delete imageBucket;
		}
	}
}

/// Writes image tile to the pipe. Frees allocated image data.
/// @param pipe Process pipe.
/// @param image Image data.
void ImdisplayThread::writeTile(QThread &worker, QProcess &pipe, const TileImage &image) {
	TileHeader tileHeader;
	tileHeader.x0 = image.x0;
	tileHeader.x1 = image.x1;
	tileHeader.y0 = image.y0;
	tileHeader.y1 = image.y1;
	writeHeader(worker, pipe, tileHeader);

	const int numPixels = image.image->getWidth() * image.image->getHeight();

	write(worker, pipe, numPixels, sizeof(float) * 4, image.image->getPixelData());
}

/// Writes end of file marker to the pipe.
/// @param pipe Process pipe.
void ImdisplayThread::writeEOF(QThread &worker, QProcess &pipe) {
	TileHeader eof;
	eof.x0 = -2;
	writeHeader(worker, pipe, eof);
}

/// Write to pipe.
/// @param numElements Elements count.
/// @param elementSize Element size.
/// @param data Data pointer.
void ImdisplayThread::write(QThread &worker, QProcess &pipe, int numElements, int elementSize, const void *data) {
	if (pipe.state() != QProcess::Running) {
		worker.exit();
	}

	pipe.write(reinterpret_cast<const char*>(data), elementSize * numElements);
	if (!pipe.waitForBytesWritten()) {
		worker.exit();
	}
}

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

void VRayForHoudini::setImdisplayPort(int port)
{
	imdisplayThread.setPort(port);
}

int VRayForHoudini::getImdisplayPort()
{
	return imdisplayThread.getPort();
}

void VRayForHoudini::initImdisplay(VRay::VRayRenderer &renderer)
{
	Log::getLog().debug("initImdisplay()");

	imdisplayThread.init();
	imdisplayThread.restart();

	const VRay::RendererOptions &rendererOptions = renderer.getOptions();

	ImageHeaderMessage *imageHeaderMsg = new ImageHeaderMessage();
	imageHeaderMsg->imageWidth = rendererOptions.imageWidth;
	imageHeaderMsg->imageHeight = rendererOptions.imageHeight;
	imageHeaderMsg->planeNames.append("C");

	const VRay::RenderElements &reMan = renderer.getRenderElements();
	for (const VRay::RenderElement &re : reMan.getAllByType(VRay::RenderElement::NONE)) {
		imageHeaderMsg->planeNames.append(re.getName().c_str());
	}

	imdisplayThread.add(imageHeaderMsg);
}

void VRayForHoudini::setImdisplayOnStop(std::function<void()> fn)
{

}

void VRayForHoudini::closeImdisplay()
{
	Log::getLog().debug("closeImdisplay()");

	imdisplayThread.clear();
	imdisplayThread.exit();
}
