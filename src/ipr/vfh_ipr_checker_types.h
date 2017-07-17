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

/// Constants for ping pong checks
enum PingPongVersionEnum : short {
	Invalid = -1,
	// bump this when PingPongPacket changes layout
	PingPongVersion = 0x0001
};

/// Struct that is sent/recieved by the "ipr" application and the renderer module
/// Used to signal when the application is killed so rendering can stop
/// NOTE: keep small
struct PingPongPacket {
	enum class PacketInfo : short {
		INVALID, PING, PONG
	};

	explicit PingPongPacket(const void * data)
		: version(Invalid)
		, info(PacketInfo::INVALID)
	{
		const PingPongPacket * pack = reinterpret_cast<const PingPongPacket*>(data);
		if (pack->version == PingPongVersion) {
			*this = *pack;
		}
	}

	PingPongPacket(PacketInfo info)
		: version(PingPongVersion)
		, info(info)
	{}

	const char * data() const {
		return reinterpret_cast<const char *>(this);
	}

	int size() const {
		return sizeof(*this);
	}

	operator bool() const {
		return version != Invalid && info != PacketInfo::INVALID;
	}

	short version;
	PacketInfo info;
};