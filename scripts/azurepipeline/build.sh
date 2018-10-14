#!/bin/bash

# Install deps
sudo apt-get -qq update
sudo apt-get install -y git cmake g++ flex bison libbz2-dev libopenimageio-dev libtiff5-dev libpng12-dev libgtk-3-dev libopenexr-dev libgl1-mesa-dev python3-dev python3-pip python3-numpy
# Install Pillow
sudo pip3 install pillow

# Clone LinuxCompile
git clone https://github.com/LuxCoreRender/LinuxCompile.git
# Clone LuxCore (this is a bit a waste but LinuxCompile procedure
# doesn't work with symbolic links)
git clone https://github.com/LuxCoreRender/LuxCore.git

# Set up paths
cd LinuxCompile
ln -s .. LuxCore
./build-64-sse2 LuxCore
