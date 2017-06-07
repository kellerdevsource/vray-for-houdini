::
:: Copyright (c) 2015-2017, Chaos Software Ltd
::
:: V-Ray For Houdini
::
:: ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
::
:: Full license text:
::  https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
::

@echo off

:: From Jenkins
set BUILD_NUMBER=0
set JVRAYPATH=%CD%
set VRAY_BUILD_PATH_OUTPUT=%CD%\output
set CI_ROOT=%JVRAYPATH%
set VRAY_CGREPO_PATH=%CGREPO%

:: Set configuration
set CMAKE_BUILD_TYPE=Release
set VFH_HOUDINI_VERSION=16.0 (Qt 5)
set VFH_HOUDINI_VERSION_BUILD=600

set VFH_OUTPUT_DIR=%VRAY_BUILD_PATH_OUTPUT%\vray\houdini

python "%~dp0/jenkins.py"
