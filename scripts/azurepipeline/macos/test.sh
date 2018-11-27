#!/bin/bash

# Set Environment Variables
export PATH="/usr/local/opt/bison/bin:/usr/local/bin:$PATH"

# Build Test Diff Tool
pushd deps/perceptualdiff-master
cmake .
make
popd

pushd pyunittests

# Add symbolic link to release pyluxcore.so
ln -s ../lib/Release/pyluxcore.so pyluxcore.so

# Run Tests
python3 unittests.py
