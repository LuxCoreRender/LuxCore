#!/bin/bash

#Fetch Artifacts
wget https://github.com/LuxCoreRender/MacOSCompileDeps/releases/download/luxcorerender_v2.3alpha1/MacDistFiles.tar.gz
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
cmake -DOSX_DEPENDENCY_ROOT=$DEPS_SOURCE ..
cmake --build . --config Release
popd

mkdir build_ocl
cp ./build/luxcoreui ./build_ocl
cp ./build/luxcoreconsole ./build_ocl
cp ./build/lib/pyluxcore.so ./build_ocl

#==========================================================================
# Compiling OpenCL-less version"
#==========================================================================

pushd  build
cmake -DLUXRAYS_DISABLE_OPENCL=1 -DOSX_DEPENDENCY_ROOT=$DEPS_SOURCE ..
cmake --build . --config Release
popd
