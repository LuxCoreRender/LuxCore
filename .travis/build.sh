#!/bin/bash

if [[ $TRAVIS_OS_NAME == 'linux' ]]; then
  cmake -DBOOST_SEARCH_PATH=`pwd`/target-64-sse2 \
  -DOPENIMAGEIO_ROOT_DIR=`pwd`/target-64-sse2 \
  -DOPENIMAGEIO_ROOT_DIR=`pwd`/target-64-sse2 \
  -DEMBREE_SEARCH_PATH=`pwd`/target-64-sse2 \
  -DBLOSC_SEARCH_PATH=`pwd`/target-64-sse2 \
  -DTBB_SEARCH_PATH=`pwd`/target-64-sse2 \
  -DPYTHON_LIBRARY=/usr/lib/x86_64-linux-gnu/libpython3.4m.so \
  -DPYTHON_INCLUDE_DIR=/usr/include/python3.4m .
  make -j 2
  
  # The unit tests take too much time
  #make tests_subset
elif [[ $TRAVIS_OS_NAME == 'osx' ]]; then
  cmake -G Xcode -DOSX_DEPENDENCY_ROOT=$DEPS_SOURCE ..
  cmake --build . --config Release
fi
