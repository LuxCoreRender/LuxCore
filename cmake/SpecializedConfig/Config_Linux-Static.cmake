###########################################################################
#
# Configuration
#
###########################################################################

#cmake -DLUXRAYS_CUSTOM_CONFIG=cmake/SpecializedConfig/Config_Linux-Static.cmake .

MESSAGE(STATUS "Using Linux static settings")

SET(CMAKE_INCLUDE_PATH "${LuxRays_SOURCE_DIR}/../target-64-sse2/include;${LuxRays_SOURCE_DIR}/../target-64-sse2")
SET(CMAKE_LIBRARY_PATH "${LuxRays_SOURCE_DIR}/../target-64-sse2/lib;${LuxRays_SOURCE_DIR}/../target-64-sse2")
SET(Blosc_USE_STATIC_LIBS   "ON")

#SET(BUILD_LUXCORE_DLL TRUE)

#SET(CMAKE_BUILD_TYPE "Debug")
#SET(CMAKE_BUILD_TYPE "RelWithDebInfo")
SET(CMAKE_BUILD_TYPE "Release")
