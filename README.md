V-Ray For Houdini
=================

License
-------

    https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE


Binary Builds
-------------

Binary builds are available at "[V-Ray For Houdini](https://nightlies.chaosgroup.com/main#/vray4houdini)" section at http://nightlies.chaosgroup.com/.

You'll have to request access at support@chaosgroup.com for this section.

Build archive name description:

```
    vfh-701-078856b-hfs16.0.600-qt5-windows.zip
        |   |          |        |   │
        |   |          |        |   └─ OS
        |   |          |        └─ Qt version (optional, only for > 16.x)
        |   |          └─ Houdini version
        │   └─ Source revision
        └─ Incremental build number
```

Building From Source
--------------------

### Requirements

* **V-Ray Application SDK** nightly Qt build / license (write to support@chaosgroup.com)

* **V-Ray SDK** (for example, from **V-Ray For Maya** nightly build)

* **Phoenix SDK** (for example, from **Phoenix For Maya** nightly build; optional, used to load *.aur, *.vdb, *.f3d files preview)

* Compiler:

    - Windows: MSVC 2012 / MSVC 2015 for Houdini 15.5 and later
    - Linux: GCC 4.8.x
    - OS X: Any available

* CMake 3.3.x

* Ninja (> 1.5.x, optional)


### Installation

* Install V-Ray For Maya as usual (Maya itself is not needed)

* Install Phoenix For Maya and pass the SDK path as `PHXSDK_PATH` (Optional)

* Unpack V-Ray Application SDK into a directory, and pass the path to that directory as `APPSDK_PATH` to cmake

* Clone this repository with submodules:

  ```
  git clone https://github.com/ChaosGroup/vray-for-houdini.git
  cd vray-for-houdini
  git submodule update --init --recursive
  git submodule foreach git checkout master
  git submodule foreach git pull --rebase origin master
  ```

* Choose build directory on top of the source directory and generate a suitable project.

  For example, sources are cloned into `${HOME}/dev/vray-for-houdini`. Create build directory `${HOME}/build/vray-for-houdini` and run inside:

  ```
  cmake -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DHOUDINI_VERSION=15.0 \
    -DHOUDINI_VERSION_BUILD=459 \
    -DAPPSDK_PATH=${HOME}/src/appsdk/20161115/linux \
    -DVRAYSDK_PATH=/usr/ChaosGroup/V-Ray/Maya2016-x64 \
    -DPHXSDK_PATH=/usr/ChaosGroup/PhoenixFD/Maya2016-x64 \
    ${HOME}/dev/vray-for-houdini
  ```

  This will generate a Ninja project; build it and install with: `ninja install`

  It'll build plugins (with icons, shelfs, etc).

  Houdini launch wrapper script with all needed environment variables will be generated at `${HOME}/bin/hfs` or `$ENV{USERPROFILE}/Desktop/hfs.bat`.

  Use it to launch Houdini.
