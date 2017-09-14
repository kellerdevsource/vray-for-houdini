#
# Copyright (c) 2015-2017, Chaos Software Ltd
#
# V-Ray For Houdini
#
# ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
#
# Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
#

from . import utils

def getHash(repoDirPath):
    cmd = ['git', 'rev-parse', '--short', 'HEAD']
    return utils.getCmdOutput(cmd, cwd=repoDirPath)[:7]

def getBranch(repoDirPath):
    cmd = ['git', 'rev-parse', '--abbrev-ref', 'HEAD']
    return utils.getCmdOutput(cmd, cwd=repoDirPath)
