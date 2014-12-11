
###########################################################################
#
# Configuration
#
###########################################################################

#cmake -DLUXRAYS_CUSTOM_CONFIG=cmake/SpecializedConfig/Config_Dade.cmake -DPYTHON_LIBRARY=/usr/lib/x86_64-linux-gnu/libpython3.4m.so -DPYTHON_INCLUDE_DIR=/usr/include/python3.4m -DPYTHON_INCLUDE_DIR2=/usr/include/python3.4m .

MESSAGE(STATUS "Using Dade's Linux Configuration settings")

# To compile with a specific Python version
#set(Python_ADDITIONAL_VERSIONS "2.6")

# To pick a specific minimum Python version
#set(PythonLibs_FIND_VERSION 3)

#set(BOOST_SEARCH_PATH          "/home/david/projects/luxrender-dev/boost_1_53_0")
set(OPENIMAGEIO_ROOT_DIR        "/home/david/projects/luxrender-dev/oiio-RB-1.3/dist/linux64")
set(OPENEXR_ROOT                "/usr/local")
set(EMBREE_SEARCH_PATH           "/home/david/src/embree-bin-2.3.3_linux")

set(OPENCL_SEARCH_PATH        "$ENV{AMDAPPSDKROOT}")
set(OPENCL_INCLUDEPATH        "${OPENCL_SEARCH_PATH}/include")
#set(OPENCL_LIBRARYDIR         "${OPENCL_SEARCH_PATH}/lib/x86_64")

#set(LUXRAYS_DISABLE_OPENCL TRUE)

set(CMAKE_BUILD_TYPE "Debug")
#set(CMAKE_BUILD_TYPE "Release")
