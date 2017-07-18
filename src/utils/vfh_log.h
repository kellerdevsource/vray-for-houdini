//
// Copyright (c) 2015-2016, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#ifndef VRAY_FOR_HOUDINI_LOG_H
#define VRAY_FOR_HOUDINI_LOG_H

#include "vfh_defines.h"
#include "vfh_vray.h" // For proper "systemstuff.h" inclusion

#include <boost/lockfree/queue.hpp>

#include <functional>
#include <array>

namespace VRayForHoudini {
namespace Log {

enum LogLevel {
	LogLevelMsg = 0,
	LogLevelInfo,
	LogLevelProgress,
	LogLevelWarning,
	LogLevelError,
	LogLevelDebug,
};

/// Simple logger class wrapping over printf
struct Logger {
	Logger()
		: m_logLevel(LogLevelDebug)
	{}

	/// Log string with msg level, always show not taking current log level into account
	void msg(const tchar *format, ...);
	/// Log string with info level
	void info(const tchar *format, ...);
	/// Log string with progress level
	void progress(const tchar *format, ...);
	/// Log string with warning level
	void warning(const tchar *format, ...);
	/// Log string with error level
	void error(const tchar *format, ...);
	/// Log string with debug level
	void debug(const tchar *format, ...);

	/// Set max log level to be printed, unless Logger::msg is used where current filter is ignored
	void setLogLevel(LogLevel logLevel) { m_logLevel = logLevel; }

	/// Initialize the logger, needs to be called only once
	static void startLogging();
	static void stopLogging();
private:
	/// Implementation for the actual logging
	void log(LogLevel level, const tchar *format, va_list args);

	LogLevel m_logLevel; ///< Current max log level to be shown

#ifndef VFH_NO_THREAD_LOGGER
public:
	typedef std::array<tchar, 1024> LogLineType;

	struct LogPair {
		LogLevel level;
		LogLineType line;
	};

private:
	boost::lockfree::queue<LogPair> m_queue; ///< queue for messages to be logged
	static void writeMessages();
#endif
};

/// Get singleton instance to Logger
Logger &getLog();

} // namespace Log
} // namespace VRayForHoudini

#endif // VRAY_FOR_HOUDINI_LOG_H
