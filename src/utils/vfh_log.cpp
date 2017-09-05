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
#include <chrono>
#include <condition_variable>
#include <atomic>
#include <array>

#include <QThread>

#include <boost/aligned_storage.hpp>

#ifdef _WIN32
#define VS_DEBUG(...) VUtils::debug(__VA_ARGS__)
#else
#define VS_DEBUG(...)
#endif

using namespace VRayForHoudini;
using namespace VRayForHoudini::Log;

const std::thread::id MAIN_TID = std::this_thread::get_id(); ///< The ID of the main thread - used to distingquish in log

void logMessage(Logger::LogData data)
{
	tchar strTime[100], strDate[100];
	vutils_timeToStr(strTime, COUNT_OF(strTime), data.time);
	vutils_dateToStr(strDate, COUNT_OF(strDate), data.time);
	vutils_cprintf(true, VUTILS_COLOR_BLUE "[%s:%s]" VUTILS_COLOR_MAGENTA "VFH" VUTILS_COLOR_DEFAULT "| ", strDate, strTime);

	VS_DEBUG("V-Ray For Houdini [");

	switch (data.level) {
	case LogLevelInfo:     { vutils_cprintf(true, VUTILS_COLOR_BLUE   "    Info" VUTILS_COLOR_DEFAULT "| ");                     VS_DEBUG("Info"); break; }
	case LogLevelProgress: { vutils_cprintf(true, VUTILS_COLOR_BLUE   "Progress" VUTILS_COLOR_DEFAULT "| ");                     VS_DEBUG("Progress"); break; }
	case LogLevelWarning:  { vutils_cprintf(true, VUTILS_COLOR_YELLOW " Warning" VUTILS_COLOR_DEFAULT "| " VUTILS_COLOR_YELLOW); VS_DEBUG("Warning"); break; }
	case LogLevelError:    { vutils_cprintf(true, VUTILS_COLOR_RED    "   Error" VUTILS_COLOR_DEFAULT "| " VUTILS_COLOR_RED);    VS_DEBUG("Error"); break; }
	case LogLevelDebug:    { vutils_cprintf(true, VUTILS_COLOR_CYAN   "   Debug" VUTILS_COLOR_DEFAULT "| " VUTILS_COLOR_CYAN);   VS_DEBUG("Debug"); break; }
	case LogLevelMsg:      { vutils_cprintf(true, VUTILS_COLOR_GREEN  "     Msg" VUTILS_COLOR_DEFAULT "| " VUTILS_COLOR_GREEN);  VS_DEBUG("Msg"); break; }
	}

	if (data.level == LogLevelDebug) {
		const unsigned tid = std::hash<std::thread::id>()(data.tid) % 10000;
		if (data.tid == MAIN_TID) {
			vutils_cprintf(true, VUTILS_COLOR_YELLOW "(#%4u) " VUTILS_COLOR_DEFAULT, tid);
		} else {
			vutils_cprintf(true, VUTILS_COLOR_YELLOW "(%4u) " VUTILS_COLOR_DEFAULT, tid);
		}
	}

	vutils_cprintf(true, "%s\n" VUTILS_COLOR_DEFAULT, data.line.data());
	VS_DEBUG("] %s\n", data.line.data());

	fflush(stdout);
	fflush(stderr);
}


/// Thread logging any messages pushed in the Logger's queue
class ThreadedLogger:
	public QThread
{
public:
	ThreadedLogger(std::function<void()> cb): runFunction(cb) {

	}
protected:
	void run() VRAY_OVERRIDE {
		runFunction();
	}
	std::function<void()> runFunction;
};

static ThreadedLogger * loggerThread = nullptr; ///< The thread used for logging
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
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
		LogData data;
		while (!isStoppedLogger && log.m_queue.pop(data)) {
			logMessage(data);
		}
	}
}

void Logger::startLogging()
{
	if (threadedLogger.getValue()) {
		Logger & log = getLog();
		std::call_once(startLogger, [&log]() {
			// We can use unsafe here since we are the only thread accessing it
			log.m_queue.reserve_unsafe(128);
			loggerThread = new ThreadedLogger(&Logger::writeMessages);
			loggerThread->start();
		});
	}
}

void Logger::stopLogging()
{
	if (threadedLogger.getValue()) {
		static std::mutex mtx;
		if (loggerThread) {
			std::lock_guard<std::mutex> lock(mtx);
			isStoppedLogger = true;
			if (loggerThread) {
				if (!loggerThread->wait(50)) {
					loggerThread->terminate();
					loggerThread->wait();
				}
				delete loggerThread;
				loggerThread = nullptr;
			}
		}
	}
}

void Logger::valog(LogLevel level, const char *format, va_list args)
{
	const bool showMessage = level == LogLevelMsg
		? true
		: level <= m_logLevel;

	if (!showMessage) {
		return;
	}

	LogData data;
	time(&data.time);
	data.tid = std::this_thread::get_id();
	data.level = level;

	vsnprintf(data.line.data(), data.line.size(), format, args);

	if (threadedLogger.getValue()) {
		// Try to push 10 times and relent the thread after each unsuccessfull attempt
		// This will prevent endless loop in normal push
		for (int c = 0; c < 10; c++) {
			if (m_queue.bounded_push(data)) {
				return;
			}
			std::this_thread::yield();
		}
		// 10 spins did not work - try sleep for 10ms
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
		if (!m_queue.bounded_push(data)) {
			// just log the message from this thread
			logMessage(data);
		}
	} else {
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
