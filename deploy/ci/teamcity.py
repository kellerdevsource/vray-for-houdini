#
# Copyright (c) 2015-2016, Chaos Software Ltd
#
# V-Ray For Houdini
#
# ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
#
# Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
#

import argparse
import datetime
import os
import shutil
import subprocess
import sys
import tempfile


# Build mode: 'nightly', 'stable', 'debug'
_cgr_build_mode = None
_cgr_build_type = None
_cgr_release_root = None
_cgr_config_root = None

ENV_PATH_SEP = ';' if sys.platform == 'win32' else ':'


def toCmakePath(path):
    return os.path.normpath(path).replace("\\", "/")

def getPlatformSuffix():
    if sys.platform == 'win32':
        return "windows"
    elif sys.platform == 'linux':
        return "linux"
    return "osx"



def cleanDir(dirpath):
    if os.path.isdir(dirpath):
        sys.stdout.write("-- Cleaning = %s\n" % (dirpath))
        for root, dirs, files in os.walk(dirpath):
            for f in files:
                os.unlink(os.path.join(root, f))
            for d in dirs:
                shutil.rmtree(os.path.join(root, d))


def call(args):
    sys.stdout.write("-- Calling: %s\n" % " ".join(args))
    sys.stdout.flush()
    return subprocess.call(args, cwd=os.getcwd())


def upload(filepath):
    try:
        from configparser import ConfigParser
    except:
        from ConfigParser import ConfigParser

    config = ConfigParser()
    config.read(os.path.expanduser("%s/.passwd" % (_cgr_config_root)))

    now = datetime.datetime.now()
    subdir = now.strftime("%Y%m%d")

    if sys.platform == 'win32':
        ftpScriptFilepath = os.path.join(tempfile.gettempdir(), "vfh_upload.txt")

        with open(ftpScriptFilepath, 'w') as f:
            f.write('option batch abort\n')
            f.write('option confirm off\n')
            f.write('open ftp://%s:%s@%s -rawsettings ProxyMethod=%s ProxyHost=%s ProxyPort=%s\n' % (
                config.get('vfh.nightlies.ftp', 'user'),
                config.get('vfh.nightlies.ftp', 'pass'),
                config.get('vfh.nightlies.ftp', 'host'),
                config.get('vfh.nightlies.ftp', 'proxy_type'),
                config.get('vfh.nightlies.ftp', 'proxy_host'),
                config.get('vfh.nightlies.ftp', 'proxy_port'),
            ))
            f.write('option transfer binary\n')
            f.write('put %s /%s/%s/\n' % (filepath, _cgr_build_mode, subdir))
            f.write('exit\n')
            f.write('\n')

        cmd = ['winscp']
        cmd.append('/passive')
        cmd.append('/script="%s"' % ftpScriptFilepath)

        sys.stdout.write("-- Calling: %s" % " ".join(cmd))
        os.system(' '.join(cmd))

    else:
        cmd = ['curl']
        cmd.append('--no-epsv')
        cmd.append('--proxy')
        cmd.append('%s:%s' % (
            config.get('vfh.nightlies.ftp', 'proxy_host'),
            config.get('vfh.nightlies.ftp', 'proxy_port_http'),
        ))
        cmd.append('--user')
        cmd.append('%s:%s' % (
            config.get('vfh.nightlies.ftp', 'user'),
            config.get('vfh.nightlies.ftp', 'pass'),
        ))
        cmd.append('--upload-file')
        cmd.append(filepath)
        cmd.append('ftp://%s/%s/%s/' % (
            config.get('vfh.nightlies.ftp', 'host'),
            _cgr_build_mode,
            subdir,
        ))

        sys.stdout.write("-- Calling: %s" % " ".join(cmd))
        sys.stdout.flush()
        subprocess.call(cmd)


# Setup Visual Studio variables for command line usage
#
def set_utils_paths():
    if getPlatformSuffix() != 'windows':
        return

    env = {
        'PATH': [
            "C:/Program Files (x86)/CMake/bin",
            "C:/Program Files/SlikSvn/bin",
            "C:/Program Files/Git/bin",
            "C:/Program Files (x86)/WinSCP",
        ] + os.environ['PATH'].split(ENV_PATH_SEP)
    }

    for var in env:
        os.environ[var] = ENV_PATH_SEP.join(env[var])


