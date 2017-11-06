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
#define VUtilsPrintVisualStudio(...) VUtils::debug(__VA_ARGS__)
#else
#define VUtilsPrintVisualStudio(...)
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

/// Because there is no "formatter.asprintf()" in Qt4...
static QString formatter;

/// Timestamp buffers.
static tchar strTime[100];
static tchar strDate[100];

#ifdef _WIN32
static const int isDebuggerAttached = IsDebuggerPresent();
static const int isConsoleAttached = !!GetConsoleWindow();
#else
static const int isDebuggerAttached = false;
static const int isConsoleAttached = true;
#endif

static void logMessage(const VfhLogMessage &data)
{
	vutils_timeToStr(strTime, CountOf(strTime), data.time);
	vutils_dateToStr(strDate, CountOf(strDate), data.time);

	const char *msgEnd = data.message.endsWith('\r') && isConsoleAttached ? "\r" : "\n";
	const QString msgSimplified = data.message.simplified();

	const QString timeStamp(formatter.sprintf("[%s:%s]", strDate, strTime));

	// Un-aligned message.
	QString guiMsg("VFH ");
	guiMsg.append(formatter.sprintf("[%s] ", logLevelAsString(data.level)));
#ifdef VFH_DEBUG
	guiMsg.append(data.fromMain ? "* " : "");
#endif
	guiMsg.append(msgSimplified);

	if (isConsoleAttached) {
		// Message with aligned components for console fixed font.
		QString consoleMsg(VUTILS_COLOR_BLUE);
		consoleMsg.append(timeStamp);
		consoleMsg.append(VUTILS_COLOR_DEFAULT);
		consoleMsg.append(formatter.sprintf(" VFH |%s%8s" VUTILS_COLOR_DEFAULT "| ",
						  logLevelAsColor(data.level), logLevelAsString(data.level)));
#ifdef VFH_DEBUG
		consoleMsg.append(data.fromMain ? "* " : "  ");
#endif
		consoleMsg.append(logLevelAsColor(data.level));
		consoleMsg.append(msgSimplified);
		consoleMsg.append(VUTILS_COLOR_DEFAULT);

		vutils_cprintf(true, "%s%s", consoleMsg.toLocal8Bit().constData(), msgEnd);
		fflush(stdout);
	}

	// Print to debugger without timestamp.
	if (isDebuggerAttached) {
		VUtilsPrintVisualStudio("%s\n", guiMsg.toLocal8Bit().constData());
	}

	if (!isConsoleAttached) {
		guiMsg.prepend(" ");
		guiMsg.prepend(timeStamp);
		printf("%s\n", guiMsg.toLocal8Bit().constData());
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
		if (isConsoleAttached) {
			QMutexLocker lock(&mutex);
			queue.enqueue(msg);
		}
		else {
			logMessage(msg);
		}
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
			}
			else {
				VfhLogQueue logBatch; {
					QMutexLocker locker(&mutex);
					logBatch = queue;
					queue.clear();
				}

				for (const VfhLogMessage &msg : logBatch) {
					logMessage(msg);
				}
			}
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

void Logger::valog(LogLevel level, const tchar *format, va_list args) const
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
