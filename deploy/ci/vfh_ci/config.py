#
# Copyright (c) 2015-2017, Chaos Software Ltd
#
# V-Ray For Houdini
#
# ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
#
# Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
#

# andrei.izrantcev@chaosgroup.com, lyubomir.koev@chaosgroup.com, nedyalko.radev@chaosgroup.com, mila.grigorova@chaosgroup.com

import os
import re

from . import utils
from . import log
from . import git

# The current CWD is the sources root!
SOURCE_DIR = utils.toCmakePath(os.getcwd())
SOURCE_HASH = git.getHash(SOURCE_DIR)
SOURCE_BRANCH = git.getBranch(SOURCE_DIR)

# Globals
VFH_SDK_REPO = 'ssh://gitolite@mantis.chaosgroup.com:2047/vray_for_houdini_sdk_%s' % utils.getPlatform()
KDRIVE_DIR = utils.toCmakePath(os.path.join(os.environ['VRAY_CGREPO_PATH'], "sdk", utils.getSdkPlatform()))
BUILD_NUMBER = int(os.environ['BUILD_NUMBER'])

# Extracts Houdini / Qt version from a string like "16.0 (Qt 5)"
#
hdkQtMatchRe = re.compile(r'([0-9]+\.[0-9]+)(?:\s*\(Qt\s*(\d*)\)*)?')
hdkQtMatch = hdkQtMatchRe.findall(os.environ['VFH_HOUDINI_VERSION'])[0]

hdkBuildMatchRe = re.compile(r'[0-9]+')

HOUDINI_VERSION = float(hdkQtMatch[0])
HOUDINI_VERSION_BUILD = int(hdkBuildMatchRe.findall(os.environ['VFH_HOUDINI_VERSION_BUILD'])[0])
HOUDINI_QT_VERSION = int(hdkQtMatch[1] if hdkQtMatch[1] else 4)

OUTPUT_DIR = utils.toCmakePath(os.environ['VFH_OUTPUT_DIR'])
PERMANENT_DIR = utils.toCmakePath(os.path.join(os.environ['JVRAYPATH'], "houdini"))

BUILD_DIR = utils.toCmakePath(os.path.join(PERMANENT_DIR, "build"))
VFH_SDK_DIR = utils.toCmakePath(os.path.join(PERMANENT_DIR, "sdk"))

CMAKE_BUILD_TYPE = os.environ['CMAKE_BUILD_TYPE']

# Output configuration
OUTPUT_FILE_FMT = "vfh-{BUILD_NUMBER}-{SRC_GIT_HASH}-hfs{HOUDINI_VERSION}.{HOUDINI_VERSION_BUILD}{QT}-{OS}{DEBUG}.{EXT}"

log.message("Houdini version: %s.%s" % (HOUDINI_VERSION, HOUDINI_VERSION_BUILD))
log.message("HDK Qt version: %s" % HOUDINI_QT_VERSION)
log.message("VFH SDK repository: %s" % VFH_SDK_REPO)
log.message("VFH SDK directory: %s" % VFH_SDK_DIR)
log.message("SDK directory: %s" % KDRIVE_DIR)
log.message("Permanent directory: %s" % PERMANENT_DIR)
log.message("Sources directory: %s" % SOURCE_DIR)
log.message("Sources hash: %s" % SOURCE_HASH)
log.message("Sources branch: %s" % SOURCE_BRANCH)
log.message("Build directory: %s" % BUILD_DIR)
