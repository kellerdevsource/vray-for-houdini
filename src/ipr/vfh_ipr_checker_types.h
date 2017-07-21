//
// Copyright (c) 2015-2017, Chaos Software Ltd
//
// V-Ray For Houdini IPR
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#ifndef VRAY_FOR_HOUDINI_IPR_CHECKER_TYPES_H
#define VRAY_FOR_HOUDINI_IPR_CHECKER_TYPES_H

/// Constants for ping pong checks
enum PingPongVersionEnum : short {
	Invalid = -1,
	PingPongVersion = 0x0001 ///< bump this when PingPongPacket changes layout
};

/// Struct that is sent/recieved by the "ipr" application and the renderer module
/// Used to signal when the application is killed so rendering can stop
/// NOTE: keep small
struct PingPongPacket
{
	enum class PacketInfo : short {
		INVALID,
		PING,
		PONG
	};

	/// Try to create packed from memory, if the reinterpreted version mismatches
	/// then initialize with INVALID PacketInfo
	explicit PingPongPacket(const void* data)
		: version(Invalid)
		, info(PacketInfo::INVALID)
	{
		const PingPongPacket * pack = reinterpret_cast<const PingPongPacket*>(data);
		if (pack->version == PingPongVersion) {
			*this = *pack;
		}
	}

	/// Create a packet from info
	PingPongPacket(PacketInfo info)
		: version(PingPongVersion)
		, info(info)
	{}

	/// Get pointer to the packet's data
	const char* data() const {
		return reinterpret_cast<const char*>(this);
	}

	/// Get the packets size
	static int size() {
		return sizeof(PingPongPacket);
	}

	/// Test if the packet is valid
	operator bool() const {
		return version != Invalid && info != PacketInfo::INVALID;
	}

	/// Version of the creator of the packet
	/// used to check if we can understand the rest of the packet
	short version;
	/// Type of packet
	PacketInfo info;
};

#endif // VRAY_FOR_HOUDINI_IPR_CHECKER_TYPES_H
