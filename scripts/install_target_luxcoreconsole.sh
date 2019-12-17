#!/bin/bash

cd release_OSX

###luxcoreconsole
mkdir luxcoreconsole

cp ../build/luxcoreconsole luxcoreconsole

cd luxcoreconsole

#libiomp

mkdir -p ./libs/
cp -f ../../macos/lib/libomp.dylib ./libs/libomp.dylib
chmod +w ./libs/libomp.dylib
install_name_tool -id @executable_path/libs/libomp.dylib ./libs/libomp.dylib
install_name_tool -change /Users/drquader/Documents/GitHub/LuxCore/macos/lib/libomp.dylib @executable_path/libs/libomp.dylib ./libs/libomp.dylib
install_name_tool -change @rpath/libomp.dylib @executable_path/libs/libomp.dylib ./libs/libomp.dylib
install_name_tool -change /Users/drquader/Documents/GitHub/LuxCore/macos/lib/libembree3.3.dylib @executable_path/libs/libembree3.3.dylib ./libs/libomp.dylib
install_name_tool -change @rpath/libembree3.3.dylib @executable_path/libs/libembree3.3.dylib ./libs/libomp.dylib
install_name_tool -change /Users/drquader/Documents/GitHub/MacOSCompileDeps/macos/lib/libOpenImageDenoise.1.0.0.dylib @executable_path/libs/libOpenImageDenoise.1.0.0.dylib ./libs/libomp.dylib
install_name_tool -change /Users/drquader/Documents/GitHub/MacOSCompileDeps/macos/lib/libOpenImageDenoise.0.dylib @executable_path/libs/libOpenImageDenoise.1.0.0.dylib ./libs/libomp.dylib
install_name_tool -change /Users/drquader/Documents/GitHub/LuxCore/macos/lib/libtbb.dylib @executable_path/libs/libtbb.dylib ./libs/libomp.dylib
install_name_tool -change @rpath/libtbb.dylib @executable_path/libs/libtbb.dylib ./libs/libomp.dylib
install_name_tool -change /Users/drquader/Documents/GitHub/MacOSCompileDeps/macos/lib/libtiff.5.dylib @executable_path/libs/libtiff.5.dylib ./libs/libomp.dylib
install_name_tool -change /Users/drquader/Documents/GitHub/LuxCore/macos/lib/libOpenImageIO.1.8.dylib @executable_path/libs/libOpenImageIO.1.8.dylib ./libs/libomp.dylib
install_name_tool -change @rpath/libOpenImageIO.1.8.dylib @executable_path/libs/libOpenImageIO.1.8.dylib ./libs/libomp.dylib
install_name_tool -change /Users/drquader/Documents/GitHub/LuxCore/macos/lib/libtbbmalloc.dylib @executable_path/libs/libtbbmalloc.dylib ./libs/libomp.dylib
install_name_tool -change @rpath/libtbbmalloc.dylib @executable_path/libs/libtbbmalloc.dylib ./libs/libomp.dylib

#libembree

