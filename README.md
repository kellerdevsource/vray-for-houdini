V-Ray For Houdini
=================

License
-------

    https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE


Binary Builds
-------------

Binary builds are available at "[V-Ray For Houdini](https://nightlies.chaosgroup.com/main#/vray4houdini)" section at http://nightlies.chaosgroup.com/.

You'll have to request access at support@chaosgroup.com for this section.


Building From Source
--------------------

### Requirements

* **V-Ray Application SDK** nightly Qt build / license (write to support@chaosgroup.com)

* **V-Ray SDK** (for example, from **V-Ray For Maya** nightly build)

* **Phoenix SDK** (for example, from **Phonix For Maya** nightly build; optional, used to load *.aur files preview)

* Compiler:

    - Windows: MSVC 2012
    - Linux: GCC 4.8.x
    - OS X: Any available

* CMake 3.3.x

* Ninja (> 1.5.x, optional)


### Installation

* Install V-Ray For Maya as usual (Maya itself is not needed)

* Unpack V-Ray Application SDK into the directory with structure `<SomeRoot>/{APPSDK_VERSION}/{APPSDK_PLATFORM}`

  For example, `{HOME}/src/appsdk/507/linux`

  `"{HOME}/src/appsdk"` is controlled with CMake variable **APPSDK_PATH**

  `"507"` is controlled with CMake variable **APPSDK_VERSION**

  `{linux, windows, darwin}` is appended automatically by CMake scripts.

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
    -DHOUDINI_VERSION=14.0 \
    -DHOUDINI_VERSION_BUILD=361 \
    -DAPPSDK_PATH=${HOME}/src/appsdk \
    -DAPPSDK_VERSION=507 \
    ${HOME}/dev/vray-for-houdini
  ```

  If you are using Qt version of AppSDK (preffered) also add `-DAPPSDK_QT=ON`.

  This will generate a Ninja project; build it and install with: `ninja install`

  It'll build and install plugins (with icons, shelfs, etc) into the default Houdini's user settings directory.

  Houdini launch wrapper script with all needed environment variables will be generated at `${HOME}/bin/hfs`.

  Use it to launch Houdini (or copy variable definitions to your environment).
