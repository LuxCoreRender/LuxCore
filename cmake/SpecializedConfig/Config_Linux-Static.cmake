###########################################################################
#
# Configuration
#
###########################################################################

#cmake -DLUXRAYS_CUSTOM_CONFIG=cmake/SpecializedConfig/Config_Linux-Static.cmake .

MESSAGE(STATUS "Using my own settings")

set(BOOST_SEARCH_PATH         "${LuxRays_SOURCE_DIR}/../target/lib")

# set(OPENCL_SEARCH_PATH        "$ENV{ATISTREAMSDKROOT}")
set(OPENCL_SEARCH_PATH        "/usr/src/opencl-sdk/include")
set(OPENCL_LIBRARYDIR         "${OPENCL_SEARCH_PATH}/lib/x86_64")

# set(BUILD_LUXMARK TRUE)
set(CMAKE_BUILD_TYPE "Release")

# Required by something broken in Debian 9
link_directories($ENV{LDFLAGS})
