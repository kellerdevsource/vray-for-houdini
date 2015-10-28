//
// Copyright (c) 2015, Chaos Software Ltd
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

struct Logger {
	Logger()
		: m_logLevel(LogLevelDebug)
	{}

	void     info(const tchar *format, ...);
	void     warning(const tchar *format, ...);
	void     error(const tchar *format, ...);
	void     debug(const tchar *format, ...);
	void     progress(const tchar *format, ...);
	void     msg(const tchar *format, ...);

	void     setLogLevel(LogLevel logLevel) { m_logLevel = logLevel; }

private:
	void     log(LogLevel level, const tchar *format, va_list args);

	LogLevel m_logLevel;

};

Logger &getLog();

} // namespace Log
} // namespace VRayForHoudini

#endif // VRAY_FOR_HOUDINI_LOG_H
