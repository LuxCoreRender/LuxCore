#!/bin/sh

rm -rf test-sdk
mkdir test-sdk

cp -r scenes test-sdk
mkdir test-sdk/lib
cd test-sdk/lib

# Merge everything in a single library
echo "create libluxcore.a
addlib ../../lib/libluxrays.a
addlib ../../lib/libslg-core.a
addlib ../../lib/libslg-film.a
addlib ../../lib/libslg-kernels.a
addlib ../../lib/libluxcore.a
save
end" | ar -M

# Copy headers
cp -v --parents include/luxcore/cfg.h test-sdk
cp -v --parents include/luxcore/luxcore.h test-sdk
cp -v --parents include/luxrays/utils/exportdefs.h test-sdk
cp -v --parents include/luxrays/utils/properties.h test-sdk
cp -v --parents include/luxrays/utils/utils.h test-sdk
cp -v --parents include/luxrays/utils/ocl.h test-sdk
cp -v --parents include/luxrays/utils/oclerror.h test-sdk
cp -v --parents include/luxrays/utils/cyhair/cyHairFile.h test-sdk

# Copy samples
cp -rv samples test-sdk

# Copy info files
cp -v README.txt test-sdk
cp -v AUTHORS.txt test-sdk
cp -v COPYING.txt test-sdk

# Copy compiled file
cp -v --parents bin/luxcoreui test-sdk
cp -v --parents bin/luxcoreconsole test-sdk
cp -v --parents bin/luxcoredemo test-sdk
cp -v --parents bin/luxcorescenedemo test-sdk

# Copy cmake files
# To run cmake: cmake -DBOOST_SEARCH_PATH=/home/david/projects/luxrender-dev/boost_1_56_0-bin .
cp -v sdk/CMakeLists.txt test-sdk
