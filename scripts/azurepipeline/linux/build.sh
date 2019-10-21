#!/bin/bash

# Install deps
sudo apt-get -qq update
sudo apt-get install -y wget libtool git cmake g++ flex bison libbz2-dev libgtk-3-dev libopenexr-dev libgl1-mesa-dev python3-dev python3-pip python3-numpy ocl-icd-opencl-dev
#sudo apt-get install -y wget libtool git cmake g++ flex bison libbz2-dev libopenimageio-dev libtiff5-dev libpng12-dev libgtk-3-dev libopenexr-dev libgl1-mesa-dev python3-dev python3-pip python3-numpy ocl-icd-opencl-dev

# Clone LinuxCompile
git clone https://github.com/LuxCoreRender/LinuxCompile.git

# Set up correct names for release version and SDK
if [[ -z "$VERSION_STRING" ]] ; then
    VERSION_STRING=latest
fi

if [[ "$FINAL" == "TRUE" ]] ; then
    SDK_BUILD=-sdk
	# Required to link executables
	export LD_LIBRARY_PATH="`pwd`/LinuxCompile/target-64-sse2/lib:$LD_LIBRARY_PATH"
fi

# Set up paths
cd LinuxCompile

#==========================================================================
# Compiling OpenCL-less version"
#==========================================================================

# Clone LuxCore (this is a bit a waste but LinuxCompile procedure
# doesn't work with symbolic links)
git clone .. LuxCore$SDK_BUILD
./build-64-sse2 LuxCore$SDK_BUILD
cp target-64-sse2/LuxCore$SDK_BUILD.tar.bz2 target-64-sse2/luxcorerender-$VERSION_STRING-linux64$SDK_BUILD.tar.bz2
mv target-64-sse2/LuxCore$SDK_BUILD.tar.bz2 $BUILD_ARTIFACTSTAGINGDIRECTORY/luxcorerender-$VERSION_STRING-linux64$SDK_BUILD.tar.bz2

#==========================================================================
# Compiling OpenCL version"
#==========================================================================

# Clone LuxCore (this is a bit a waste but LinuxCompile procedure
# doesn't work with symbolic links)
git clone .. LuxCore-opencl$SDK_BUILD
./build-64-sse2 LuxCore-opencl$SDK_BUILD 5
cp target-64-sse2/LuxCore-opencl$SDK_BUILD.tar.bz2 target-64-sse2/luxcorerender-$VERSION_STRING-linux64-opencl$SDK_BUILD.tar.bz2
mv target-64-sse2/LuxCore-opencl$SDK_BUILD.tar.bz2 $BUILD_ARTIFACTSTAGINGDIRECTORY/luxcorerender-$VERSION_STRING-linux64-opencl$SDK_BUILD.tar.bz2

cd ..
