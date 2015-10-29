
###########################################################################
#
# Configuration
#
###########################################################################

#cmake -DLUXRAYS_CUSTOM_CONFIG=cmake/SpecializedConfig/Config_Dade.cmake -DPYTHON_LIBRARY=/usr/lib/x86_64-linux-gnu/libpython3.4m.so -DPYTHON_INCLUDE_DIR=/usr/include/python3.4m -DPYTHON_INCLUDE_DIR2=/usr/include/python3.4m .

MESSAGE(STATUS "Using Dade's Linux Configuration settings")

# To compile with a specific Python version
#SET(Python_ADDITIONAL_VERSIONS "2.6")

# To pick a specific minimum Python version
#SET(PythonLibs_FIND_VERSION 3)

#SET(BOOST_SEARCH_PATH		"/home/david/projects/luxrender-dev/boost_1_53_0")
#SET(OPENIMAGEIO_ROOT_DIR	"/home/david/projects/luxrender-dev/oiio-RB-1.3/dist/linux64")
#SET(OPENEXR_ROOT			"/usr/local")
SET(EMBREE_SEARCH_PATH		"/home/david/src/embree-bin-2.4_linux")
SET(OPENSUBDIV_SEARCH_PATH	"/home/david/src/opensubdiv")
SET(OPENSUBDIV_LIBRARYDIR	"${OPENSUBDIV_SEARCH_PATH}/build/lib")

SET(OPENCL_SEARCH_PATH		"$ENV{AMDAPPSDKROOT}")
SET(OPENCL_INCLUDEPATH		"${OPENCL_SEARCH_PATH}/include")
#SET(OPENCL_LIBRARYDIR		"${OPENCL_SEARCH_PATH}/lib/x86_64")

#SET(LUXRAYS_DISABLE_OPENCL TRUE)

#SET(CMAKE_BUILD_TYPE "Debug")
SET(CMAKE_BUILD_TYPE "Release")
