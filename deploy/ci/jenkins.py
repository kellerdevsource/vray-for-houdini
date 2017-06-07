#
# Copyright (c) 2015-2016, Chaos Software Ltd
#
# V-Ray For Houdini
#
# ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
#
# Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
#

# # Build mode: 'nightly', 'stable', 'debug'
# _cgr_build_mode = None
# _cgr_build_type = None
# _cgr_release_root = None
# _cgr_config_root = None

# def main(args):
#     # lets polute global space
#     global _cgr_build_mode
#     global _cgr_build_type
#     global _cgr_release_root
#     global _cgr_config_root

#     _cgr_build_mode = args.CGR_BUILD_MODE
#     _cgr_build_type = args.CGR_BUILD_TYPE.title()
#     _cgr_release_root = args.CGR_RELEASE_ROOT
#     _cgr_config_root = args.CGR_CONFIG_ROOT

#     set_utils_paths()
#     srcHash = args.src_hash[:7]

#     ReleaseDir = os.path.expanduser("%s/vray_for_houdini/%s" % (_cgr_release_root, sys.platform))
#     ReleaseArchive = os.path.join(ReleaseDir, "vfh-{BUILD_NUMBER}-{SRC_GIT_HASH}-hfs{HOUDINI_VERSION}.{HOUDINI_VERSION_BUILD}-{OS}{DEBUG}.{EXT}".format(
#         SRC_GIT_HASH=srcHash,
#         HOUDINI_VERSION=args.CGR_HOUDINI_VERSION,
#         HOUDINI_VERSION_BUILD=args.CGR_HOUDINI_VERSION_BUILD,
#         EXT=getArchiveExt(),
#         OS=getPlatformSuffix(),
#         BUILD_NUMBER=args.BUILD_NUMBER,
#         DEBUG="-dbg" if _cgr_build_type == "Debug" else "",
#     ))
#     SdkPath = args.CGR_SDKPATH

#     ninja_path = os.path.dirname(which('ninja'))

#     cmake = [which('cmake')]
#     cmake.append('-GNinja')

#     old_path = os.environ['PATH']
#     if args.jenkins and getPlatformSuffix() == 'windows':
#         # clear all possible conflicts from path
#         os.environ['PATH'] = os.pathsep.join([ninja_path, os.path.dirname(which('git')), os.path.dirname(which('7z'))])

#     if sys.platform == 'win32':
#         houdiniMajorVer = float(args.CGR_HOUDINI_VERSION)
#         if houdiniMajorVer >= 15.5:
#             cmake.append('-DMSVC_VERSION=1900')
#             setup_msvc_2015(args.CGR_SDK)
#         else:
#             cmake.append('-DMSVC_VERSION=1700')
#             setup_msvc_2012(args.CGR_SDK)

#     if sys.platform == 'linux':
#         cmake.append('-DCMAKE_CXX_COMPILER=%s' % args.CGR_CXX_COMPILER)
#         cmake.append('-DWITH_STATIC_LIBC=ON')

#     cmake.append('-DCMAKE_BUILD_TYPE=%s' % _cgr_build_type)
#     cmake.append('-DSDK_PATH=%s'              % toCmakePath(SdkPath))
#     cmake.append('-DHOUDINI_VERSION=%s'       % args.CGR_HOUDINI_VERSION)
#     cmake.append('-DHOUDINI_VERSION_BUILD=%s' % args.CGR_HOUDINI_VERSION_BUILD)
#     cmake.append('-DAPPSDK_VERSION=%s'        % args.CGR_APPSDK_VERSION)
#     cmake.append('-DPHXSDK_VERSION=%s'        % args.CGR_PHXSDK_VERSION)
#     cmake.append('-DVRAYSDK_VERSION=%s'       % args.CGR_VRAYSDK_VERSION)
#     cmake.append('-DCGR_SRC_HASH=%s' % srcHash)
#     cmake.append('-DINSTALL_LOCAL=OFF')
#     cmake.append('-DINSTALL_RELEASE=ON')
#     cmake.append('-DINSTALL_RELEASE_ROOT=%s' % toCmakePath(ReleaseDir))
#     if _cgr_build_type == "Debug":
#         cmake.append('-DINSTALL_RELEASE_SUFFIX=-dbg')
#     cmake.append('-DINSTALL_RELEASE_ARCHIVE_FILEPATH=%s' % toCmakePath(ReleaseArchive))
#     cmake.append(args.src_dir)

