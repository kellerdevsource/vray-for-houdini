//
// Copyright (c) 2015-2017, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#ifndef VRAY_FOR_HOUDINI_VFH_ERROR_H
#define VRAY_FOR_HOUDINI_VFH_ERROR_H

#ifndef VASSERT_ENABLED

#include <exception>

namespace VRayForHoudini {
namespace Error {

/// Singleton class to register/unregister handler for std::terminate
class ErrorChaser
{
public:
	~ErrorChaser();

	/// Get the ErrorChaser instance
	static ErrorChaser & getInstance() {
		static ErrorChaser errorChaser;
		return errorChaser;
	}

	/// Check if the crash handler is registered
	int isEnabled() const { return enabled; }

	/// Register or unregister the crash handler
	int enable(bool val);

private:
	/// Construct a non-enabled ErrorChaser
	ErrorChaser();

	/// To be executed on std::terminate if registered
	static void crashHandler();
private:
	bool enabled;
	std::terminate_handler lastTerminateHnldr;

};

} // namespace Error
} // namespace VRayForHoudini

#endif // VASSERT_ENABLED
#endif // VRAY_FOR_HOUDINI_VFH_ERROR_H
