//
// Copyright (c) 2015-2017, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#include <QThread>
#include <QMutexLocker>
#include <QQueue>
#include <QApplication>

#include <thread>

#include "vfh_defines.h"
#include "vfh_log.h"

#ifdef _WIN32
#define VS_DEBUG(...) VUtils::debug(__VA_ARGS__)
#else
#define VS_DEBUG(...)
#endif

using namespace VRayForHoudini;
using namespace VRayForHoudini::Log;

/// The ID of the main thread - used to distingquish in log.
const std::thread::id mainThreadID = std::this_thread::get_id();

static const char* logLevelAsString(LogLevel level)
{
	switch (level) {
		case LogLevelMsg:      return "Message";
		case LogLevelInfo:     return "Info";
		case LogLevelProgress: return "Progress";
		case LogLevelWarning:  return "Warning";
		case LogLevelError:    return "Error";
		case LogLevelDebug:    return "Debug";
		default:               return "Unknown";
	}
}

static const char* logLevelAsColor(LogLevel level)
{
	switch (level) {
		case LogLevelMsg:      return VUTILS_COLOR_DEFAULT;
		case LogLevelInfo:     return VUTILS_COLOR_DEFAULT;
		case LogLevelProgress: return VUTILS_COLOR_GREEN;
		case LogLevelWarning:  return VUTILS_COLOR_YELLOW;
		case LogLevelError:    return VUTILS_COLOR_RED;
		case LogLevelDebug:    return VUTILS_COLOR_CYAN;
		default:               return VUTILS_COLOR_DEFAULT;
	}
}

struct VfhLogMessage {
	/// The message's log level.
	LogLevel level;

	/// The message.
	QString message;

	/// The time the log was made.
	time_t time;

	/// The thread ID of the caller thread.
	int fromMain;
};

static void logMessage(const VfhLogMessage &data)
{
#ifdef _WIN32
	const int isDebuggerAttached = IsDebuggerPresent();
	const int isConsoleAttached = !!GetConsoleWindow();
#else
	const int isDebuggerAttached = false;
	const int isConsoleAttached = true;
#endif

	tchar strTime[100];
	tchar strDate[100];
	vutils_timeToStr(strTime, CountOf(strTime), data.time);
	vutils_dateToStr(strDate, CountOf(strDate), data.time);

	// Because there is no "formatter.sprintf()" in Qt4...
	QString formatter;

	const QString timeStamp(formatter.sprintf("[%s:%s]", strDate, strTime));

	QString colorTimeStamp(VUTILS_COLOR_BLUE);
	colorTimeStamp.append(timeStamp);
	colorTimeStamp.append(VUTILS_COLOR_DEFAULT);

	// Message with aligned components for console fixed font.
	QString consoleMsg(colorTimeStamp);
	consoleMsg.append(" VFH ");
	consoleMsg.append(formatter.sprintf("|%s%8s" VUTILS_COLOR_DEFAULT "| ",
					  logLevelAsColor(data.level), logLevelAsString(data.level)));

	// Un-aligned message.
	QString guiMsg("VFH ");
	guiMsg.append(formatter.sprintf("[%s] ", logLevelAsString(data.level)));

#ifdef VFH_DEBUG
	consoleMsg.append(data.fromMain ? "* " : "  ");
	guiMsg.append(    data.fromMain ? "* " : "");
#endif

	consoleMsg.append(logLevelAsColor(data.level));
	consoleMsg.append(data.message.simplified());
	consoleMsg.append(VUTILS_COLOR_DEFAULT);

	guiMsg.append(data.message.simplified());

	if (isDebuggerAttached) {
		VS_DEBUG("%s\n", guiMsg.toLocal8Bit().constData());
	}

	guiMsg.prepend(" ");
	guiMsg.prepend(timeStamp);

	const QString &printMessage = isConsoleAttached ? consoleMsg : guiMsg;

	vutils_cprintf(true, "%s\n", printMessage.toLocal8Bit().constData());

	if (isConsoleAttached) {
		fflush(stdout);
		fflush(stderr);
	}
}

class VfhLogThread
	: public QThread
{
public:
	~VfhLogThread() {
		stopLogging();
		quit();
		wait();
	}

	void add(const VfhLogMessage &msg) {
		QMutexLocker lock(&mutex);
		queue.enqueue(msg);
	}

	void startLogging() {
		isWorking = true;
	}

	void stopLogging() {
		isWorking = false;
	}

protected:
	void run() VRAY_OVERRIDE {
		while (isWorking) {
			if (queue.isEmpty()) {
				msleep(100);
				continue;
			}

			VfhLogMessage msg; {
				QMutexLocker locker(&mutex);
				msg = queue.dequeue();
			}

			logMessage(msg);
		}
	}

private:
	typedef QQueue<VfhLogMessage> VfhLogQueue;

	/// Queue lock.
	QMutex mutex;
	QAtomicInt isWorking;

	/// Log message queue.
	VfhLogQueue queue;
} logThread;

void Logger::startLogging()
{
	logThread.startLogging();
	logThread.start(QThread::LowestPriority);
}

void Logger::stopLogging()
{
	logThread.stopLogging();
	logThread.quit();
	logThread.wait();
}

void Logger::valog(LogLevel level, const tchar *format, va_list args)
{
	// Show all messages in debug.
#ifndef VFH_DEBUG
	const bool showMessage = level == LogLevelMsg ? true : level <= logLevel;
	if (!showMessage)
		return;
#endif

	tchar msgBuf[2048];
	vsnprintf(msgBuf, CountOf(msgBuf), format, args);

	VfhLogMessage msg;
	time(&msg.time);
	msg.level = level;
	msg.message = msgBuf;
	msg.fromMain = std::this_thread::get_id() == mainThreadID;

	logThread.add(msg);
}

void Logger::log(LogLevel level, const tchar *format, ...) const
{
	va_list args;
	va_start(args, format);

	valog(level, format, args);

	va_end(args);
}

void Logger::info(const tchar *format, ...) const
{
	va_list args;
	va_start(args, format);

	valog(Log::LogLevelInfo, format, args);

	va_end(args);
}

void Logger::warning(const tchar *format, ...) const
{
	va_list args;
	va_start(args, format);

	valog(Log::LogLevelWarning, format, args);

	va_end(args);
}

void Logger::error(const tchar *format, ...) const
{
	va_list args;
	va_start(args, format);

	valog(Log::LogLevelError, format, args);

	va_end(args);
}

void Logger::debug(const tchar *format, ...) const
{
	va_list args;
	va_start(args, format);

	valog(Log::LogLevelDebug, format, args);

	va_end(args);
}

void Logger::progress(const tchar *format, ...) const
{
	va_list args;
	va_start(args, format);

	valog(Log::LogLevelProgress, format, args);

	va_end(args);
}

void Logger::msg(const tchar *format, ...) const
{
	va_list args;
	va_start(args, format);

	valog(Log::LogLevelMsg, format, args);

	va_end(args);
}

VRayForHoudini::Log::Logger &VRayForHoudini::Log::getLog()
{
	static Logger logger;
	return logger;
}
