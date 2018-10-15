#!/bin/bash

# Install deps
sudo pip3 install pillow

cd LinxuCompile/LuxCore
export LD_LIBRARY_PATH=`pwd`/../target-64-sse2/lib
make tests