#     ninja = ["ninja"]
#     ninja.append("install")

#     if args.clean:
#         # clean build dir
#         cleanDir(os.getcwd())

#     sys.stdout.write('PATH:\n\t%s\n' % '\n\t'.join(os.environ['PATH'].split(os.pathsep)))
#     sys.stdout.write('cmake args:\n%s\n' % '\n\t'.join(cmake))
#     sys.stdout.flush()

#     err = call(cmake)

#     if not err:
#         err = call(ninja)

#     os.environ['PATH'] = old_path
#     if not err:
#         if args.upload:
#             upload(ReleaseArchive)

#         sys.stdout.write("##teamcity[setParameter name='env.CGR_ARTIFACTS' value='%s']" % (ReleaseArchive))
#         sys.stdout.flush()

#     return err


# def get_cmd_arguments():
#     parser = argparse.ArgumentParser(usage="python3 teamcity.py [options]")
#     parser.add_argument('--src_hash', default="unknown", help="Sources hash")
#     parser.add_argument('--src_dir', default=False, help="Sources directory")
#     parser.add_argument('--upload', action='store_true', default=False, help="Upload build")
#     parser.add_argument('--clean', action='store_true', default=False, help="Clean build directory")
#     parser.add_argument('--jenkins', action='store_true', default=False, help=argparse.SUPPRESS)

#     if sys.platform == 'linux':
#         parser.add_argument('--CGR_CXX_COMPILER', default='/usr/bin/g++4.8', help="Compiler location")

#     env_vars = [
#         'BUILD_NUMBER', 'CGR_APPSDK_VERSION', 'CGR_BUILD_MODE', 'CGR_BUILD_TYPE',
#         'CGR_CONFIG_ROOT', 'CGR_HOUDINI_VERSION', 'CGR_HOUDINI_VERSION_BUILD',
#         'CGR_PHXSDK_VERSION', 'CGR_RELEASE_ROOT', 'CGR_SDK', 'CGR_VRAYSDK_VERSION'
#     ]

#     default = 'UNSET_DEFAULT'

#     for var in env_vars:
#         parser.add_argument('--%s' % var, default=default, help='Optional if not provided will be read from env.')

#     args = parser.parse_args()

#     stop = False
#     for var in env_vars:
#         value = getattr(args, var)
#         if value == default:
#             if var in os.environ:
#                 env_val = os.environ[var]
#                 sys.stdout.write('Missing value for "%s", reading from environ [%s]\n' % (var, env_val))
#                 sys.stdout.flush()
#                 setattr(args, var, env_val)
#             else:
#                 sys.stderr.write('Missing alue for "%s", which is also missing from env!\n' % var)
#                 sys.stderr.flush()
#                 stop = True

#     if stop:
#         sys.exit(-1)

#     return args


# if __name__ == '__main__':
#     sys.exit(main(get_cmd_arguments()))



# def _get_cmd_output_ex(cmd, workDir=None):
#     pwd = os.getcwd()
#     if workDir:
#         os.chdir(workDir)

#     res = "None"
#     code = 0
#     if hasattr(subprocess, "check_output"):
#         try:
#             res = subprocess.check_output(cmd)
#         except subprocess.CalledProcessError as e:
#             code = e.returncode
#             res = e.output
#     else:
#         proc = subprocess.Popen(cmd, stdout=subprocess.PIPE)
#         res = proc.communicate()[0]
#         code = proc.returncode
#     res = res.decode().strip(" \n\r\t")

