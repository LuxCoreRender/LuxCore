#!/bin/bash

#automated script build for macos 10.13 should patch with dependencies in root/macos

rm -rf release_OSX_ocl

mkdir release_OSX_ocl

cp -R macos/mac_bundle/LuxCore.app release_OSX_ocl

###luxcoreui bundle
cp build/luxcoreui release_OSX_ocl/LuxCore.app/Contents/MacOS

cd release_OSX_ocl

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
cp ../../macos/bin/denoise .

dylibbundler -of -b -x denoise -d ./ -p @executable_path/

chmod 755 ./denoise

cd ../../

tar -czf LuxCoreRender.tar.gz ./release_OSX
