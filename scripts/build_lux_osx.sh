#!/bin/bash

#automated script build for macos 10.13 should compile and patch with dependencies in root/macos
# set python and bison env *
eval "$(pyenv init -)"
pyenv shell 3.7.4
export PATH="/usr/local/opt/bison/bin:/usr/local/bin:$PATH"

# build luxcore
rm -rf build

mkdir build

cd build

cmake ..

make -j8

cd ..

# bundle things 
rm -rf release_OSX 

mkdir release_OSX

cp -R macos/mac_bundle/LuxCore.app release_OSX

###luxcoreui bundle
cp build/luxcoreui release_OSX/LuxCore.app/Contents/MacOS

cd release_OSX

dylibbundler -cd -of -b -x LuxCore.app/Contents/MacOS/luxcoreui -d LuxCore.app/Contents/Resources/libs -p @executable_path/../Resources/libs

###luxcoreconsole
mkdir luxcoreconsole

cp ../build/luxcoreconsole luxcoreconsole

cd luxcoreconsole

dylibbundler -cd -of -b -x luxcoreconsole -d ./libs -p @executable_path/libs

cd ..
###pyluxcore

mkdir pyluxcore

cp ../build/lib/pyluxcore* pyluxcore

cd pyluxcore

dylibbundler -of -b -x pyluxcore.so -d ./ -p @loader_path/

###denoise
cp ../../macos/mac_bundle/denoise .

cp ../../macos/mac_bundle/libOpenImageDenoise.1.1.0.dylib .

#dylibbundler -of -b -x denoise -d ./ -p @executable_path/

chmod 755 denoise

cd ../../

#tar -czf LuxCoreRender.tar.gz ./release_OSX

hdiutil create LuxCoreRender2.3alpha0.dmg -volname "LuxCoreRender2.3alpha0" -fs HFS+ -srcfolder release_OSX/
