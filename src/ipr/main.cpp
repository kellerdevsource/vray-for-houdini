//
// Copyright (c) 2015-2017, Chaos Software Ltd
//
// V-Ray For Houdini IPR
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#include "vfh_ipr_server.h"
#include "vfh_log.h"

#include <QApplication>
#include <QtCore>
#include <QProcessEnvironment>
#include <QDir>

#include <unistd.h>
#include <cstdio>

void initLinux()
{
#ifndef WIN32
	pid_t parentPID = getppid();
	pid_t myPID = getpid();
	char fname[1024] = {0,};
	sprintf(fname, "/tmp/%d", static_cast<int>(parentPID));

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

	// We need to load "platform" plugins for windows, so we use one from houdini installation
	QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
	QString hfsPath = env.value("HFS", "");
	if (hfsPath.isEmpty()) {
		VRayForHoudini::Log::getLog().error("Environment variable \"HFS\" missiing! IPR not started!");
		return 1;
	}

#ifdef WIN32
	QCoreApplication::addLibraryPath(hfsPath + "/bin/Qt_plugins/");
#else
	QCoreApplication::addLibraryPath(hfsPath + "/dsolib/Qt_plugins/");
#endif
	QApplication app(argc, argv);
	Server server;

	return app.exec();
}
