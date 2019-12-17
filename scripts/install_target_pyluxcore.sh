#!/bin/bash

cd release_OSX

mkdir pyluxcore

cp ../build/lib/pyluxcore* pyluxcore

cd pyluxcore

#libomp

cp -f ../../macos/lib/libomp.dylib ./libomp.dylib
chmod +w ./libomp.dylib
install_name_tool -id @loader_path/libomp.dylib ./libomp.dylib
install_name_tool -change /Users/drquader/Documents/GitHub/LuxCore/macos/lib/libomp.dylib @loader_path/libomp.dylib ./libomp.dylib
install_name_tool -change @rpath/libomp.dylib @loader_path/libomp.dylib ./libomp.dylib
install_name_tool -change /Users/drquader/Documents/GitHub/LuxCore/macos/lib/libembree3.3.dylib @loader_path/libembree3.3.dylib ./libomp.dylib
install_name_tool -change @rpath/libembree3.3.dylib @loader_path/libembree3.3.dylib ./libomp.dylib
install_name_tool -change /Users/drquader/Documents/GitHub/LuxCore/macos/lib/libtbb.dylib @loader_path/libtbb.dylib ./libomp.dylib
install_name_tool -change @rpath/libtbb.dylib @loader_path/libtbb.dylib ./libomp.dylib
install_name_tool -change /Users/drquader/Documents/GitHub/MacOSCompileDeps/macos/lib/libtiff.5.dylib @loader_path/libtiff.5.dylib ./libomp.dylib
install_name_tool -change /Users/drquader/Documents/GitHub/LuxCore/macos/lib/libOpenImageIO.1.8.dylib @loader_path/libOpenImageIO.1.8.dylib ./libomp.dylib
install_name_tool -change @rpath/libOpenImageIO.1.8.dylib @loader_path/libOpenImageIO.1.8.dylib ./libomp.dylib
install_name_tool -change /Users/drquader/Documents/GitHub/LuxCore/macos/lib/libtbbmalloc.dylib @loader_path/libtbbmalloc.dylib ./libomp.dylib
install_name_tool -change @rpath/libtbbmalloc.dylib @loader_path/libtbbmalloc.dylib ./libomp.dylib

#libembree

cp -f ../../macos/lib/libembree3.3.dylib ./libembree3.3.dylib
chmod +w ./libembree3.3.dylib
install_name_tool -id @loader_path/libembree3.3.dylib ./libembree3.3.dylib
install_name_tool -change /Users/drquader/Documents/GitHub/LuxCore/macos/lib/libomp.dylib @loader_path/libomp.dylib ./libembree3.3.dylib
install_name_tool -change @rpath/libomp.dylib @loader_path/libomp.dylib ./libembree3.3.dylib
install_name_tool -change /Users/drquader/Documents/GitHub/LuxCore/macos/lib/libembree3.3.dylib @loader_path/libembree3.3.dylib ./libembree3.3.dylib
install_name_tool -change @rpath/libembree3.3.dylib @loader_path/libembree3.3.dylib ./libembree3.3.dylib
install_name_tool -change /Users/drquader/Documents/GitHub/LuxCore/macos/lib/libtbb.dylib @loader_path/libtbb.dylib ./libembree3.3.dylib
install_name_tool -change @rpath/libtbb.dylib @loader_path/libtbb.dylib ./libembree3.3.dylib
install_name_tool -change /Users/drquader/Documents/GitHub/MacOSCompileDeps/macos/lib/libtiff.5.dylib @loader_path/libtiff.5.dylib ./libembree3.3.dylib
install_name_tool -change /Users/drquader/Documents/GitHub/LuxCore/macos/lib/libOpenImageIO.1.8.dylib @loader_path/libOpenImageIO.1.8.dylib ./libembree3.3.dylib
install_name_tool -change @rpath/libOpenImageIO.1.8.dylib @loader_path/libOpenImageIO.1.8.dylib ./libembree3.3.dylib
install_name_tool -change /Users/drquader/Documents/GitHub/LuxCore/macos/lib/libtbbmalloc.dylib @loader_path/libtbbmalloc.dylib ./libembree3.3.dylib
install_name_tool -change @rpath/libtbbmalloc.dylib @loader_path/libtbbmalloc.dylib ./libembree3.3.dylib

#libtbb

