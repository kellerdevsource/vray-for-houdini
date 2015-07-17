#
# Copyright (c) 2015, Chaos Software Ltd
#
# V-Ray For Houdini
#
# Andrei Izrantcev <andrei.izrantcev@chaosgroup.com>
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

if(WIN32)
	# Houdini specific
	set(CMAKE_CXX_FLAGS "/wd4355 /w14996 /wd4800 /wd4244 /wd4305 /wd4251 /wd4275 /wd4396 /wd4018 /wd4267 /wd4146 /EHsc /GT /bigobj")

else()
	if (APPLE)
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -stdlib=libstdc++")
		add_definitions(-DBOOST_NO_CXX11_RVALUE_REFERENCES)
	else()
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x -stdlib=libstdc++")
	endif()

	# Houdini specific
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-attributes")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-parentheses -Wno-sign-compare -Wno-reorder -Wno-uninitialized -Wno-unused-parameter -Wno-deprecated -fno-strict-aliasing")

	# Houdini SDK / V-Ray SDK
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-switch -Wno-narrowing -Wno-int-to-pointer-cast")

	if (NOT APPLE)
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-literal-suffix -Wno-unused-local-typedefs")

		set(CMAKE_CXX_CREATE_SHARED_LIBRARY "<CMAKE_CXX_COMPILER> <CMAKE_SHARED_LIBRARY_CXX_FLAGS> <LANGUAGE_COMPILE_FLAGS> <LINK_FLAGS> <CMAKE_SHARED_LIBRARY_CREATE_CXX_FLAGS> <SONAME_FLAG><TARGET_SONAME> -o <TARGET> -Wl,--start-group <OBJECTS> <LINK_LIBRARIES> -Wl,--end-group")
	endif()
endif()

message(STATUS "Using flags: ${CMAKE_CXX_FLAGS}")
