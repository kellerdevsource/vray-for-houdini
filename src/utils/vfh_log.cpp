//
// Copyright (c) 2015-2017, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#include "vfh_log.h"
#include "getenvvars.h"

#include <stdarg.h>

#include <thread>
#include <condition_variable>
#include <atomic>
#include <array>

#include <QThread>

#ifdef _WIN32
#define VS_DEBUG(...) VUtils::debug(__VA_ARGS__)
#else
#define VS_DEBUG(...)
#endif

using namespace VRayForHoudini;
using namespace VRayForHoudini::Log;

const std::thread::id MAIN_TID = std::this_thread::get_id(); ///< The ID of the main thread - used to distingquish in log

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

static void logMessage(const Logger::LogData &data)
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
	const unsigned tid = std::hash<std::thread::id>()(data.tid) % 10000;

	consoleMsg.append(formatter.sprintf("(%s%4u) ", data.tid == MAIN_TID ? "#" : " ", tid));
	guiMsg.append(formatter.sprintf("(%s%u) ",      data.tid == MAIN_TID ? "#" : "",  tid));
#endif
	
	consoleMsg.append(logLevelAsColor(data.level));

	const QString message(QString(data.line.data()).simplified());

	consoleMsg.append(message);
	consoleMsg.append(VUTILS_COLOR_DEFAULT);

	guiMsg.append(message);

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

/// Thread logging any messages pushed in the Logger's queue
class ThreadedLogger
	: public QThread
{
public:
	explicit ThreadedLogger(std::function<void()> cb)
		: runFunction(cb)
	{}

protected:
	void run() VRAY_OVERRIDE {
		runFunction();
	}

	std::function<void()> runFunction;
};

static ThreadedLogger *loggerThread = nullptr; ///< The thread used for logging
static std::once_flag startLogger; ///< Flag to ensure we start the thread only once
static volatile bool isStoppedLogger = false; ///< Stop flag for the thread
static VUtils::GetEnvVarInt threadedLogger("VFH_THREADED_LOGGER", 1);

void Logger::writeMessages()
{
	if (threadedLogger.getValue() == 0) {
		return;
	}
	auto & log = getLog();
	while (!isStoppedLogger) {
		if (log.m_queue.empty()) {
			std::unique_lock<std::mutex> lock(log.m_mtx);
			if (log.m_queue.empty()) {
				while (!isStoppedLogger && log.m_queue.empty()) {
					log.m_condVar.wait(lock);
				}
			}
		}
		{
			std::lock_guard<std::mutex> lock(log.m_mtx);
			while (!isStoppedLogger && !log.m_queue.empty()) {
				const LogData data = log.m_queue.front();
				log.m_queue.pop_front();
				logMessage(data);
			}
		}
	}
}

void Logger::startLogging()
{
	if (threadedLogger.getValue()) {
		Logger & log = getLog();
		std::call_once(startLogger, [&log]() {
			loggerThread = new ThreadedLogger(&Logger::writeMessages);
			loggerThread->start();
		});
	}
}

void Logger::stopLogging()
{
	if (threadedLogger.getValue()) {
		// this protects stopping
		static std::mutex mtx;
		if (loggerThread) {
			std::lock_guard<std::mutex> lock(mtx);
			if (!loggerThread) {
				return;
			}
			{
				std::lock_guard<std::mutex> loggerLock(getLog().m_mtx);
				isStoppedLogger = true;
			}

			getLog().m_condVar.notify_all();

			if (!loggerThread->wait(100)) {
				loggerThread->terminate();
				loggerThread->wait();
			}

			FreePtr(loggerThread);
		}
	}
}

void Logger::valog(LogLevel level, const tchar *format, va_list args)
{
	// Show all messages in debug.
#ifndef VFH_DEBUG
	const bool showMessage = level == LogLevelMsg
		? true
		: level <= m_logLevel;

	if (!showMessage) {
		return;
	}
#endif

	LogData data;
	time(&data.time);
	data.tid = std::this_thread::get_id();
	data.level = level;

	vsnprintf(data.line.data(), data.line.size(), format, args);

	if (threadedLogger.getValue() && !isStoppedLogger) {
		{
			std::lock_guard<std::mutex> lock(m_mtx);
			if (!isStoppedLogger) {
				m_queue.push_back(data);
			}
		}
		m_condVar.notify_one();
	}
	else {
		logMessage(data);
	}
}


void Logger::log(LogLevel level, const tchar *format, ...)
{
	va_list args;
	va_start(args, format);

	valog(level, format, args);

	va_end(args);
}


void Logger::info(const tchar *format, ...)
{
	va_list args;
	va_start(args, format);

	valog(Log::LogLevelInfo, format, args);

	va_end(args);
}


void Logger::warning(const tchar *format, ...)
{
	va_list args;
	va_start(args, format);

	valog(Log::LogLevelWarning, format, args);

	va_end(args);
}


void Logger::error(const tchar *format, ...)
{
	va_list args;
	va_start(args, format);

	valog(Log::LogLevelError, format, args);

	va_end(args);
}


void Logger::debug(const tchar *format, ...)
{
	va_list args;
	va_start(args, format);

	valog(Log::LogLevelDebug, format, args);

	va_end(args);
}


void Logger::progress(const tchar *format, ...)
{
	va_list args;
	va_start(args, format);

	valog(Log::LogLevelProgress, format, args);

	va_end(args);
}


void Logger::msg(const tchar *format, ...)
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
