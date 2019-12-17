#!/bin/bash

pushd release_OSX

pushd pyluxcore

#libOpenImageDenoise

cp -f ../../macos/lib/libOpenImageDenoise.1.0.0.dylib ./libOpenImageDenoise.1.0.0.dylib
chmod +w ./libOpenImageDenoise.1.0.0.dylib
install_name_tool -id @executable_path/libOpenImageDenoise.1.0.0.dylib ./libOpenImageDenoise.1.0.0.dylib
install_name_tool -change /Users/drquader/Documents/GitHub/LuxCore/macos/lib/libomp.dylib @executable_path/libomp.dylib libOpenImageDenoise.1.0.0.dylib
install_name_tool -change @rpath/libomp.dylib @executable_path/libomp.dylib libOpenImageDenoise.1.0.0.dylib
install_name_tool -change /Users/drquader/Documents/GitHub/LuxCore/macos/lib/libembree3.3.dylib @executable_path/libembree3.3.dylib libOpenImageDenoise.1.0.0.dylib
install_name_tool -change @rpath/libembree3.3.dylib @executable_path/libembree3.3.dylib libOpenImageDenoise.1.0.0.dylib
install_name_tool -change /Users/drquader/Documents/GitHub/MacOSCompileDeps/macos/lib/libOpenImageDenoise.1.0.0.dylib @executable_path/libOpenImageDenoise.1.0.0.dylib libOpenImageDenoise.1.0.0.dylib
install_name_tool -change /Users/drquader/Documents/GitHub/MacOSCompileDeps/macos/lib/libOpenImageDenoise.0.dylib @executable_path/libOpenImageDenoise.1.0.0.dylib libOpenImageDenoise.1.0.0.dylib
install_name_tool -change /Users/drquader/Documents/GitHub/LuxCore/macos/lib/libtbb.dylib @executable_path/libtbb.dylib libOpenImageDenoise.1.0.0.dylib
install_name_tool -change @rpath/libtbb.dylib @executable_path/libtbb.dylib libOpenImageDenoise.1.0.0.dylib
install_name_tool -change /Users/drquader/Documents/GitHub/MacOSCompileDeps/macos/lib/libtiff.5.dylib @executable_path/libtiff.5.dylib libOpenImageDenoise.1.0.0.dylib
install_name_tool -change /Users/drquader/Documents/GitHub/LuxCore/macos/lib/libOpenImageIO.1.8.dylib @executable_path/libOpenImageIO.1.8.dylib libOpenImageDenoise.1.0.0.dylib
install_name_tool -change @rpath/libOpenImageIO.1.8.dylib @executable_path/libOpenImageIO.1.8.dylib libOpenImageDenoise.1.0.0.dylib
install_name_tool -change /Users/drquader/Documents/GitHub/LuxCore/macos/lib/libtbbmalloc.dylib @executable_path/libtbbmalloc.dylib libOpenImageDenoise.1.0.0.dylib
install_name_tool -change @rpath/libtbbmalloc.dylib @executable_path/libtbbmalloc.dylib libOpenImageDenoise.1.0.0.dylib

#denoise
cp ../../macos/bin/denoise .
install_name_tool -id @executable_path/denoise ./denoise
install_name_tool -change /Users/drquader/Documents/GitHub/MacOSCompileDeps/macos/lib/libOpenImageDenoise.1.0.0.dylib @executable_path/libOpenImageDenoise.1.0.0.dylib denoise
install_name_tool -change /Users/drquader/Documents/GitHub/MacOSCompileDeps/macos/lib/libOpenImageDenoise.0.dylib @executable_path/libOpenImageDenoise.1.0.0.dylib denoise
install_name_tool -change @rpath/libtbb.dylib @executable_path/libtbb.dylib denoise
install_name_tool -change @rpath/libtbbmalloc.dylib @executable_path/libtbbmalloc.dylib denoise

popd
popd