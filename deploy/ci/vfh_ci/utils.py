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

from . import log

def getPlatform():
    """Returns platform name matching SDK naming"""
    if sys.platform.startswith('win'):
        return "windows"
    elif sys.platform.startswith('linux'):
        return "linux"
    return "mac"

def cleanDir(dirpath):
    if not os.path.isdir(dirpath):
        return

    log.message("Cleaning \"%s\"\n" % (dirpath))

    for root, dirs, files in os.walk(dirpath):
        for f in files:
            os.unlink(os.path.join(root, f))
        for d in dirs:
            shutil.rmtree(os.path.join(root, d))


def remove_directory(path):
    # Don't know why, but when deleting from python
    # on Windows it fails to delete '.git' direcotry,
    # so using shell command
    log.message("Deleting path: {}\n".format(path))

    if sys.platform in {'win32'}:
        os.system("rmdir /Q /S %s" % path)
        # Well yes, on Windows one remove is not enough...
        if os.path.exists(path):
            os.system("rmdir /Q /S %s" % path)
    else:
        shutil.rmtree(path)


def getArchiveExt():
    if sys.platform in {'win32'}:
        return "zip"
    return "tar.bz2"


def toCmakePath(path):
    """Converts path slashes to UNIX style"""
    return os.path.normpath(os.path.expanduser(path)).replace("\\", "/")


def which(program, add_ext=False):
    """Returns full path to "program" or None"""

    def is_exe(fpath):
        return os.path.exists(fpath) and os.access(fpath, os.X_OK)

    fpath, fname = os.path.split(program)
    if fpath:
        if is_exe(program):
            return program
    else:
        for path in os.environ["PATH"].split(os.pathsep):
            exe_file = os.path.join(path, program)
            sys.stdout.write('Checking if path [%s] is path for [%s] ... ' % (exe_file, fname))
            if is_exe(exe_file):
                sys.stdout.write('yes!\n')
                sys.stdout.flush()
                return exe_file
            sys.stdout.write('no!\n')
            sys.stdout.flush()

        if sys.platform in {'win32'} and not add_ext:
            return which('%s.exe' % program, True)

    return None
