#!/bin/bash

#Install Build Tools
brew install bison
brew outdated python3 || brew upgrade python3
brew outdated cmake || brew upgrade cmake
brew install freeimage
brew install pyside
brew install numpy --with-python3
sudo pip3 install pillow
