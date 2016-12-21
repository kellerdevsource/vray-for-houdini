#
# Copyright (c) 2015-2016, Chaos Software Ltd
#
# V-Ray For Houdini
#
# ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
#
# Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
#

import os
import sys
import shutil
import argparse
import subprocess

import teamcity as TC


def _get_cmd_output_ex(cmd, workDir=None):
    pwd = os.getcwd()
    if workDir:
        os.chdir(workDir)

    res = "None"
    code = 0
    if hasattr(subprocess, "check_output"):
        try:
            res = subprocess.check_output(cmd)
        except subprocess.CalledProcessError as e:
            code = e.returncode
            res = e.output
    else:
        proc = subprocess.Popen(cmd, stdout=subprocess.PIPE)
        res = proc.communicate()[0]
        code = proc.returncode
    res = res.decode().strip(" \n\r\t")

    if workDir:
        os.chdir(pwd)

    return {'code': code, 'output': res}


def _get_cmd_output(cmd, workDir=None):
    return _get_cmd_output_ex(cmd, workDir)['output']


def get_git_head_hash(root):
    git_rev = ['git', 'rev-parse', '--short', 'HEAD']
    return _get_cmd_output(git_rev, root)


def get_git_repourl(repo_rootdir):
    repo_out = _get_cmd_output('git remote -v'.split(), repo_rootdir)
    repo_out = repo_out.split()
    return repo_out[1] if len(repo_out) > 1 else ""


def remove_directory(path):
    # Don't know why, but when deleting from python
    # on Windows it fails to delete '.git' direcotry,
    # so using shell command
    sys.stdout.write("Deleting path: {}\n".format(path))

    if TC.getPlatformSuffix() == 'windows':
        os.system("rmdir /Q /S %s" % path)
        # Well yes, on Windows one remove is not enough...
        if os.path.exists(path):
            os.system("rmdir /Q /S %s" % path)
    else:
        shutil.rmtree(path)


def get_repo(repo_url, branch='master', target_dir=None, target_name=None, submodules=[]):
    """
    This will clone the repo in CWD. If target_dir != None it will copy 
    the sources to target_dir"""

    sys.stdout.write("Repo [%s]\n" % repo_url)
    sys.stdout.flush()

    repo_name = target_name if target_name else os.path.basename(repo_url)
    cwd = os.getcwd()
    clone_dir = os.path.join(cwd, repo_name)

    if os.path.exists(clone_dir):
        my_repo_url = get_git_repourl(clone_dir)
        if my_repo_url != repo_url:
            sys.stdout.write("Repo dir already exists but repo url is different {} -> {}\n".format(my_repo_url, repo_url))
            remove_directory(clone_dir)

    if not os.path.exists(clone_dir):
        if target_name and not target_dir:
            # just rename clone
            os.system("git clone %s %s" % (repo_url, target_name))
        else:
            os.system("git clone %s" % repo_url)
        os.chdir(clone_dir)
        os.system("git pull origin %s" % branch)
        os.system("git checkout %s" % branch)
    else:
        os.chdir(clone_dir)
        os.system("git checkout -- .")
        os.system("git pull origin %s" % branch)

    for module in submodules:
        os.system("git submodule update --init --remote --recursive %s" % module)

    if target_dir:
        to_dir = os.path.join(target_dir, repo_name)
        if target_name:
            to_dir = os.path.join(target_dir, target_name)

        sys.stdout.write("Exporting sources %s -> %s\n" % (clone_dir, to_dir))
        sys.stdout.flush()

        if os.path.exists(to_dir):
            remove_directory(to_dir)

        shutil.copytree(clone_dir, to_dir)

    os.chdir(cwd)


