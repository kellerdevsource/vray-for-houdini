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

#include <IMG/IMG_TileDevice.h>
#include <IMG/IMG_TileOptions.h>
#include <TIL/TIL_TileMPlay.h>

using namespace VRayForHoudini;

static const int tileSize = 128;

TileImageMessage::~TileImageMessage()
{
	for (const TileImage *image : images) {
		delete image;
	}
}

ImdisplayThread::ImdisplayThread()
{
	setObjectName("ImdisplayThread");
}

ImdisplayThread::~ImdisplayThread()
{
	isRunning = false;

	quit();
	wait();
}

void ImdisplayThread::restart()
{
	isRunning = true;

	clear();
	start(LowPriority);
}

void ImdisplayThread::add(TileQueueMessage *msg)
{
	QMutexLocker locker(&mutex);

	if (msg->type() == TileQueueMessage::messageTypeImageHeader) {
		// Clear the queue if new resolution comes.
		freeQueue();
	}
	else {
		// Remove previous messages of the same type.
		QMutableListIterator<TileQueueMessage*> queueIt(queue);
		while (queueIt.hasNext()) {
			const TileQueueMessage *queueMsg = queueIt.next();
			if (queueMsg->type() == msg->type()) {
				delete queueMsg;
				queueIt.remove();
			}
		}
	}

	queue.enqueue(msg);
}

void ImdisplayThread::clear()
{
	QMutexLocker locker(&mutex);
	freeQueue();
}

void ImdisplayThread::freeQueue()
{
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
	device->setSendPIDFlag(false);
	device->terminateOnConnectionLost(false);

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

		switch (msg->type()) {
			case TileQueueMessage::messageTypeImageHeader: {
				processImageHeaderMessage(static_cast<ImageHeaderMessage&>(*msg));
				break;
			}
			case TileQueueMessage::messageTypeImageTiles: {
				processTileMessage(static_cast<TileImageMessage&>(*msg));
				break;
			}
			case TileQueueMessage::messageTypeProgress: {
				processProgressMessage(static_cast<TileProgressMessage&>(*msg));
				break;
			}
			default: {
				break;
			}
		}

		FreePtr(msg);
	}

	clear();

	FreePtr(device);
}

TileImage::~TileImage()
{
	FreePtr(image);
}

void TileImage::setRegion(const VRay::ImageRegion &region)
{
	x0 = region.getX();
	y0 = region.getY();

	// Those are inclusive.
	x1 = x0 + region.getWidth() - 1;
	y1 = y0 + region.getHeight() - 1;
}

void TileImage::flipImage() const
{
	int w = 0;
	int h = 0;
	if (!image->getSize(w, h))
		return;

	VRay::AColor *pixels = image->getPixelData();

	const int halfH = h / 2;

	const int rowItems = w;
	const int rowBytes = rowItems * sizeof(VRay::AColor);

	VRay::AColor *rowBuf = new VRay::AColor[rowItems];

	for (int i = 0; i < halfH; ++i) {
		VRay::AColor *toRow   = pixels + i       * rowItems;
		VRay::AColor *fromRow = pixels + (h-i-1) * rowItems;

		vutils_memcpy(rowBuf,  toRow,   rowBytes);
		vutils_memcpy(toRow,   fromRow, rowBytes);
		vutils_memcpy(fromRow, rowBuf,  rowBytes);
	}

	FreePtrArr(rowBuf);
}

void ImdisplayThread::processImageHeaderMessage(ImageHeaderMessage &msg) const
{
	IMG_TileOptionList tileOptionList;

	for (const QString &planeName : msg.planeNames) {
		IMG_TileOptions	*tileOptions = new IMG_TileOptions();
		tileOptions->setPlaneInfo(_toChar(msg.ropName),
								  _toChar(planeName),
								  0,
								  IMG_FLOAT32,
								  IMG_RGBA);
		tileOptions->setFormatOption("socketport",
									 _toChar(QString::number(port)));

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
		device->flush();
	}
}

void ImdisplayThread::processTileMessage(TileImageMessage &msg) 
{
	int planeIndex = 0;

	for (const TileImage *image : msg.images) {
		if (!isRunning)
			return;

		image->flipImage();

		writeTileBuckets(planeIndex, *image);

		planeIndex++;
	}
}

void ImdisplayThread::processProgressMessage(const TileProgressMessage &msg) const
{
	if (!isRunning)
		return;
#if 0
	const QString percentage(QString::number(msg.percentage));

	VUtils::Table<const char*, 1> msgData(1, NULL);
	msgData[0] = _toChar(msg.message);

	VUtils::Table<const char*, 1> percData(1, NULL);
	percData[0] = _toChar(percentage);

	Log::getLog().debug("IPR: Received message: \"%s\" %s", msgData[0], percData[0]);

	// This locks sometimes...
	// device->writeKnownTag(IMG_TAG_PROGRESS_MSG, 1, msgData.elements);
	// device->writeKnownTag(IMG_TAG_PROGRESS, 1, percData.elements);
#endif
}

void ImdisplayThread::writeTileBuckets(int planeIndex, const TileImage &image)
{
	int width = 0;
	int height = 0;
	if (!image.image->getSize(width, height))
		return;

	const int numY = height / tileSize + (height % tileSize ? 1 : 0);
	const int numX = width  / tileSize + (width  % tileSize ? 1 : 0);

	for (int m = 0; m < numY && isRunning; ++m) {
		const int currentMRes = m * tileSize;
		for (int i = 0; i < numX && isRunning; ++i) {
			const int currentIRes = i * tileSize;
			const int maxXRes = (currentIRes + tileSize - 1) < width  ? (currentIRes + tileSize - 1) : width  - 1;
			const int maxYRes = (currentMRes + tileSize - 1) < height ? (currentMRes + tileSize - 1) : height - 1;

			VRay::VRayImage *cropImage = image.image->crop(currentIRes,
			                                               currentMRes,
			                                               maxXRes - currentIRes + 1,
			                                               maxYRes - currentMRes + 1);

			TileImage imageBucket(cropImage, image.name);
			imageBucket.x0 = currentIRes;
			imageBucket.x1 = maxXRes;
			imageBucket.y0 = currentMRes;
			imageBucket.y1 = maxYRes;

			writeTile(planeIndex, imageBucket);
		}
	}
}

void ImdisplayThread::writeTile(int planeIndex, const TileImage &image)
{
	if (!device->isOpen() || device->wasRemoteQuitRequested()) {
		// We are using failed write as close event.
		isRunning = false;
	}

	if (!isRunning)
		return;

	if (!device->writeDeepTile(planeIndex, image.image->getPixelData(), image.x0, image.x1, image.y0, image.y1)) {
		// We are using failed write as close event.
		isRunning = false;
	}
	else {
		device->flush();
	}
}

#ifndef Q_MOC_RUN
#include <vfh_ipr_imdisplay_viewer.h.moc>
#endif
