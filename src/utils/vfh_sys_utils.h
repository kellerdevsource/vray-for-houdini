//
// Copyright (c) 2015-2018, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text:
//  https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#ifndef VRAY_FOR_HOUDINI_VFH_SYS_UTILS_H
#define VRAY_FOR_HOUDINI_VFH_SYS_UTILS_H

#include "vfh_vray.h"

#define VFH_BIT(b) (1<<(b))

namespace VRayForHoudini {

FORCEINLINE int isBitSet(int storage, int mask) {
	return (storage & mask) != 0;
}

namespace Sys {

/// Returns environment variable value as string.
struct GetEnvVar {
	explicit GetEnvVar(const char *varName)
		: varName(varName)
		, varValue(nullptr)
		, initialized(false)
	{}

	const char *getValue();

	/// Returns variable name.
	const char *getName() const;

	/// Resets inializastion state.
	/// Forces to retrieve new value.
	void resetValue();

private:
	/// Variable name.
	const char *varName;

	/// Variable value.
	const char *varValue;

	/// Initialization state.
	int initialized;
};

} // namespace Sys
} // namespace VRayForHoudini

#endif // VRAY_FOR_HOUDINI_VFH_SYS_UTILS_H