if __name__ == '__main__':
    build_dir = os.getcwd()
    parser = argparse.ArgumentParser(usage="python3 teamcity.py [options]")
    parser.add_argument('--perm_dir', required=True, help="Directory for permenents (libs)")
    parser.add_argument('--output_dir', required=True, help="Directory for output files")
    parser.add_argument('--libs_repo', required=True, help="Git repo for libs needed for build")

    parser.add_argument('--build_type', required=True, help="release or debug", choices=['debug', 'release'])
    parser.add_argument('--build_number', help="Current build number, defaults to 42", default='42')
    parser.add_argument('--appsdk_version', help="Appsdk version", default='20160510')

    parser.add_argument('--cgr_houdini_version', default='15.0', help="Houdini target version")
    parser.add_argument('--cgr_houdini_version_build', default='459', help="Houdini target build version")
    parser.add_argument('--cgr_phxsdk_version', default='2016_22501', help="Phoenix SDK version")
    parser.add_argument('--cgr_vraysdk_version', default='2016', help="VRay SDK version")

    args = parser.parse_args()


    if not os.path.exists(args.output_dir):
        sys.stdout.write("Path output_dir == [%s] missing, trying to create." % args.output_dir)
        sys.stdout.flush()
        os.makedirs(args.output_dir)

    perm_dir = os.path.join(args.perm_dir, 'houdini-dependencies')
    if not os.path.exists(perm_dir):
        sys.stdout.write("Path perm_dir/houdini-dependencies == [%s] missing, trying to create." % perm_dir)
        sys.stdout.flush()
        os.makedirs(perm_dir)

    os.chdir(perm_dir)
    get_repo(args.libs_repo, target_name='vray_for_houdini_sdk')
    os.chdir(build_dir)

    source_path = os.path.dirname(os.path.dirname(sys.path[0]))
    source_hash = get_git_head_hash(source_path)

    os_name = {
        'linux' : 'linux',
        'osx' : 'mac',
        'windows' : 'win'
    }[TC.getPlatformSuffix()]

    if TC.getPlatformSuffix() == 'windows':
        ninja_path = os.path.join(os.environ['VRAY_CGREPO_PATH'], 'build_scripts', 'cmake', 'tools', 'bin')
    else:
        ninja_path = os.path.join(os.environ['CI_ROOT'], 'ninja', 'ninja')
    sys.stdout.write('Ninja path [%s]\n' % ninja_path)
    sys.stdout.flush()
    os.environ['PATH'] = ninja_path + os.pathsep + os.environ['PATH']


    class ArgsReplacement():
        args = {
            'jenkins': True,
            'src_hash': source_hash,
            'src_dir': source_path,
            'upload': False,
            'clean': False,
            'BUILD_NUMBER': args.build_number,
            'CGR_APPSDK_VERSION': args.appsdk_version,
            'CGR_BUILD_MODE': 'nightly',
            'CGR_BUILD_TYPE': args.build_type,
            'CGR_CONFIG_ROOT': perm_dir,
            'CGR_HOUDINI_VERSION': args.cgr_houdini_version,
            'CGR_HOUDINI_VERSION_BUILD': args.cgr_houdini_version_build,
            'CGR_PHXSDK_VERSION': args.cgr_phxsdk_version,
            'CGR_VRAYSDK_VERSION': args.cgr_vraysdk_version,
            'CGR_RELEASE_ROOT': os.path.join(args.output_dir, 'release'),
            'CGR_SDK': os.path.join(os.environ['VRAY_CGREPO_PATH'], 'sdk', os_name),
            'CGR_SDKPATH': os.path.join(perm_dir, 'vray_for_houdini_sdk'),
        }

        def __getattr__(self, attr):
            if attr in self.args:
                return self.args[attr]
            sys.stderr.write('Missing argument "%s".' % attr)
            sys.stderr.flush()
            sys.exit(-1)

    args = ArgsReplacement()
    if TC.getPlatformSuffix() == 'linux':
        args.CGR_CXX_COMPILER = 'g++'
        args.CGR_C_COMPILER = 'gcc'

    TC.main(args)
