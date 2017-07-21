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
#include "vfh_ipr_viewer.h"

#include <QThread>
#include <QProcess>
#include <QQueue>
#include <QMutex>

#include <functional>

class TileQueueMessage;
class ImageHeaderMessage;
class TileImageMessage;
class TileImage;

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

	void init();

	void restart();

	void setOnStopCallback(std::function<void()> cb);

	void add(TileQueueMessage *msg);

	void clear();
	
	/// Set imdisplay port.
	/// @param value Port.
	void setPort(int value);

	/// Returns current imdisplay port.
	int getPort() const;

protected Q_SLOTS:
	void onPipeClose();

	void onPipeError(QProcess::ProcessError error);

	void onPipeStateChange(QProcess::ProcessState newState);


protected:
	void run() VRAY_OVERRIDE;

private:
	/// Writes image header data to the pipe.
	/// @param pipe Process pipe.
	/// @param msg ImageHeaderMessage message.
	static void processImageHeaderMessage(QThread &worker, QProcess &pipe, ImageHeaderMessage &msg);

	/// Writes image tile message to the pipe.
	/// @param pipe Process pipe.
	/// @param msg TileImageMessage message.
	static void processTileMessage(QThread &worker, QProcess &pipe, TileImageMessage &msg);

	/// Writes image tile to the pipe splitted into buckets.
	/// @param pipe Process pipe.
	/// @param image Image data.
	static void writeTileBuckets(QThread &worker, QProcess &pipe, const TileImage &image);

	/// Writes image tile to the pipe. Frees allocated image data.
	/// @param pipe Process pipe.
	/// @param image Image data.
	static void writeTile(QThread &worker, QProcess &pipe, const TileImage &image);

	/// Writes end of file marker to the pipe.
	/// @param pipe Process pipe.
	static void writeEOF(QThread &worker, QProcess &pipe);

	/// Write specified header to pipe.
	/// @param header Imdisplay header.
	template <typename HeaderType>
	static void writeHeader(QThread &worker, QProcess &pipe, const HeaderType &header) {
		write(worker, pipe, 1, sizeof(HeaderType), &header);
	}

	/// Write to pipe.
	/// @param numElements Elements count.
	/// @param elementSize Element size.
	/// @param data Data pointer.
	static void write(QThread &worker, QProcess &pipe, int numElements, int elementSize, const void *data);

	QStringList arguments;

	/// Imdisplay port.
	int port;

	/// Message queue.
	TileMessageQueue queue;

	/// Queue lock.
	QMutex mutex;

	/// Callback to be called if pipe closes unexpectedly
	std::function<void()> onStop;

	VfhDisableCopy(ImdisplayThread)
};

#endif // VRAY_FOR_HOUDINI_IPR_IMDISPLAY_VIEWER_H
