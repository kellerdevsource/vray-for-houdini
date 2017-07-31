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

#include <thread>
#include <memory>

using namespace VRayForHoudini;

class LnxProcessCheck:
	public ProcessCheck
{
public:
	LnxProcessCheck(OnStop cb, const std::string &name)
		: ProcessCheck(cb, name)
		, waitThread(nullptr)
		, childPid(0)
		, checkRunning(false)
	{}

	bool start() override;

	bool stop() override;
private:
	/// Thread waiting in waitpid
	std::thread * waitThread;
	/// The pid of the child we are waiting for
	pid_t childPid;
	/// Flag keeping the thread running
	bool checkRunning;
};

ProcessCheckPtr makeProcessChecker(ProcessCheck::OnStop cb, const std::string &name) {
	return std::make_unique<LnxProcessCheck>(cb, name);
}

bool LnxProcessCheck::start() {
	pid_t myPid = getpid();
	char pidFileName[1024] = {0,};
	sprintf(pidFileName, "/tmp/%d", static_cast<int>(myPid));

	auto pidFile = std::shared_ptr<FILE>(fopen(pidFileName, "rb"), [](FILE * file) {
		if (file) {
			fclose(file);
		}
	});

	if (!pidFile) {
		return false;
	}

	if (!fread(reinterpret_cast<char*>(&childPid), sizeof(childPid), 1, pidFile.get()) != 1) {
		return false;
	}

	int status = 0;
	pid_t testPid = waitpid(childPid, &status, WNOHANG);
	if (testPid < 0) {
		if (errno == ECHILD) {
			Log::getLog().error("Failed waiting for child process - pid no longer child");
		} else {
			Log::getLog().error("Failed waiting for child process - %d", errno);
		}
		return false;
	} else if (testPid > 0) {
		Log::getLog().debug("Process closed before waiting for it");
		stopCallback();
		return true;
	}

	checkRunning = true;
	waitThread = new std::thread([this]() {
		pid_t resPid = 0;
		int status = 0;
		while ((resPid = waitpid(this->childPid, &status, WNOHANG)) == 0 && this->checkRunning) {
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}

		if (resPid > 0) {
			checkRunning = false;
			this->stopCallback();
		} else {
			Log::getLog().error("Thread failed waiting for child process - %d", errno);
		}
	});

	return true;
}

bool LnxProcessCheck::stop() {
	checkRunning = false;
	if (waitThread) {
		if (std::this_thread::get_id() == waitThread->get_id()) {
			Log::getLog().error("LnxProcessCheck::stop() called from waiting thread!");
			assert(false);
		} else if (waitThread->joinable()) {
			waitThread->join();
			delete waitThread;
			waitThread = nullptr;
		} else {
			Log::getLog().error("Can't join LnxProcessCheck's waiting thread - leaking the handle");
			assert(false);
			waitThread = nullptr;
		}
	}
	return true;
}
