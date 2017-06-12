#
# Copyright (c) 2015-2017, Chaos Software Ltd
#
# V-Ray For Houdini
#
# ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
#
# Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
#
#
# Setup environment variables for Visual Studio usage from command line.
#

import os
import sys

from . import utils


def setup_ninja():
    if sys.platform in {'win32'}:
        ninjaPath = os.path.join(os.environ['VRAY_CGREPO_PATH'], "build_scripts/cmake/tools/bin")
    else:
        ninjaPath = os.path.join(os.environ['CI_ROOT'], "ninja/ninja")

    os.environ['PATH'] = os.pathsep.join([ninjaPath] + os.environ['PATH'].split(os.pathsep))


def setup_msvc_2012(sdkPath):
    env = {
        'INCLUDE' : [
            "{KDRIVE}/msvs2012/PlatformSDK/Include/shared",
            "{KDRIVE}/msvs2012/PlatformSDK/Include/um",
            "{KDRIVE}/msvs2012/PlatformSDK/Include/winrt",
            "{KDRIVE}/msvs2012/include",
            "{KDRIVE}/msvs2012/atlmfc/include",
        ],

        'LIB' : [
            "{KDRIVE}/msvs2012/PlatformSDK/Lib/win8/um/x64",
            "{KDRIVE}/msvs2012/atlmfc/lib/amd64",
            "{KDRIVE}/msvs2012/lib/amd64",
        ],

        'PATH' : [
            "{KDRIVE}/msvs2012/bin/amd64",
            "{KDRIVE}/msvs2012/bin",
            "{KDRIVE}/msvs2012/PlatformSDK/bin/x64",
        ] + os.environ['PATH'].split(os.pathsep),
    }

    for var in env:
        os.environ[var] = os.pathsep.join(env[var]).format(KDRIVE=sdkPath)


def setup_msvc_2015(sdkPath):
    env = {
        'INCLUDE' : [
            "{KDRIVE}/msvs2015/PlatformSDK/Include/shared",
            "{KDRIVE}/msvs2015/PlatformSDK/Include/um",
            "{KDRIVE}/msvs2015/PlatformSDK/Include/winrt",
            "{KDRIVE}/msvs2015/PlatformSDK/Include/ucrt",
            "{KDRIVE}/msvs2015/include",
            "{KDRIVE}/msvs2015/atlmfc/include",
        ],

        'LIB' : [
            "{KDRIVE}/msvs2015/PlatformSDK/Lib/winv6.3/um/x64",
            "{KDRIVE}/msvs2015/PlatformSDK/Lib/ucrt/x64",
            "{KDRIVE}/msvs2015/atlmfc/lib/amd64",
            "{KDRIVE}/msvs2015/lib/amd64",
        ],

        'PATH' : [
            "{KDRIVE}/msvs2015/bin/amd64",
            "{KDRIVE}/msvs2015/bin",
            "{KDRIVE}/msvs2015/PlatformSDK/bin/x64",
        ] + os.environ['PATH'].split(os.pathsep),

        '__MS_VC_INSTALL_PATH' : [
            "{KDRIVE}/msvs2015"
        ],
    }

    for var in env:
        os.environ[var] = os.pathsep.join(env[var]).format(KDRIVE=sdkPath)


def setup_compiler(houdiniMajorVersion, sdkPath):
    setup_ninja()

    if sys.platform in {'win32'}:
        if houdiniMajorVersion >= 15.5:
            setup_msvc_2015(sdkPath)
        else:
            setup_msvc_2012(sdkPath)
