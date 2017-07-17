//
// Copyright (c) 2015-2017, Chaos Software Ltd
//
// V-Ray For Houdini IPR
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#ifndef VRAY_FOR_HOUDINI_IPR_SERVER_H
#define VRAY_FOR_HOUDINI_IPR_SERVER_H

#include <QDialog>
class QTcpSocket;
class QTcpServer;

/// Simple echo server to keep the renderer running
class Server:
	public QDialog
{
	Q_OBJECT

public:
	explicit Server(QWidget *parent = Q_NULLPTR);
	~Server();
private:
	QTcpSocket * client; ///< Connected socket for client
	QTcpServer * server; ///< Server created on startup

protected Q_SLOTS:
	/// Slot called when client connects, used to connect QTcpSocket::readyRead to onData slot
	void onConnected();

	/// Slot called when there is data to read from the client
	void onData();
};

#endif // VRAY_FOR_HOUDINI_IPR_SERVER_H
