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

#include <Windows.h>
#include <TlHelp32.h>

void disableSIGPIPE() {}

/// Type for creating HANDLE that will be close with *CloseHandle*!
typedef std::shared_ptr<void> shared_handle;

namespace {
shared_handle make_handle(void *ptr) {
	return shared_handle(ptr, [](void *handle) {
		if (handle) {
			CloseHandle(handle);
		}
	});
}
}


class Win32ProcessCheck:
	public ProcessCheck
{
public:
	Win32ProcessCheck(OnStop cb, const std::string &name)
		: ProcessCheck(cb, name)
		, pid(0)
		, waitHandle(nullptr)
	{}

	bool start() override;

	bool stop() override;

	bool isAlive() override;


	DWORD getPID() const;
private:
	/// The pid of the child process
	DWORD pid;
	/// Handle to the wait object - needs to be unregistered (so it can't use shared_handle wrapper)
	HANDLE waitHandle;
	/// Handle to the child process
	shared_handle procHandle;
};


ProcessCheckPtr makeProcessChecker(ProcessCheck::OnStop cb, const std::string &name) {
	return std::make_shared<Win32ProcessCheck>(cb, name);
}

bool Win32ProcessCheck::start() {
	pid = getPID();
	if (!pid) {
		return false;
	}

	procHandle = make_handle(OpenProcess(SYNCHRONIZE | PROCESS_QUERY_INFORMATION, FALSE, pid));

	if (!procHandle) {
		return false;
	}

	if (!isAlive()) {
		stopCallback();
		return true;
	}

	auto callback = [](PVOID context, BOOLEAN) {
		auto *self = reinterpret_cast<Win32ProcessCheck*>(context);
		self->stop();
		self->stopCallback();
	};

	if (!RegisterWaitForSingleObject(&waitHandle, procHandle.get(), callback, this, INFINITE, WT_EXECUTEONLYONCE)) {
		return false;
	}

	return true;
}

bool Win32ProcessCheck::isAlive() {
	if (procHandle) {
		DWORD exitCode = 0;
		GetExitCodeProcess(procHandle.get(), &exitCode);
		return exitCode != STILL_ACTIVE;
	}
	return false;
}

bool Win32ProcessCheck::stop() {
	if (waitHandle) {
		UnregisterWait(waitHandle);
		waitHandle = nullptr;
		return true;
	}
	return false;
}

DWORD Win32ProcessCheck::getPID() const {
	auto snapshot = make_handle(CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0));
	if (!snapshot) {
		return 0;
	}

	PROCESSENTRY32 iterator;
	iterator.dwSize = sizeof(PROCESSENTRY32);
	if (Process32First(snapshot.get(), &iterator)) {
		do {
			if (!strcmp(iterator.szExeFile, processName.c_str()) && iterator.th32ParentProcessID == GetCurrentProcessId()) {
				return iterator.th32ProcessID;
			}
		} while (Process32Next(snapshot.get(), &iterator));
	}

	return 0;
}
