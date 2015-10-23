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
import os
import subprocess
import sys


def setup_msvc_2012():
    # Setup Visual Studio 2012 variables for usage from command line
    # Assumes default installation path
    #
    PATH = os.environ['PATH'].split(';')
    PATH.extend([
        r'C:\Program Files (x86)\Microsoft Visual Studio 12.0\Common7\IDE\CommonExtensions\Microsoft\TestWindow',
        r'C:\Program Files (x86)\MSBuild\12.0\bin\amd64',
        r'C:\Program Files (x86)\Microsoft Visual Studio 12.0\VC\BIN\amd64',
        r'C:\Windows\Microsoft.NET\Framework64\v4.0.30319',
        r'C:\Program Files (x86)\Microsoft Visual Studio 12.0\VC\VCPackages',
        r'C:\Program Files (x86)\Microsoft Visual Studio 12.0\Common7\IDE',
        r'C:\Program Files (x86)\Microsoft Visual Studio 12.0\Common7\Tools',
        r'C:\Program Files (x86)\HTML Help Workshop',
        r'C:\Program Files (x86)\Microsoft Visual Studio 12.0\Team Tools\Performance Tools\x64',
        r'C:\Program Files (x86)\Microsoft Visual Studio 12.0\Team Tools\Performance Tools',
        r'C:\Program Files (x86)\Windows Kits\8.1\bin\x64',
        r'C:\Program Files (x86)\Windows Kits\8.1\bin\x86',
        r'C:\Program Files (x86)\Microsoft SDKs\Windows\v8.1A\bin\NETFX 4.5.1 Tools\x64',
        r'C:\ProgramData\Oracle\Java\javapath',
        r'C:\Windows\system32',
        r'C:\Windows',
        r'C:\Windows\System32\Wbem',
        r'C:\Windows\System32\WindowsPowerShell\v1.0',
        r'C:\Program Files (x86)\Windows Kits\8.1\Windows Performance Toolkit',
        r'C:\Program Files\Microsoft SQL Server\110\Tools\Binn',
    ])

    INCLUDE = (
        r'C:\Program Files (x86)\Microsoft Visual Studio 12.0\VC\INCLUDE',
        r'C:\Program Files (x86)\Microsoft Visual Studio 12.0\VC\ATLMFC\INCLUDE',
        r'C:\Program Files (x86)\Windows Kits\8.1\include\shared',
        r'C:\Program Files (x86)\Windows Kits\8.1\include\um',
        r'C:\Program Files (x86)\Windows Kits\8.1\include\winrt',
    )

    LIB = (
        r'C:\Program Files (x86)\Microsoft Visual Studio 12.0\VC\LIB\amd64',
        r'C:\ProgramFiles (x86)\Microsoft Visual Studio 12.0\VC\ATLMFC\LIB\amd64',
        r'C:\Program Files (x86)\Windows Kits\8.1\lib\winv6.3\um\x64',
    )

    LIBPATH = (
        r'C:\Windows\Microsoft.NET\Framework64\v4.0.30319',
        r'C:\Program Files (x86)\Microsoft Visual Studio 12.0\VC\LIB\amd64',
        r'C:\Program Files (x86)\Microsoft Visual Studio 12.0\VC\ATLMFC\LIB\amd64',
        r'C:\Program Files (x86)\Windows Kits\8.1\References\CommonConfiguration\Neutral',
        r'C:\Program Files (x86)\Microsoft SDKs\Windows\v8.1\ExtensionSDKs\Microsoft.VCLibs\12.0\References\CommonConfiguration\neutral',
    )

    os.environ['PATH']    = ";".join(PATH)
    os.environ['INCLUDE'] = ";".join(INCLUDE)
    os.environ['LIB']     = ";".join(LIB)
    os.environ['LIBPATH'] = ";".join(LIBPATH)


def main(args):
    cmake = ["cmake"]
    cmake.append('-G Ninja')

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

    cmake.append('-DCGR_SRC_HASH=%s' % args.src_hash)
    cmake.append(args.src_dir)

    err = subprocess.call(cmake, cwd=os.getcwd())

    if not err and args.upload:
        upload(filepath)

    return err


if __name__ == '__main__':
    parser = argparse.ArgumentParser(usage="python3 teamcity.py [options]")
    parser.add_argument('--src_hash', default="unknown", help="Sources hash")
    parser.add_argument('--src_dir', default=False, help="Sources directory")
    parser.add_argument('--upload', action='store_true', default=False, help="Upload build")

    sys.exit(main(parser.parse_args()))
