#!/bin/bash

# Luxcoreui patching

###luxcoreui bundle
cp -R macos/mac_bundle/LuxCore.app release_OSX
cp build/luxcoreui release_OSX/LuxCore.app/Contents/MacOS

pushd release_OSX

mkdir -p LuxCore.app/Contents/Resources/libs/

#libiomp.dylib

cp -f ../macos/lib/libomp.dylib LuxCore.app/Contents/Resources/libs/libomp.dylib
chmod +w LuxCore.app/Contents/Resources/libs/libomp.dylib
install_name_tool -id @executable_path/../Resources/libs/libomp.dylib LuxCore.app/Contents/Resources/libs/libomp.dylib
install_name_tool -change /Users/drquader/Documents/GitHub/LuxCore/macos/lib/libomp.dylib @executable_path/../Resources/libs/libomp.dylib LuxCore.app/Contents/Resources/libs/libomp.dylib
install_name_tool -change @rpath/libomp.dylib @executable_path/../Resources/libs/libomp.dylib LuxCore.app/Contents/Resources/libs/libomp.dylib
install_name_tool -change /Users/drquader/Documents/GitHub/LuxCore/macos/lib/libembree3.3.dylib @executable_path/../Resources/libs/libembree3.3.dylib LuxCore.app/Contents/Resources/libs/libomp.dylib
install_name_tool -change @rpath/libembree3.3.dylib @executable_path/../Resources/libs/libembree3.3.dylib LuxCore.app/Contents/Resources/libs/libomp.dylib
install_name_tool -change /Users/drquader/Documents/GitHub/MacOSCompileDeps/macos/lib/libOpenImageDenoise.1.0.0.dylib @executable_path/../Resources/libs/libOpenImageDenoise.1.0.0.dylib LuxCore.app/Contents/Resources/libs/libomp.dylib
install_name_tool -change /Users/drquader/Documents/GitHub/MacOSCompileDeps/macos/lib/libOpenImageDenoise.0.dylib @executable_path/../Resources/libs/libOpenImageDenoise.1.0.0.dylib LuxCore.app/Contents/Resources/libs/libomp.dylib
install_name_tool -change /Users/drquader/Documents/GitHub/LuxCore/macos/lib/libtbb.dylib @executable_path/../Resources/libs/libtbb.dylib LuxCore.app/Contents/Resources/libs/libomp.dylib
install_name_tool -change @rpath/libtbb.dylib @executable_path/../Resources/libs/libtbb.dylib LuxCore.app/Contents/Resources/libs/libomp.dylib
install_name_tool -change /Users/drquader/Documents/GitHub/MacOSCompileDeps/macos/lib/libtiff.5.dylib @executable_path/../Resources/libs/libtiff.5.dylib LuxCore.app/Contents/Resources/libs/libomp.dylib
install_name_tool -change /Users/drquader/Documents/GitHub/LuxCore/macos/lib/libOpenImageIO.1.8.dylib @executable_path/../Resources/libs/libOpenImageIO.1.8.dylib LuxCore.app/Contents/Resources/libs/libomp.dylib
install_name_tool -change @rpath/libOpenImageIO.1.8.dylib @executable_path/../Resources/libs/libOpenImageIO.1.8.dylib LuxCore.app/Contents/Resources/libs/libomp.dylib
install_name_tool -change /Users/drquader/Documents/GitHub/LuxCore/macos/lib/libtbbmalloc.dylib @executable_path/../Resources/libs/libtbbmalloc.dylib LuxCore.app/Contents/Resources/libs/libomp.dylib
install_name_tool -change @rpath/libtbbmalloc.dylib @executable_path/../Resources/libs/libtbbmalloc.dylib LuxCore.app/Contents/Resources/libs/libomp.dylib

#libembree

