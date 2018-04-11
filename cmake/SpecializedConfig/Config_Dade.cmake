###########################################################################
#
# Configuration
#
###########################################################################

#cmake -DLUXRAYS_CUSTOM_CONFIG=cmake/SpecializedConfig/Config_Dade.cmake -DPYTHON_LIBRARY=/usr/lib/x86_64-linux-gnu/libpython3.4m.so -DPYTHON_INCLUDE_DIR=/usr/include/python3.4m .

MESSAGE(STATUS "Using Dade's Linux Configuration settings")

# To compile with a specific Python version
#SET(Python_ADDITIONAL_VERSIONS "2.6")

# To pick a specific minimum Python version
#SET(PythonLibs_FIND_VERSION 3)

# To compile boost with Python 3:
# ./bootstrap.sh --with-python=/usr/bin/python3
# ./b2 -j 12 install --prefix=/home/david/projects/luxcorerender/boost_1_56_0-bin
SET(BOOST_SEARCH_PATH		"/home/david/projects/luxcorerender/LinuxCompile/target-64-sse2")

# To compile OpenImageIO form source:
# cp ../linux/distfiles-pyluxcore/oiio-1.3.13-plugin.cpp src/libutil/plugin.cpp (fix #include path by adding OpenImageIO/)
# make nuke
# make -j 12 EMBEDPLUGINS=1 USE_OPENGL=0 USE_QT=0 USE_OPENSSL=0 USE_PYTHON=0 BUILDSTATIC=0 OIIO_BUILD_TOOLS=0 OIIO_BUILD_TESTS=0 STOP_ON_WARNING=0 BOOST_HOME=/home/david/projects/luxcorerender/boost_1_56_0-bin
SET(OPENIMAGEIO_ROOT_DIR	"/home/david/projects/luxcorerender/LinuxCompile/target-64-sse2")
#SET(OPENEXR_ROOT			"/usr/local")

# To compile Embree:
# mkdir build
# cd build
# cmake -DENABLE_STATIC_LIB=ON -DENABLE_ISPC_SUPPORT=OFF ..
SET(EMBREE_SEARCH_PATH		"/home/david/projects/luxcorerender/LinuxCompile/target-64-sse2")

SET(OPENCL_SEARCH_PATH		"$ENV{AMDAPPSDKROOT}")

# To compile c-blosc-1.14.2
# Add a SET(CMAKE_C_FLAGS "-fPIC ${CMAKE_C_FLAGS}") in CMakeLists.txt
# mkdir build
# cd build
# cmake -DCMAKE_INSTALL_PREFIX=/home/david/projects/luxcorerender/c-blosc-1.14.2-bin ..

# To compile OpenVDB v3.3.0
# Disable NO_DEFAULT_PATH from cmake/FindGLFW.cmake
# Force [SET ( ILMIMF_LIBRARY_NAME IlmImf )] in cmake/FindILMBase.cmake
# Add [set(CMAKE_CXX_FLAGS "-std=c++11 ${CMAKE_CXX_FLAGS}")] to openvdb/CMakeLists.txt
# Add points/points.cc to OPENVDB_LIBRARY_SOURCE_FILES in openvdb/CMakeLists.txt
# cmake -DCMAKE_INSTALL_PREFIX=/home/david/projects/luxcorerender/openvdb-3.3.0-bin -DBOOST_ROOT=/home/david/projects/luxcorerender/LinuxCompile/target-64-sse2 -DBLOSC_LOCATION=/home/david/projects/luxcorerender/c-blosc-1.14.2-bin -DTBB_LOCATION=/home/david/projects/luxcorerender/tbb2018_20171205oss -DILMBASE_LOCATION=/home/david/projects/luxcorerender/LinuxCompile/target-64-sse2 -DIlmbase_USE_STATIC_LIBS=1 -DOPENEXR_LOCATION=/home/david/projects/luxcorerender/LinuxCompile/target-64-sse2 -DOpenexr_USE_STATIC_LIBS=1 -DILMBASE_NAMESPACE_VERSIONING=OFF -DOPENEXR_NAMESPACE_VERSIONING=OFF .
# make -j 12
# make install

SET(BLOSC_SEARCH_PATH       "/home/david/projects/luxcorerender/LinuxCompile/target-64-sse2")
SET(Blosc_USE_STATIC_LIBS   "ON")
SET(TBB_SEARCH_PATH         "/home/david/projects/luxcorerender/LinuxCompile/target-64-sse2")

#SET(LUXRAYS_DISABLE_OPENCL TRUE)
#SET(BUILD_LUXCORE_DLL TRUE)

#SET(CMAKE_BUILD_TYPE "Debug")
SET(CMAKE_BUILD_TYPE "Release")
