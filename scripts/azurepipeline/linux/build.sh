#!/bin/bash

# Install deps
sudo apt-get -qq update
sudo apt-get install -y libtool-bin cmake flex bison libgtk-3-dev libgl1-mesa-dev ocl-icd-opencl-dev patchelf

# Install Python 3.10
sudo apt install software-properties-common -y
sudo add-apt-repository ppa:deadsnakes/ppa -y
sudo apt-get install -y python3.10 python3.10-dev python3-numpy 
sudo update-alternatives --install /usr/bin/python python /usr/bin/python3.10 1
sudo update-alternatives --install /usr/bin/python3 python3 /usr/bin/python3.10 1

# Install CUDA 10.1 Update 2 (https://docs.nvidia.com/cuda/archive/10.1/cuda-installation-guide-linux/index.html)
wget https://developer.download.nvidia.com/compute/cuda/repos/ubuntu1804/x86_64/cuda-repo-ubuntu1804_10.1.243-1_amd64.deb
sudo dpkg -i cuda-repo-ubuntu1804_10.1.243-1_amd64.deb
sudo apt-key adv --fetch-keys https://developer.download.nvidia.com/compute/cuda/repos/ubuntu1804/x86_64/7fa2af80.pub
sudo apt-get -q update
sudo apt-get install -y cuda-10-1
export PATH=/usr/local/cuda-10.1/bin${PATH:+:${PATH}}
export LD_LIBRARY_PATH=/usr/local/cuda-10.1/lib64${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}

# Get cl.hpp file
wget https://www.khronos.org/registry/OpenCL/api/2.1/cl.hpp
sudo cp cl.hpp /usr/include/CL/cl.hpp

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
# Compiling unified CPU/OpenCL/CUDA version"
#==========================================================================

# Clone LuxCore (this is a bit a waste but LinuxCompile procedure
# doesn't work with symbolic links)
git clone .. LuxCore$SDK_BUILD
./build-64-sse2 LuxCore$SDK_BUILD
cp target-64-sse2/LuxCore$SDK_BUILD.tar.bz2 target-64-sse2/luxcorerender-$VERSION_STRING-linux64$SDK_BUILD.tar.bz2
mv target-64-sse2/LuxCore$SDK_BUILD.tar.bz2 $BUILD_ARTIFACTSTAGINGDIRECTORY/luxcorerender-$VERSION_STRING-linux64$SDK_BUILD.tar.bz2

cd ..
