#!/bin/bash


#Fetch Artifacts
wget https://github.com/LuxCoreRender/MacOSCompileDeps/releases/download/luxcorerender_v2.1beta2/MacDistFiles.tar.gz
tar xzf MacDistFiles.tar.gz

# Set Environment Variables
export PATH="/usr/local/opt/bison/bin:/usr/local/bin:$PATH"
DEPS_SOURCE=`pwd`/macos

#Build LuxCore
cmake -G Xcode -DLUXRAYS_DISABLE_OPENCL=1 -DOSX_DEPENDENCY_ROOT=$DEPS_SOURCE .
cmake --build . --config Release
