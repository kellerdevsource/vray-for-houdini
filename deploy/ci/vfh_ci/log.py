#
# Copyright (c) 2015-2017, Chaos Software Ltd
#
# V-Ray For Houdini
#
# ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
#
# Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
#

import sys

def message(msg):
    """Prints log message in CMake message(STATUS ...) manner"""
    sys.stdout.write("-- %s\n" % (msg))
    sys.stdout.flush()
