//
// Copyright (c) 2015, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#include "vfh_error.h"
#include "vfh_log.h"

#include <UT/UT_StackTrace.h>


using namespace VRayForHoudini;


void Error::ErrorChaser::crashHandler()
{
	std::exception_ptr exptr = std::current_exception();
	if (exptr != 0) {
		// the only useful feature of std::exception_ptr is that it can be rethrown...
		try {
			std::rethrow_exception(exptr);
		}
		catch (VRay::VRayException &ex) {
			Log::getLog().error("Terminating due to uncaught VRayException :\n%s", ex.what());
		}
		catch (std::exception &ex) {
			Log::getLog().error("Terminating due to uncaught exception:\n%s", ex.what());
		}
		catch (...) {
			Log::getLog().error("Terminating due to unknown exception.");
		}
	}
	else {
		Log::getLog().error("Terminating due to unknown reason.");
	}

	UT_StackTrace stackTrace;
	stackTrace.setVerbose(true);
	stackTrace.doTraceback();

	ErrorChaser &errChaser = getErrorChaser();
	errChaser.lastTerminateHnldr();
}


Error::ErrorChaser::ErrorChaser():
	enabled(false),
	lastTerminateHnldr(nullptr)
{ }


Error::ErrorChaser::~ErrorChaser()
{
	enable(false);
}


int Error::ErrorChaser::enable(bool val)
{
	if (enabled == val) {
		return false;
	}

	if (val) {
		vassert( isEnabled() == false );
		vassert( lastTerminateHnldr == nullptr );

		lastTerminateHnldr = std::set_terminate(crashHandler);
		enabled = true;
	}
	else {
		vassert( isEnabled() == true );
		vassert( lastTerminateHnldr != nullptr );

		std::set_terminate(lastTerminateHnldr);
		lastTerminateHnldr = nullptr;
		enabled = false;
	}

	return true;
}


VRayForHoudini::Error::ErrorChaser &VRayForHoudini::Error::getErrorChaser()
{
	static ErrorChaser errChaser;
	return errChaser;
}
