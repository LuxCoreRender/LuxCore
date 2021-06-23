#!/bin/bash

#Fetch BuildDeps
wget https://github.com/LuxCoreRender/MacOSCompileDeps/releases/download/luxcorerender_v2.6alpha/MacDistFiles_39.tar.gz
tar xzf MacDistFiles_39.tar.gz

# Set Environment Variables
export PATH="/usr/local/opt/bison/bin:/usr/local/bin:$PATH"
DEPS_SOURCE=`pwd`/macos
eval "$(pyenv init -)"

#==========================================================================
# Compiling Unified version"
#==========================================================================

mkdir build
pushd  build
cmake -DOSX_DEPENDENCY_ROOT=$DEPS_SOURCE -DCMAKE_BUILD_TYPE=Release ..
make
popd
