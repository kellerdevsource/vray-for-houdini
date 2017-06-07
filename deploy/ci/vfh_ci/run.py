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

def call(cmd, cwd=os.getcwd()):
    if type(cmd) in {list}:
        cmd = " ".join(cmd)
    log.message("Calling: %s" % cmd)
    # return subprocess.call(cmd, cwd=cwd)
