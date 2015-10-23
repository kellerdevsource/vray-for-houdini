#
# Copyright (c) 2015, Chaos Software Ltd
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
import subprocess
import sys
import tempfile


def upload(filepath):
    try:
        from configparser import ConfigParser
    except:
        from ConfigParser import ConfigParser

    config = ConfigParser()
    config.read(os.path.expanduser("~/.passwd"))

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
            f.write('put %s /%s/\n' % (filepath, subdir))
            f.write('exit\n')
            f.write('\n')

        cmd = ['winscp']
        cmd.append('/passive')
        cmd.append('/script="%s"' % ftpScriptFilepath)

        if not self.mode_test:
            os.system(' '.join(cmd))

    else:
        cmd = ['curl']
        cmd.append('--no-epsv')
        if self.use_proxy:
            cmd.append('--proxy')
            cmd.append(self.use_proxy)
        cmd.append('--user')
        cmd.append('%s:%s' % (
            config.get('vfh.nightlies.ftp', 'user'),
            config.get('vfh.nightlies.ftp', 'pass'),
        ))
        cmd.append('--upload-file')
        cmd.append(filepath)
        cmd.append('ftp://%s/%s/' % (
            config.get('vfh.nightlies.ftp', 'host'),
            subdir,
        ))

        if not self.mode_test:
            subprocess.call(cmd)


def setup_msvc_2012():
    # Setup Visual Studio 2012 variables for usage from command line
    # Assumes default installation path
    #
    PATH = os.environ['PATH'].split(';')
    PATH.extend([
        r'C:\Program Files (x86)\CMake\bin',
        r'C:\Program Files (x86)\HTML Help Workshop',
        r'C:\Program Files (x86)\Microsoft SDKs\Windows\v7.0A\Bin',
        r'C:\Program Files (x86)\Microsoft SDKs\Windows\v7.0A\Bin\x64',
        r'C:\Program Files (x86)\Microsoft SDKs\Windows\v8.0A\bin\NETFX 4.0 Tools\x64',
        r'C:\Program Files (x86)\Microsoft SDKs\Windows\v8.0A\bin\NETFX4.0 Tools',
        r'C:\Program Files (x86)\Microsoft Visual Studio 11.0\Common7\IDE',
        r'C:\Program Files (x86)\Microsoft Visual Studio 11.0\Common7\IDE\CommonExtensions\Microsoft\TestWindow',
        r'C:\Program Files (x86)\Microsoft Visual Studio 11.0\Common7\Tools',
        r'C:\Program Files (x86)\Microsoft Visual Studio 11.0\Team Tools\Performance Tools',
        r'C:\Program Files (x86)\Microsoft Visual Studio 11.0\Team Tools\Performance Tools\x64',
        r'C:\Program Files (x86)\Microsoft Visual Studio 11.0\VC\BIN\amd64',
        r'C:\Program Files (x86)\Microsoft Visual Studio 11.0\VC\VCPackages',
        r'C:\Program Files (x86)\NVIDIA Corporation\PhysX\Common',
        r'C:\Program Files (x86)\Windows Kits\8.0\bin\x64',
        r'C:\Program Files (x86)\Windows Kits\8.0\bin\x86',
        r'C:\Program Files (x86)\Windows Kits\8.1\Windows Performance Toolkit',
        r'C:\Program Files (x86)\WinSCP',
        r'C:\Program Files\Common Files\Autodesk Shared',
        r'C:\Program Files\Git\cmd',
        r'C:\Program Files\Git\mingw64\bin',
        r'C:\Program Files\Microsoft SQL Server\110\Tools\Binn',
        r'C:\Program Files\SlikSvn\bin',
        r'C:\ProgramData\Oracle\Java\javapath',
        r'C:\Users\bdancer\AppData\Local\atom\bin',
        r'C:\Windows',
        r'C:\Windows\Microsoft.NET\Framework64\v3.5',
        r'C:\Windows\Microsoft.NET\Framework64\v4.0.30319',
        r'C:\Windows\system32',
        r'C:\Windows\System32\Wbem',
        r'C:\Windows\System32\WindowsPowerShell\v1.0',
    ])

    INCLUDE = (
        r'C:\Program Files (x86)\Microsoft Visual Studio 11.0\VC\ATLMFC\INCLUDE',
        r'C:\Program Files (x86)\Microsoft Visual Studio 11.0\VC\INCLUDE',
        r'C:\Program Files (x86)\Windows Kits\8.0\include\shared',
        r'C:\Program Files (x86)\Windows Kits\8.0\include\um',
        r'C:\Program Files (x86)\Windows Kits\8.0\include\winrt',
    )

    LIB = (
        r'C:\Program Files (x86)\Microsoft Visual Studio 11.0\VC\ATLMFC\LIB\amd64',
        r'C:\Program Files (x86)\Microsoft Visual Studio 11.0\VC\LIB\amd64',
        r'C:\Program Files (x86)\Windows Kits\8.0\lib\win8\um\x64',
    )

    LIBPATH = (
        r'C:\Program Files (x86)\Microsoft SDKs\Windows\v8.0\ExtensionSDKs\Microsoft.VCLibs\11.0\References\CommonConfiguration\neutral',
        r'C:\Program Files (x86)\Microsoft Visual Studio 11.0\VC\ATLMFC\LIB\amd64',
        r'C:\Program Files (x86)\Microsoft Visual Studio 11.0\VC\LIB\amd64',
        r'C:\Program Files (x86)\Windows Kits\8.0\References\CommonConfiguration\Neutral',
        r'C:\Windows\Microsoft.NET\Framework64\v3.5',
        r'C:\Windows\Microsoft.NET\Framework64\v4.0.30319',
    )

    os.environ['PATH']    = ";".join(PATH)
    os.environ['INCLUDE'] = ";".join(INCLUDE)
    os.environ['LIB']     = ";".join(LIB)
    os.environ['LIBPATH'] = ";".join(LIBPATH)


