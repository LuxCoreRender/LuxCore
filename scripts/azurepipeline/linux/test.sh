#!/bin/bash

# Install deps
sudo pip3 install --upgrade pip
sudo pip3 install --upgrade setuptools
sudo pip3 install pillow

cd LinuxCompile/LuxCore
export LD_LIBRARY_PATH=`pwd`/../target-64-sse2/lib
make tests
