//
// Copyright (c) 2015-2017, Chaos Software Ltd
//
// V-Ray For Houdini Python IPR Module
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#include "vfh_log.h"
#include "vfh_ipr_server.h"
#include "vfh_ipr_client.h"
#include "vfh_ipr_checker_types.h"

#include <QWidget>
#include <QTimer>
#include <QtNetwork/QTcpSocket>
#include <QtNetwork/QHostAddress>

const int PING_INTERVAL = 50; // time in MS to send ping packets
const int FAIL_THRESHOLD = 10; // times we can fail or have dropped


PingPongClient::PingPongClient(QDialog *parent)
	: QDialog(parent)
	, socket(new QTcpSocket(this))
	, timer(new QTimer(this))
	, diff(0)
	, fail(0)
	, resetDiff(0)
	, cbCalled(false)
{
	timer->setInterval(PING_INTERVAL);
	connect(timer, &QTimer::timeout, this, &PingPongClient::sendPing);
}

PingPongClient::~PingPongClient() {
	delete timer;
	delete socket;
}

void PingPongClient::setCallback(std::function<void()> value) {
	cb = value;
}

void PingPongClient::start() {
	fail = diff = resetDiff = 0;
	cbCalled = false;
	socket->connectToHost(QHostAddress::LocalHost, 5050);
	connect(socket, &QTcpSocket::disconnected, this, &PingPongClient::callCallback);
	connect(socket, &QTcpSocket::readyRead, this, &PingPongClient::onData);
	timer->start();
}

void PingPongClient::stop() {
	timer->stop();
	disconnect(socket, 0, 0, 0);
}

void PingPongClient::callCallback() {
	if (!cbCalled) {
		cbCalled = true;
		cb();
	}
}

void PingPongClient::onData() {
	auto data = socket->read(PingPongPacket::size());
	if (data.size()) {
		PingPongPacket pongPack(data.data());
		if (pongPack && pongPack.info == PingPongPacket::PacketInfo::PONG) {
			diff--;
		}
	}
}

void PingPongClient::sendPing() {
	// if we have some difference - dropped packets, then count wait 10 iterations and reset them if we didnt disconnect
	// this avoids the case where some packets are dropped due to heavy load of the system or packets sent before initialization
	if (diff > 0) {
		resetDiff += diff < FAIL_THRESHOLD;
	} else {
		resetDiff = 0;
	}

	if (resetDiff >= FAIL_THRESHOLD) {
		diff = 0;
		resetDiff = 0;
	}

	PingPongPacket pingPack(PingPongPacket::PacketInfo::PING);
	if (socket->write(pingPack.data(), PingPongPacket::size()) != PingPongPacket::size()) {
		++fail;
	} else {
		fail = 0;
		diff++;
	}

	if (fail >= FAIL_THRESHOLD) {
		// 10 failed writes
		fail = 0;
		callCallback();
		return;
	}

	if (diff > FAIL_THRESHOLD) {
		// 10 pings without pong
		diff = 0;
		callCallback();
		return;
	} else if (diff < -FAIL_THRESHOLD) {
		// 10 pongs, without sending ping
		// this should not happen since server sends pong only when it receives ping
		// but add this here to detect inconsistensies
		VRayForHoudini::Log::getLog().warning("Stopping IPR since vfh_ipr.exe is misbehaving");
		diff = 0;
		callCallback();
		return;
	}
}
