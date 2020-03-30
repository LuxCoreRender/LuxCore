###########################################################################
#
# Configuration
#
###########################################################################

# To compile c-blosc with -fPIC on linux: add "set(CMAKE_POSITION_INDEPENDENT_CODE ON)" to CMakeLists.txt

#cmake -DLUXRAYS_CUSTOM_CONFIG=cmake/SpecializedConfig/Config_Dade.cmake -DPYTHON_LIBRARY=/usr/lib/x86_64-linux-gnu/libpython3.7m.so -DPYTHON_INCLUDE_DIR=/usr/include/python3.7m .

MESSAGE(STATUS "Using Dade's Linux Configuration settings")

SET(CMAKE_INCLUDE_PATH "/home/david/projects/luxcorerender/LinuxCompile/target-64-sse2/include;/home/david/projects/luxcorerender/LinuxCompile/target-64-sse2")
SET(CMAKE_LIBRARY_PATH "/home/david/projects/luxcorerender/LinuxCompile/target-64-sse2/lib;/home/david/projects/luxcorerender/LinuxCompile/target-64-sse2")
SET(Blosc_USE_STATIC_LIBS   "ON")

# Note: CUDA requires to have enabled OpenCL support too
#SET(LUXRAYS_ENABLE_CUDA TRUE)
#SET(LUXRAYS_DISABLE_OPENCL TRUE)
#SET(BUILD_LUXCORE_DLL TRUE)

#SET(CMAKE_BUILD_TYPE "Debug")
SET(CMAKE_BUILD_TYPE "Release")
