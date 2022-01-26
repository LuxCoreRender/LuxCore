#!/bin/bash

DEPS_SOURCE=`pwd`/macos

### packing opencl less version

mkdir release_OSX

###luxcoreui bundle

echo "Bundeling Unified LuxCore Version"

cp -R macos/mac_bundle/LuxCore.app release_OSX
mkdir -p release_OSX/LuxCore.app/Contents/MacOS
cp build/Release/luxcoreui release_OSX/LuxCore.app/Contents/MacOS

cd release_OSX

mkdir -p LuxCore.app/Contents/Resources/libs/

# #libomp
#
# cp -f $DEPS_SOURCE/lib/libomp.dylib LuxCore.app/Contents/Resources/libs/libomp.dylib
# chmod +w LuxCore.app/Contents/Resources/libs/libomp.dylib
# install_name_tool -id @executable_path/../Resources/libs/libomp.dylib LuxCore.app/Contents/Resources/libs/libomp.dylib

#libembree

cp -f $DEPS_SOURCE/lib/libembree3.3.dylib LuxCore.app/Contents/Resources/libs/libembree3.3.dylib
chmod +w LuxCore.app/Contents/Resources/libs/libembree3.3.dylib
install_name_tool -id @executable_path/../Resources/libs/libembree3.3.dylib
install_name_tool -change @rpath/libtbb.dylib @executable_path/../Resources/libs/libtbb.dylib LuxCore.app/Contents/Resources/libs/libembree3.3.dylib
install_name_tool -change @rpath/libtbbmalloc.dylib @executable_path/../Resources/libs/libtbbmalloc.dylib LuxCore.app/Contents/Resources/libs/libembree3.3.dylib

# #libOpenImageDenoise
#
# cp -f $DEPS_SOURCE/lib/libOpenImageDenoise.1.4.0.dylib LuxCore.app/Contents/Resources/libs/libOpenImageDenoise.1.4.0.dylib
# chmod +w LuxCore.app/Contents/Resources/libs/libOpenImageDenoise.1.4.0.dylib
# install_name_tool -id @executable_path/../Resources/libs/libOpenImageDenoise.1.4.0.dylib LuxCore.app/Contents/Resources/libs/libOpenImageDenoise.1.4.0.dylib
# install_name_tool -change @rpath/libtbb.dylib @executable_path/../Resources/libs/libtbb.dylib LuxCore.app/Contents/Resources/libs/libOpenImageDenoise.1.4.0.dylib
# install_name_tool -change @rpath/libtbbmalloc.dylib @executable_path/../Resources/libs/libtbbmalloc.dylib LuxCore.app/Contents/Resources/libs/libOpenImageDenoise.1.4.0.dylib

#libtbb

cp -f $DEPS_SOURCE/lib/libtbb.dylib LuxCore.app/Contents/Resources/libs/libtbb.dylib
chmod +w LuxCore.app/Contents/Resources/libs/libtbb.dylib
install_name_tool -id @executable_path/../Resources/libs/libtbb.dylib LuxCore.app/Contents/Resources/libs/libtbb.dylib

# #libtiff
#
# cp -f $DEPS_SOURCE/lib/libtiff.5.dylib LuxCore.app/Contents/Resources/libs/libtiff.5.dylib
# chmod +w LuxCore.app/Contents/Resources/libs/libtiff.5.dylib
# install_name_tool -id @executable_path/../Resources/libs/libtiff.5.dylib LuxCore.app/Contents/Resources/libs/libtiff.5.dylib
#
# #libOpenImageIO
#
# cp -f $DEPS_SOURCE/lib/libOpenImageIO.2.2.dylib LuxCore.app/Contents/Resources/libs/libOpenImageIO.2.2.dylib
# chmod +w LuxCore.app/Contents/Resources/libs/libOpenImageIO.2.2.dylib
# install_name_tool -id @executable_path/../Resources/libs/libOpenImageIO.2.2.dylib LuxCore.app/Contents/Resources/libs/libOpenImageIO.2.2.dylib
# install_name_tool -change @rpath/libtiff.5.dylib @executable_path/../Resources/libs/libtiff.5.dylib LuxCore.app/Contents/Resources/libs/libOpenImageIO.2.2.dylib

#libtbbmalloc

cp -f $DEPS_SOURCE/lib/libtbbmalloc.dylib LuxCore.app/Contents/Resources/libs/libtbbmalloc.dylib
chmod +w LuxCore.app/Contents/Resources/libs/libtbbmalloc.dylib
install_name_tool -id @executable_path/../Resources/libs/libtbbmalloc.dylib LuxCore.app/Contents/Resources/libs/libtbbmalloc.dylib

