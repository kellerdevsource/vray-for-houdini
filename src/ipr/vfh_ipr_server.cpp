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

#include "vfh_ipr_server.h"
#include "vfh_ipr_checker_types.h"

#include <QtWidgets>
#include <QDialog>
#include <QByteArray>
#include <QtNetwork/QTcpServer>
#include <QtNetwork/QHostAddress>
#include <QtNetwork/QTcpSocket>
#include <QtNetwork/QHostAddress>
#include <QThread>


Server::Server(QWidget *parent)
	: QDialog(parent)
	, client(Q_NULLPTR)
	, server(new QTcpServer())
{
	if (!server->listen(QHostAddress(QHostAddress::Any), 5050)) {
		QMessageBox::critical(this, tr("IPR Server"), tr("Unable to start the server: %1.").arg(server->errorString()));
	}
	connect(server, &QTcpServer::newConnection, this, &Server::onConnected);
}

Server::~Server() {
	delete client;
	delete server;
}

void Server::onConnected() {
	client = server->nextPendingConnection();
	connect(client, &QTcpSocket::readyRead, this, &Server::onData);
	connect(client, &QAbstractSocket::disconnected, client, &QObject::deleteLater);
}

void Server::onData() {
	const int packetSize = sizeof(PingPongPacket);
	auto data = client->read(packetSize);
	if (data.size() != packetSize) {
		Q_ASSERT(!"Invalid packed received");
	} else {
		auto recv = PingPongPacket(data.data());
		if (recv && recv.info == PingPongPacket::PacketInfo::PING) {
			PingPongPacket pongPack(PingPongPacket::PacketInfo::PONG);
			const int sentSize = client->write(pongPack.data(), pongPack.size());
			Q_ASSERT(sentSize == packetSize && "Failed to write pong to clinet");
			
		}
	}
}
