//
// Copyright (c) 2015-2017, Chaos Software Ltd
//
// V-Ray For Houdini Python IPR Module
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#ifndef VRAY_FOR_HOUDINI_PROCESS_CHECK_H
#define VRAY_FOR_HOUDINI_PROCESS_CHECK_H

#include <string>
#include <functional>
#include <memory>


/// Class used to register a function that should be called when a specified *child* process exits
/// This should be implemented for each supported platform
/// Use global makeProcessChecker function to obtain instance
class ProcessCheck
{
public:
	typedef std::function<void()> OnStop;

	ProcessCheck(OnStop cb, const std::string &name)
		: stopCallback(cb)
		, processName(name) {}

	/// Start waiting for the process
	/// @return false if waiting failed for some reason
	virtual bool start() = 0;

	/// Do a sync check if the process is running
	/// @return true if the process is alive
	virtual bool isAlive() = 0;

	/// Stop waiting for the child
	/// @return true if we actually stopped waiting
	virtual bool stop() = 0;


	virtual ~ProcessCheck() {}
protected:
	/// Function called when the specified process exits
	OnStop stopCallback;
	/// The name of the process we are waiting for
	std::string processName;
};

typedef std::shared_ptr<ProcessCheck> ProcessCheckPtr;

/// Crate instance of ProcessCheck for current platform
/// @param cb - the function to be called
/// @param name - the name of the process to be waited
ProcessCheckPtr makeProcessChecker(ProcessCheck::OnStop cb, const std::string &name);

/// Set handler to ignore the SIGPIPE signal
/// NOTE: this is here to make use of the different implementation for win and lnx0
void disableSIGPIPE();

/// Return string containing a path to a directory to put PID file for linux
std::string getTempDir();

#endif // VRAY_FOR_HOUDINI_PROCESS_CHECK_H
