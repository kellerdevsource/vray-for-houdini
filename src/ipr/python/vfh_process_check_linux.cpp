//
// Copyright (c) 2015-2017, Chaos Software Ltd
//
// V-Ray For Houdini Python IPR Module
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#include "vfh_process_check.h"
#include "vfh_log.h"

#include <unistd.h>
#include <cstdio>
#include <cstdlib>

#include <sys/wait.h>
#include <sys/stat.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>

#include <thread>
#include <memory>

using namespace VRayForHoudini;

namespace {
bool isDir(const char * path) {
	if (!path) {
		return false;
	}

	struct stat res;
	return stat(path, &res) == 0 && S_ISDIR(res.st_mode);
}
}

std::string getTempDir() {
	const std::string defaultTmp = "/tmp";
	if (isDir(defaultTmp.c_str())) {
		return defaultTmp;
	}

	const char *envTmp[] = {"TMP", "TEMP", "TMPDIR", "TEMPDIR"};
	const int envCount = sizeof(envTmp) / sizeof(envTmp[0]);
	for (int c = 0; c < envCount; c++) {
		const char *envVal = getenv(envTmp[c]);
		if (isDir(envVal)) {
			return envVal;
		}
	}

	Log::getLog().error("Failed to find suitable temp dir to write to!");
	vassert(false && "Failed to find suitable temp dir to write to!");
	return "."; // try pwd if all else fails
}

void disableSIGPIPE() {
	struct sigaction sa;
	sa.sa_handler = SIG_IGN;
	sa.sa_flags = 0;
	if (sigaction(SIGPIPE, &sa, 0) == -1) {
		Log::getLog().error("Failed to disable SIGPIPE error: [%d]", errno);
	}
}


class LnxProcessCheck:
	public ProcessCheck
{
public:
	LnxProcessCheck(OnStop cb, const std::string &name)
		: ProcessCheck(cb, name)
		, waitThread(nullptr)
		, childPid(0)
		, checkRunning(false)
		, tempDir(getTempDir())
	{}

	enum class ChildState {
		Running, Stopped, Error
	};

	ChildState getChildState();

	bool start() override;

	bool stop() override;
	
	bool isAlive() override;
private:
	/// Thread waiting in waitpid
	std::thread * waitThread;
	/// The pid of the child we are waiting for
	pid_t childPid;
	/// Flag keeping the thread running
	/// Volatile to discourage compiler optimizing reads in checker thread
	volatile bool checkRunning;
	/// Cache the temp dir returned by getTempDir()
	std::string tempDir;
};

ProcessCheckPtr makeProcessChecker(ProcessCheck::OnStop cb, const std::string &name) {
	return ProcessCheckPtr(new LnxProcessCheck(cb, name));
}

LnxProcessCheck::ChildState LnxProcessCheck::getChildState() {
	if (childPid > 0) {
		int status = 0;
		pid_t resPid = waitpid(childPid, &status, WNOHANG);
		if (resPid == 0) {
			return ChildState::Running;
		} else if (resPid > 0) {
			return ChildState::Stopped;
		} else {
			if (errno == ECHILD) {
				return ChildState::Stopped;
			}
		}
	}
	return ChildState::Error;
}

bool LnxProcessCheck::start() {
	pid_t myPid = getpid();
	Log::getLog().debug("Starting LnxProcessCheck from pid %d", (int)myPid);
	char pidFileName[1024] = {0,};
	sprintf(pidFileName, "%s/%d", tempDir.c_str(), static_cast<int>(myPid));

	auto pidFile = std::shared_ptr<FILE>(fopen(pidFileName, "rb"), [](FILE * file) {
		if (file) {
			fclose(file);
		}
	});

	if (!pidFile) {
		Log::getLog().warning("Trying to start LnxProcessCheck but pid file is missing!");
		return false;
	}

	if (fread(reinterpret_cast<char*>(&childPid), sizeof(childPid), 1, pidFile.get()) != 1) {
		Log::getLog().warning("Trying to start LnxProcessCheck but can't read from pid file.");
		return false;
	}

	Log::getLog().debug("Child vfh_ipr is running with pid %d", (int)childPid);

	ChildState state = getChildState();
	if (state == ChildState::Stopped) {
		Log::getLog().debug("Process closed before waiting for it");
		stopCallback();
		return true;
	} else if (state == ChildState::Error) {
		Log::getLog().error("Failed waiting for child process - %d", errno);
		return false;
	}

	Log::getLog().debug("Starting thread monitoring child pid");

	checkRunning = true;
	waitThread = new std::thread([this]() {
		Log::getLog().debug("Proc wait thread started");
		LnxProcessCheck::ChildState state;
		while ((state = getChildState()) == ChildState::Running && this->checkRunning) {
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}

		if (state == ChildState::Stopped) {
			Log::getLog().debug("Child with pid [%d] exited", (int)childPid);
		} else if (state == ChildState::Error) {
			Log::getLog().error("Thread failed waiting for child process - %d", errno);
		}
		checkRunning = false;
		this->stopCallback();
	});

	return true;
}

bool LnxProcessCheck::isAlive() {
	return getChildState() == ChildState::Running;
}

bool LnxProcessCheck::stop() {
	checkRunning = false;
	Log::getLog().debug("Trying to stop LnxProcessCheck with thread [%p]", waitThread);
	if (waitThread) {
		if (std::this_thread::get_id() == waitThread->get_id()) {
			Log::getLog().error("LnxProcessCheck::stop() called from waiting thread!");
			vassert(false && "LnxProcessCheck::stop() called from waiting thread!");
		} else if (waitThread->joinable()) {
			Log::getLog().debug("Calling .join() on proc wait thread");
			waitThread->join();
			delete waitThread;
			waitThread = nullptr;
		} else {
			Log::getLog().error("Can't join LnxProcessCheck's waiting thread - leaking the handle");
			vassert(false && "LnxProcessCheck's wait thread is not joinable");
			waitThread = nullptr;
		}
	}
	return true;
}
