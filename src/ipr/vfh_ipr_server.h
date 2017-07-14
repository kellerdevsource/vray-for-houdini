#pragma once

#include <QDialog>
class QTcpSocket;
class QTcpServer;

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

