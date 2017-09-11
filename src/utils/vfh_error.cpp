//
// Copyright (c) 2015-2017, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#ifndef VASSERT_ENABLED

#include "vfh_error.h"
#include "vfh_log.h"

#include <UT/UT_StackTrace.h>
#include <UT/UT_String.h>

#include <cstdio>

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

	UT_String filename("$HIP/crash.vfhlog.$USER.$HIPNAME.txt");
	filename.expandVariables();
	FILE *file = vutils_fopen(filename.c_str(), "w");
	if (file) {
		Log::getLog().msg("Saving crash dump to %s", filename.c_str());
	}
	else {
		file = stderr;
	}

	UT_StackTrace stackTrace(file);
	stackTrace.setVerbose(true);
	stackTrace.doTraceback();

	ErrorChaser &errChaser = ErrorChaser::getInstance();
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
		UT_ASSERT( isEnabled() == false );
		UT_ASSERT( lastTerminateHnldr == nullptr );

		lastTerminateHnldr = std::set_terminate(crashHandler);
		enabled = true;
	}
	else {
		UT_ASSERT( isEnabled() == true );
		UT_ASSERT( lastTerminateHnldr != nullptr );

		std::set_terminate(lastTerminateHnldr);
		lastTerminateHnldr = nullptr;
		enabled = false;
	}

	return true;
}

#endif