cp -f ../../macos/lib/libembree3.3.dylib ./libs/libembree3.3.dylib
chmod +w ./libs/libembree3.3.dylib
install_name_tool -id @executable_path/libs/libembree3.3.dylib ./libs/libembree3.3.dylib
install_name_tool -change /Users/drquader/Documents/GitHub/LuxCore/macos/lib/libomp.dylib @executable_path/libs/libomp.dylib ./libs/libembree3.3.dylib
install_name_tool -change @rpath/libomp.dylib @executable_path/libs/libomp.dylib ./libs/libembree3.3.dylib
install_name_tool -change /Users/drquader/Documents/GitHub/LuxCore/macos/lib/libembree3.3.dylib @executable_path/libs/libembree3.3.dylib ./libs/libembree3.3.dylib
install_name_tool -change @rpath/libembree3.3.dylib @executable_path/libs/libembree3.3.dylib ./libs/libembree3.3.dylib
install_name_tool -change /Users/drquader/Documents/GitHub/MacOSCompileDeps/macos/lib/libOpenImageDenoise.1.0.0.dylib @executable_path/libs/libOpenImageDenoise.1.0.0.dylib ./libs/libembree3.3.dylib
install_name_tool -change /Users/drquader/Documents/GitHub/MacOSCompileDeps/macos/lib/libOpenImageDenoise.0.dylib @executable_path/libs/libOpenImageDenoise.1.0.0.dylib ./libs/libembree3.3.dylib
install_name_tool -change /Users/drquader/Documents/GitHub/LuxCore/macos/lib/libtbb.dylib @executable_path/libs/libtbb.dylib ./libs/libembree3.3.dylib
install_name_tool -change @rpath/libtbb.dylib @executable_path/libs/libtbb.dylib ./libs/libembree3.3.dylib
install_name_tool -change /Users/drquader/Documents/GitHub/MacOSCompileDeps/macos/lib/libtiff.5.dylib @executable_path/libs/libtiff.5.dylib ./libs/libembree3.3.dylib
install_name_tool -change /Users/drquader/Documents/GitHub/LuxCore/macos/lib/libOpenImageIO.1.8.dylib @executable_path/libs/libOpenImageIO.1.8.dylib ./libs/libembree3.3.dylib
install_name_tool -change @rpath/libOpenImageIO.1.8.dylib @executable_path/libs/libOpenImageIO.1.8.dylib ./libs/libembree3.3.dylib
install_name_tool -change /Users/drquader/Documents/GitHub/LuxCore/macos/lib/libtbbmalloc.dylib @executable_path/libs/libtbbmalloc.dylib ./libs/libembree3.3.dylib
install_name_tool -change @rpath/libtbbmalloc.dylib @executable_path/libs/libtbbmalloc.dylib ./libs/libembree3.3.dylib

#libOpenImageDenoise

cp -f ../../macos/lib/libOpenImageDenoise.1.0.0.dylib ./libs/libOpenImageDenoise.1.0.0.dylib
chmod +w ./libs/libOpenImageDenoise.1.0.0.dylib
install_name_tool -id @executable_path/libs/libOpenImageDenoise.1.0.0.dylib ./libs/libOpenImageDenoise.1.0.0.dylib
install_name_tool -change /Users/drquader/Documents/GitHub/LuxCore/macos/lib/libomp.dylib @executable_path/libs/libomp.dylib ./libs/libOpenImageDenoise.1.0.0.dylib
install_name_tool -change @rpath/libomp.dylib @executable_path/libs/libomp.dylib ./libs/libOpenImageDenoise.1.0.0.dylib
install_name_tool -change /Users/drquader/Documents/GitHub/LuxCore/macos/lib/libembree3.3.dylib @executable_path/libs/libembree3.3.dylib ./libs/libOpenImageDenoise.1.0.0.dylib
install_name_tool -change @rpath/libembree3.3.dylib @executable_path/libs/libembree3.3.dylib ./libs/libOpenImageDenoise.1.0.0.dylib
install_name_tool -change /Users/drquader/Documents/GitHub/MacOSCompileDeps/macos/lib/libOpenImageDenoise.1.0.0.dylib @executable_path/libs/libOpenImageDenoise.1.0.0.dylib ./libs/libOpenImageDenoise.1.0.0.dylib
install_name_tool -change /Users/drquader/Documents/GitHub/MacOSCompileDeps/macos/lib/libOpenImageDenoise.0.dylib @executable_path/libs/libOpenImageDenoise.1.0.0.dylib ./libs/libOpenImageDenoise.1.0.0.dylib
install_name_tool -change /Users/drquader/Documents/GitHub/LuxCore/macos/lib/libtbb.dylib @executable_path/libs/libtbb.dylib ./libs/libOpenImageDenoise.1.0.0.dylib
install_name_tool -change @rpath/libtbb.dylib @executable_path/libs/libtbb.dylib ./libs/libOpenImageDenoise.1.0.0.dylib
install_name_tool -change /Users/drquader/Documents/GitHub/MacOSCompileDeps/macos/lib/libtiff.5.dylib @executable_path/libs/libtiff.5.dylib ./libs/libOpenImageDenoise.1.0.0.dylib
install_name_tool -change /Users/drquader/Documents/GitHub/LuxCore/macos/lib/libOpenImageIO.1.8.dylib @executable_path/libs/libOpenImageIO.1.8.dylib ./libs/libOpenImageDenoise.1.0.0.dylib
install_name_tool -change @rpath/libOpenImageIO.1.8.dylib @executable_path/libs/libOpenImageIO.1.8.dylib ./libs/libOpenImageDenoise.1.0.0.dylib
install_name_tool -change /Users/drquader/Documents/GitHub/LuxCore/macos/lib/libtbbmalloc.dylib @executable_path/libs/libtbbmalloc.dylib ./libs/libOpenImageDenoise.1.0.0.dylib
install_name_tool -change @rpath/libtbbmalloc.dylib @executable_path/libs/libtbbmalloc.dylib ./libs/libOpenImageDenoise.1.0.0.dylib

