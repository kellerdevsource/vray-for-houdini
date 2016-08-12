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
_cgr_build_mode = os.environ['CGR_BUILD_MODE']
_cgr_build_type = os.environ['CGR_BUILD_TYPE']
_cgr_release_root = os.environ['CGR_RELEASE_ROOT']
_cgr_config_root = os.environ['CGR_CONFIG_ROOT']


def toCmakePath(path):
    return os.path.normpath(path).replace("\\", "/")


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
def get_utils_paths():
    return [
        "C:/Program Files (x86)/CMake/bin",
        "C:/Program Files/SlikSvn/bin",
        "C:/Program Files/Git/bin",
        "C:/Program Files (x86)/WinSCP",
    ]


def setup_msvc_2012():
    cgrSdkPath = os.environ['CGR_SDK']

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

        'PATH' : os.environ['PATH'].split(';') + 
            get_utils_paths() +
            [
                "{CGR_SDK}/msvs2012/bin/amd64",
                "{CGR_SDK}/msvs2012/bin",
                "{CGR_SDK}/msvs2012/PlatformSDK/bin/x64",
            ]
        ,
    }

    for var in env:
        os.environ[var] = ";".join(env[var]).format(CGR_SDK=cgrSdkPath)


def setup_msvc_2015():
    cgrSdkPath = os.environ['CGR_SDK']

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

        'PATH' : os.environ['PATH'].split(';') +
            get_utils_paths() +
            [
                "{CGR_SDK}/msvs2015/bin/amd64",
                "{CGR_SDK}/msvs2015/bin",
                "{CGR_SDK}/msvs2015/PlatformSDK/bin/x64",
            ]
        ,
    }

    os.environ['__MS_VC_INSTALL_PATH'] = "{CGR_SDK}/msvs2015"
    for var in env:
        os.environ[var] = ";".join(env[var]).format(CGR_SDK=cgrSdkPath)


def getArchiveExt():
    if sys.platform == 'win32':
        return "zip"
    return "tar.bz2"


def getPlatformSuffix():
    if sys.platform == 'win32':
        return "windows"
    elif sys.platform == 'linux':
        return "linux"
    return "osx"


def main(args):
    srcHash = args.src_hash[:7]

    ReleaseDir = os.path.expanduser("%s/vray_for_houdini/%s" % (_cgr_release_root, sys.platform))
    ReleaseArchive = os.path.join(ReleaseDir, "vfh-{BUILD_NUMBER}-{SRC_GIT_HASH}-hfs{HOUDINI_VERSION}.{HOUDINI_VERSION_BUILD}-{OS}{DEBUG}.{EXT}".format(
        SRC_GIT_HASH=srcHash,
        HOUDINI_VERSION=os.environ['CGR_HOUDINI_VERSION'],
        HOUDINI_VERSION_BUILD=os.environ['CGR_HOUDINI_VERSION_BUILD'],
        EXT=getArchiveExt(),
        OS=getPlatformSuffix(),
        BUILD_NUMBER=os.environ['BUILD_NUMBER'],
        DEBUG="-dbg" if _cgr_build_type == "Debug" else "",
    ))
    SdkPath = os.environ.get('CGR_SDKPATH', "")

    cmake = ["cmake"]
    cmake.append('-GNinja')

    if sys.platform == 'linux':
        cmake.append('-DCMAKE_CXX_COMPILER=%s' % os.environ.get('CGR_CXX_COMPILER', "/usr/bin/g++4.8"))
        cmake.append('-DWITH_STATIC_LIBC=ON')

    cmake.append('-DCMAKE_BUILD_TYPE=%s' % _cgr_build_type)
    cmake.append('-DSDK_PATH=%s'              % toCmakePath(SdkPath))
    cmake.append('-DHOUDINI_VERSION=%s'       % os.environ['CGR_HOUDINI_VERSION'])
    cmake.append('-DHOUDINI_VERSION_BUILD=%s' % os.environ['CGR_HOUDINI_VERSION_BUILD'])
    cmake.append('-DAPPSDK_VERSION=%s'        % os.environ['CGR_APPSDK_VERSION'])
    cmake.append('-DPHXSDK_VERSION=%s'        % os.environ['CGR_PHXSDK_VERSION'])

    if sys.platform == 'win32':
        houdiniMajorVer = float(os.environ['CGR_HOUDINI_VERSION'])
        if houdiniMajorVer >= 15.5:
            cmake.append('-DMSVC_VERSION=1900')
            setup_msvc_2015()
        else:
            cmake.append('-DMSVC_VERSION=1800')
            setup_msvc_2012()

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
        buildPath = os.path.join(_cgr_config_root, "build")
        if  os.path.exists(buildPath):
            shutil.rmtree(buildPath,ignore_errors=True)

    err = call(cmake)

    if not err:
        err = call(ninja)

    if not err:
        if args.upload:
            upload(ReleaseArchive)

    sys.stdout.write("##teamcity[setParameter name='env.CGR_ARTIFACTS' value='%s']" % (ReleaseArchive))
    sys.stdout.flush()

    return err


if __name__ == '__main__':
    parser = argparse.ArgumentParser(usage="python3 teamcity.py [options]")
    parser.add_argument('--src_hash', default="unknown", help="Sources hash")
    parser.add_argument('--src_dir', default=False, help="Sources directory")
    parser.add_argument('--upload', action='store_true', default=False, help="Upload build")
    parser.add_argument('--clean', action='store_true', default=False, help="Delete build directory")

    sys.exit(main(parser.parse_args()))
