//
// Copyright (c) 2015, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#ifndef VRAY_FOR_HOUDINI_VFH_ERROR_H
#define VRAY_FOR_HOUDINI_VFH_ERROR_H

#include <exception>

namespace VRayForHoudini {
namespace Error {


class GlobalErrorHandler
{
public:
	GlobalErrorHandler();
	~GlobalErrorHandler();

	int isEnabled() const { return enabled; }
	int enable(bool val);

private:
	static void terminateHandler();
	static void printStacktrace();

private:
	bool enabled;
	std::terminate_handler lastTerminateHnldr;

};

GlobalErrorHandler &getGlobalErrorHandler();

} // namespace Error
} // namespace VRayForHoudini

#endif // VRAY_FOR_HOUDINI_VFH_ERROR_H
