#
# Copyright (c) 2015-2017, Chaos Software Ltd
#
# V-Ray For Houdini
#
# ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
#
# Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
#

if __name__ == '__main__':
    import os
    import sys

    from vfh_ci import config
    from vfh_ci import log
    from vfh_ci import compiler
    from vfh_ci import run
    from vfh_ci import utils

    compiler.setup_compiler(config.HOUDINI_VERSION, config.KDRIVE_DIR)

    nameArgs = {
        'SRC_GIT_HASH' : config.SOURCE_HASH,
        'HOUDINI_VERSION' : config.HOUDINI_VERSION,
        'HOUDINI_VERSION_BUILD' : config.HOUDINI_VERSION_BUILD,
        'EXT' : utils.getArchiveExt(),
        'OS' : utils.getPlatform(),
        'BUILD_NUMBER' : config.BUILD_NUMBER,
        'DEBUG' : "-dbg" if config.CMAKE_BUILD_TYPE == "Debug" else "",
        'QT' : "-qt%s" % (config.HOUDINI_QT_VERSION) if config.HOUDINI_VERSION >= 16.0 else "",
    }

    archiveFilename = config.OUTPUT_FILE_FMT.format(**nameArgs)
    log.message("Archive filename: %s" % (archiveFilename))

    archiveFilePath = utils.p(config.OUTPUT_DIR, archiveFilename)
    log.message("Archive directory: %s" % (config.OUTPUT_DIR))

    # Clean up old repository
    utils.removeDir(utils.p(config.PERMANENT_DIR, "houdini-dependencies"))

    log.message("Resetting submodules to origin/master...")
    run.call("git submodule foreach git clean -dxfq", config.SOURCE_DIR)
    run.call("git submodule foreach git fetch", config.SOURCE_DIR)
    run.call("git submodule foreach git reset --hard origin/master", config.SOURCE_DIR)

    log.message("Clone / update SDK...")
    if os.path.exists(config.VFH_SDK_DIR):
        run.call("git clean -dxfq", config.VFH_SDK_DIR)
        run.call("git fetch", config.VFH_SDK_DIR)
        run.call("git reset --hard origin/master", config.VFH_SDK_DIR)

        # Needed after SDK history reset
        # run.call("git gc --aggressive --prune=all", config.VFH_SDK_DIR)
    else:
        if not os.path.exists(config.VFH_SDK_DIR):
            os.makedirs(config.VFH_SDK_DIR)
        run.call("git clone %s %s" % (config.VFH_SDK_REPO, config.VFH_SDK_DIR), config.PERMANENT_DIR)

    log.message("Cleaning build directory...")
    utils.cleanDir(config.BUILD_DIR)
    if not os.path.exists(config.BUILD_DIR):
        os.makedirs(config.BUILD_DIR)

    log.message("Configuring the project...")
    cmake = ['cmake']
    cmake.append('-GNinja')
    cmake.append('-DCMAKE_BUILD_TYPE=%s' % config.CMAKE_BUILD_TYPE)

    if utils.getPlatform() in {'windows'}:
        if config.HOUDINI_VERSION >= 15.5:
            cmake.append('-DMSVC_VERSION=1900')
        else:
            cmake.append('-DMSVC_VERSION=1700')
    elif utils.getPlatform() in {'linux'}:
        cmake.append('-DWITH_STATIC_LIBC=ON')
        cmake.append('-DCMAKE_CXX_COMPILER=g++482')
        cmake.append('-DCMAKE_C_COMPILER=gcc482')

    cmake.append('-DSDK_PATH=%s' % utils.p(config.VFH_SDK_DIR))
    cmake.append('-DHOUDINI_VERSION=%s' % config.HOUDINI_VERSION)
    cmake.append('-DHOUDINI_VERSION_BUILD=%s' % config.HOUDINI_VERSION_BUILD)
    cmake.append('-DHOUDINI_QT_VERSION=%s' % config.HOUDINI_QT_VERSION)
    cmake.append('-DCGR_SRC_HASH=%s' % config.SOURCE_HASH)
    cmake.append('-DINSTALL_RELEASE=ON')
    cmake.append('-DINSTALL_RELEASE_ROOT=%s' % utils.p(config.OUTPUT_DIR))
    if config.CMAKE_BUILD_TYPE in {"Debug"}:
        cmake.append('-DINSTALL_RELEASE_SUFFIX=-dbg')
    cmake.append('-DINSTALL_RELEASE_ARCHIVE_FILEPATH=%s' % utils.p(archiveFilePath))
    cmake.append(config.SOURCE_DIR)

    err = run.call(cmake, config.BUILD_DIR)
    if err:
        sys.exit(err)

    log.message("Building the project...")
    ninja = ["ninja", "install"]

    err = run.call(ninja, config.BUILD_DIR)
    if err:
        sys.exit(err)
