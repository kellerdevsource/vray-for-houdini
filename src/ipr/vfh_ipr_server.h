//
// Copyright (c) 2015-2017, Chaos Software Ltd
//
// V-Ray For Houdini IPR
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#pragma once

#include <QDialog>
class QTcpSocket;
class QTcpServer;

/// Simple echo server to keep the renderer running
class Server: public QDialog {
	Q_OBJECT

public:
	explicit Server(QWidget *parent = Q_NULLPTR);
	~Server();
private:
	QTcpSocket * client;
	QTcpServer * server;

protected Q_SLOTS:
	void onConnected();
	void onData();
};

