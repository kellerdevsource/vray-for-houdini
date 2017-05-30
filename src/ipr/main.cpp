//
// Copyright (c) 2015-2017, Chaos Software Ltd
//
// V-Ray For Houdini IPR
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#include <Windows.h>

#include <stdio.h>
#include <stdlib.h>

#if 0
#define PRINT_DEBUG(...)
#else
#define PRINT_DEBUG(...) { \
	fprintf(stdout, "V-Ray For Houdini IPR| "); \
	fprintf(stdout, __VA_ARGS__); \
	fprintf(stdout, "\n"); \
	fflush(stdout); \
}
#endif

void done() {
	PRINT_DEBUG("Done");

	// Exit with a clear error code.
	_exit(0);
}

#if defined(_WIN32)
BOOL WINAPI ctrl_c_handler(DWORD dwCtrlType)
{
	if (dwCtrlType == CTRL_BREAK_EVENT ||
		dwCtrlType == CTRL_C_EVENT ||
		dwCtrlType == CTRL_CLOSE_EVENT ||
		dwCtrlType == CTRL_LOGOFF_EVENT ||
		dwCtrlType == CTRL_SHUTDOWN_EVENT)
	{
		PRINT_DEBUG("Break even trapped");
		done();
	}

	return FALSE;
}
#elif defined(XUNIX)
static void ctrl_c_handler(int /*sig*/) {
	done();
}
#endif

int main(int argc, char const *argv[])
{
	PRINT_DEBUG("Start");
	PRINT_DEBUG("argc = %i", argc);

	for (int i = 1; i < argc; i++) {
		PRINT_DEBUG("argv[%i] = %s", i, argv[i]);
	}

#if defined(_WIN32)
	SetConsoleCtrlHandler(ctrl_c_handler, true);
	SetErrorMode(SEM_FAILCRITICALERRORS);
#elif defined(XUNIX)
	struct sigaction sigIntHandler;
	sigIntHandler.sa_handler = ctrl_c_handler;
	sigemptyset(&sigIntHandler.sa_mask);
	sigIntHandler.sa_flags = 0;

	sigaction(SIGINT, &sigIntHandler, NULL);
	sigaction(SIGTERM, &sigIntHandler, NULL);
	sigaction(SIGQUIT, &sigIntHandler, NULL);
#endif

	int i = 0;
	while (true) {
		PRINT_DEBUG("%i", i++);
		Sleep(1000);
	}

	return 0;
}
