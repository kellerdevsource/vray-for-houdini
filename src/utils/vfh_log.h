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
	void msg(const tchar *format, ...) const;
	/// Log string with info level
	void info(const tchar *format, ...) const;
	/// Log string with progress level
	void progress(const tchar *format, ...) const;
	/// Log string with warning level
	void warning(const tchar *format, ...) const;
	/// Log string with error level
	void error(const tchar *format, ...) const;
	/// Log string with debug level
	void debug(const tchar *format, ...) const;

	/// Set max log level to be printed, unless Logger::msg is used where current filter is ignored
	void setLogLevel(LogLevel logLevel) { m_logLevel = logLevel; }

private:
	/// Implementation for the actual logging
	void log(LogLevel level, const tchar *format, va_list args) const;

	LogLevel m_logLevel; ///< Current max log level to be shown

};

/// Get singleton instance to Logger
Logger &getLog();

} // namespace Log
} // namespace VRayForHoudini

#endif // VRAY_FOR_HOUDINI_LOG_H
