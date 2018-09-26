#!/bin/bash

if [[ $TRAVIS_OS_NAME == 'linux' ]]; then
  # Install deps
  sudo apt-get -qq update
  sudo apt-get install -y git cmake g++ flex bison libbz2-dev libopenimageio-dev libtiff5-dev libpng12-dev libgtk-3-dev libopenexr-dev libgl1-mesa-dev python3-dev python3-pip python3-numpy
  # Install deps
  wget https://github.com/LuxCoreRender/LinuxCompileDeps/releases/download/luxcorerender_v2.1alpha4/target-64-sse2.tgz
  tar zxf target-64-sse2.tgz
  # Install Pillow
  sudo pip3 install pillow
  # Set OpenMP threads
  export OMP_NUM_THREADS=4
elif [[ $TRAVIS_OS_NAME == 'osx' ]]; then
  #Install Build Tools
  brew install bison
  brew outdated python3 || brew upgrade python3
  brew outdated cmake || brew upgrade cmake
  #Install Deps
  wget https://github.com/rebpdx/MacLuxDeps/releases/download/luxcorerender_v2.1alpha4/MacDistFiles.tar.gz
  tar xzf MacDistFiles.tar.gz
fi