# #cuda
#
# cp -f $DEPS_SOURCE/lib/libcuda.dylib LuxCore.app/Contents/Resources/libs/libcuda.dylib
# cp -f $DEPS_SOURCE/lib/libnvrtc.dylib LuxCore.app/Contents/Resources/libs/libnvrtc.dylib

#luxcoreui

#install_name_tool -change @rpath/libomp.dylib @executable_path/../Resources/libs/libomp.dylib LuxCore.app/Contents/MacOS/luxcoreui
install_name_tool -change @rpath/libembree3.3.dylib @executable_path/../Resources/libs/libembree3.3.dylib LuxCore.app/Contents/MacOS/luxcoreui
install_name_tool -change @rpath/libtbb.dylib @executable_path/../Resources/libs/libtbb.dylib LuxCore.app/Contents/MacOS/luxcoreui
#install_name_tool -change @rpath/libOpenImageIO.2.2.dylib @executable_path/../Resources/libs/libOpenImageIO.2.2.dylib LuxCore.app/Contents/MacOS/luxcoreui
install_name_tool -change @rpath/libtbbmalloc.dylib @executable_path/../Resources/libs/libtbbmalloc.dylib LuxCore.app/Contents/MacOS/luxcoreui
#install_name_tool -change @rpath/libtiff.5.dylib @executable_path/../Resources/libs/libtiff.5.dylib LuxCore.app/Contents/MacOS/luxcoreui
#install_name_tool -change @rpath/libOpenImageDenoise.1.dylib @executable_path/../Resources/libs/libOpenImageDenoise.1.4.0.dylib LuxCore.app/Contents/MacOS/luxcoreui

echo "LuxCoreUi installed"

###luxcoreconsole

cp ../build/Release/luxcoreconsole LuxCore.app/Contents/MacOS

#luxcoreconsole

#install_name_tool -change @rpath/libomp.dylib @executable_path/../Resources/libs/libomp.dylib ./LuxCore.app/Contents/MacOS/luxcoreconsole
install_name_tool -change @rpath/libembree3.3.dylib @executable_path/../Resources/libs/libembree3.3.dylib ./LuxCore.app/Contents/MacOS/luxcoreconsole
install_name_tool -change @rpath/libtbb.dylib @executable_path/../Resources/libs/libtbb.dylib ./LuxCore.app/Contents/MacOS/luxcoreconsole
#install_name_tool -change @rpath/libOpenImageIO.2.2.dylib @executable_path/../Resources/libs/libOpenImageIO.2.2.dylib ./LuxCore.app/Contents/MacOS/luxcoreconsole
install_name_tool -change @rpath/libtbbmalloc.dylib @executable_path/../Resources/libs/libtbbmalloc.dylib ./LuxCore.app/Contents/MacOS/luxcoreconsole
#install_name_tool -change @rpath/libtiff.5.dylib @executable_path/../Resources/libs/libtiff.5.dylib ./LuxCore.app/Contents/MacOS/luxcoreconsole
#install_name_tool -change @rpath/libOpenImageDenoise.1.dylib @executable_path/../Resources/libs/libOpenImageDenoise.1.4.0.dylib ./LuxCore.app/Contents/MacOS/luxcoreconsole

echo "LuxCoreConsole installed"

### pyluxcore.so

mkdir pyluxcore

cp ../build/lib/Release/pyluxcore.so pyluxcore
cp ../build/lib/pyluxcoretools.zip pyluxcore

cd pyluxcore

# #libomp
#
# cp -f $DEPS_SOURCE/lib/libomp.dylib ./libomp.dylib
# chmod +w ./libomp.dylib
# install_name_tool -id @loader_path/libomp.dylib ./libomp.dylib

#libembree

cp -f $DEPS_SOURCE/lib/libembree3.3.dylib ./libembree3.3.dylib

#libtbb

cp -f $DEPS_SOURCE/lib/libtbb.dylib ./libtbb.dylib
chmod +w ./libtbb.dylib
install_name_tool -id @loader_path/libtbb.dylib ./libtbb.dylib

# #libtiff
#
# cp -f $DEPS_SOURCE/lib/libtiff.5.dylib ./libtiff.5.dylib
# chmod +w ./libtiff.5.dylib
# install_name_tool -id @loader_path/libtiff.5.dylib ./libtiff.5.dylib
#
# #libOpenImageIO
#
# cp -f $DEPS_SOURCE/lib/libOpenImageIO.2.2.dylib ./libOpenImageIO.2.2.dylib
# chmod +w ./libOpenImageIO.2.2.dylib
# install_name_tool -id @loader_path/libOpenImageIO.2.2.dylib ./libOpenImageIO.2.2.dylib
# install_name_tool -change @rpath/libtiff.5.dylib @loader_path/libtiff.5.dylib ./libOpenImageIO.2.2.dylib

