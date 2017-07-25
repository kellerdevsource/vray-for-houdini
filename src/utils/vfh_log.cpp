//
// Copyright (c) 2015-2016, Chaos Software Ltd
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

#ifndef VFH_NO_THREAD_LOGGER
void logMessage(LogLevel level, Logger::LogLineType logBuff) {
	vutils_cprintf(true, VUTILS_COLOR_MAGENTA "V-Ray For Houdini" VUTILS_COLOR_DEFAULT "|");
	VS_DEBUG("V-Ray For Houdini [");

	switch (level) {
	case LogLevelInfo:     { vutils_cprintf(true, VUTILS_COLOR_BLUE   "    Info" VUTILS_COLOR_DEFAULT "| ");                     VS_DEBUG("Info"); break; }
	case LogLevelProgress: { vutils_cprintf(true, VUTILS_COLOR_BLUE   "Progress" VUTILS_COLOR_DEFAULT "| ");                     VS_DEBUG("Progress"); break; }
	case LogLevelWarning:  { vutils_cprintf(true, VUTILS_COLOR_YELLOW " Warning" VUTILS_COLOR_DEFAULT "| " VUTILS_COLOR_YELLOW); VS_DEBUG("Warning"); break; }
	case LogLevelError:    { vutils_cprintf(true, VUTILS_COLOR_RED    "   Error" VUTILS_COLOR_DEFAULT "| " VUTILS_COLOR_RED);    VS_DEBUG("Error"); break; }
	case LogLevelDebug:    { vutils_cprintf(true, VUTILS_COLOR_CYAN   "   Debug" VUTILS_COLOR_DEFAULT "| " VUTILS_COLOR_CYAN);   VS_DEBUG("Debug"); break; }
	case LogLevelMsg:      { vutils_cprintf(true, VUTILS_COLOR_GREEN  "     Msg" VUTILS_COLOR_DEFAULT "| " VUTILS_COLOR_GREEN);  VS_DEBUG("Msg"); break; }
	}

	vutils_cprintf(true, "%s\n" VUTILS_COLOR_DEFAULT, logBuff.data());
	VS_DEBUG("] %s\n", logBuff.data());

	fflush(stdout);
	fflush(stderr);
}

#endif

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

static ThreadedLogger * loggerThread = nullptr; ///< the thread used for logging
static std::once_flag startLogger; ///< flag to ensure we start the thread only once
static volatile bool isStoppedLogger = false; ///< stop flag for the thread
static VUtils::GetEnvVarInt threadedLogger("VFH_THREADED_LOGGER", 1);
const auto appStart = std::chrono::high_resolution_clock::now();
const std::thread::id MAIN_TID = std::this_thread::get_id();

void Logger::writeMessages() {
	if (threadedLogger.getValue() == 0) {
		return;
	}
	auto & log = getLog();
	while (!isStoppedLogger) {
		if (log.m_queue.empty()) {
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
		LogPair msg;
		while (log.m_queue.pop(msg)) {
			logMessage(msg.level, msg.line);
		}
	}
}

void Logger::startLogging() {
	if (threadedLogger.getValue()) {
		Logger & log = getLog();
		std::call_once(startLogger, [&log]() {
			log.m_queue.reserve_unsafe(128);
			loggerThread = new ThreadedLogger(&Logger::writeMessages);
			loggerThread->start();
		});
	}
}

void Logger::stopLogging() {
	if (threadedLogger.getValue()) {
		static std::mutex mtx;
		if (loggerThread) {
			std::lock_guard<std::mutex> lock(mtx);
			isStoppedLogger = true;
			if (loggerThread) {
				loggerThread->terminate();
				loggerThread->wait();
				delete loggerThread;
				loggerThread = nullptr;
			}
		}
	}
}

void Logger::log(LogLevel level, const char *format, va_list args)
{
	LogLineType buf;

	using namespace std::chrono;
	const auto now = high_resolution_clock::now() - appStart;
	const unsigned msecs = duration_cast<milliseconds>(now).count() % 1000;
	const unsigned secs = duration_cast<seconds>(now).count() % 60;
	const std::thread::id thisTID = std::this_thread::get_id();
	const unsigned tid = std::hash<std::thread::id>()(thisTID) % 10000;

	if (thisTID == MAIN_TID) {
		const int realPad = snprintf(buf.data(), buf.size(), VUTILS_COLOR_YELLOW "[%2u:%-3u](#%4u) " VUTILS_COLOR_DEFAULT, secs, msecs, tid);
		const int pad = (sizeof(VUTILS_COLOR_YELLOW) - 1) + 16 + (sizeof(VUTILS_COLOR_DEFAULT) - 1);
		vassert(realPad == pad && "Unexpected length for time and TID in log message");
		vsnprintf(buf.data() + pad, buf.size() - pad, format, args);
	} else {
		const int realPad = snprintf(buf.data(), buf.size(), VUTILS_COLOR_YELLOW "[%2u:%-3u](%4u) " VUTILS_COLOR_DEFAULT, secs, msecs, tid);
		const int pad = (sizeof(VUTILS_COLOR_YELLOW) - 1) + 15 + (sizeof(VUTILS_COLOR_DEFAULT) - 1);
		vassert(realPad == pad && "Unexpected length for time and TID in log message");
		vsnprintf(buf.data() + pad, buf.size() - pad, format, args);
	}

	const int showMessage = level == LogLevelMsg
							? true
							: level <= m_logLevel;

	if (showMessage) {
		if (threadedLogger.getValue()) {
			// try to push 10 times and relent the thread after each unsuccessfull attempt
			// this will prevent endless loop in normal push
			const LogPair pair = {level, buf};
			for (int c = 0; c < 10; c++) {
				if (m_queue.bounded_push(pair)) {
					return;
				}
				// TODO: is it worth to sleep(1) after 5 attempts?
				std::this_thread::yield();
			}
			// at this point we tried 10 pushes but did not work, so just log the message from this thread
			logMessage(level, buf);
		} else {
			logMessage(level, buf);
		}
	}
}


void Logger::info(const tchar *format, ...)
{
	va_list args;
	va_start(args, format);

	log(Log::LogLevelInfo, format, args);

	va_end(args);
}


void Logger::warning(const tchar *format, ...)
{
	va_list args;
	va_start(args, format);

	log(Log::LogLevelWarning, format, args);

	va_end(args);
}


void Logger::error(const tchar *format, ...)
{
	va_list args;
	va_start(args, format);

	log(Log::LogLevelError, format, args);

	va_end(args);
}


void Logger::debug(const tchar *format, ...)
{
	va_list args;
	va_start(args, format);

	log(Log::LogLevelDebug, format, args);

	va_end(args);
}


void Logger::progress(const tchar *format, ...)
{
	va_list args;
	va_start(args, format);

	log(Log::LogLevelProgress, format, args);

	va_end(args);
}


void Logger::msg(const tchar *format, ...)
{
	va_list args;
	va_start(args, format);

	log(Log::LogLevelMsg, format, args);

	va_end(args);
}


VRayForHoudini::Log::Logger &VRayForHoudini::Log::getLog()
{
	static Logger logger;
	return logger;
}
