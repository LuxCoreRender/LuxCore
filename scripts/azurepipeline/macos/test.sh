#!/bin/bash

# Set Environment Variables
export PATH="/usr/local/bin:$PATH"
eval "$(pyenv init -)"
pyenv shell 3.7.7

PLUXSO=`pwd`/build/lib/Release/pyluxcore.so

# Build Test Diff Tool
pushd deps/perceptualdiff-master
cmake .
make
popd

pushd pyunittests

# Add symbolic link to release pyluxcore.so
ln -s $PLUXSO ./pyluxcore.so

# Run Tests
python3 unittests.py
