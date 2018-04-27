//
// Copyright (c) 2015-2018, Chaos Software Ltd
//
// V-Ray For Houdini Python IPR Module
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#ifndef VRAY_FOR_HOUDINI_IPR_IMDISPLAY_VIEWER_H
#define VRAY_FOR_HOUDINI_IPR_IMDISPLAY_VIEWER_H

#include "vfh_defines.h"
#include "vfh_log.h"

#include <QThread>
#include <QQueue>
#include <QMutex>

class TIL_TileMPlay;

struct TileImage {
	explicit TileImage(VRay::VRayImage *image=nullptr, const QString &name="C")
		: image(image)
		, name(name)
		, x0(0)
		, x1(0)
		, y0(0)
		, y1(0)
	{}
	~TileImage();

	/// Sets image tile region.
	/// @param region Image region.
	void setRegion(const VRay::ImageRegion &region);

	/// Flips image rows.
	void flipImage() const;

	/// Image data.
	mutable VRay::VRayImage *image = nullptr;

	/// Image plane name.
	QString name;

	/// Image region.
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
		messageTypeProgress,
	};
	virtual ~TileQueueMessage() = default;

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
	QString ropName;
	QList<QString> planeNames;
};

/// Image planes message.
struct TileImageMessage
	: TileQueueMessage
{
	explicit TileImageMessage(const PlaneImages &images)
		: images(images)
	{}
	~TileImageMessage() VRAY_DTOR_OVERRIDE;

	TileQueueMessageType type() const VRAY_OVERRIDE { return messageTypeImageTiles; }

	PlaneImages images;
};

struct TileProgressMessage
	: TileQueueMessage
{
	TileQueueMessageType type() const VRAY_OVERRIDE { return messageTypeProgress; }

	QString message;
	int percentage = 0;
};

/// A queue of pipe writing tasks.
typedef QQueue<TileQueueMessage*> TileMessageQueue;

/// A thread for writing into "imdisplay".
class ImdisplayThread
	: public QThread
{
	Q_OBJECT

public:
	ImdisplayThread();
	~ImdisplayThread();

	/// Restarts image writer thread.
	void restart();

	/// Set imdisplay port.
	/// @param value Port.
	void setPort(int value);

	/// Returns current imdisplay port.
	int getPort() const;

	/// Adds new message to the queue.
	void add(TileQueueMessage *msg);

	/// Clears message queue.
	void clear();

protected:
	void run() VRAY_OVERRIDE;

private:
	/// Writes image header data to the pipe.
	/// @param msg ImageHeaderMessage message.
	void processImageHeaderMessage(ImageHeaderMessage &msg) const;

	/// Writes image tile message to the pipe.
	/// @param msg TileImageMessage message.
	void processTileMessage(TileImageMessage &msg);

	/// Writes progess tag to pipe.
	/// @param msg TileProgressMessage message.
	void processProgressMessage(const TileProgressMessage &msg) const;

	/// Writes image tile to the pipe splitted into buckets.
	/// @param planeIndex Image plane index.
	/// @param image Image data.
	void writeTileBuckets(int planeIndex, const TileImage &image);

	/// Writes image tile to the pipe. Frees allocated image data.
	/// @param planeIndex Image plane index.
	/// @param image Image data.
	void writeTile(int planeIndex, const TileImage &image);

	/// Frees all queued messages.
	void freeQueue();

	/// MPlay Port.
	int port = 0;

	/// Message queue.
	TileMessageQueue queue;

	/// Flag set to true when the thread can run, set to false in the stop() method.
	QAtomicInt isRunning = false;

	/// Queue lock.
	QMutex mutex;

	/// Tile image writing device.
	TIL_TileMPlay *device = nullptr;

	VUTILS_DISABLE_COPY(ImdisplayThread)
};

#endif // VRAY_FOR_HOUDINI_IPR_IMDISPLAY_VIEWER_H
