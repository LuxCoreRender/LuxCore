#!/bin/bash

#Fetch Artifacts
wget https://github.com/LuxCoreRender/MacOSCompileDeps/releases/download/luxcorerender_v2.1beta3/MacDistFiles.tar.gz
tar xzf MacDistFiles.tar.gz

# Set Environment Variables
export PATH="/usr/local/opt/bison/bin:/usr/local/bin:$PATH"
DEPS_SOURCE=`pwd`/macos
eval "$(pyenv init -)"

#==========================================================================
# Compiling OpenCL-less version"
#==========================================================================

mkdir build
pushd build
cmake -G Xcode -DLUXRAYS_DISABLE_OPENCL=1 -DOSX_DEPENDENCY_ROOT=$DEPS_SOURCE ..
cmake --build . --config Release
popd

#==========================================================================
# Compiling OpenCL version"
#==========================================================================

mkdir build_opencl
pushd build_opencl
cmake -G Xcode -DOSX_DEPENDENCY_ROOT=$DEPS_SOURCE ..
cmake --build . --config Release
popd
