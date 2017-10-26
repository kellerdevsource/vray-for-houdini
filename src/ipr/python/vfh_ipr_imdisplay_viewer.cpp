//
// Copyright (c) 2015-2017, Chaos Software Ltd
//
// V-Ray For Houdini Python IPR Module
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#include "vfh_ipr_imdisplay_viewer.h"
#include "vfh_log.h"

using namespace VRayForHoudini;

static const int tileSize = 64;

ImdisplayThread::ImdisplayThread()
	: port(0)
	, onStop([]() {})
{
	setObjectName("ImdisplayPipe");
}

void ImdisplayThread::restart()
{
	clear();
	isRunning = true;

	start(LowPriority);
}

void ImdisplayThread::stop(bool callCallback)
{
	if (callCallback) {
		onStop();
	}

	isRunning = false;
}

void ImdisplayThread::setOnStopCallback(std::function<void()> cb)
{
	onStop = cb;
}

void ImdisplayThread::add(TileQueueMessage *msg)
{
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

void ImdisplayThread::clear()
{
	QMutexLocker locker(&mutex);
	for (const TileQueueMessage *msg : queue) {
		delete msg;
	}
	queue.clear();
}

void ImdisplayThread::setPort(int value)
{
	port = value;
}

int ImdisplayThread::getPort() const
{
	return port;
}

void ImdisplayThread::run()
{
	device = static_cast<TIL_TileMPlay*>(IMG_TileDevice::newMPlayDevice(0));

	while (isRunning) {
		TileQueueMessage *msg = nullptr; {
			QMutexLocker locker(&mutex);
			if (!queue.isEmpty()) {
				msg = queue.dequeue();
			}
		}

		if (!msg) {
			msleep(100);
			continue;
		}

		if (device->wasRemoteQuitRequested()) {
			Log::getLog().debug("Remote interrupt requested...");
			break;
		}

		switch (msg->type()) {
			case TileQueueMessage::messageTypeImageHeader: {
				processImageHeaderMessage(static_cast<ImageHeaderMessage&>(*msg));
				break;
			}
			case TileQueueMessage::messageTypeImageTiles: {
				processTileMessage(static_cast<TileImageMessage&>(*msg));
				break;
			}
			default: {
				break;
			}
		}

		FreePtr(msg);
	}

	Log::getLog().debug("Deleting tile device...");
	FreePtr(device);

	clear();
}

void ImdisplayThread::processImageHeaderMessage(ImageHeaderMessage &msg)
{
	IMG_TileOptionList tileOptionList;

	for (const QString &planeName : msg.planeNames) {
		IMG_TileOptions	*tileOptions = new IMG_TileOptions();
		tileOptions->setPlaneInfo("ip",
								  planeName.toLocal8Bit().constData(),
								  0,
								  IMG_FLOAT32,
								  IMG_RGBA);
		tileOptions->setFormatOption("socketport",
									 QString::number(port).toLocal8Bit().constData());

		tileOptionList.append(tileOptions);
	}

	if (device->isOpen()) {
		device->flush();
		device->close(false);
	}

	if (!device->openMulti(tileOptionList, msg.imageWidth, msg.imageHeight, tileSize, tileSize, 1.0)) {
		Log::getLog().error("Error opening tile device!");
	}
	else {
		device->terminateOnConnectionLost(false);
		device->flush();
	}
}

void ImdisplayThread::processTileMessage(TileImageMessage &msg)
{
	int imageIdx = 0;
	for (const TileImage *image : msg.images) {
		if (!isRunning) {
			return;
		}

#if USE_BUCKETS
		writeTileBuckets(*image);
#else
		writeTile(*image);
#endif
		imageIdx++;
	}
}

void ImdisplayThread::writeTileBuckets(const TileImage &image)
{
	int width = 0;
	int height = 0;
	image.image->getSize(width, height);

	const int numY = height / tileSize + (height % tileSize ? 1 : 0);
	const int numX = width  / tileSize + (width  % tileSize ? 1 : 0);

	for (int m = 0; m < numY && isRunning; ++m) {
		const int currentMRes = m * tileSize;
		for (int i = 0; i < numX && isRunning; ++i) {
			const int currentIRes = i * tileSize;
			const int maxXRes = (currentIRes + tileSize - 1) < width  ? (currentIRes + tileSize - 1) : width  - 1;
			const int maxYRes = (currentMRes + tileSize - 1) < height ? (currentMRes + tileSize - 1) : height - 1;

			std::unique_ptr<VRay::VRayImage> cropImage =
				std::unique_ptr<VRay::VRayImage>(image.image->crop(currentIRes,
												 currentMRes,
												 maxXRes - currentIRes
												 + 1,
												 maxYRes - currentMRes
												 + 1));

			if (!isRunning) {
				return;
			}

			const std::unique_ptr<TileImage> imageBucket =
				std::make_unique<TileImage>(cropImage.release(), image.name);
			imageBucket->x0 = currentIRes;
			imageBucket->x1 = maxXRes;
			imageBucket->y0 = currentMRes;
			imageBucket->y1 = maxYRes;

			writeTile(*imageBucket);
		}
	}
}

void ImdisplayThread::writeTile(const TileImage &image)
{
	if (!device->isOpen() ||
		device->wasRemoteQuitRequested())
	{
		isRunning = false;
	}
	if (!isRunning) {
		return;
	}

	try {
		if (!device->writeTile(image.image->getPixelData(), image.x0, image.x1, image.y0, image.y1)) {
			Log::getLog().debug("Error writing tile data \"%s\" [%d %d %d %d]!\n",
								image.name.toLocal8Bit().constData(),
								image.x0, image.x1, image.y0, image.y1);
		}
		else {
			device->flush();
		}
	}
	catch (...) {
		isRunning = false;
	}
}
