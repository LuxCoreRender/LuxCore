#!/bin/sh

# Use https://bitbucket.org/luxrender/linux to build LuxCore binaries and libraries (i.e. DLL type)

rm -rf test-sdk
mkdir test-sdk

echo "================ Copy Scenes ================"
cp -r scenes test-sdk

echo "================ Copy LuxCore DLL ================"
mkdir test-sdk/lib
cp -v /tmp/luxbuild/luxrays/lib/libluxcore.so lib
cp -v /tmp/luxbuild/luxrays/lib/pyluxcore.so lib

echo "================ Copy headers ================"
cp -v --parents include/luxcore/cfg.h test-sdk
cp -v --parents include/luxcore/luxcore.h test-sdk
cp -v --parents include/luxrays/utils/exportdefs.h test-sdk
cp -v --parents include/luxrays/utils/properties.h test-sdk
cp -v --parents include/luxrays/utils/utils.h test-sdk
cp -v --parents include/luxrays/utils/ocl.h test-sdk
cp -v --parents include/luxrays/utils/oclerror.h test-sdk
cp -v --parents include/luxrays/utils/cyhair/cyHairFile.h test-sdk

echo "================ Copy samples ================"
cp -rv samples test-sdk

echo "================ Copy info files ================"
cp -v README.txt test-sdk
cp -v AUTHORS.txt test-sdk
cp -v COPYING.txt test-sdk

echo "================ Copy compiled file ================"
cp -v --parents bin/luxcoreui test-sdk
cp -v --parents bin/luxcoreconsole test-sdk
cp -v --parents bin/luxcoredemo test-sdk
cp -v --parents bin/luxcorescenedemo test-sdk

echo "================ Copy cmake files ================"
# To run cmake: cmake -DBOOST_SEARCH_PATH=/home/david/projects/luxrender-dev/boost_1_56_0-bin .
cp -v sdk/CMakeLists.txt test-sdk
mkdir test-sdk/cmake
cp -v cmake/Packages/FindOpenCL.cmake test-sdk/cmake
