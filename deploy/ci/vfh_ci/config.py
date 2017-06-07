#
# Copyright (c) 2015-2017, Chaos Software Ltd
#
# V-Ray For Houdini
#
# ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
#
# Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
#

import os
import re

from . import utils
from . import log

# Globals
VFH_SDK_REPO = 'ssh://gitolite@mantis.chaosgroup.com:2047/vray_for_houdini_sdk_%s' % utils.getPlatform()

KDRIVE_DIR = utils.toCmakePath(os.path.join(os.environ['VRAY_CGREPO_PATH'], "sdk", utils.getPlatform()))

# Extracts Houdini / Qt version from a string like "16.0 (Qt 5)"
#
hdkQtMatchRe = re.compile(r'([0-9]+\.[0-9]+)(?:\s*\(Qt\s*(\d*)\)*)?')
hdkQtMatch = hdkQtMatchRe.findall(os.environ['VFH_HOUDINI_VERSION'])[0]

hdkBuildMatchRe = re.compile(r'[0-9]+')

HOUDINI_VERSION = float(hdkQtMatch[0])
HOUDINI_VERSION_BUILD = int(hdkBuildMatchRe.findall(os.environ['VFH_HOUDINI_VERSION_BUILD'])[0])
HOUDINI_QT_VERSION = int(hdkQtMatch[1] if hdkQtMatch[1] else 4)

OUTPUT_DIR = utils.toCmakePath(os.environ['VFH_OUTPUT_DIR'])
CHECKOUT_DIR = utils.toCmakePath(os.path.join(os.environ['JVRAYPATH'], "houdini"))

log.message("Houdini version: %s.%s" % (HOUDINI_VERSION, HOUDINI_VERSION_BUILD))
log.message("HDK Qt version: %s" % HOUDINI_QT_VERSION)
log.message("VFH SDK repository: %s" % VFH_SDK_REPO)
log.message("SDK directory: %s" % KDRIVE_DIR)
log.message("Output directory: %s" % OUTPUT_DIR)
log.message("Checkout directory: %s" % CHECKOUT_DIR)

