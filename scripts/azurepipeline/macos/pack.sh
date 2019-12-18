#!/bin/bash

DEPS_SOURCE=`pwd`/macos

mkdir release_OSX_OCL

###luxcoreui bundle

cp -R macos/mac_bundle/LuxCore.app release_OSX_OCL
cp build/luxcoreui release_OSX_OCL/LuxCore.app/Contents/MacOS

cd release_OSX_OCL

mkdir -p LuxCore.app/Contents/Resources/libs/

#libomp

cp -f $DEPS_SOURCE/lib/libomp.dylib LuxCore.app/Contents/Resources/libs/libomp.dylib
chmod +w LuxCore.app/Contents/Resources/libs/libomp.dylib
install_name_tool -id @executable_path/../Resources/libs/libomp.dylib LuxCore.app/Contents/Resources/libs/libomp.dylib

#libembree

cp -f $DEPS_SOURCE/lib/libembree3.3.dylib LuxCore.app/Contents/Resources/libs/libembree3.3.dylib
chmod +w LuxCore.app/Contents/Resources/libs/libembree3.3.dylib
install_name_tool -id @executable_path/../Resources/libs/libembree3.3.dylib LuxCore.app/Contents/Resources/libs/libembree3.3.dylib
install_name_tool -change @rpath/libtbb.dylib @executable_path/../Resources/libs/libtbb.dylib LuxCore.app/Contents/Resources/libs/libembree3.3.dylib
install_name_tool -change @rpath/libtbbmalloc.dylib @executable_path/../Resources/libs/libtbbmalloc.dylib LuxCore.app/Contents/Resources/libs/libembree3.3.dylib

#libOpenImageDenoise

cp -f $DEPS_SOURCE/lib/libOpenImageDenoise.1.0.0.dylib LuxCore.app/Contents/Resources/libs/libOpenImageDenoise.1.0.0.dylib
chmod +w LuxCore.app/Contents/Resources/libs/libOpenImageDenoise.1.0.0.dylib
install_name_tool -id @executable_path/../Resources/libs/libOpenImageDenoise.1.0.0.dylib LuxCore.app/Contents/Resources/libs/libOpenImageDenoise.1.0.0.dylib
install_name_tool -change @rpath/libtbb.dylib @executable_path/../Resources/libs/libtbb.dylib LuxCore.app/Contents/Resources/libs/libOpenImageDenoise.1.0.0.dylib
install_name_tool -change @rpath/libtbbmalloc.dylib @executable_path/../Resources/libs/libtbbmalloc.dylib LuxCore.app/Contents/Resources/libs/libOpenImageDenoise.1.0.0.dylib

#libtbb

cp -f $DEPS_SOURCE/lib/libtbb.dylib LuxCore.app/Contents/Resources/libs/libtbb.dylib
chmod +w LuxCore.app/Contents/Resources/libs/libtbb.dylib
install_name_tool -id @executable_path/../Resources/libs/libtbb.dylib LuxCore.app/Contents/Resources/libs/libtbb.dylib

#libtiff

cp -f $DEPS_SOURCE/lib/libtiff.5.dylib LuxCore.app/Contents/Resources/libs/libtiff.5.dylib
chmod +w LuxCore.app/Contents/Resources/libs/libtiff.5.dylib
install_name_tool -id @executable_path/../Resources/libs/libtiff.5.dylib LuxCore.app/Contents/Resources/libs/libtiff.5.dylib

#libOpenImageIO

cp -f $DEPS_SOURCE/lib/libOpenImageIO.1.8.dylib LuxCore.app/Contents/Resources/libs/libOpenImageIO.1.8.dylib
chmod +w LuxCore.app/Contents/Resources/libs/libOpenImageIO.1.8.dylib
install_name_tool -id @executable_path/../Resources/libs/libOpenImageIO.1.8.dylib LuxCore.app/Contents/Resources/libs/libOpenImageIO.1.8.dylib
install_name_tool -change @rpath/libtiff.5.dylib @executable_path/../Resources/libs/libtiff.5.dylib LuxCore.app/Contents/Resources/libs/libOpenImageIO.1.8.dylib