cp -f ../macos/lib/libembree3.3.dylib LuxCore.app/Contents/Resources/libs/libembree3.3.dylib
chmod +w LuxCore.app/Contents/Resources/libs/libembree3.3.dylib
install_name_tool -id @executable_path/../Resources/libs/libembree3.3.dylib LuxCore.app/Contents/Resources/libs/libembree3.3.dylib
install_name_tool -change /Users/drquader/Documents/GitHub/LuxCore/macos/lib/libomp.dylib @executable_path/../Resources/libs/libomp.dylib LuxCore.app/Contents/Resources/libs/libembree3.3.dylib
install_name_tool -change @rpath/libomp.dylib @executable_path/../Resources/libs/libomp.dylib LuxCore.app/Contents/Resources/libs/libembree3.3.dylib
install_name_tool -change /Users/drquader/Documents/GitHub/LuxCore/macos/lib/libembree3.3.dylib @executable_path/../Resources/libs/libembree3.3.dylib LuxCore.app/Contents/Resources/libs/libembree3.3.dylib
install_name_tool -change @rpath/libembree3.3.dylib @executable_path/../Resources/libs/libembree3.3.dylib LuxCore.app/Contents/Resources/libs/libembree3.3.dylib
install_name_tool -change /Users/drquader/Documents/GitHub/MacOSCompileDeps/macos/lib/libOpenImageDenoise.1.0.0.dylib @executable_path/../Resources/libs/libOpenImageDenoise.1.0.0.dylib LuxCore.app/Contents/Resources/libs/libembree3.3.dylib
install_name_tool -change /Users/drquader/Documents/GitHub/MacOSCompileDeps/macos/lib/libOpenImageDenoise.0.dylib @executable_path/../Resources/libs/libOpenImageDenoise.1.0.0.dylib LuxCore.app/Contents/Resources/libs/libembree3.3.dylib
install_name_tool -change /Users/drquader/Documents/GitHub/LuxCore/macos/lib/libtbb.dylib @executable_path/../Resources/libs/libtbb.dylib LuxCore.app/Contents/Resources/libs/libembree3.3.dylib
install_name_tool -change @rpath/libtbb.dylib @executable_path/../Resources/libs/libtbb.dylib LuxCore.app/Contents/Resources/libs/libembree3.3.dylib
install_name_tool -change /Users/drquader/Documents/GitHub/MacOSCompileDeps/macos/lib/libtiff.5.dylib @executable_path/../Resources/libs/libtiff.5.dylib LuxCore.app/Contents/Resources/libs/libembree3.3.dylib
install_name_tool -change /Users/drquader/Documents/GitHub/LuxCore/macos/lib/libOpenImageIO.1.8.dylib @executable_path/../Resources/libs/libOpenImageIO.1.8.dylib LuxCore.app/Contents/Resources/libs/libembree3.3.dylib
install_name_tool -change @rpath/libOpenImageIO.1.8.dylib @executable_path/../Resources/libs/libOpenImageIO.1.8.dylib LuxCore.app/Contents/Resources/libs/libembree3.3.dylib
install_name_tool -change /Users/drquader/Documents/GitHub/LuxCore/macos/lib/libtbbmalloc.dylib @executable_path/../Resources/libs/libtbbmalloc.dylib LuxCore.app/Contents/Resources/libs/libembree3.3.dylib
install_name_tool -change @rpath/libtbbmalloc.dylib @executable_path/../Resources/libs/libtbbmalloc.dylib LuxCore.app/Contents/Resources/libs/libembree3.3.dylib

#libOpenImageDenoise

