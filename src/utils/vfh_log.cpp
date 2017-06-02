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

#include <stdarg.h>


using namespace VRayForHoudini;
using namespace VRayForHoudini::Log;


void Logger::log(LogLevel level, const char *format, va_list args) const
{
	tchar buf[1024] = "";
	vsnprintf(buf, CountOf(buf), format, args);

	const int showMessage = level == LogLevelMsg
							? true
							: level <= m_logLevel;

	if (showMessage) {
		vutils_cprintf(true, VUTILS_COLOR_MAGENTA "V-Ray For Houdini" VUTILS_COLOR_DEFAULT "|");

		switch (level) {
			case LogLevelInfo:    { vutils_cprintf(true, VUTILS_COLOR_BLUE   "    Info" VUTILS_COLOR_DEFAULT "| "); break; }
			case LogLevelProgress:{ vutils_cprintf(true, VUTILS_COLOR_BLUE   "Progress" VUTILS_COLOR_DEFAULT "| "); break; }
			case LogLevelWarning: { vutils_cprintf(true, VUTILS_COLOR_YELLOW " Warning" VUTILS_COLOR_DEFAULT "| " VUTILS_COLOR_YELLOW); break; }
			case LogLevelError:   { vutils_cprintf(true, VUTILS_COLOR_RED    "   Error" VUTILS_COLOR_DEFAULT "| " VUTILS_COLOR_RED); break; }
			case LogLevelDebug:   { vutils_cprintf(true, VUTILS_COLOR_CYAN   "   Debug" VUTILS_COLOR_DEFAULT "| " VUTILS_COLOR_CYAN); break; }
			case LogLevelMsg:     { vutils_cprintf(true, VUTILS_COLOR_GREEN  "     Msg" VUTILS_COLOR_DEFAULT "| " VUTILS_COLOR_GREEN); break; }
		}

		vutils_cprintf(true, "%s" VUTILS_COLOR_DEFAULT, buf);

		if (level == LogLevelDebug) {
			VUtils::debug("V-Ray For Houdini: %s\n", buf);
		}

		if (level != LogLevelProgress) {
			printf("\n");
		}
		fflush(stdout);
	}
}


void Logger::info(const tchar *format, ...) const
{
	va_list args;
	va_start(args, format);

	log(Log::LogLevelInfo, format, args);

	va_end(args);
}


void Logger::warning(const tchar *format, ...) const
{
	va_list args;
	va_start(args, format);

	log(Log::LogLevelWarning, format, args);

	va_end(args);
}


void Logger::error(const tchar *format, ...) const
{
	va_list args;
	va_start(args, format);

	log(Log::LogLevelError, format, args);

	va_end(args);
}


void Logger::debug(const tchar *format, ...) const
{
	va_list args;
	va_start(args, format);

	log(Log::LogLevelDebug, format, args);

	va_end(args);
}


void Logger::progress(const tchar *format, ...) const
{
	va_list args;
	va_start(args, format);

	log(Log::LogLevelProgress, format, args);

	va_end(args);
}


void Logger::msg(const tchar *format, ...) const
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
