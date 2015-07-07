# Source directory is the location of the script
SRC_DIR=`dirname $0`

cmake -G Ninja \
-DCMAKE_BUILD_TYPE=Debug \
-DHOUDINI_VERSION=14.0 \
-DHOUDINI_VERSION_BUILD=248 \
-DAPPSDK_VERSION=447 \
$* \
${SRC_DIR}
