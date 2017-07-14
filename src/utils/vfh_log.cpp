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

#ifdef _WIN32
#define VS_DEBUG(...) VUtils::debug(__VA_ARGS__)
#else
#define VS_DEBUG(...)
#endif

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
		VS_DEBUG("V-Ray For Houdini [");

		switch (level) {
			case LogLevelInfo:    { vutils_cprintf(true, VUTILS_COLOR_BLUE   "    Info" VUTILS_COLOR_DEFAULT "| ");                     VS_DEBUG("Info"); break; }
			case LogLevelProgress:{ vutils_cprintf(true, VUTILS_COLOR_BLUE   "Progress" VUTILS_COLOR_DEFAULT "| ");                     VS_DEBUG("Progress"); break; }
			case LogLevelWarning: { vutils_cprintf(true, VUTILS_COLOR_YELLOW " Warning" VUTILS_COLOR_DEFAULT "| " VUTILS_COLOR_YELLOW); VS_DEBUG("Warning"); break; }
			case LogLevelError:   { vutils_cprintf(true, VUTILS_COLOR_RED    "   Error" VUTILS_COLOR_DEFAULT "| " VUTILS_COLOR_RED);    VS_DEBUG("Error"); break; }
			case LogLevelDebug:   { vutils_cprintf(true, VUTILS_COLOR_CYAN   "   Debug" VUTILS_COLOR_DEFAULT "| " VUTILS_COLOR_CYAN);   VS_DEBUG("Debug"); break; }
			case LogLevelMsg:     { vutils_cprintf(true, VUTILS_COLOR_GREEN  "     Msg" VUTILS_COLOR_DEFAULT "| " VUTILS_COLOR_GREEN);  VS_DEBUG("Msg"); break; }
		}

		vutils_cprintf(true, "%s\n" VUTILS_COLOR_DEFAULT, buf);
		VS_DEBUG("] %s\n", buf);

		fflush(stdout);
		fflush(stderr);
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