#     if workDir:
#         os.chdir(pwd)

#     return {'code': code, 'output': res}


# def _get_cmd_output(cmd, workDir=None):
#     return _get_cmd_output_ex(cmd, workDir)['output']


# def get_git_head_hash(root):
#     git_rev = ['git', 'rev-parse', '--short', 'HEAD']
#     return _get_cmd_output(git_rev, root)


# def get_git_repourl(repo_rootdir):
#     repo_out = _get_cmd_output('git remote -v'.split(), repo_rootdir)
#     repo_out = repo_out.split()
#     return repo_out[1] if len(repo_out) > 1 else ""

# def get_repo(repo_url, branch='master', target_dir=None, target_name=None, submodules=[]):
#     """
#     This will clone the repo in CWD. If target_dir != None it will copy 
#     the sources to target_dir"""

#     log_msg("Repo [%s]\n" % repo_url)

#     repo_name = target_name if target_name else os.path.basename(repo_url)
#     cwd = os.getcwd()
#     clone_dir = os.path.join(cwd, repo_name)

#     if os.path.exists(clone_dir):
#         my_repo_url = get_git_repourl(clone_dir)
#         if my_repo_url != repo_url:
#             log_msg("Repo dir already exists but repo url is different {} -> {}\n".format(my_repo_url, repo_url))
#             remove_directory(clone_dir)

#     if not os.path.exists(clone_dir):
#         if target_name and not target_dir:
#             # just rename clone
#             os.system("git clone %s %s" % (repo_url, target_name))
#         else:
#             os.system("git clone %s" % repo_url)
#         os.chdir(clone_dir)
#         os.system("git pull origin %s" % branch)
#         os.system("git checkout %s" % branch)
#     else:
#         os.chdir(clone_dir)
#         os.system("git checkout -- .")
#         os.system("git pull origin %s" % branch)

#     for module in submodules:
#         os.system("git submodule update --init --remote --recursive %s" % module)

#     if target_dir:
#         to_dir = os.path.join(target_dir, repo_name)
#         if target_name:
#             to_dir = os.path.join(target_dir, target_name)

#         log_msg("Exporting sources %s -> %s\n" % (clone_dir, to_dir))

#         if os.path.exists(to_dir):
#             remove_directory(to_dir)

#         shutil.copytree(clone_dir, to_dir)

#     os.chdir(cwd)


# if __name__ == '__main__':
#     build_dir = os.getcwd()
#     parser = argparse.ArgumentParser(usage="python3 teamcity.py [options]")
#     parser.add_argument('--perm_dir', required=True, help="Directory for permenents (libs)")
#     parser.add_argument('--output_dir', required=True, help="Directory for output files")
#     parser.add_argument('--libs_repo', required=True, help="Git repo for libs needed for build")

#     parser.add_argument('--build_type', required=True, help="release or debug", choices=['debug', 'release'])
#     parser.add_argument('--build_number', help="Current build number, defaults to 42", default='42')
#     parser.add_argument('--appsdk_version', help="Appsdk version", default='20160510')

#     parser.add_argument('--cgr_houdini_version', default='15.0', help="Houdini target version")
#     parser.add_argument('--cgr_houdini_version_build', default='459', help="Houdini target build version")
#     parser.add_argument('--cgr_phxsdk_version', default='2016_22501', help="Phoenix SDK version")
#     parser.add_argument('--cgr_vraysdk_version', default='2016', help="VRay SDK version")

#     args = parser.parse_args()


#     if not os.path.exists(args.output_dir):
#         log_msg("Path output_dir == [%s] missing, trying to create." % args.output_dir)
#         os.makedirs(args.output_dir)

#     perm_dir = os.path.join(args.perm_dir, 'houdini-dependencies')
#     if not os.path.exists(perm_dir):
#         log_msg("Path perm_dir/houdini-dependencies == [%s] missing, trying to create." % perm_dir)
#         os.makedirs(perm_dir)

