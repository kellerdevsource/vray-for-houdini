//
// Copyright (c) 2015-2017, Chaos Software Ltd
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

/// Write image as buckets.
#define USE_BUCKETS 1

/// Write render channels in IRP.
#define USE_RENDER_CHANNELS 1

class TIL_TileMPlay;

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

	/// Flips image rows.
	void flipImage() const;

	mutable VRay::VRayImage *image;
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

	virtual ~TileImageMessage() {
		for (const TileImage *image : images) {
			delete image;
		}
	}

	TileQueueMessageType type() const VRAY_OVERRIDE { return messageTypeImageTiles; }

	PlaneImages images;
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

	/// Writes image tile to the pipe splitted into buckets.
	/// @param planeIndex Image plane index.
	/// @param image Image data.
	void writeTileBuckets(int planeIndex, const TileImage &image);

	/// Writes image tile to the pipe. Frees allocated image data.
	/// @param planeIndex Image plane index.
	/// @param image Image data.
	void writeTile(int planeIndex, const TileImage &image);

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
