V-Ray For Houdini (Early Access)
================================

Compilation
-----------

### Requirements

* Obtain **V-Ray Application SDK** nightly build / license (write to support@chaosgroup.com)

* Obtain **V-Ray SDK** (for example, from **V-Ray For Maya** nightly build)

* Suitable compiler per platform

* CMake

* Ninja (optional)


### Installation

* Install V-Ray For Maya as usual (Maya itself is not needed)

* Unpack V-Ray Application SDK into the directory with structure `<SomeRoot>/{APPSDK_VERSION}/{APPSDK_PLATFORM}`

  For example, `{HOME}/src/appsdk/507/linux`

  `"{HOME}/src/appsdk"` is controlled with CMake variable **APPSDK_PATH**

  `"507"` is controlled with CMake variable **APPSDK_VERSION**
  
  `{linux, windows, darwin}` is appended automatically by CMake scripts.

* Clone this repository with submodules

* Choose build directory on top of the source directory and generate a suitable project.
  
  For example, sources are cloned into `${HOME}/dev/vray-for-houdini`. Create build directory `${HOME}/build/vray-for-houdini` and run inside:

  ```
  cmake -G Ninja \
    -DCMAKE_BUILD_TYPE=Debug \
    -DUSE_LANCHER=ON \
    -DHOUDINI_VERSION=14.0 -DHOUDINI_VERSION_BUILD=361 \
    -DAPPSDK_PATH=${HOME}/src/appsdk -DAPPSDK_VERSION=507 \
    ${HOME}/dev/vray-for-houdini
  ```

  This will generate a Ninja project; build it and install with: `ninja install`

  It'll build and install plugins (with icons, shelfs, etc) into the default Houdini's user settings directory.
  
  CMake variable `USE_LANCHER` will generate `${HOME}/bin/hfs` script with all needed environment varibales.
  
  Use it to launch Houdini (or copy variable definitions to your environment).

Known Issues
------------

* V-Ray Frame Buffer is not working correctly in OS X at this moment.