#libtbbmalloc

cp -f $DEPS_SOURCE/lib/libtbbmalloc.dylib LuxCore.app/Contents/Resources/libs/libtbbmalloc.dylib
chmod +w LuxCore.app/Contents/Resources/libs/libtbbmalloc.dylib
install_name_tool -id @executable_path/../Resources/libs/libtbbmalloc.dylib LuxCore.app/Contents/Resources/libs/libtbbmalloc.dylib

#luxcoreui

install_name_tool -change @rpath/libomp.dylib @executable_path/../Resources/libs/libomp.dylib LuxCore.app/Contents/MacOS/luxcoreui
install_name_tool -change @rpath/libembree3.3.dylib @executable_path/../Resources/libs/libembree3.3.dylib LuxCore.app/Contents/MacOS/luxcoreui
install_name_tool -change @rpath/libtbb.dylib @executable_path/../Resources/libs/libtbb.dylib LuxCore.app/Contents/MacOS/luxcoreui
install_name_tool -change @rpath/libOpenImageIO.1.8.dylib @executable_path/../Resources/libs/libOpenImageIO.1.8.dylib LuxCore.app/Contents/MacOS/luxcoreui
install_name_tool -change @rpath/libtbbmalloc.dylib @executable_path/../Resources/libs/libtbbmalloc.dylib LuxCore.app/Contents/MacOS/luxcoreui
install_name_tool -change @rpath/libtiff.5.dylib @executable_path/../Resources/libs/libtiff.5.dylib LuxCore.app/Contents/MacOS/luxcoreui
install_name_tool -change @rpath/libOpenImageDenoise.0.dylib @executable_path/../Resources/libs/libOpenImageDenoise.1.0.0.dylib LuxCore.app/Contents/MacOS/luxcoreui

echo "LuxCoreUi installed"

###luxcoreconsole bundle

mkdir luxcoreconsole

cp ../build/luxcoreconsole luxcoreconsole

cd luxcoreconsole

#libomp

mkdir -p ./libs/
cp -f $DEPS_SOURCE/lib/libomp.dylib ./libs/libomp.dylib
chmod +w ./libs/libomp.dylib
install_name_tool -id @executable_path/libs/libomp.dylib ./libs/libomp.dylib

#libembree 

cp -f $DEPS_SOURCE/lib/libembree3.3.dylib ./libs/libembree3.3.dylib
chmod +w ./libs/libembree3.3.dylib
install_name_tool -id @executable_path/libs/libembree3.3.dylib ./libs/libembree3.3.dylib
install_name_tool -change @rpath/libtbb.dylib @executable_path/libs/libtbb.dylib ./libs/libembree3.3.dylib
install_name_tool -change @rpath/libtbbmalloc.dylib @executable_path/libs/libtbbmalloc.dylib ./libs/libembree3.3.dylib

#libOpenImageDenoise

cp -f $DEPS_SOURCE/lib/libOpenImageDenoise.1.0.0.dylib ./libs/libOpenImageDenoise.1.0.0.dylib
chmod +w ./libs/libOpenImageDenoise.1.0.0.dylib
install_name_tool -id @executable_path/libs/libOpenImageDenoise.1.0.0.dylib ./libs/libOpenImageDenoise.1.0.0.dylib
install_name_tool -change @rpath/libtbb.dylib @executable_path/libs/libtbb.dylib ./libs/libOpenImageDenoise.1.0.0.dylib
install_name_tool -change @rpath/libtbbmalloc.dylib @executable_path/libs/libtbbmalloc.dylib ./libs/libOpenImageDenoise.1.0.0.dylib

#libtbb