cp -f ../macos/lib/libOpenImageDenoise.1.0.0.dylib LuxCore.app/Contents/Resources/libs/libOpenImageDenoise.1.0.0.dylib
chmod +w LuxCore.app/Contents/Resources/libs/libOpenImageDenoise.1.0.0.dylib
install_name_tool -id @executable_path/../Resources/libs/libOpenImageDenoise.1.0.0.dylib LuxCore.app/Contents/Resources/libs/libOpenImageDenoise.1.0.0.dylib
install_name_tool -change /Users/drquader/Documents/GitHub/LuxCore/macos/lib/libomp.dylib @executable_path/../Resources/libs/libomp.dylib LuxCore.app/Contents/Resources/libs/libOpenImageDenoise.1.0.0.dylib
install_name_tool -change @rpath/libomp.dylib @executable_path/../Resources/libs/libomp.dylib LuxCore.app/Contents/Resources/libs/libOpenImageDenoise.1.0.0.dylib
install_name_tool -change /Users/drquader/Documents/GitHub/LuxCore/macos/lib/libembree3.3.dylib @executable_path/../Resources/libs/libembree3.3.dylib LuxCore.app/Contents/Resources/libs/libOpenImageDenoise.1.0.0.dylib
install_name_tool -change @rpath/libembree3.3.dylib @executable_path/../Resources/libs/libembree3.3.dylib LuxCore.app/Contents/Resources/libs/libOpenImageDenoise.1.0.0.dylib
install_name_tool -change /Users/drquader/Documents/GitHub/MacOSCompileDeps/macos/lib/libOpenImageDenoise.1.0.0.dylib @executable_path/../Resources/libs/libOpenImageDenoise.1.0.0.dylib LuxCore.app/Contents/Resources/libs/libOpenImageDenoise.1.0.0.dylib
install_name_tool -change /Users/drquader/Documents/GitHub/MacOSCompileDeps/macos/lib/libOpenImageDenoise.0.dylib @executable_path/../Resources/libs/libOpenImageDenoise.1.0.0.dylib LuxCore.app/Contents/Resources/libs/libOpenImageDenoise.1.0.0.dylib
install_name_tool -change /Users/drquader/Documents/GitHub/LuxCore/macos/lib/libtbb.dylib @executable_path/../Resources/libs/libtbb.dylib LuxCore.app/Contents/Resources/libs/libOpenImageDenoise.1.0.0.dylib
install_name_tool -change @rpath/libtbb.dylib @executable_path/../Resources/libs/libtbb.dylib LuxCore.app/Contents/Resources/libs/libOpenImageDenoise.1.0.0.dylib
install_name_tool -change /Users/drquader/Documents/GitHub/MacOSCompileDeps/macos/lib/libtiff.5.dylib @executable_path/../Resources/libs/libtiff.5.dylib LuxCore.app/Contents/Resources/libs/libOpenImageDenoise.1.0.0.dylib
install_name_tool -change /Users/drquader/Documents/GitHub/LuxCore/macos/lib/libOpenImageIO.1.8.dylib @executable_path/../Resources/libs/libOpenImageIO.1.8.dylib LuxCore.app/Contents/Resources/libs/libOpenImageDenoise.1.0.0.dylib
install_name_tool -change @rpath/libOpenImageIO.1.8.dylib @executable_path/../Resources/libs/libOpenImageIO.1.8.dylib LuxCore.app/Contents/Resources/libs/libOpenImageDenoise.1.0.0.dylib
install_name_tool -change /Users/drquader/Documents/GitHub/LuxCore/macos/lib/libtbbmalloc.dylib @executable_path/../Resources/libs/libtbbmalloc.dylib LuxCore.app/Contents/Resources/libs/libOpenImageDenoise.1.0.0.dylib
install_name_tool -change @rpath/libtbbmalloc.dylib @executable_path/../Resources/libs/libtbbmalloc.dylib LuxCore.app/Contents/Resources/libs/libOpenImageDenoise.1.0.0.dylib

#libtbb

