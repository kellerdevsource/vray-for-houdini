#
# Copyright (c) 2015-2017, Chaos Software Ltd
#
# V-Ray For Houdini
#
# ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
#
# Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
#

import subprocess
import os

from . import log

def call(cmd, cwd):
    if type(cmd) in {str}:
        cmd = cmd.split()
    log.message("Calling: \"%s\"" % " ".join(cmd))
    return subprocess.call(cmd, cwd=cwd)
