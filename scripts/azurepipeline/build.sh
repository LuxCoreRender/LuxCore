#!/bin/bash

# Install deps
sudo apt-get -qq update
sudo apt-get install -y wget libtool git cmake3 g++ flex bison libbz2-dev libopenimageio-dev libtiff5-dev libpng12-dev libgtk-3-dev libopenexr-dev libgl1-mesa-dev python3-dev python3-pip python3-numpy

# Clone LinuxCompile
git clone https://github.com/LuxCoreRender/LinuxCompile.git

# Set up paths
cd LinuxCompile
# Clone LuxCore (this is a bit a waste but LinuxCompile procedure
# doesn't work with symbolic links)
git clone .. LuxCore
./build-64-sse2 LuxCore
mv target-64-sse2/LuxCore.tar.bz2 target-64-sse2/luxcorerender-latest-linux64.tar.bz2
cd ..
