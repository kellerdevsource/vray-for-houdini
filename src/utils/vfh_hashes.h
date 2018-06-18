// MurmurHash3 was written by Austin Appleby, and is placed in the
// public domain. The author hereby disclaims copyright to this source
// code.
//
// Taken from https://github.com/PeterScott/murmur3
//

#ifndef VRAY_FOR_HOUDINI_HASHES_H
#define VRAY_FOR_HOUDINI_HASHES_H

#include "vfh_vray.h" // For proper "systemstuff.h" inclusion

#include <UT/UT_String.h>

namespace VRayForHoudini {
namespace Hash {

typedef uint32_t MHash;

void MurmurHash3_x86_32 (const void *key, int len, uint32_t seed, void *out);
void MurmurHash3_x86_128(const void *key, int len, uint32_t seed, void *out);
void MurmurHash3_x64_128(const void *key, int len, uint32_t seed, void *out);

FORCEINLINE uint32 hashLittle(const char *key)
{
	if (key && *key)
		return VUtils::hashlittle(key, strlen(key));
	return 0;
}

template <typename T>
uint32 hashLittle(const T &key)
{
	return VUtils::hashlittle(&key, sizeof(T));
}

FORCEINLINE uint32 hashLittle(const VUtils::CharString &key)
{
	if (key.empty())
		return 0;
	return VUtils::hashlittle(key.ptr(), key.length());
}

FORCEINLINE uint32 hashLittle(const UT_String &key)
{
	if (key.isstring())
		return VUtils::hashlittle(key.buffer(), key.length());
	return 0;
}

template <typename T>
MHash hashMur(const T &key) {
	MHash keyHash = 0;
	Hash::MurmurHash3_x86_32(&key, sizeof(T), 42, &keyHash);
	return keyHash;
}

} // namespace Hash
} // namespace VRayForHoudini

#endif // VRAY_FOR_HOUDINI_HASHES_H
