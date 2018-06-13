// MurmurHash3 was written by Austin Appleby, and is placed in the
// public domain. The author hereby disclaims copyright to this source
// code.
//
// Taken from https://github.com/PeterScott/murmur3
//

#ifndef VRAY_FOR_HOUDINI_HASHES_H
#define VRAY_FOR_HOUDINI_HASHES_H

#include "vfh_vray.h" // For proper "systemstuff.h" inclusion

namespace VRayForHoudini {
namespace Hash {

typedef uint32_t MHash;

void MurmurHash3_x86_32 (const void *key, int len, uint32_t seed, void *out);
void MurmurHash3_x86_128(const void *key, int len, uint32_t seed, void *out);
void MurmurHash3_x64_128(const void *key, int len, uint32_t seed, void *out);

FORCEINLINE uint32 hashLittle(const char *key) {
	return VUtils::hashlittle(key, strlen(key));
}

FORCEINLINE uint32 hashLittle(const VUtils::CharString &key) {
	return VUtils::hashlittle(key.ptr(), key.length());
}

template <typename T>
uint32 hashLittle(const T &key) {
	return VUtils::hashlittle(&key, sizeof(T));
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
