#!/bin/bash

# Install deps
sudo apt-get -qq update
sudo apt-get install -y wget git cmake g++ flex bison libbz2-dev libopenimageio-dev libtiff5-dev libpng12-dev libgtk-3-dev libopenexr-dev libgl1-mesa-dev python3-dev python3-pip python3-numpy

wget https://dl.bintray.com/boostorg/release/1.67.0/source/boost_1_67_0.tar.gz
tar xf boost_1_67_0.tar.gz
cd boost_1_67_0
./bootstrap.sh
./b2

# Clone LinuxCompile
#git clone https://github.com/LuxCoreRender/LinuxCompile.git

# Set up paths
#cd LinuxCompile
# Clone LuxCore (this is a bit a waste but LinuxCompile procedure
# doesn't work with symbolic links)
#git clone .. LuxCore
#./build-64-sse2 LuxCore
