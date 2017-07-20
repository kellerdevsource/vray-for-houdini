//
// Copyright (c) 2015-2017, Chaos Software Ltd
//
// V-Ray For Houdini Python IPR Module
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#include "vfh_ipr_server.h"
#include "vfh_ipr_client.h"
#include "vfh_ipr_checker_types.h"

#include <QWidget>
#include <QTimer>
#include <QtNetwork/QTcpSocket>
#include <QtNetwork/QHostAddress>

PingPongClient::PingPongClient(QDialog *parent)
	: QDialog(parent)
	, socket(new QTcpSocket(this))
	, timer(new QTimer(this))
	, diff(0)
	, fail(0)
	, cbCalled(false)
{
	timer->setInterval(10);
	connect(timer, &QTimer::timeout, this, &PingPongClient::tick);
}

PingPongClient::~PingPongClient() {
	delete timer;
	delete socket;
}

void PingPongClient::setCallback(std::function<void()> value) {
	cb = value;
}

void PingPongClient::start() {
	cbCalled = false;
	socket->connectToHost(QHostAddress::LocalHost, 5050);
	connect(socket, &QTcpSocket::disconnected, this, &PingPongClient::callCallback);
	timer->start();
}

void PingPongClient::stop() {
	timer->stop();
}

void PingPongClient::callCallback() {
	if (!cbCalled) {
		cbCalled = true;
		cb();
	}
}

void PingPongClient::tick() {
	PingPongPacket pingPack(PingPongPacket::PacketInfo::PING);
	
	auto data = socket->read(pingPack.size());
	if (data.size()) {
		PingPongPacket pongPack(data.data());
		if (pongPack && pongPack.info == PingPongPacket::PacketInfo::PONG) {
			diff--;
		}
	}

	if (socket->write(pingPack.data(), pingPack.size()) != pingPack.size()) {
		++fail;
	} else {
		fail = 0;
		diff++;
	}

	if (fail >= 10) {
		// 10 failed writes
		fail = 0;
		callCallback();
		return;
	}

	if (diff > 10) {
		// 10 pings without pong
		diff = 0;
		callCallback();
		return;
	} else if (diff < -10) {
		// 10 pongs, without sending ping
		diff = 0;
		callCallback();
		return;
	}
}
