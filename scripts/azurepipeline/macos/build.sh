#!/bin/bash

#Fetch BuildDeps
wget https://github.com/LuxCoreRender/MacOSCompileDeps/releases/download/luxcorerender_v2.9_python_3.11/MacDistFiles_311_intel_static.tar.gz
tar xzf MacDistFiles_311_intel_static.tar.gz

# Set Environment Variables
export PATH="/usr/local/opt/bison/bin:/usr/local/bin:$PATH"
DEPS_SOURCE=`pwd`/macos
eval "$(pyenv init -)"

#==========================================================================
# Compiling Unified version"
#==========================================================================

mkdir build
pushd  build
cmake -DCMAKE_XCODE_BUILD_SYSTEM=1 -DOSX_DEPENDENCY_ROOT=$DEPS_SOURCE -DCMAKE_BUILD_TYPE=Release ..
make
popd
