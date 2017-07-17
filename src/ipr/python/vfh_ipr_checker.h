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

	void setCallback(std::function<void()> value);

	void start();

	void stop();

private Q_SLOTS :
	void tick();
private:
	QTcpSocket * socket;
	QTimer * timer;
	int diff;
	int fail;
	std::function<void()> cb;
};