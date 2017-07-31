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


class LnxProcessCheck:
	public ProcessCheck
{
public:
	LnxProcessCheck(OnStop cb, const std::string &name)
		: ProcessCheck(cb, name)
	{}

	bool start() override;

	bool stop() override;
};

ProcessCheckPtr makeProcessChecker(ProcessCheck::OnStop cb, const std::string &name) {
	return std::make_unique<LnxProcessCheck>(cb, name);
}

bool LnxProcessCheck::start() {
	return true;
}

bool LnxProcessCheck::stop() {
	return true;
}
