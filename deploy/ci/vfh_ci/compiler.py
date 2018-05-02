#
# Copyright (c) 2015-2018, Chaos Software Ltd
#
# V-Ray For Houdini
#
# ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
#
# Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
#
# Setup environment variables for Visual Studio usage from command line.
#

import os
import sys

from . import utils

def setup_ninja():
    if utils.getPlatform() == 'windows':
        ninjaPath = os.path.join(os.environ['VRAY_CGREPO_PATH'], "build_scripts/cmake/tools/bin")
    else:
        ninjaPath = os.path.join(os.environ['CI_ROOT'], "ninja/ninja")

    os.environ['PATH'] = os.pathsep.join([ninjaPath] + os.environ['PATH'].split(os.pathsep))

def setup_msvc_2017(sdkPath):
    env = {
        'INCLUDE' : [
            "{KDRIVE}/msvs2015/PlatformSDK/Include/shared",
            "{KDRIVE}/msvs2015/PlatformSDK/Include/um",
            "{KDRIVE}/msvs2015/PlatformSDK/Include/winrt",
            "{KDRIVE}/msvs2015/PlatformSDK/Include/ucrt",
            "{KDRIVE}/msvs2017/include",
            "{KDRIVE}/msvs2017/atlmfc/include",
        ],

        'LIB' : [
            "{KDRIVE}/msvs2015/PlatformSDK/Lib/winv6.3/um/x64",
            "{KDRIVE}/msvs2015/PlatformSDK/Lib/ucrt/x64",
            "{KDRIVE}/msvs2017/atlmfc/lib/x64",
            "{KDRIVE}/msvs2017/lib/x64",
        ],

        'PATH' : [
            "{KDRIVE}/msvs2017/bin/Hostx64/x64",
            "{KDRIVE}/msvs2017/bin",
            "{KDRIVE}/msvs2015/PlatformSDK/bin/x64",
        ] + os.environ['PATH'].split(os.pathsep),

        '__MS_VC_INSTALL_PATH' : [
            "{KDRIVE}/msvs2017"
        ],
    }

    for var in env:
        os.environ[var] = os.pathsep.join(env[var]).format(KDRIVE=sdkPath)

def setup_compiler(houdiniMajorVersion, sdkPath):
    setup_ninja()

    if utils.getPlatform() == 'windows':
        setup_msvc_2017(sdkPath)
