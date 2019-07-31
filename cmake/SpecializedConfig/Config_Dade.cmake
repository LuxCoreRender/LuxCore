###########################################################################
#
# Configuration
#
###########################################################################

#cmake -DLUXRAYS_CUSTOM_CONFIG=cmake/SpecializedConfig/Config_Dade.cmake -DPYTHON_LIBRARY=/usr/lib/x86_64-linux-gnu/libpython3.6m.so -DPYTHON_INCLUDE_DIR=/usr/include/python3.6m .

MESSAGE(STATUS "Using Dade's Linux Configuration settings")

SET(CMAKE_INCLUDE_PATH "/home/david/projects/luxcorerender/LinuxCompile/target-64-sse2/include;/home/david/projects/luxcorerender/LinuxCompile/target-64-sse2")
SET(CMAKE_LIBRARY_PATH "/home/david/projects/luxcorerender/LinuxCompile/target-64-sse2/lib;/home/david/projects/luxcorerender/LinuxCompile/target-64-sse2")
SET(Blosc_USE_STATIC_LIBS   "ON")

#SET(LUXRAYS_DISABLE_OPENCL TRUE)
#SET(BUILD_LUXCORE_DLL TRUE)

#SET(CMAKE_BUILD_TYPE "Debug")
SET(CMAKE_BUILD_TYPE "Release")
