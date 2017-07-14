#pragma once

enum PingPongVersionEnum : short {
	Invalid = -1,
	// bump this when PingPongPacket changes layout
	PingPongVersion = 0x0001
};

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