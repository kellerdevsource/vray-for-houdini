//
// Copyright (c) 2015-2017, Chaos Software Ltd
//
// V-Ray For Houdini Python IPR Module
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#pragma once

#include <QDialog>
#include <functional>

class QTcpSocket;
class QTimer;

class PingPongClient: public QDialog {
	Q_OBJECT

	Q_DISABLE_COPY(PingPongClient)
public:
	PingPongClient(QDialog *parent = Q_NULLPTR);

	~PingPongClient();

	/// Set function callback to be called when we can't reach the "server"
	void setCallback(std::function<void()> value);

	/// Start checking for "server" running
	void start();

	/// Stop checking for server
	void stop();

private Q_SLOTS:
	/// Ping server, read data from socket, and decide if it needs to call
	/// the set callback
	/// NOTE: Since this is connected to the QTimer::timeout signal it will be
	/// called on the main thread
	void tick();
private:
	QTcpSocket * socket; ///< Socket that connects to the server
	QTimer * timer; ///< Timer used to schedule pings to server
	int diff; ///< The difference between sent pings and received pongs
	int fail; ///< Number of failed writes to the socket
	std::function<void()> cb; ///< Callback function when we abs(diff) > 10 or fail > 10
};