cp -f ../macos/lib/libtbb.dylib LuxCore.app/Contents/Resources/libs/libtbb.dylib
chmod +w LuxCore.app/Contents/Resources/libs/libtbb.dylib
install_name_tool -id @executable_path/../Resources/libs/libtbb.dylib LuxCore.app/Contents/Resources/libs/libtbb.dylib
install_name_tool -change /Users/drquader/Documents/GitHub/LuxCore/macos/lib/libomp.dylib @executable_path/../Resources/libs/libomp.dylib LuxCore.app/Contents/Resources/libs/libtbb.dylib
install_name_tool -change @rpath/libomp.dylib @executable_path/../Resources/libs/libomp.dylib LuxCore.app/Contents/Resources/libs/libtbb.dylib
install_name_tool -change /Users/drquader/Documents/GitHub/LuxCore/macos/lib/libembree3.3.dylib @executable_path/../Resources/libs/libembree3.3.dylib LuxCore.app/Contents/Resources/libs/libtbb.dylib
install_name_tool -change @rpath/libembree3.3.dylib @executable_path/../Resources/libs/libembree3.3.dylib LuxCore.app/Contents/Resources/libs/libtbb.dylib
install_name_tool -change /Users/drquader/Documents/GitHub/MacOSCompileDeps/macos/lib/libOpenImageDenoise.1.0.0.dylib @executable_path/../Resources/libs/libOpenImageDenoise.1.0.0.dylib LuxCore.app/Contents/Resources/libs/libtbb.dylib
install_name_tool -change /Users/drquader/Documents/GitHub/MacOSCompileDeps/macos/lib/libOpenImageDenoise.0.dylib @executable_path/../Resources/libs/libOpenImageDenoise.1.0.0.dylib LuxCore.app/Contents/Resources/libs/libtbb.dylib
install_name_tool -change /Users/drquader/Documents/GitHub/LuxCore/macos/lib/libtbb.dylib @executable_path/../Resources/libs/libtbb.dylib LuxCore.app/Contents/Resources/libs/libtbb.dylib
install_name_tool -change @rpath/libtbb.dylib @executable_path/../Resources/libs/libtbb.dylib LuxCore.app/Contents/Resources/libs/libtbb.dylib
install_name_tool -change /Users/drquader/Documents/GitHub/MacOSCompileDeps/macos/lib/libtiff.5.dylib @executable_path/../Resources/libs/libtiff.5.dylib LuxCore.app/Contents/Resources/libs/libtbb.dylib
install_name_tool -change /Users/drquader/Documents/GitHub/LuxCore/macos/lib/libOpenImageIO.1.8.dylib @executable_path/../Resources/libs/libOpenImageIO.1.8.dylib LuxCore.app/Contents/Resources/libs/libtbb.dylib
install_name_tool -change @rpath/libOpenImageIO.1.8.dylib @executable_path/../Resources/libs/libOpenImageIO.1.8.dylib LuxCore.app/Contents/Resources/libs/libtbb.dylib
install_name_tool -change /Users/drquader/Documents/GitHub/LuxCore/macos/lib/libtbbmalloc.dylib @executable_path/../Resources/libs/libtbbmalloc.dylib LuxCore.app/Contents/Resources/libs/libtbb.dylib
install_name_tool -change @rpath/libtbbmalloc.dylib @executable_path/../Resources/libs/libtbbmalloc.dylib LuxCore.app/Contents/Resources/libs/libtbb.dylib

#libtiff

cp -f ../macos/lib/libtiff.5.dylib LuxCore.app/Contents/Resources/libs/libtiff.5.dylib
chmod +w LuxCore.app/Contents/Resources/libs/libtiff.5.dylib
install_name_tool -id @executable_path/../Resources/libs/libtiff.5.dylib LuxCore.app/Contents/Resources/libs/libtiff.5.dylib
install_name_tool -change /Users/drquader/Documents/GitHub/LuxCore/macos/lib/libomp.dylib @executable_path/../Resources/libs/libomp.dylib LuxCore.app/Contents/Resources/libs/libtiff.5.dylib
install_name_tool -change @rpath/libomp.dylib @executable_path/../Resources/libs/libomp.dylib LuxCore.app/Contents/Resources/libs/libtiff.5.dylib
install_name_tool -change /Users/drquader/Documents/GitHub/LuxCore/macos/lib/libembree3.3.dylib @executable_path/../Resources/libs/libembree3.3.dylib LuxCore.app/Contents/Resources/libs/libtiff.5.dylib
install_name_tool -change @rpath/libembree3.3.dylib @executable_path/../Resources/libs/libembree3.3.dylib LuxCore.app/Contents/Resources/libs/libtiff.5.dylib
install_name_tool -change /Users/drquader/Documents/GitHub/MacOSCompileDeps/macos/lib/libOpenImageDenoise.1.0.0.dylib @executable_path/../Resources/libs/libOpenImageDenoise.1.0.0.dylib LuxCore.app/Contents/Resources/libs/libtiff.5.dylib
install_name_tool -change /Users/drquader/Documents/GitHub/MacOSCompileDeps/macos/lib/libOpenImageDenoise.0.dylib @executable_path/../Resources/libs/libOpenImageDenoise.1.0.0.dylib LuxCore.app/Contents/Resources/libs/libtiff.5.dylib
install_name_tool -change /Users/drquader/Documents/GitHub/LuxCore/macos/lib/libtbb.dylib @executable_path/../Resources/libs/libtbb.dylib LuxCore.app/Contents/Resources/libs/libtiff.5.dylib
install_name_tool -change @rpath/libtbb.dylib @executable_path/../Resources/libs/libtbb.dylib LuxCore.app/Contents/Resources/libs/libtiff.5.dylib
install_name_tool -change /Users/drquader/Documents/GitHub/MacOSCompileDeps/macos/lib/libtiff.5.dylib @executable_path/../Resources/libs/libtiff.5.dylib LuxCore.app/Contents/Resources/libs/libtiff.5.dylib
install_name_tool -change /Users/drquader/Documents/GitHub/LuxCore/macos/lib/libOpenImageIO.1.8.dylib @executable_path/../Resources/libs/libOpenImageIO.1.8.dylib LuxCore.app/Contents/Resources/libs/libtiff.5.dylib
install_name_tool -change @rpath/libOpenImageIO.1.8.dylib @executable_path/../Resources/libs/libOpenImageIO.1.8.dylib LuxCore.app/Contents/Resources/libs/libtiff.5.dylib
install_name_tool -change /Users/drquader/Documents/GitHub/LuxCore/macos/lib/libtbbmalloc.dylib @executable_path/../Resources/libs/libtbbmalloc.dylib LuxCore.app/Contents/Resources/libs/libtiff.5.dylib
install_name_tool -change @rpath/libtbbmalloc.dylib @executable_path/../Resources/libs/libtbbmalloc.dylib LuxCore.app/Contents/Resources/libs/libtiff.5.dylib

