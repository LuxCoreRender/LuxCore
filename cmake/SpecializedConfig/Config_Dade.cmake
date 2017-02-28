
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

# To compile boost with Python 3:
# ./bootstrap.sh --with-python=/usr/bin/python3
# ./b2 -j 12 install --prefix=/home/david/projects/luxrender-dev/boost_1_56_0-bin
SET(BOOST_SEARCH_PATH		"/home/david/projects/luxrender-dev/boost_1_56_0-bin")

# To compile OpenImageIO form source:
# cp ../linux/distfiles-pyluxcore/oiio-1.3.13-plugin.cpp src/libutil/plugin.cpp (fix #include path by adding OpenImageIO/)
# make nuke
# make -j 12 EMBEDPLUGINS=1 USE_OPENGL=0 USE_QT=0 USE_OPENSSL=0 USE_PYTHON=0 BUILDSTATIC=0 OIIO_BUILD_TOOLS=0 OIIO_BUILD_TESTS=0 STOP_ON_WARNING=0 BOOST_HOME=/home/david/projects/luxrender-dev/boost_1_56_0-bin
SET(OPENIMAGEIO_ROOT_DIR	"/home/david/projects/luxrender-dev/oiio/dist/linux64")
#SET(OPENEXR_ROOT			"/usr/local")

# To compile Embree:
# mkdir build
# cd build
# cmake -DENABLE_STATIC_LIB=ON -DENABLE_ISPC_SUPPORT=OFF ..
SET(EMBREE_SEARCH_PATH		"/home/david/projects/luxrender-dev/embree-dade")
#SET(EMBREE_SEARCH_PATH		"/home/david/src/embree-bin-2.4_linux")

SET(OPENCL_SEARCH_PATH	"$ENV{AMDAPPSDKROOT}")
SET(OPENCL_INCLUDEPATH	"${OPENCL_SEARCH_PATH}/include")
#SET(OPENCL_LIBRARYDIR	"${OPENCL_SEARCH_PATH}/lib/x86_64")

#SET(LUXRAYS_DISABLE_OPENCL TRUE)
#SET(LUXCORE_DISABLE_EMBREE_BVH_BUILDER TRUE)

SET(CMAKE_BUILD_TYPE "Debug")
#SET(CMAKE_BUILD_TYPE "Release")