def main(args):
    ReleaseDir = "H:/release/vray_for_houdini/%s" % sys.platform

    cmake = ["cmake"]
    cmake.append('-GNinja')
    cmake.append('-DCMAKE_BUILD_TYPE=%s' % os.environ['CGR_BUILD_TYPE'])
    cmake.append('-DHOUDINI_VERSION=%s'       % os.environ['CGR_HOUDINI_VERSION'])
    cmake.append('-DHOUDINI_VERSION_BUILD=%s' % os.environ['CGR_HOUDINI_VERSION_BUILD'])
    cmake.append('-DAPPSDK_VERSION=%s'        % os.environ['CGR_APPSDK_VERSION'])

    if sys.platform == 'linux':
        cmake.append('-DCMAKE_CXX_COMPILER=/usr/bin/g++-4.9.3')
    elif sys.platform == 'win32':
        setup_msvc_2012()
        cmake.append('-DAPPSDK_PATH=%s' % "H:/src/appsdk")
    else:
        pass
    cmake.append('-DCGR_SRC_HASH=%s' % args.src_hash[:7])
    cmake.append('-DINSTALL_RELEASE=ON')
    cmake.append('-DINSTALL_RELEASE_ROOT=%s' % ReleaseDir)
    cmake.append(args.src_dir)

    print("-- Calling: %s" % " ".join(cmake))

    err = subprocess.call(cmake, cwd=os.getcwd())

    if not err:
        if args.upload:
            upload(os.path.join(ReleaseDir, "vfh-{SRC_GIT_HASH}-hfs{HOUDINI_VERSION}.{HOUDINI_VERSION_BUILD}.zip".format(
                SRC_GIT_HASH=args.src_hash,
                HOUDINI_VERSION=os.environ['CGR_HOUDINI_VERSION'],
                HOUDINI_VERSION_BUILD=os.environ['CGR_HOUDINI_VERSION_BUILD'],
            )))

    return err


if __name__ == '__main__':
    parser = argparse.ArgumentParser(usage="python3 teamcity.py [options]")
    parser.add_argument('--src_hash', default="unknown", help="Sources hash")
    parser.add_argument('--src_dir', default=False, help="Sources directory")
    parser.add_argument('--upload', action='store_true', default=False, help="Upload build")

    sys.exit(main(parser.parse_args()))
