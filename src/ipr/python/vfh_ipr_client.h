//
// Copyright (c) 2015-2017, Chaos Software Ltd
//
// V-Ray For Houdini Python IPR Module
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#ifndef VRAY_FOR_HOUDINI_IPR_CHECKER_H
#define VRAY_FOR_HOUDINI_IPR_CHECKER_H

#include <QDialog>

#include <atomic>
#include <functional>

class QTcpSocket;
class QTimer;

class PingPongClient:
	public QDialog
{
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
	void sendPing();

	/// Called when there is data to be read from socket
	void onData();

private:
	/// Call the callback passed as argument, ensures it is called only once, *not* thread safe
	void callCallback();

private:
	/// Socket that connects to the server
	QTcpSocket *socket;
	/// Timer used to schedule pings to server
	QTimer *timer;
	/// The difference between sent pings and received pongs
	std::atomic<int> diff;
	/// Counter to reset the diff in ping/pong packets
	int resetDiff;
	/// Number of failed writes to the socket
	int fail;
	/// Callback function when we abs(diff) > 10 or fail > 10
	std::function<void()> cb;
	/// True if we called the cb already, false otherwise
	bool cbCalled;
};

#endif // VRAY_FOR_HOUDINI_IPR_CHECKER_H
