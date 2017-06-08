// MurmurHash3 was written by Austin Appleby, and is placed in the
// public domain. The author hereby disclaims copyright to this source
// code.
//
// Taken from https://github.com/PeterScott/murmur3
//

#ifndef VRAY_FOR_HOUDINI_HASHES_H
#define VRAY_FOR_HOUDINI_HASHES_H

#include "vfh_vray.h" // For proper "systemstuff.h" inclusion

#include <unordered_set>
#include <unordered_map>

// extending namespace std with the proper specialization is the "correct" way according to the standard
namespace std {
	template <> struct hash<VRay::Plugin> {
		size_t operator()(VRay::Plugin plugin) const {
			return std::hash<const char *>()(plugin.getName());
		}
	};
};

namespace VRayForHoudini {
namespace Hash {

/// Hash set of plugin instances
typedef std::unordered_set<VRay::Plugin> PluginHashSet;

typedef uint32_t MHash;

void MurmurHash3_x86_32 (const void *key, int len, uint32_t seed, void *out);
void MurmurHash3_x86_128(const void *key, int len, uint32_t seed, void *out);
void MurmurHash3_x64_128(const void *key, const int len, const uint32_t seed, void *out);

} // namespace Hash
} // namespace VRayForHoudini

#endif // VRAY_FOR_HOUDINI_HASHES_H
