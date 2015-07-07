#
# Copyright (c) 2015, Chaos Software Ltd
#
# V-Ray For Houdini
#
# Andrei Izrantcev <andrei.izrantcev@chaosgroup.com>
# 
# All rights reserved. These coded instructions, statements and
# computer programs contain unpublished information proprietary to
# Chaos Group Ltd, which is protected by the appropriate copyright
# laws and may not be disclosed to third parties or copied or
# duplicated, in whole or in part, without prior written consent of
# Chaos Group Ltd.
#

cmake_minimum_required(VERSION 3.0.0)

set(CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH};${CMAKE_CURRENT_SOURCE_DIR}/cmake")

project(vray_for_houdini)

add_subdirectory(src)
