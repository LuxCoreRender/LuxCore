#!/bin/bash

#Install Build Tools
brew install bison
brew install freeimage
brew install pyenv
brew install dylibbundler

eval "$(pyenv init -)"

pyenv install 3.7.0
pyenv global 3.7.0
pyenv shell 3.7.0

sudo pip3 install -U numpy
sudo pip3 install pillow
sudo pip3 install pyside2
