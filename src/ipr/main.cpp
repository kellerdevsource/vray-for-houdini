//
// Copyright (c) 2015-2017, Chaos Software Ltd
//
// V-Ray For Houdini IPR
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#include "vfh_log.h"
#include "vfh_process_check.h"

#include <unistd.h>
#include <cstdio>
#include <string>

void initLinux()
{
#ifndef WIN32
	const std::string tmpDir = getTempDir();
	const pid_t parentPID = getppid();
	const pid_t myPID = getpid();
	char fname[1024] = {0,};
	sprintf(fname, "%s/%d", tmpDir.c_str(), static_cast<int>(parentPID));

	FILE * pidFile = fopen(fname, "wb");
	if (!pidFile) {
		exit(-1);
	}

	if (fwrite(&myPID, sizeof(myPID), 1, pidFile) != 1) {
		exit(-1);
	}

	fclose(pidFile);
#endif
}

int main(int argc, char ** argv)
{
	initLinux();

	VRayForHoudini::Log::Logger::startLogging();
	std::shared_ptr<void> _stopLoggingAtExit = std::shared_ptr<void>(nullptr, [](void*) {
		VRayForHoudini::Log::Logger::stopLogging();
	});

	while (1) {
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
}
