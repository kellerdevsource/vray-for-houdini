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

#include <stdexcept>
#include <execinfo.h>


using namespace VRayForHoudini;


void Error::GlobalErrorHandler::printStacktrace()
{
	void *array[20];
	size_t size = backtrace(array, sizeof(array) / sizeof(array[0]));
	backtrace_symbols_fd(array, size, STDERR_FILENO);
}


void Error::GlobalErrorHandler::terminateHandler()
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
		Log::getLog().error("Terminated due to unknown reason.");
	}

	printStacktrace();

	GlobalErrorHandler &errorHandler = getGlobalErrorHandler();
	errorHandler.lastTerminateHnldr();
}


Error::GlobalErrorHandler::GlobalErrorHandler():
	enabled(false),
	lastTerminateHnldr(nullptr)
{ }


Error::GlobalErrorHandler::~GlobalErrorHandler()
{
	enable(false);
}


int Error::GlobalErrorHandler::enable(bool val)
{
	if (enabled == val) {
		return false;
	}

	if (val) {
		vassert( isEnabled() == false );
		vassert( errorHandler.lastTerminateHnldr == nullptr );

		lastTerminateHnldr = std::set_terminate(terminateHandler);
		enabled = true;
	}
	else {
		vassert( isEnabled() == true );
		vassert( errorHandler.lastTerminateHnldr != nullptr );

		std::set_terminate(lastTerminateHnldr);
		lastTerminateHnldr = nullptr;
		enabled = false;
	}

	return true;
}


VRayForHoudini::Error::GlobalErrorHandler &VRayForHoudini::Error::getGlobalErrorHandler()
{
	static GlobalErrorHandler errorHandler;
	return errorHandler;
}
