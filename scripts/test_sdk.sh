#!/bin/sh

rm -rf test-sdk
mkdir test-sdk

cp -r scenes test-sdk
mkdir test-sdk/lib
cd test-sdk/lib

echo "================ Merge everything in a single library ================"
echo "create libluxcore.a
addlib ../../lib/libluxrays.a
addlib ../../lib/libslg-core.a
addlib ../../lib/libslg-film.a
addlib ../../lib/libslg-kernels.a
addlib ../../lib/libluxcore.a
save
end" | ar -M

cd ../..

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

#/usr/bin/c++  -fPIC -mtune=generic -std=c++11 -static-libgcc -mmmx -msse -msse2 -O3 -pipe -mfpmath=sse  -fvisibility-inlines-hidden -ftree-vectorize -fno-math-errno -fno-signed-zeros -fno-trapping-math -fassociative-math -fno-rounding-math -fno-signaling-nans -fcx-limited-range -DBOOST_DISABLE_ASSERTS -floop-interchange -floop-strip-mine -floop-block -fsee -ftree-loop-linear -ftree-loop-distribution -ftree-loop-im -fivopts -ftracer -fomit-frame-pointer -DHAVE_PTHREAD_H -fPIC -fno-stack-protector -pthread -lrt  -Wall -Wno-long-long -pedantic -msse -msse2 -msse3 -mssse3 -fPIC -fopenmp -DNDEBUG -O3 -ftree-vectorize -funroll-loops -fvariable-expansion-in-unroller -Wl,--version-script='/tmp/luxbuild/luxrays/cmake/exportmaps/linux_symbol_exports.map' -shared -Wl,-soname,luxcore.so -o luxcore.so -Wl,--whole-archive libluxcore.a -Wl,--no-whole-archive libslg-core.a libslg-film.a libslg-kernels.a libluxrays.a /home/david/projects/luxrender-dev/linux/target-64-sse2/lib/libembree.so.2 /home/david/projects/luxrender-dev/linux/target-64-sse2/lib/libboost_thread.a /home/david/projects/luxrender-dev/linux/target-64-sse2/lib/libboost_program_options.a /home/david/projects/luxrender-dev/linux/target-64-sse2/lib/libboost_filesystem.a /home/david/projects/luxrender-dev/linux/target-64-sse2/lib/libboost_serialization.a /home/david/projects/luxrender-dev/linux/target-64-sse2/lib/libboost_iostreams.a /home/david/projects/luxrender-dev/linux/target-64-sse2/lib/libboost_regex.a /home/david/projects/luxrender-dev/linux/target-64-sse2/lib/libboost_system.a /home/david/projects/luxrender-dev/linux/target-64-sse2/lib/libboost_python.a /home/david/projects/luxrender-dev/linux/target-64-sse2/lib/libboost_chrono.a -lpthread /home/david/projects/luxrender-dev/linux/target-64-sse2/lib/libtiff.a /home/david/projects/luxrender-dev/linux/target-64-sse2/lib/libIex.a /home/david/projects/luxrender-dev/linux/target-64-sse2/lib/libIlmImf.a /home/david/projects/luxrender-dev/linux/target-64-sse2/lib/libHalf.a /home/david/projects/luxrender-dev/linux/target-64-sse2/lib/libImath.a /home/david/projects/luxrender-dev/linux/target-64-sse2/lib/libIlmThread.a /home/david/projects/luxrender-dev/linux/target-64-sse2/lib/libpng.a /home/david/projects/luxrender-dev/linux/target-64-sse2/lib/libz.a /home/david/projects/luxrender-dev/linux/target-64-sse2/lib/libjpeg.a /home/david/projects/luxrender-dev/linux/target-64-sse2/lib/libOpenImageIO.a -lGL -lOpenCL -Wl,-rpath,"\$ORIGIN"