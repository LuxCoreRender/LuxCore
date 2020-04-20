#!/bin/bash
 
#Install Build Tools
brew install bison
brew install pyenv
brew install freeimage
#brew install create-dmg

eval "$(pyenv init -)"

pyenv install 3.7.4
pyenv global 3.7.4
pyenv shell 3.7.4

sudo pip3 install -U numpy
sudo pip3 install pillow
sudo pip3 install pyside2