#libOpenImageIO

cp -f ../macos/lib/libOpenImageIO.1.8.dylib LuxCore.app/Contents/Resources/libs/libOpenImageIO.1.8.dylib
chmod +w LuxCore.app/Contents/Resources/libs/libOpenImageIO.1.8.dylib
install_name_tool -id @executable_path/../Resources/libs/libOpenImageIO.1.8.dylib LuxCore.app/Contents/Resources/libs/libOpenImageIO.1.8.dylib
install_name_tool -change /Users/drquader/Documents/GitHub/LuxCore/macos/lib/libomp.dylib @executable_path/../Resources/libs/libomp.dylib LuxCore.app/Contents/Resources/libs/libOpenImageIO.1.8.dylib
install_name_tool -change @rpath/libomp.dylib @executable_path/../Resources/libs/libomp.dylib LuxCore.app/Contents/Resources/libs/libOpenImageIO.1.8.dylib
install_name_tool -change /Users/drquader/Documents/GitHub/LuxCore/macos/lib/libembree3.3.dylib @executable_path/../Resources/libs/libembree3.3.dylib LuxCore.app/Contents/Resources/libs/libOpenImageIO.1.8.dylib
install_name_tool -change @rpath/libembree3.3.dylib @executable_path/../Resources/libs/libembree3.3.dylib LuxCore.app/Contents/Resources/libs/libOpenImageIO.1.8.dylib
install_name_tool -change /Users/drquader/Documents/GitHub/MacOSCompileDeps/macos/lib/libOpenImageDenoise.1.0.0.dylib @executable_path/../Resources/libs/libOpenImageDenoise.1.0.0.dylib LuxCore.app/Contents/Resources/libs/libOpenImageIO.1.8.dylib
install_name_tool -change /Users/drquader/Documents/GitHub/MacOSCompileDeps/macos/lib/libOpenImageDenoise.0.dylib @executable_path/../Resources/libs/libOpenImageDenoise.1.0.0.dylib LuxCore.app/Contents/Resources/libs/libOpenImageIO.1.8.dylib
install_name_tool -change /Users/drquader/Documents/GitHub/LuxCore/macos/lib/libtbb.dylib @executable_path/../Resources/libs/libtbb.dylib LuxCore.app/Contents/Resources/libs/libOpenImageIO.1.8.dylib
install_name_tool -change @rpath/libtbb.dylib @executable_path/../Resources/libs/libtbb.dylib LuxCore.app/Contents/Resources/libs/libOpenImageIO.1.8.dylib
install_name_tool -change /Users/drquader/Documents/GitHub/MacOSCompileDeps/macos/lib/libtiff.5.dylib @executable_path/../Resources/libs/libtiff.5.dylib LuxCore.app/Contents/Resources/libs/libOpenImageIO.1.8.dylib
install_name_tool -change /Users/drquader/Documents/GitHub/LuxCore/macos/lib/libOpenImageIO.1.8.dylib @executable_path/../Resources/libs/libOpenImageIO.1.8.dylib LuxCore.app/Contents/Resources/libs/libOpenImageIO.1.8.dylib
install_name_tool -change @rpath/libOpenImageIO.1.8.dylib @executable_path/../Resources/libs/libOpenImageIO.1.8.dylib LuxCore.app/Contents/Resources/libs/libOpenImageIO.1.8.dylib
install_name_tool -change /Users/drquader/Documents/GitHub/LuxCore/macos/lib/libtbbmalloc.dylib @executable_path/../Resources/libs/libtbbmalloc.dylib LuxCore.app/Contents/Resources/libs/libOpenImageIO.1.8.dylib
install_name_tool -change @rpath/libtbbmalloc.dylib @executable_path/../Resources/libs/libtbbmalloc.dylib LuxCore.app/Contents/Resources/libs/libOpenImageIO.1.8.dylib
install_name_tool -rpath /tmp/macdepsbuild/oiio-Release-1.8.11/dist/macosx/lib @executable_path/../Resources/libs/ LuxCore.app/Contents/Resources/libs/libOpenImageIO.1.8.dylib

