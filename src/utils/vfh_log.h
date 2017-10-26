//
// Copyright (c) 2015-2017, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#ifndef VRAY_FOR_HOUDINI_LOG_H
#define VRAY_FOR_HOUDINI_LOG_H

#include "vfh_vray.h" // For proper "systemstuff.h" inclusion

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

/// Simple logger class wrapping over printf.
struct Logger {
	Logger()
		: logLevel(LogLevelDebug)
	{}

	/// Log string with msg level, always show not taking current log level into account.
	void msg(const tchar *format, ...) const;
	
	/// Log string with info level.
	void info(const tchar *format, ...) const;

	/// Log string with progress level.
	void progress(const tchar *format, ...) const;

	/// Log string with warning level.
	void warning(const tchar *format, ...) const;

	/// Log string with error level
	void error(const tchar *format, ...) const;

	/// Log string with debug level.
	void debug(const tchar *format, ...) const;

	/// Log string with custom level.
	void log(LogLevel level, const tchar *format, ...) const;

	/// Set max log level to be printed, unless Logger::msg is used where current filter is ignored.
	void setLogLevel(LogLevel value) { logLevel = value; }

	/// Initialize the logger, needs to be called only once.
	/// Don't call this from static variable's ctor, which is inside a module (causes deadlock).
	static void startLogging();

	/// Call this before exiting the application, and not from static variable's dtor
	/// It will stop and join the logger thread.
	static void stopLogging();

private:
	/// Implementation for the actual logging.
	void valog(LogLevel level, const tchar *format, va_list args) const;

	/// Current max log level to be shown.
	LogLevel logLevel;
};

/// Get singleton instance to Logger
Logger &getLog();

} // namespace Log
} // namespace VRayForHoudini

#endif // VRAY_FOR_HOUDINI_LOG_H