cp -f ../../macos/lib/libtbb.dylib ./libtbb.dylib
chmod +w ./libtbb.dylib
install_name_tool -id @loader_path/libtbb.dylib ./libtbb.dylib
install_name_tool -change /Users/drquader/Documents/GitHub/LuxCore/macos/lib/libomp.dylib @loader_path/libomp.dylib ./libtbb.dylib
install_name_tool -change @rpath/libomp.dylib @loader_path/libomp.dylib ./libtbb.dylib
install_name_tool -change /Users/drquader/Documents/GitHub/LuxCore/macos/lib/libembree3.3.dylib @loader_path/libembree3.3.dylib ./libtbb.dylib
install_name_tool -change @rpath/libembree3.3.dylib @loader_path/libembree3.3.dylib ./libtbb.dylib
install_name_tool -change /Users/drquader/Documents/GitHub/LuxCore/macos/lib/libtbb.dylib @loader_path/libtbb.dylib ./libtbb.dylib
install_name_tool -change @rpath/libtbb.dylib @loader_path/libtbb.dylib ./libtbb.dylib
install_name_tool -change /Users/drquader/Documents/GitHub/MacOSCompileDeps/macos/lib/libtiff.5.dylib @loader_path/libtiff.5.dylib ./libtbb.dylib
install_name_tool -change /Users/drquader/Documents/GitHub/LuxCore/macos/lib/libOpenImageIO.1.8.dylib @loader_path/libOpenImageIO.1.8.dylib ./libtbb.dylib
install_name_tool -change @rpath/libOpenImageIO.1.8.dylib @loader_path/libOpenImageIO.1.8.dylib ./libtbb.dylib
install_name_tool -change /Users/drquader/Documents/GitHub/LuxCore/macos/lib/libtbbmalloc.dylib @loader_path/libtbbmalloc.dylib ./libtbb.dylib
install_name_tool -change @rpath/libtbbmalloc.dylib @loader_path/libtbbmalloc.dylib ./libtbb.dylib

#libtiff

cp -f ../../macos/lib/libtiff.5.dylib ./libtiff.5.dylib
chmod +w ./libtiff.5.dylib
install_name_tool -id @loader_path/libtiff.5.dylib ./libtiff.5.dylib
install_name_tool -change /Users/drquader/Documents/GitHub/LuxCore/macos/lib/libomp.dylib @loader_path/libomp.dylib ./libtiff.5.dylib
install_name_tool -change @rpath/libomp.dylib @loader_path/libomp.dylib ./libtiff.5.dylib
install_name_tool -change /Users/drquader/Documents/GitHub/LuxCore/macos/lib/libembree3.3.dylib @loader_path/libembree3.3.dylib ./libtiff.5.dylib
install_name_tool -change @rpath/libembree3.3.dylib @loader_path/libembree3.3.dylib ./libtiff.5.dylib
install_name_tool -change /Users/drquader/Documents/GitHub/LuxCore/macos/lib/libtbb.dylib @loader_path/libtbb.dylib ./libtiff.5.dylib
install_name_tool -change @rpath/libtbb.dylib @loader_path/libtbb.dylib ./libtiff.5.dylib
install_name_tool -change /Users/drquader/Documents/GitHub/MacOSCompileDeps/macos/lib/libtiff.5.dylib @loader_path/libtiff.5.dylib ./libtiff.5.dylib
install_name_tool -change /Users/drquader/Documents/GitHub/LuxCore/macos/lib/libOpenImageIO.1.8.dylib @loader_path/libOpenImageIO.1.8.dylib ./libtiff.5.dylib
install_name_tool -change @rpath/libOpenImageIO.1.8.dylib @loader_path/libOpenImageIO.1.8.dylib ./libtiff.5.dylib
install_name_tool -change /Users/drquader/Documents/GitHub/LuxCore/macos/lib/libtbbmalloc.dylib @loader_path/libtbbmalloc.dylib ./libtiff.5.dylib
install_name_tool -change @rpath/libtbbmalloc.dylib @loader_path/libtbbmalloc.dylib ./libtiff.5.dylib

#libOpenImageIO

cp -f ../../macos/lib/libOpenImageIO.1.8.dylib ./libOpenImageIO.1.8.dylib
chmod +w ./libOpenImageIO.1.8.dylib
install_name_tool -id @loader_path/libOpenImageIO.1.8.dylib ./libOpenImageIO.1.8.dylib
install_name_tool -change /Users/drquader/Documents/GitHub/LuxCore/macos/lib/libomp.dylib @loader_path/libomp.dylib ./libOpenImageIO.1.8.dylib
install_name_tool -change @rpath/libomp.dylib @loader_path/libomp.dylib ./libOpenImageIO.1.8.dylib
install_name_tool -change /Users/drquader/Documents/GitHub/LuxCore/macos/lib/libembree3.3.dylib @loader_path/libembree3.3.dylib ./libOpenImageIO.1.8.dylib
install_name_tool -change @rpath/libembree3.3.dylib @loader_path/libembree3.3.dylib ./libOpenImageIO.1.8.dylib
install_name_tool -change /Users/drquader/Documents/GitHub/LuxCore/macos/lib/libtbb.dylib @loader_path/libtbb.dylib ./libOpenImageIO.1.8.dylib
install_name_tool -change @rpath/libtbb.dylib @loader_path/libtbb.dylib ./libOpenImageIO.1.8.dylib
install_name_tool -change /Users/drquader/Documents/GitHub/MacOSCompileDeps/macos/lib/libtiff.5.dylib @loader_path/libtiff.5.dylib ./libOpenImageIO.1.8.dylib
install_name_tool -change /Users/drquader/Documents/GitHub/LuxCore/macos/lib/libOpenImageIO.1.8.dylib @loader_path/libOpenImageIO.1.8.dylib ./libOpenImageIO.1.8.dylib
install_name_tool -change @rpath/libOpenImageIO.1.8.dylib @loader_path/libOpenImageIO.1.8.dylib ./libOpenImageIO.1.8.dylib
install_name_tool -change /Users/drquader/Documents/GitHub/LuxCore/macos/lib/libtbbmalloc.dylib @loader_path/libtbbmalloc.dylib ./libOpenImageIO.1.8.dylib
install_name_tool -change @rpath/libtbbmalloc.dylib @loader_path/libtbbmalloc.dylib ./libOpenImageIO.1.8.dylib
install_name_tool -rpath /tmp/macdepsbuild/oiio-Release-1.8.11/dist/macosx/lib @loader_path/ ./libOpenImageIO.1.8.dylib