#libtbbmalloc

cp -f ../macos/lib/libtbbmalloc.dylib LuxCore.app/Contents/Resources/libs/libtbbmalloc.dylib
chmod +w LuxCore.app/Contents/Resources/libs/libtbbmalloc.dylib
install_name_tool -id @executable_path/../Resources/libs/libtbbmalloc.dylib LuxCore.app/Contents/Resources/libs/libtbbmalloc.dylib
install_name_tool -change /Users/drquader/Documents/GitHub/LuxCore/macos/lib/libomp.dylib @executable_path/../Resources/libs/libomp.dylib LuxCore.app/Contents/Resources/libs/libtbbmalloc.dylib
install_name_tool -change @rpath/libomp.dylib @executable_path/../Resources/libs/libomp.dylib LuxCore.app/Contents/Resources/libs/libtbbmalloc.dylib
install_name_tool -change /Users/drquader/Documents/GitHub/LuxCore/macos/lib/libembree3.3.dylib @executable_path/../Resources/libs/libembree3.3.dylib LuxCore.app/Contents/Resources/libs/libtbbmalloc.dylib
install_name_tool -change @rpath/libembree3.3.dylib @executable_path/../Resources/libs/libembree3.3.dylib LuxCore.app/Contents/Resources/libs/libtbbmalloc.dylib
install_name_tool -change /Users/drquader/Documents/GitHub/MacOSCompileDeps/macos/lib/libOpenImageDenoise.1.0.0.dylib @executable_path/../Resources/libs/libOpenImageDenoise.1.0.0.dylib LuxCore.app/Contents/Resources/libs/libtbbmalloc.dylib
install_name_tool -change /Users/drquader/Documents/GitHub/MacOSCompileDeps/macos/lib/libOpenImageDenoise.0.dylib @executable_path/../Resources/libs/libOpenImageDenoise.1.0.0.dylib LuxCore.app/Contents/Resources/libs/libtbbmalloc.dylib
install_name_tool -change /Users/drquader/Documents/GitHub/LuxCore/macos/lib/libtbb.dylib @executable_path/../Resources/libs/libtbb.dylib LuxCore.app/Contents/Resources/libs/libtbbmalloc.dylib
install_name_tool -change @rpath/libtbb.dylib @executable_path/../Resources/libs/libtbb.dylib LuxCore.app/Contents/Resources/libs/libtbbmalloc.dylib
install_name_tool -change /Users/drquader/Documents/GitHub/MacOSCompileDeps/macos/lib/libtiff.5.dylib @executable_path/../Resources/libs/libtiff.5.dylib LuxCore.app/Contents/Resources/libs/libtbbmalloc.dylib
install_name_tool -change /Users/drquader/Documents/GitHub/LuxCore/macos/lib/libOpenImageIO.1.8.dylib @executable_path/../Resources/libs/libOpenImageIO.1.8.dylib LuxCore.app/Contents/Resources/libs/libtbbmalloc.dylib
install_name_tool -change @rpath/libOpenImageIO.1.8.dylib @executable_path/../Resources/libs/libOpenImageIO.1.8.dylib LuxCore.app/Contents/Resources/libs/libtbbmalloc.dylib
install_name_tool -change /Users/drquader/Documents/GitHub/LuxCore/macos/lib/libtbbmalloc.dylib @executable_path/../Resources/libs/libtbbmalloc.dylib LuxCore.app/Contents/Resources/libs/libtbbmalloc.dylib
install_name_tool -change @rpath/libtbbmalloc.dylib @executable_path/../Resources/libs/libtbbmalloc.dylib LuxCore.app/Contents/Resources/libs/libtbbmalloc.dylib

