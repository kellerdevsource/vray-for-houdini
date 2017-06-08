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
import os
import subprocess
import shutil

from . import log


def getPlatform():
    """Returns platform name"""
    if sys.platform.startswith('win'):
        return "windows"
    elif sys.platform.startswith('linux'):
        return "linux"
    return "mac"


def getSdkPlatform():
    """Returns platform name matching SDK naming"""
    if sys.platform.startswith('win'):
        return "win"
    elif sys.platform.startswith('linux'):
        return "linux"
    return "mac"


def getArchiveExt():
    if sys.platform in {'win32'}:
        return "zip"
    return "tar.bz2"


def toCmakePath(path):
    """Converts path slashes to UNIX style"""
    return os.path.abspath(os.path.normpath(os.path.expanduser(path))).replace("\\", "/")


def cleanDir(dirpath):
    if not os.path.isdir(dirpath):
        return

    log.message("Cleaning \"%s\"\n" % (dirpath))

    for root, dirs, files in os.walk(dirpath):
        for f in files:
            os.unlink(os.path.join(root, f))
        for d in dirs:
            shutil.rmtree(os.path.join(root, d))


def removeDir(path):
    if not os.path.exists(path):
        return
    if not os.path.isdir(path):
        return

    # Don't know why, but when deleting from python
    # on Windows it fails to delete '.git' direcotry,
    # so using shell command
    log.message("Deleting path: %s" % path)

    if getPlatform() == 'windows':
        # Need Windows slashes here
        path = os.path.normpath(path)
        os.system("rmdir /Q /S %s" % path)
        # Well yes, on Windows one remove is not enough...
        if os.path.exists(path):
            os.system("rmdir /Q /S %s" % path)
    else:
        shutil.rmtree(path)


def getCmdOutput(cmd, cwd=os.getcwd()):
    try:
        if type(cmd) is str:
            cmd = cmd.split()
        res = subprocess.check_output(cmd, cwd=cwd)
        if res is not None:
            if type(res) is bytes:
                res = res.decode("utf-8")
            return res.strip()
    except:
        pass


def p(*args):
    """Custom path join function with user home expansion
    and slash conversion"""
    p = os.path.join(*args)
    p = os.path.expanduser(p)
    p = os.path.normpath(p)
    return toCmakePath(p)
