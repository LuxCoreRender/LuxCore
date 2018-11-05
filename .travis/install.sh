#!/bin/bash

if [[ $TRAVIS_OS_NAME == 'osx' ]]; then
  #Install Build Tools
  brew install bison
  brew outdated python3 || brew upgrade python3
  brew outdated cmake || brew upgrade cmake
  brew install pyside
  brew install numpy --with-python3
  #Install Deps
  wget https://github.com/LuxCoreRender/MacOSCompileDeps/releases/download/luxcorerender_v2.1alpha4/MacDistFiles.tar.gz
  tar xzf MacDistFiles.tar.gz
fi