#luxcoreui

install_name_tool -change /Users/drquader/Documents/GitHub/LuxCore/macos/lib/libomp.dylib @executable_path/../Resources/libs/libomp.dylib LuxCore.app/Contents/MacOS/luxcoreui
install_name_tool -change @rpath/libomp.dylib @executable_path/../Resources/libs/libomp.dylib LuxCore.app/Contents/MacOS/luxcoreui
install_name_tool -change /Users/drquader/Documents/GitHub/LuxCore/macos/lib/libembree3.3.dylib @executable_path/../Resources/libs/libembree3.3.dylib LuxCore.app/Contents/MacOS/luxcoreui
install_name_tool -change @rpath/libembree3.3.dylib @executable_path/../Resources/libs/libembree3.3.dylib LuxCore.app/Contents/MacOS/luxcoreui
install_name_tool -change /Users/drquader/Documents/GitHub/MacOSCompileDeps/macos/lib/libOpenImageDenoise.1.0.0.dylib @executable_path/../Resources/libs/libOpenImageDenoise.1.0.0.dylib LuxCore.app/Contents/MacOS/luxcoreui
install_name_tool -change /Users/drquader/Documents/GitHub/MacOSCompileDeps/macos/lib/libOpenImageDenoise.0.dylib @executable_path/../Resources/libs/libOpenImageDenoise.1.0.0.dylib LuxCore.app/Contents/MacOS/luxcoreui
install_name_tool -change /Users/drquader/Documents/GitHub/LuxCore/macos/lib/libtbb.dylib @executable_path/../Resources/libs/libtbb.dylib LuxCore.app/Contents/MacOS/luxcoreui
install_name_tool -change @rpath/libtbb.dylib @executable_path/../Resources/libs/libtbb.dylib LuxCore.app/Contents/MacOS/luxcoreui
install_name_tool -change /Users/drquader/Documents/GitHub/MacOSCompileDeps/macos/lib/libtiff.5.dylib @executable_path/../Resources/libs/libtiff.5.dylib LuxCore.app/Contents/MacOS/luxcoreui
install_name_tool -change /Users/drquader/Documents/GitHub/LuxCore/macos/lib/libOpenImageIO.1.8.dylib @executable_path/../Resources/libs/libOpenImageIO.1.8.dylib LuxCore.app/Contents/MacOS/luxcoreui
install_name_tool -change @rpath/libOpenImageIO.1.8.dylib @executable_path/../Resources/libs/libOpenImageIO.1.8.dylib LuxCore.app/Contents/MacOS/luxcoreui
install_name_tool -change /Users/drquader/Documents/GitHub/LuxCore/macos/lib/libtbbmalloc.dylib @executable_path/../Resources/libs/libtbbmalloc.dylib LuxCore.app/Contents/MacOS/luxcoreui
install_name_tool -change @rpath/libtbbmalloc.dylib @executable_path/../Resources/libs/libtbbmalloc.dylib LuxCore.app/Contents/MacOS/luxcoreui
install_name_tool -rpath @loader_path @executable_path/../Resources/libs/ LuxCore.app/Contents/MacOS/luxcoreui
install_name_tool -rpath /Users/drquader/Documents/GitHub/LuxCore/macos/lib @executable_path/ LuxCore.app/Contents/MacOS/luxcoreui

popd