#libtbbmalloc

cp -f ../../macos/lib/libtbbmalloc.dylib ./libtbbmalloc.dylib
chmod +w ./libtbbmalloc.dylib
install_name_tool -id @loader_path/libtbbmalloc.dylib ./libtbbmalloc.dylib
install_name_tool -change /Users/drquader/Documents/GitHub/LuxCore/macos/lib/libomp.dylib @loader_path/libomp.dylib ./libtbbmalloc.dylib
install_name_tool -change @rpath/libomp.dylib @loader_path/libomp.dylib ./libtbbmalloc.dylib
install_name_tool -change /Users/drquader/Documents/GitHub/LuxCore/macos/lib/libembree3.3.dylib @loader_path/libembree3.3.dylib ./libtbbmalloc.dylib
install_name_tool -change @rpath/libembree3.3.dylib @loader_path/libembree3.3.dylib ./libtbbmalloc.dylib
install_name_tool -change /Users/drquader/Documents/GitHub/LuxCore/macos/lib/libtbb.dylib @loader_path/libtbb.dylib ./libtbbmalloc.dylib
install_name_tool -change @rpath/libtbb.dylib @loader_path/libtbb.dylib ./libtbbmalloc.dylib
install_name_tool -change /Users/drquader/Documents/GitHub/MacOSCompileDeps/macos/lib/libtiff.5.dylib @loader_path/libtiff.5.dylib ./libtbbmalloc.dylib
install_name_tool -change /Users/drquader/Documents/GitHub/LuxCore/macos/lib/libOpenImageIO.1.8.dylib @loader_path/libOpenImageIO.1.8.dylib ./libtbbmalloc.dylib
install_name_tool -change @rpath/libOpenImageIO.1.8.dylib @loader_path/libOpenImageIO.1.8.dylib ./libtbbmalloc.dylib
install_name_tool -change /Users/drquader/Documents/GitHub/LuxCore/macos/lib/libtbbmalloc.dylib @loader_path/libtbbmalloc.dylib ./libtbbmalloc.dylib
install_name_tool -change @rpath/libtbbmalloc.dylib @loader_path/libtbbmalloc.dylib ./libtbbmalloc.dylib

#pyluxcore.so

install_name_tool -change /Users/drquader/Documents/GitHub/LuxCore/macos/lib/libomp.dylib @loader_path/libomp.dylib pyluxcore.so
install_name_tool -change @rpath/libomp.dylib @loader_path/libomp.dylib pyluxcore.so
install_name_tool -change /Users/drquader/Documents/GitHub/LuxCore/macos/lib/libembree3.3.dylib @loader_path/libembree3.3.dylib pyluxcore.so
install_name_tool -change @rpath/libembree3.3.dylib @loader_path/libembree3.3.dylib pyluxcore.so
install_name_tool -change /Users/drquader/Documents/GitHub/LuxCore/macos/lib/libtbb.dylib @loader_path/libtbb.dylib pyluxcore.so
install_name_tool -change @rpath/libtbb.dylib @loader_path/libtbb.dylib pyluxcore.so
install_name_tool -change /Users/drquader/Documents/GitHub/MacOSCompileDeps/macos/lib/libtiff.5.dylib @loader_path/libtiff.5.dylib pyluxcore.so
install_name_tool -change /Users/drquader/Documents/GitHub/LuxCore/macos/lib/libOpenImageIO.1.8.dylib @loader_path/libOpenImageIO.1.8.dylib pyluxcore.so
install_name_tool -change @rpath/libOpenImageIO.1.8.dylib @loader_path/libOpenImageIO.1.8.dylib pyluxcore.so
install_name_tool -change /Users/drquader/Documents/GitHub/LuxCore/macos/lib/libtbbmalloc.dylib @loader_path/libtbbmalloc.dylib pyluxcore.so
install_name_tool -change @rpath/libtbbmalloc.dylib @loader_path/libtbbmalloc.dylib pyluxcore.so
install_name_tool -rpath @loader_path @loader_path/ pyluxcore.so
install_name_tool -rpath /Users/drquader/Documents/GitHub/LuxCore/macos/lib @loader_path/ pyluxcore.so