def setup_msvc_2012(cgrSdkPath):
    env = {
        'INCLUDE' : [
            "{CGR_SDK}/msvs2012/PlatformSDK/Include/shared",
            "{CGR_SDK}/msvs2012/PlatformSDK/Include/um",
            "{CGR_SDK}/msvs2012/PlatformSDK/Include/winrt",
            "{CGR_SDK}/msvs2012/include",
            "{CGR_SDK}/msvs2012/atlmfc/include",
        ],

        'LIB' : [
            "{CGR_SDK}/msvs2012/PlatformSDK/Lib/win8/um/x64",
            "{CGR_SDK}/msvs2012/atlmfc/lib/amd64",
            "{CGR_SDK}/msvs2012/lib/amd64",
        ],

        'PATH' : os.environ['PATH'].split(ENV_PATH_SEP) +
            [
                "{CGR_SDK}/msvs2012/bin/amd64",
                "{CGR_SDK}/msvs2012/bin",
                "{CGR_SDK}/msvs2012/PlatformSDK/bin/x64",
            ]
        ,
    }

    for var in env:
        os.environ[var] = ENV_PATH_SEP.join(env[var]).format(CGR_SDK=cgrSdkPath)


def setup_msvc_2015(cgrSdkPath):
    env = {
        'INCLUDE' : [
            "{CGR_SDK}/msvs2015/PlatformSDK/Include/shared",
            "{CGR_SDK}/msvs2015/PlatformSDK/Include/um",
            "{CGR_SDK}/msvs2015/PlatformSDK/Include/winrt",
            "{CGR_SDK}/msvs2015/PlatformSDK/Include/ucrt",
            "{CGR_SDK}/msvs2015/include",
            "{CGR_SDK}/msvs2015/atlmfc/include",
        ],

        'LIB' : [
            "{CGR_SDK}/msvs2015/PlatformSDK/Lib/winv6.3/um/x64",
            "{CGR_SDK}/msvs2015/PlatformSDK/Lib/ucrt/x64",
            "{CGR_SDK}/msvs2015/atlmfc/lib/amd64",
            "{CGR_SDK}/msvs2015/lib/amd64",
        ],

        'PATH' : os.environ['PATH'].split(ENV_PATH_SEP) +
            [
                "{CGR_SDK}/msvs2015/bin/amd64",
                "{CGR_SDK}/msvs2015/bin",
                "{CGR_SDK}/msvs2015/PlatformSDK/bin/x64",
            ]
        ,
    }

    os.environ['__MS_VC_INSTALL_PATH'] = "{CGR_SDK}/msvs2015"
    for var in env:
        os.environ[var] = ENV_PATH_SEP.join(env[var]).format(CGR_SDK=cgrSdkPath)


def getArchiveExt():
    if sys.platform == 'win32':
        return "zip"
    return "tar.bz2"


def which(program, add_ext=False):
    """
      Returns full path of "program" or None
    """

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

        if getPlatformSuffix() == 'windows' and not add_ext:
            return which('%s.exe' % program, True)

    return None