#libtbb

cp -f ../../macos/lib/libtbb.dylib ./libs/libtbb.dylib
chmod +w ./libs/libtbb.dylib
install_name_tool -id @executable_path/libs/libtbb.dylib ./libs/libtbb.dylib
install_name_tool -change /Users/drquader/Documents/GitHub/LuxCore/macos/lib/libomp.dylib @executable_path/libs/libomp.dylib ./libs/libtbb.dylib
install_name_tool -change @rpath/libomp.dylib @executable_path/libs/libomp.dylib ./libs/libtbb.dylib
install_name_tool -change /Users/drquader/Documents/GitHub/LuxCore/macos/lib/libembree3.3.dylib @executable_path/libs/libembree3.3.dylib ./libs/libtbb.dylib
install_name_tool -change @rpath/libembree3.3.dylib @executable_path/libs/libembree3.3.dylib ./libs/libtbb.dylib
install_name_tool -change /Users/drquader/Documents/GitHub/MacOSCompileDeps/macos/lib/libOpenImageDenoise.1.0.0.dylib @executable_path/libs/libOpenImageDenoise.1.0.0.dylib ./libs/libtbb.dylib
install_name_tool -change /Users/drquader/Documents/GitHub/MacOSCompileDeps/macos/lib/libOpenImageDenoise.0.dylib @executable_path/libs/libOpenImageDenoise.1.0.0.dylib ./libs/libtbb.dylib
install_name_tool -change /Users/drquader/Documents/GitHub/LuxCore/macos/lib/libtbb.dylib @executable_path/libs/libtbb.dylib ./libs/libtbb.dylib
install_name_tool -change @rpath/libtbb.dylib @executable_path/libs/libtbb.dylib ./libs/libtbb.dylib
install_name_tool -change /Users/drquader/Documents/GitHub/MacOSCompileDeps/macos/lib/libtiff.5.dylib @executable_path/libs/libtiff.5.dylib ./libs/libtbb.dylib
install_name_tool -change /Users/drquader/Documents/GitHub/LuxCore/macos/lib/libOpenImageIO.1.8.dylib @executable_path/libs/libOpenImageIO.1.8.dylib ./libs/libtbb.dylib
install_name_tool -change @rpath/libOpenImageIO.1.8.dylib @executable_path/libs/libOpenImageIO.1.8.dylib ./libs/libtbb.dylib
install_name_tool -change /Users/drquader/Documents/GitHub/LuxCore/macos/lib/libtbbmalloc.dylib @executable_path/libs/libtbbmalloc.dylib ./libs/libtbb.dylib
install_name_tool -change @rpath/libtbbmalloc.dylib @executable_path/libs/libtbbmalloc.dylib ./libs/libtbb.dylib

