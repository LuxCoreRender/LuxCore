###########################################################################
#
# Configuration
#
###########################################################################

#cmake -DLUXRAYS_CUSTOM_CONFIG=cmake/SpecializedConfig/Config_Linux-Static.cmake .

MESSAGE(STATUS "Using my own settings")

# set(OPENCL_SEARCH_PATH        "$ENV{ATISTREAMSDKROOT}")
set(OPENCL_SEARCH_PATH        "/usr/src/opencl-sdk/include")
set(OPENCL_LIBRARYDIR         "${OPENCL_SEARCH_PATH}/lib/x86_64")

SET(BLOSC_SEARCH_PATH       "${LuxRays_SOURCE_DIR}/../target-64-sse2")
SET(Blosc_USE_STATIC_LIBS   "ON")
SET(TBB_SEARCH_PATH         "${LuxRays_SOURCE_DIR}/../target-64-sse2")

# set(BUILD_LUXMARK TRUE)
set(CMAKE_BUILD_TYPE "Release")

link_directories(${LuxRays_SOURCE_DIR}/../target-64-sse2/lib)
