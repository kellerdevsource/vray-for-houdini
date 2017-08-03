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
#include "vfh_process_check.h"

#include <QThread>
#include <QProcess>
#include <QQueue>
#include <QMutex>

#include <functional>

class TileQueueMessage;
class ImageHeaderMessage;
class TileImageMessage;
class TileImage;

// TODO: use smart pointers for the messages
/// A queue of pipe writing tasks.
typedef QQueue<TileQueueMessage*> TileMessageQueue;

/// A thread for writing into "imdisplay".
class ImdisplayThread
	: public QThread
{
	Q_OBJECT
public:
	ImdisplayThread();

	void init();

	void restart();
	
	/// Stop the thread
	/// tries to stop gracefully for 250ms, after that calls terminate() if thread has not stopped
	/// @param callCallback - if true it will also call the onStop callback
	void stop(bool callCallback = false);

	void setOnStopCallback(std::function<void()> cb);

	void add(TileQueueMessage *msg);

	void clear();

	/// Set the process checker
	void setProcCheck(ProcessCheckPtr p) { pCheck = p; };

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
	void processImageHeaderMessage(QProcess &pipe, ImageHeaderMessage &msg);

	/// Writes image tile message to the pipe.
	/// @param pipe Process pipe.
	/// @param msg TileImageMessage message.
	void processTileMessage(QProcess &pipe, TileImageMessage &msg);

	/// Writes image tile to the pipe splitted into buckets.
	/// @param pipe Process pipe.
	/// @param image Image data.
	void writeTileBuckets(QProcess &pipe, const TileImage &image);

	/// Writes image tile to the pipe. Frees allocated image data.
	/// @param pipe Process pipe.
	/// @param image Image data.
	void writeTile(QProcess &pipe, const TileImage &image);

	/// Writes end of file marker to the pipe.
	/// @param pipe Process pipe.
	void writeEOF(QProcess &pipe);

	/// Write specified header to pipe.
	/// @param header Imdisplay header.
	template <typename HeaderType>
	void writeHeader(QProcess &pipe, const HeaderType &header) {
		write(pipe, 1, sizeof(HeaderType), &header);
	}

	/// Write to pipe.
	/// @param numElements Elements count.
	/// @param elementSize Element size.
	/// @param data Data pointer.
	void write(QProcess &pipe, int numElements, int elementSize, const void *data);

	/// Arguments to be passed to the started process
	QStringList arguments;

	/// Imdisplay port.
	int port;

	/// Message queue.
	TileMessageQueue queue;

	/// Flag set to true when the thread can run, set to false in the stop() method
	/// This is volatile to discourage compiler optimizing the read since we dont lock when stopping the thread
	volatile bool isRunning;

	/// Queue lock.
	QMutex mutex;

	/// Callback to be called if pipe closes unexpectedly
	std::function<void()> onStop;

	/// Pointer to the process checker - used to explicitly check before every write
	ProcessCheckPtr pCheck;

	VfhDisableCopy(ImdisplayThread)
};

#endif // VRAY_FOR_HOUDINI_IPR_IMDISPLAY_VIEWER_H