#libtiff

cp -f ../../macos/lib/libtiff.5.dylib ./libs/libtiff.5.dylib
chmod +w ./libs/libtiff.5.dylib
install_name_tool -id @executable_path/libs/libtiff.5.dylib ./libs/libtiff.5.dylib
install_name_tool -change /Users/drquader/Documents/GitHub/LuxCore/macos/lib/libomp.dylib @executable_path/libs/libomp.dylib ./libs/libtiff.5.dylib
install_name_tool -change @rpath/libomp.dylib @executable_path/libs/libomp.dylib ./libs/libtiff.5.dylib
install_name_tool -change /Users/drquader/Documents/GitHub/LuxCore/macos/lib/libembree3.3.dylib @executable_path/libs/libembree3.3.dylib ./libs/libtiff.5.dylib
install_name_tool -change @rpath/libembree3.3.dylib @executable_path/libs/libembree3.3.dylib ./libs/libtiff.5.dylib
install_name_tool -change /Users/drquader/Documents/GitHub/MacOSCompileDeps/macos/lib/libOpenImageDenoise.1.0.0.dylib @executable_path/libs/libOpenImageDenoise.1.0.0.dylib ./libs/libtiff.5.dylib
install_name_tool -change /Users/drquader/Documents/GitHub/MacOSCompileDeps/macos/lib/libOpenImageDenoise.0.dylib @executable_path/libs/libOpenImageDenoise.1.0.0.dylib ./libs/libtiff.5.dylib
install_name_tool -change /Users/drquader/Documents/GitHub/LuxCore/macos/lib/libtbb.dylib @executable_path/libs/libtbb.dylib ./libs/libtiff.5.dylib
install_name_tool -change @rpath/libtbb.dylib @executable_path/libs/libtbb.dylib ./libs/libtiff.5.dylib
install_name_tool -change /Users/drquader/Documents/GitHub/MacOSCompileDeps/macos/lib/libtiff.5.dylib @executable_path/libs/libtiff.5.dylib ./libs/libtiff.5.dylib
install_name_tool -change /Users/drquader/Documents/GitHub/LuxCore/macos/lib/libOpenImageIO.1.8.dylib @executable_path/libs/libOpenImageIO.1.8.dylib ./libs/libtiff.5.dylib
install_name_tool -change @rpath/libOpenImageIO.1.8.dylib @executable_path/libs/libOpenImageIO.1.8.dylib ./libs/libtiff.5.dylib
install_name_tool -change /Users/drquader/Documents/GitHub/LuxCore/macos/lib/libtbbmalloc.dylib @executable_path/libs/libtbbmalloc.dylib ./libs/libtiff.5.dylib
install_name_tool -change @rpath/libtbbmalloc.dylib @executable_path/libs/libtbbmalloc.dylib ./libs/libtiff.5.dylib

#libOpenImageIO

