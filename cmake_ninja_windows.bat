@echo off

:: Source directory is the location of the script
set SRC_DIR=%~dp0

cmake -G Ninja ^
-DCMAKE_BUILD_TYPE=Release ^
-DHOUDINI_VERSION=14.0 ^
-DHOUDINI_VERSION_BUILD=201.13 ^
-DAPPSDK_VERSION=447 ^
%* ^
%SRC_DIR%