cp -f $DEPS_SOURCE/lib/libtbb.dylib ./libs/libtbb.dylib
chmod +w ./libs/libtbb.dylib
install_name_tool -id @executable_path/libs/libtbb.dylib ./libs/libtbb.dylib

#libtiff

cp -f $DEPS_SOURCE/lib/libtiff.5.dylib ./libs/libtiff.5.dylib
chmod +w ./libs/libtiff.5.dylib
install_name_tool -id @executable_path/libs/libtiff.5.dylib ./libs/libtiff.5.dylib

#libOpenImageIO

cp -f $DEPS_SOURCE/lib/libOpenImageIO.1.8.dylib ./libs/libOpenImageIO.1.8.dylib
chmod +w ./libs/libOpenImageIO.1.8.dylib
install_name_tool -id @executable_path/libs/libOpenImageIO.1.8.dylib ./libs/libOpenImageIO.1.8.dylib
install_name_tool -change @rpath/libtiff.5.dylib @executable_path/libs/libtiff.5.dylib ./libs/libOpenImageIO.1.8.dylib

#libtbbmalloc

cp -f $DEPS_SOURCE/lib/libtbbmalloc.dylib ./libs/libtbbmalloc.dylib
chmod +w ./libs/libtbbmalloc.dylib
install_name_tool -id @executable_path/libs/libtbbmalloc.dylib ./libs/libtbbmalloc.dylib

#luxcoreconsole

install_name_tool -change @rpath/libomp.dylib @executable_path/libs/libomp.dylib ./luxcoreconsole
install_name_tool -change @rpath/libembree3.3.dylib @executable_path/libs/libembree3.3.dylib ./luxcoreconsole
install_name_tool -change @rpath/libtbb.dylib @executable_path/libs/libtbb.dylib ./luxcoreconsole
install_name_tool -change @rpath/libOpenImageIO.1.8.dylib @executable_path/libs/libOpenImageIO.1.8.dylib ./luxcoreconsole
install_name_tool -change @rpath/libtbbmalloc.dylib @executable_path/libs/libtbbmalloc.dylib ./luxcoreconsole
install_name_tool -change @rpath/libtiff.5.dylib @executable_path/libs/libtiff.5.dylib ./luxcoreconsole
install_name_tool -change @rpath/libOpenImageDenoise.0.dylib @executable_path/libs/libOpenImageDenoise.1.0.0.dylib ./luxcoreconsole

echo "LuxCoreConsole installed"
cd ..

### pyluxcore.so

mkdir pyluxcore

cp ../build/lib/pyluxcore* pyluxcore

cd pyluxcore

#libomp

cp -f $DEPS_SOURCE/lib/libomp.dylib ./libomp.dylib
chmod +w ./libomp.dylib
install_name_tool -id @loader_path/libomp.dylib ./libomp.dylib

#libembree 

cp -f $DEPS_SOURCE/lib/libembree3.3.dylib ./libembree3.3.dylib
chmod +w ./libembree3.3.dylib
install_name_tool -id @loader_path/libembree3.3.dylib ./libembree3.3.dylib
install_name_tool -change @rpath/libtbb.dylib @loader_path/libtbb.dylib ./libembree3.3.dylib
install_name_tool -change @rpath/libtbbmalloc.dylib @loader_path/libtbbmalloc.dylib ./libembree3.3.dylib

#libtbb

cp -f $DEPS_SOURCE/lib/libtbb.dylib ./libtbb.dylib
chmod +w ./libtbb.dylib
install_name_tool -id @loader_path/libtbb.dylib ./libtbb.dylib

#libtiff

cp -f $DEPS_SOURCE/lib/libtiff.5.dylib ./libtiff.5.dylib
chmod +w ./libtiff.5.dylib
install_name_tool -id @loader_path/libtiff.5.dylib ./libtiff.5.dylib

#libOpenImageIO