cp -f ../../macos/lib/libOpenImageIO.1.8.dylib ./libs/libOpenImageIO.1.8.dylib
chmod +w ./libs/libOpenImageIO.1.8.dylib
install_name_tool -id @executable_path/libs/libOpenImageIO.1.8.dylib ./libs/libOpenImageIO.1.8.dylib
install_name_tool -change /Users/drquader/Documents/GitHub/LuxCore/macos/lib/libomp.dylib @executable_path/libs/libomp.dylib ./libs/libOpenImageIO.1.8.dylib
install_name_tool -change @rpath/libomp.dylib @executable_path/libs/libomp.dylib ./libs/libOpenImageIO.1.8.dylib
install_name_tool -change /Users/drquader/Documents/GitHub/LuxCore/macos/lib/libembree3.3.dylib @executable_path/libs/libembree3.3.dylib ./libs/libOpenImageIO.1.8.dylib
install_name_tool -change @rpath/libembree3.3.dylib @executable_path/libs/libembree3.3.dylib ./libs/libOpenImageIO.1.8.dylib
install_name_tool -change /Users/drquader/Documents/GitHub/MacOSCompileDeps/macos/lib/libOpenImageDenoise.1.0.0.dylib @executable_path/libs/libOpenImageDenoise.1.0.0.dylib ./libs/libOpenImageIO.1.8.dylib
install_name_tool -change /Users/drquader/Documents/GitHub/MacOSCompileDeps/macos/lib/libOpenImageDenoise.0.dylib @executable_path/libs/libOpenImageDenoise.1.0.0.dylib ./libs/libOpenImageIO.1.8.dylib
install_name_tool -change /Users/drquader/Documents/GitHub/LuxCore/macos/lib/libtbb.dylib @executable_path/libs/libtbb.dylib ./libs/libOpenImageIO.1.8.dylib
install_name_tool -change @rpath/libtbb.dylib @executable_path/libs/libtbb.dylib ./libs/libOpenImageIO.1.8.dylib
install_name_tool -change /Users/drquader/Documents/GitHub/MacOSCompileDeps/macos/lib/libtiff.5.dylib @executable_path/libs/libtiff.5.dylib ./libs/libOpenImageIO.1.8.dylib
install_name_tool -change /Users/drquader/Documents/GitHub/LuxCore/macos/lib/libOpenImageIO.1.8.dylib @executable_path/libs/libOpenImageIO.1.8.dylib ./libs/libOpenImageIO.1.8.dylib
install_name_tool -change @rpath/libOpenImageIO.1.8.dylib @executable_path/libs/libOpenImageIO.1.8.dylib ./libs/libOpenImageIO.1.8.dylib
install_name_tool -change /Users/drquader/Documents/GitHub/LuxCore/macos/lib/libtbbmalloc.dylib @executable_path/libs/libtbbmalloc.dylib ./libs/libOpenImageIO.1.8.dylib
install_name_tool -change @rpath/libtbbmalloc.dylib @executable_path/libs/libtbbmalloc.dylib ./libs/libOpenImageIO.1.8.dylib
install_name_tool -rpath /tmp/macdepsbuild/oiio-Release-1.8.11/dist/macosx/lib @executable_path/libs/ ./libs/libOpenImageIO.1.8.dylib

#libtbbmalloc

cp -f ../../macos/lib/libtbbmalloc.dylib ./libs/libtbbmalloc.dylib
chmod +w ./libs/libtbbmalloc.dylib
install_name_tool -id @executable_path/libs/libtbbmalloc.dylib ./libs/libtbbmalloc.dylib
install_name_tool -change /Users/drquader/Documents/GitHub/LuxCore/macos/lib/libomp.dylib @executable_path/libs/libomp.dylib ./libs/libtbbmalloc.dylib
install_name_tool -change @rpath/libomp.dylib @executable_path/libs/libomp.dylib ./libs/libtbbmalloc.dylib
install_name_tool -change /Users/drquader/Documents/GitHub/LuxCore/macos/lib/libembree3.3.dylib @executable_path/libs/libembree3.3.dylib ./libs/libtbbmalloc.dylib
install_name_tool -change @rpath/libembree3.3.dylib @executable_path/libs/libembree3.3.dylib ./libs/libtbbmalloc.dylib
install_name_tool -change /Users/drquader/Documents/GitHub/MacOSCompileDeps/macos/lib/libOpenImageDenoise.1.0.0.dylib @executable_path/libs/libOpenImageDenoise.1.0.0.dylib ./libs/libtbbmalloc.dylib
install_name_tool -change /Users/drquader/Documents/GitHub/MacOSCompileDeps/macos/lib/libOpenImageDenoise.0.dylib @executable_path/libs/libOpenImageDenoise.1.0.0.dylib ./libs/libtbbmalloc.dylib
install_name_tool -change /Users/drquader/Documents/GitHub/LuxCore/macos/lib/libtbb.dylib @executable_path/libs/libtbb.dylib ./libs/libtbbmalloc.dylib
install_name_tool -change @rpath/libtbb.dylib @executable_path/libs/libtbb.dylib ./libs/libtbbmalloc.dylib
install_name_tool -change /Users/drquader/Documents/GitHub/MacOSCompileDeps/macos/lib/libtiff.5.dylib @executable_path/libs/libtiff.5.dylib ./libs/libtbbmalloc.dylib
install_name_tool -change /Users/drquader/Documents/GitHub/LuxCore/macos/lib/libOpenImageIO.1.8.dylib @executable_path/libs/libOpenImageIO.1.8.dylib ./libs/libtbbmalloc.dylib
install_name_tool -change @rpath/libOpenImageIO.1.8.dylib @executable_path/libs/libOpenImageIO.1.8.dylib ./libs/libtbbmalloc.dylib
install_name_tool -change /Users/drquader/Documents/GitHub/LuxCore/macos/lib/libtbbmalloc.dylib @executable_path/libs/libtbbmalloc.dylib ./libs/libtbbmalloc.dylib
install_name_tool -change @rpath/libtbbmalloc.dylib @executable_path/libs/libtbbmalloc.dylib ./libs/libtbbmalloc.dylib

