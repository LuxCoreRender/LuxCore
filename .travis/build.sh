#!/bin/bash

if [[ $TRAVIS_OS_NAME == 'osx' ]]; then
  cmake -G Xcode -DLUXRAYS_DISABLE_OPENCL=1 -DOSX_DEPENDENCY_ROOT=$DEPS_SOURCE .
  cmake --build . --config Release
fi
