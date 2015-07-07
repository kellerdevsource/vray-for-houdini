:: V-Ray For Houdini Windows Launcher
::

@echo off

:: Use OpenVDB < 3
::
set HOUDINI13_VOLUME_COMPATIBILITY=1

set HOUDINI_VER_MAJOR=14.0
set HOUDINI_VER_FULL=%HOUDINI_VER_MAJOR%.201.13

:: DO NOT EDIT STUFF BELOW (well, only if you really know what you're doing)
::
set BAT_LOCATION=%~dp0
set HFS=C:\Program Files\Side Effects Software\Houdini %HOUDINI_VER_FULL%

:: V-Ray Application SDK variables
set VRAY_SDK=%BAT_LOCATION%\appsdk
set VRAY_PATH=%VRAY_SDK%\bin

:: V-Ray JSON plugin descriptions
set VRAY_PLUGIN_DESC_PATH=%BAT_LOCATION%\vray_json

:: Add libs to path
set PATH=%HFS%\bin;%VRAY_PATH%;%PATH%

:: Report loading errors
set HOUDINI_DSO_ERROR=1

:: Install Files
if NOT DEFINED HOME set HOME=%USERPROFILE%\Documents

set HOUDINI_USER_DIR=%HOME%\houdini%HOUDINI_VER_MAJOR%

:: TODO: Set plugins for Phoenix
:: set VRAY_FOR_HOUDINI_PLUGINS=%HOUDINI_USER_DIR%\vray_for_houdini

xcopy /s /y "%BAT_LOCATION%\dso\vray_for_houdini.dll" "%HOUDINI_USER_DIR%\dso\"
xcopy /s /y "%BAT_LOCATION%\deploy\ROP_vray.svg" "%HOUDINI_USER_DIR%\config\Icons\"
xcopy /s /y "%BAT_LOCATION%\deploy\vfh.shelf" "%HOUDINI_USER_DIR%\toolbar\"
:: xcopy /s /y "%BAT_LOCATION%\deploy\plugins\*.dll" "%VRAY_FOR_HOUDINI_PLUGINS%\"

:: Start Houdini
start "V-Ray For Houdini" /D "%USERPROFILE%\Desktop\" "%HFS%\bin\houdini.exe" -foreground %*
