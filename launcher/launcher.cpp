#include "string_defines.h"

#include <windows.h>
#include <stdio.h>
#include <tchar.h>
#include <cstdlib>

#include <stdio.h>  
#include <conio.h>  
#include <process.h> 

#include <map>
#include <vector>
#include <algorithm>

typedef std::map<std::string, std::string> EnvMap;

EnvMap getCurrent() {
	EnvMap map;
	char * prevEnv = GetEnvironmentStrings();
	char * iter = prevEnv;
	std::vector<char> buffer(4096, 0);
	while (true) {
		const int len = strlen(iter);
		if (!len) {
			break;
		}
		buffer.resize(std::max<int>(len, buffer.size()));
		strcpy(buffer.data(), iter);
		char * ptr = strchr(buffer.data(), '=');
		*ptr = 0;
		++ptr;
		if (strlen(buffer.data())) {
			map[buffer.data()] = ptr;
		}

		iter += len + 1;
	}
	return map;
}


int main() {
	EnvMap updateEnv = {
		{"VRAY_APPSDK", VFH_LAUNCHER_VRAY_APPSDK},
		{"VRAY_PLUGIN_DESC_PATH", VFH_LAUNCHER_VRAY_PLUGIN_DESC_PATH},
		{"VRAY_UI_DS_PATH", VFH_LAUNCHER_VRAY_UI_DS_PATH},
		{"VRAY_FOR_HOUDINI_AURA_LOADERS", VFH_LAUNCHER_VRAY_FOR_HOUDINI_AURA_LOADERS},
		{"PYTHONPATH", VFH_LAUNCHER_PYTHONPATH},
		{"HFS", VFH_LAUNCHER_HFS},
		{"HOUDINI_PATH", VFH_LAUNCHER_HOUDINI_PATH},
		{"HOUDINI_DSO_PATH", VFH_LAUNCHER_HOUDINI_DSO_PATH},
		{"HOUDINI13_VOLUME_COMPATIBILITY", VFH_LAUNCHER_HOUDINI13_VOLUME_COMPATIBILITY},
		{"HOUDINI_DSO_ERROR", VFH_LAUNCHER_HOUDINI_DSO_ERROR},
		{"HOUDINI_SOHO_DEVELOPER", VFH_LAUNCHER_HOUDINI_SOHO_DEVELOPER},
		{"HOUDINI_DISABLE_CONSOLE", VFH_LAUNCHER_HOUDINI_DISABLE_CONSOLE},
		{"HOUDINI_TEXT_CONSOLE", VFH_LAUNCHER_HOUDINI_TEXT_CONSOLE},
		{"HOUDINI_WINDOW_CONSOLE", VFH_LAUNCHER_HOUDINI_WINDOW_CONSOLE},
		{"HOUDINI_VERBOSE_ERROR", VFH_LAUNCHER_HOUDINI_VERBOSE_ERROR},
		{"PATH", VFH_LAUNCHER_PATH},
		{"QT_QPA_PLATFORM_PLUGIN_PATH", VFH_LAUNCHER_QT_QPA_PLATFORM_PLUGIN_PATH},
	};

	auto env = getCurrent();
	for (const auto & updateVar : updateEnv) {
		auto envIter = env.find(updateVar.first);
		if (envIter != env.end()) {
			const auto newValue = updateVar.second + ";" + envIter->second;
			envIter->second = newValue;
		} else {
			env[updateVar.first] = updateVar.second;
		}
	}

	std::vector<std::string> envItemStrings(env.size());
	std::vector<const char *> envItems; // these will point to envItemStrings so it must live longer than this
	int c = 0;
	for (auto & iter : env) {
		envItemStrings[c].reserve(iter.first.length() + 1 + iter.second.length() + 2);
		envItemStrings[c] = iter.first + "=" + iter.second;
		envItems.push_back(envItemStrings[c].c_str());
		c++;
	}
	envItems.push_back(nullptr);

	char *const argv[] = {
		VFH_LAUNCHER_HFS, nullptr
	};
	int res = execvpe(VFH_LAUNCHER_HFS_BIN, argv, envItems.data());

	return 0;
}
