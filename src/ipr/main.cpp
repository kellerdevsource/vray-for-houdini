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

#include <QApplication>
#include <QtCore>
#include <QProcessEnvironment>
#include <QDir>
#include <QDebug>

int main(int argc, char ** argv)
{
	// We need to load "platform" plugins for windows, so we use one from houdini installation
	QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
	QString hfsPath = env.value("HFS", "");
	if (hfsPath.isEmpty()) {
		puts("Failed to find HFS to start IPR");
		return 1;
	}
	QCoreApplication::addLibraryPath(hfsPath + "/bin/Qt_plugins/");

	QApplication app(argc, argv);
	Server server;

	return app.exec();
}