#libtbbmalloc

cp -f $DEPS_SOURCE/lib/libtbbmalloc.dylib ./libtbbmalloc.dylib
chmod +w ./libtbbmalloc.dylib
install_name_tool -id @loader_path/libtbbmalloc.dylib ./libtbbmalloc.dylib

# #libOpenImageDenoise
#
# cp -f $DEPS_SOURCE/lib/libOpenImageDenoise.1.4.0.dylib ./libOpenImageDenoise.1.4.0.dylib
# chmod +w ./libOpenImageDenoise.1.4.0.dylib
# install_name_tool -id @loader_path/libOpenImageDenoise.1.4.0.dylib ./libOpenImageDenoise.1.4.0.dylib
# install_name_tool -change @rpath/libtbb.dylib @loader_path/libtbb.dylib ./libOpenImageDenoise.1.4.0.dylib
# install_name_tool -change @rpath/libtbbmalloc.dylib @loader_path/libtbbmalloc.dylib ./libOpenImageDenoise.1.4.0.dylib
#
# #cuda
#
# cp -f $DEPS_SOURCE/lib/libcuda.dylib ./libcuda.dylib
# cp -f $DEPS_SOURCE/lib/libnvrtc.dylib ./libnvrtc.dylib

#pyluxcore.so

#install_name_tool -change @rpath/libomp.dylib @loader_path/libomp.dylib pyluxcore.so
install_name_tool -change @rpath/libembree3.3.dylib @loader_path/libembree3.3.dylib pyluxcore.so
install_name_tool -change @rpath/libtbb.dylib @loader_path/libtbb.dylib pyluxcore.so
#install_name_tool -change @rpath/libtiff.5.dylib @loader_path/libtiff.5.dylib pyluxcore.so
#install_name_tool -change @rpath/libOpenImageIO.2.2.dylib @loader_path/libOpenImageIO.2.2.dylib pyluxcore.so
install_name_tool -change @rpath/libtbbmalloc.dylib @loader_path/libtbbmalloc.dylib pyluxcore.so
#install_name_tool -change @rpath/libOpenImageDenoise.1.dylib @loader_path/libOpenImageDenoise.1.4.0.dylib pyluxcore.so

echo "PyLuxCore installed"

### oidnDenoise

#oidnDenoise
cp ../../macos/bin/oidnDenoise .
chmod +w ./oidnDenoise
install_name_tool -id @executable_path/oidnDenoise ./oidnDenoise
#install_name_tool -change @rpath/libOpenImageDenoise.1.dylib @executable_path/libOpenImageDenoise.1.4.0.dylib oidnDenoise
#install_name_tool -change @rpath/libOpenImageDenoise.1.4.0.dylib @executable_path/libOpenImageDenoise.1.4.0.dylib oidnDenoise
install_name_tool -change @rpath/libtbb.dylib @executable_path/libtbb.dylib oidnDenoise
install_name_tool -change @rpath/libtbbmalloc.dylib @executable_path/libtbbmalloc.dylib oidnDenoise

echo "oidnDenoise installed"

cd ../..

./scripts/azurepipeline/macos/codesign.sh ./release_OSX/pyluxcore

# Set up correct names for release version and SDK
if [[ -z "$VERSION_STRING" ]] ; then
    VERSION_STRING=latest
fi

# if [[ "$FINAL" == "TRUE" ]] ; then
    # SDK_BUILD=-sdk
	# # Required to link executables
	# export LD_LIBRARY_PATH="`pwd`/LinuxCompile/target-64-sse2/lib:$LD_LIBRARY_PATH"
# fi

#for fancy background dmg

#create-dmg --volname "LuxCoreRender$VERSION_STRING" --background macos/mac_bundle/back-dmg.jpg --window-size 512 300 --app-drop-link 60 175 --icon-size 64 --text-size 12 --icon LuxCore.app 0 65 --icon luxcoreconsole 120 65 --icon pyluxcore 250 65  --volicon macos/mac_bundle/LuxCore.app/Contents/Resources/luxcoreui.icns luxcorerender-$VERSION_STRING-mac64$SDK_BUILD.dmg release_OSX/

### creating opencl less DMG

echo "Creating Unified Version DMG ..."

hdiutil create luxcorerender-$VERSION_STRING-mac64$SDK_BUILD.dmg -volname "luxcorerender-$VERSION_STRING-mac64" -fs HFS+ -srcfolder release_OSX/

echo "Staging Unified Version DMG"

mv luxcorerender-$VERSION_STRING-mac64$SDK_BUILD.dmg $BUILD_ARTIFACTSTAGINGDIRECTORY/luxcorerender-$VERSION_STRING-mac64$SDK_BUILD.dmg

echo "Finished ! All is good !"