cp -f $DEPS_SOURCE/lib/libOpenImageIO.1.8.dylib ./libOpenImageIO.1.8.dylib
chmod +w ./libOpenImageIO.1.8.dylib
install_name_tool -id @loader_path/libOpenImageIO.1.8.dylib ./libOpenImageIO.1.8.dylib
install_name_tool -change @rpath/libtiff.5.dylib @loader_path/libtiff.5.dylib ./libOpenImageIO.1.8.dylib

#libtbbmalloc

cp -f $DEPS_SOURCE/lib/libtbbmalloc.dylib ./libtbbmalloc.dylib
chmod +w ./libtbbmalloc.dylib
install_name_tool -id @loader_path/libtbbmalloc.dylib ./libtbbmalloc.dylib

#pyluxcore.so

install_name_tool -change @rpath/libomp.dylib @loader_path/libomp.dylib pyluxcore.so
install_name_tool -change @rpath/libembree3.3.dylib @loader_path/libembree3.3.dylib pyluxcore.so
install_name_tool -change @rpath/libtbb.dylib @loader_path/libtbb.dylib pyluxcore.so
install_name_tool -change @rpath/libtiff.5.dylib @loader_path/libtiff.5.dylib pyluxcore.so
install_name_tool -change @rpath/libOpenImageIO.1.8.dylib @loader_path/libOpenImageIO.1.8.dylib pyluxcore.so
install_name_tool -change @rpath/libtbbmalloc.dylib @loader_path/libtbbmalloc.dylib pyluxcore.so

echo "PyLuxCore installed"

### denoise

#libOpenImageDenoise

cp -f $DEPS_SOURCE/lib/libOpenImageDenoise.1.0.0.dylib ./libOpenImageDenoise.1.0.0.dylib
chmod +w ./libOpenImageDenoise.1.0.0.dylib
install_name_tool -id @executable_path/libOpenImageDenoise.1.0.0.dylib ./libOpenImageDenoise.1.0.0.dylib
install_name_tool -change @rpath/libtbb.dylib @executable_path/libtbb.dylib ./libOpenImageDenoise.1.0.0.dylib
install_name_tool -change @rpath/libtbbmalloc.dylib @executable_path/libtbbmalloc.dylib ./libOpenImageDenoise.1.0.0.dylib

#denoise
cp ../../macos/bin/denoise .
chmod +w ./denoise
install_name_tool -id @executable_path/denoise ./denoise
install_name_tool -change @rpath/libOpenImageDenoise.0.dylib @executable_path/libOpenImageDenoise.1.0.0.dylib denoise
install_name_tool -change @rpath/libOpenImageDenoise.1.0.0.dylib @executable_path/libOpenImageDenoise.1.0.0.dylib denoise
install_name_tool -change @rpath/libtbb.dylib @executable_path/libtbb.dylib denoise
install_name_tool -change @rpath/libtbbmalloc.dylib @executable_path/libtbbmalloc.dylib denoise

echo "denoise installed"

cd ../..

echo "Creating DMG ..."

hdiutil create LuxCoreRender2.3alpha0.dmg -volname "LuxCoreRender2.3alpha0" -fs HFS+ -srcfolder release_OSX_OCL/
<<<<<<< HEAD

#hdiutil create LuxCoreRender2.3alpha0.dmg -volname "LuxCoreRender2.3alpha0" -fs HFS+ -srcfolder release_OSX_OCL/
=======
>>>>>>> parent of fd385f060... Added version string to name of MacOS artifacts

mv LuxCoreRender2.3alpha0.dmg $BUILD_ARTIFACTSTAGINGDIRECTORY/LuxCoreRender2.3alpha0.dmg

<<<<<<< HEAD
=======
mv LuxCoreRender2.3alpha0.dmg $BUILD_ARTIFACTSTAGINGDIRECTORY/LuxCoreRender2.3alpha0.dmg
>>>>>>> parent of fd385f060... Added version string to name of MacOS artifacts