#luxcoreconsole

install_name_tool -change /Users/drquader/Documents/GitHub/LuxCore/macos/lib/libomp.dylib @executable_path/libs/libomp.dylib luxcoreconsole
install_name_tool -change @rpath/libomp.dylib @executable_path/libs/libomp.dylib luxcoreconsole
install_name_tool -change /Users/drquader/Documents/GitHub/LuxCore/macos/lib/libembree3.3.dylib @executable_path/libs/libembree3.3.dylib luxcoreconsole
install_name_tool -change @rpath/libembree3.3.dylib @executable_path/libs/libembree3.3.dylib luxcoreconsole
install_name_tool -change /Users/drquader/Documents/GitHub/MacOSCompileDeps/macos/lib/libOpenImageDenoise.1.0.0.dylib @executable_path/libs/libOpenImageDenoise.1.0.0.dylib luxcoreconsole
install_name_tool -change /Users/drquader/Documents/GitHub/MacOSCompileDeps/macos/lib/libOpenImageDenoise.0.dylib @executable_path/libs/libOpenImageDenoise.1.0.0.dylib luxcoreconsole
install_name_tool -change /Users/drquader/Documents/GitHub/LuxCore/macos/lib/libtbb.dylib @executable_path/libs/libtbb.dylib luxcoreconsole
install_name_tool -change @rpath/libtbb.dylib @executable_path/libs/libtbb.dylib luxcoreconsole
install_name_tool -change /Users/drquader/Documents/GitHub/MacOSCompileDeps/macos/lib/libtiff.5.dylib @executable_path/libs/libtiff.5.dylib luxcoreconsole
install_name_tool -change /Users/drquader/Documents/GitHub/LuxCore/macos/lib/libOpenImageIO.1.8.dylib @executable_path/libs/libOpenImageIO.1.8.dylib luxcoreconsole
install_name_tool -change @rpath/libOpenImageIO.1.8.dylib @executable_path/libs/libOpenImageIO.1.8.dylib luxcoreconsole
install_name_tool -change /Users/drquader/Documents/GitHub/LuxCore/macos/lib/libtbbmalloc.dylib @executable_path/libs/libtbbmalloc.dylib luxcoreconsole
install_name_tool -change @rpath/libtbbmalloc.dylib @executable_path/libs/libtbbmalloc.dylib luxcoreconsole
install_name_tool -rpath @loader_path @executable_path/libs/ luxcoreconsole
install_name_tool -rpath /Users/drquader/Documents/GitHub/LuxCore/macos/lib @executable_path/libs/ luxcoreconsole
