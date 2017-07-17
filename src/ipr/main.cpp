//
// Copyright (c) 2015-2017, Chaos Software Ltd
//
// V-Ray For Houdini IPR
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#include "vfh_ipr_checker.h"
#include "vfh_ipr_server.h"

#include <QApplication>
#include <QtCore>

int main(int argc, char ** argv)
{
	QCoreApplication::addLibraryPath("./Qt_plugins");
	QApplication app(argc, argv);
	Server server;
	
	return app.exec();
}