def main(args):
    # lets polute global space
    global _cgr_build_mode
    global _cgr_build_type
    global _cgr_release_root
    global _cgr_config_root

    _cgr_build_mode = args.CGR_BUILD_MODE
    _cgr_build_type = args.CGR_BUILD_TYPE
    _cgr_release_root = args.CGR_RELEASE_ROOT
    _cgr_config_root = args.CGR_CONFIG_ROOT

    set_utils_paths()
    srcHash = args.src_hash[:7]

    ReleaseDir = os.path.expanduser("%s/vray_for_houdini/%s" % (_cgr_release_root, sys.platform))
    ReleaseArchive = os.path.join(ReleaseDir, "vfh-{BUILD_NUMBER}-{SRC_GIT_HASH}-hfs{HOUDINI_VERSION}.{HOUDINI_VERSION_BUILD}-{OS}{DEBUG}.{EXT}".format(
        SRC_GIT_HASH=srcHash,
        HOUDINI_VERSION=args.CGR_HOUDINI_VERSION,
        HOUDINI_VERSION_BUILD=args.CGR_HOUDINI_VERSION_BUILD,
        EXT=getArchiveExt(),
        OS=getPlatformSuffix(),
        BUILD_NUMBER=args.BUILD_NUMBER,
        DEBUG="-dbg" if _cgr_build_type == "Debug" else "",
    ))
    SdkPath = args.CGR_SDKPATH

    ninja_path = os.path.dirname(which('ninja'))

    cmake = [which('cmake')]
    cmake.append('-GNinja')

    old_path = os.environ['PATH']
    if args.jenkins and getPlatformSuffix() == 'windows':
        # clear all possible conflicts from path
        os.environ['PATH'] = ENV_PATH_SEP.join([ninja_path, os.path.dirname(which('git')), os.path.dirname(which('7z'))])

    if sys.platform == 'win32':
        houdiniMajorVer = float(args.CGR_HOUDINI_VERSION)
        if houdiniMajorVer >= 15.5:
            cmake.append('-DMSVC_VERSION=1900')
            setup_msvc_2015(args.CGR_SDK)
        else:
            cmake.append('-DMSVC_VERSION=1800')
            setup_msvc_2012(args.CGR_SDK)

    if sys.platform == 'linux':
        cmake.append('-DCMAKE_CXX_COMPILER=%s' % args.CGR_CXX_COMPILER)
        cmake.append('-DWITH_STATIC_LIBC=ON')

    cmake.append('-DCMAKE_BUILD_TYPE=%s' % _cgr_build_type)
    cmake.append('-DSDK_PATH=%s'              % toCmakePath(SdkPath))
    cmake.append('-DHOUDINI_VERSION=%s'       % args.CGR_HOUDINI_VERSION)
    cmake.append('-DHOUDINI_VERSION_BUILD=%s' % args.CGR_HOUDINI_VERSION_BUILD)
    cmake.append('-DAPPSDK_VERSION=%s'        % args.CGR_APPSDK_VERSION)
    cmake.append('-DPHXSDK_VERSION=%s'        % args.CGR_PHXSDK_VERSION)
    cmake.append('-DVRAYSDK_VERSION=%s'       % args.CGR_VRAYSDK_VERSION)
    cmake.append('-DCGR_SRC_HASH=%s' % srcHash)
    cmake.append('-DUSE_LAUNCHER=OFF')
    cmake.append('-DINSTALL_LOCAL=OFF')
    cmake.append('-DINSTALL_RELEASE=ON')
    cmake.append('-DINSTALL_RELEASE_ROOT=%s' % toCmakePath(ReleaseDir))
    if _cgr_build_type == "Debug":
        cmake.append('-DINSTALL_RELEASE_SUFFIX=-dbg')
    cmake.append('-DINSTALL_RELEASE_ARCHIVE_FILEPATH=%s' % toCmakePath(ReleaseArchive))
    cmake.append(args.src_dir)

    ninja = ["ninja"]
    ninja.append("install")

    if args.clean:
        # clean build dir
        cleanDir(os.getcwd())

    sys.stdout.write('PATH:\n\t%s\n' % '\n\t'.join(os.environ['PATH'].split(ENV_PATH_SEP)))
    sys.stdout.write('cmake args:\n%s\n' % '\n\t'.join(cmake))
    sys.stdout.flush()

    err = call(cmake)

    if not err:
        err = call(ninja)

    os.environ['PATH'] = old_path
    if not err:
        if args.upload:
            upload(ReleaseArchive)

        sys.stdout.write("##teamcity[setParameter name='env.CGR_ARTIFACTS' value='%s']" % (ReleaseArchive))
        sys.stdout.flush()

    return err


def get_cmd_arguments():
    parser = argparse.ArgumentParser(usage="python3 teamcity.py [options]")
    parser.add_argument('--src_hash', default="unknown", help="Sources hash")
    parser.add_argument('--src_dir', default=False, help="Sources directory")
    parser.add_argument('--upload', action='store_true', default=False, help="Upload build")
    parser.add_argument('--clean', action='store_true', default=False, help="Clean build directory")
    parser.add_argument('--jenkins', action='store_true', default=False, help=argparse.SUPPRESS)

    if sys.platform == 'linux':
        parser.add_argument('--CGR_CXX_COMPILER', default='/usr/bin/g++4.8', help="Compiler location")

    env_vars = [
        'BUILD_NUMBER', 'CGR_APPSDK_VERSION', 'CGR_BUILD_MODE', 'CGR_BUILD_TYPE',
        'CGR_CONFIG_ROOT', 'CGR_HOUDINI_VERSION', 'CGR_HOUDINI_VERSION_BUILD',
        'CGR_PHXSDK_VERSION', 'CGR_RELEASE_ROOT', 'CGR_SDK', 'CGR_VRAYSDK_VERSION'
    ]

    default = 'UNSET_DEFAULT'

    for var in env_vars:
        parser.add_argument('--%s' % var, default=default, help='Optional if not provided will be read from env.')

    args = parser.parse_args()

    stop = False

    for var in env_vars:
        value = getattr(args, var)
        if value == default:
            if var in os.environ:
                env_val = os.environ[var]
                sys.stdout.write('Missing value for "%s", reading from environ [%s]\n' % (var, env_val))
                sys.stdout.flush()
                setattr(args, var, env_val)
            else:
                sys.stderr.write('Missing alue for "%s", which is also missing from env!\n' % var)
                sys.stderr.flush()
                stop = True

    return args


if __name__ == '__main__':
    sys.exit(main(get_cmd_arguments()))
