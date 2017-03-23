#!/bin/sh

# Use https://bitbucket.org/luxrender/linux to build LuxCore SDK binaries and
# libraries (i.e. DLL type)

rm -rf luxcore-sdk
mkdir luxcore-sdk

echo "================ Copy Scenes ================"
cp -r scenes luxcore-sdk

echo "================ Copy LuxCore DLL ================"
mkdir luxcore-sdk/lib
cp -v ../../target-64-sse2/lux-*/libluxcore.so luxcore-sdk/lib
cp -v ../../target-64-sse2/lux-*/pyluxcore.so luxcore-sdk/lib
cp -v ../../target-64-sse2/lux-*/libembree.so.2 luxcore-sdk/lib
cp -v ../../target-64-sse2/lux-*/libtbb.so.2 luxcore-sdk/lib

echo "================ Copy headers ================"
cp -v --parents include/luxcore/cfg.h luxcore-sdk
cp -v --parents include/luxcore/luxcore.h luxcore-sdk
cp -v --parents include/luxrays/utils/exportdefs.h luxcore-sdk
cp -v --parents include/luxrays/utils/properties.h luxcore-sdk
cp -v --parents include/luxrays/utils/utils.h luxcore-sdk
cp -v --parents include/luxrays/utils/ocl.h luxcore-sdk
cp -v --parents include/luxrays/utils/oclerror.h luxcore-sdk
cp -v --parents include/luxrays/utils/cyhair/cyHairFile.h luxcore-sdk

echo "================ Copy samples ================"
cp -r samples luxcore-sdk

echo "================ Copy info files ================"
cp -v README.txt luxcore-sdk
cp -v AUTHORS.txt luxcore-sdk
cp -v COPYING.txt luxcore-sdk

echo "================ Copy compiled file ================"
mkdir luxcore-sdk/bin
cp -v ../../target-64-sse2/lux-*/luxcoreui luxcore-sdk/bin
cp -v ../../target-64-sse2/lux-*/luxcoreconsole luxcore-sdk/bin
cp -v ../../target-64-sse2/lux-*/luxcoredemo luxcore-sdk/bin
cp -v ../../target-64-sse2/lux-*/luxcorescenedemo luxcore-sdk/bin

echo "================ Copy cmake files ================"
# To run cmake (OpenCL): cmake -DBOOST_SEARCH_PATH=/home/david/projects/luxrender-dev/boost_1_56_0-bin -DOPENCL_SEARCH_PATH="$AMDAPPSDKROOT" .
# To run cmake (No OpenCL): cmake -DBOOST_SEARCH_PATH=/home/david/projects/luxrender-dev/boost_1_56_0-bin -DLUXRAYS_DISABLE_OPENCL=1 .
cp -v sdk/CMakeLists.txt luxcore-sdk
mkdir luxcore-sdk/cmake
cp -v cmake/Packages/FindOpenCL.cmake luxcore-sdk/cmake

tar zcvf ../../target-64-sse2/luxcore-sdk.tgz luxcore-sdk
