#!/bin/bash

# Install deps
sudo apt-get -qq update
sudo apt-get install -y wget libtool git cmake3 g++ flex bison libbz2-dev libopenimageio-dev libtiff5-dev libpng12-dev libgtk-3-dev libopenexr-dev libgl1-mesa-dev python3-dev python3-pip python3-numpy ocl-icd-opencl-dev

# Clone LinuxCompile
git clone https://github.com/LuxCoreRender/LinuxCompile.git

# Detect release type (official or daily) and set version tag
echo "$RELEASE_BUILD"
if [[ $RELEASE_BUILD == "TRUE" ]] ; then
    echo "This is an official release build"
    echo $BUILD_SOURCEVERSION
    GIT_TAG=$(git tag --points-at $BUILD_SOURCEVERSION | cut -d'_' -f 2)
else
    GIT_TAG="latest"
fi

echo "$GIT_TAG"
if [[ -z "$GIT_TAG" ]] ; then
    echo "No git tag found, one is needed for an official release"
    exit 1
fi

# Set up paths
cd LinuxCompile

#==========================================================================
# Compiling OpenCL-less version"
#==========================================================================

# Clone LuxCore (this is a bit a waste but LinuxCompile procedure
# doesn't work with symbolic links)
git clone .. LuxCore
./build-64-sse2 LuxCore
cp target-64-sse2/LuxCore.tar.bz2 target-64-sse2/luxcorerender-$GIT_TAG-linux64.tar.bz2
mv target-64-sse2/LuxCore.tar.bz2 $BUILD_ARTIFACTSTAGINGDIRECTORY/luxcorerender-$GIT_TAG-linux64.tar.bz2

#==========================================================================
# Compiling OpenCL version"
#==========================================================================

# Clone LuxCore (this is a bit a waste but LinuxCompile procedure
# doesn't work with symbolic links)
git clone .. LuxCore-opencl
./build-64-sse2 LuxCore-opencl 5
cp target-64-sse2/LuxCore-opencl.tar.bz2 target-64-sse2/luxcorerender-$GIT_TAG-linux64-opencl.tar.bz2
mv target-64-sse2/LuxCore-opencl.tar.bz2 $BUILD_ARTIFACTSTAGINGDIRECTORY/luxcorerender-$GIT_TAG-linux64-opencl.tar.bz2

cd ..
