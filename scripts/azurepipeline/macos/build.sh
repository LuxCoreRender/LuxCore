#!/bin/bash

#Fetch Artifacts
wget https://github.com/LuxCoreRender/MacOSCompileDeps/releases/download/luxcorerender_v2.4beta1/MacDistFiles.tar.gz
tar xzf MacDistFiles.tar.gz

# Set Environment Variables
export PATH="/usr/local/opt/bison/bin:/usr/local/bin:$PATH"
DEPS_SOURCE=`pwd`/macos
eval "$(pyenv init -)"

#==========================================================================
# Compiling OpenCL version"
#==========================================================================

mkdir build
pushd  build
cmake -DOSX_DEPENDENCY_ROOT=$DEPS_SOURCE -DCMAKE_BUILD_TYPE=Release ..
make
popd

mkdir build_ocl
cp ./build/Release/luxcoreui ./build_ocl
cp ./build/Release/luxcoreconsole ./build_ocl
cp ./build/lib/Release/pyluxcore.so ./build_ocl

#==========================================================================
# Compiling OpenCL-less version"
#==========================================================================

pushd  build
cmake -DLUXRAYS_DISABLE_OPENCL=1 -DOSX_DEPENDENCY_ROOT=$DEPS_SOURCE -DCMAKE_BUILD_TYPE=Release ..
make
popd