#     os.chdir(perm_dir)
#     get_repo(args.libs_repo, target_name='vray_for_houdini_sdk')
#     os.chdir(build_dir)

#     source_path = os.path.dirname(os.path.dirname(sys.path[0]))
#     source_hash = get_git_head_hash(source_path)

#     os_name = {
#         'linux' : 'linux',
#         'osx' : 'mac',
#         'windows' : 'win'
#     }[TC.getPlatformSuffix()]

#     if TC.getPlatformSuffix() == 'windows':
#         ninja_path = os.path.join(os.environ['VRAY_CGREPO_PATH'], 'build_scripts', 'cmake', 'tools', 'bin')
#     else:
#         ninja_path = os.path.join(os.environ['CI_ROOT'], 'ninja', 'ninja')
#     log_msg('Ninja path [%s]\n' % ninja_path)
#     os.environ['PATH'] = ninja_path + os.pathsep + os.environ['PATH']

#     class ArgsReplacement():
#         args = {
#             'jenkins': True,
#             'src_hash': source_hash,
#             'src_dir': source_path,
#             'upload': False,
#             'clean': False,
#             'BUILD_NUMBER': args.build_number,
#             'CGR_APPSDK_VERSION': args.appsdk_version,
#             'CGR_BUILD_MODE': 'nightly',
#             'CGR_BUILD_TYPE': args.build_type,
#             'CGR_CONFIG_ROOT': perm_dir,
#             'CGR_HOUDINI_VERSION': args.cgr_houdini_version,
#             'CGR_HOUDINI_VERSION_BUILD': args.cgr_houdini_version_build,
#             'CGR_PHXSDK_VERSION': args.cgr_phxsdk_version,
#             'CGR_VRAYSDK_VERSION': args.cgr_vraysdk_version,
#             'CGR_RELEASE_ROOT': os.path.join(args.output_dir, 'release'),
#             'CGR_SDK': os.path.join(os.environ['VRAY_CGREPO_PATH'], 'sdk', os_name),
#             'CGR_SDKPATH': os.path.join(perm_dir, 'vray_for_houdini_sdk'),
#         }

#         def __getattr__(self, attr):
#             if attr in self.args:
#                 return self.args[attr]
#             sys.stderr.write('Missing argument "%s".' % attr)
#             sys.stderr.flush()
#             sys.exit(-1)

#     args = ArgsReplacement()
#     if TC.getPlatformSuffix() == 'linux':
#         args.CGR_CXX_COMPILER = 'g++'
#         args.CGR_C_COMPILER = 'gcc'

#     # get latest master of submodules
#     os.system("git submodule foreach git clean -dxf")
#     os.system("git submodule foreach git fetch")
#     os.system('git submodule foreach git reset --hard origin/master')

#     TC.main(args)

# --output_dir=%VFH_OUTPUT_DIR% ^
# --perm_dir=%JVRAYPATH%/houdini ^
# --cgr_houdini_version=%cgr_houdini_version% ^
# --cgr_houdini_version_build=%cgr_houdini_version_build% ^
# --cgr_phxsdk_version=%cgr_phxsdk_version% ^
# --cgr_vraysdk_version=%cgr_vraysdk_version% ^
# --appsdk_version=%appsdk_version% ^
# --build_type=release ^
# --build_number=%BUILD_NUMBER%

if __name__ == '__main__':
    import argparse
    import datetime
    import os
    import shutil
    import subprocess
    import sys
    import tempfile

    from vfh_ci import config
    from vfh_ci import log
    from vfh_ci import compiler
    from vfh_ci import run

    compiler.setup_compiler(config.HOUDINI_VERSION, config.KDRIVE_DIR)

    # Reset submodules to origin/master
    run.call("git submodule foreach git clean -dxfq")
    run.call("git submodule foreach git fetch")
    run.call("git submodule foreach git reset --hard origin/master")
