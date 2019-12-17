#!/bin/bash

#automated script for installing and patching binaries on macos 10.13
# bundle things 

rm -rf release_OSX 

mkdir release_OSX

###luxcoreui

./scripts/install_target_luxcoreui.sh

echo "LuxCoreUi installed"

###luxcoreconsole

./scripts/install_target_luxcoreconsole.sh

echo "LuxCoreConsole installed"

###pyluxcore

./scripts/install_target_pyluxcore.sh

echo "PyLuxCore installed"

###denoise

./scripts/install_target_denoise.sh

echo "denoise installed"

#tar -czf LuxCoreRender.tar.gz ./release_OSX

echo "Creating DMG ..."

hdiutil create LuxCoreRender2.3alpha0.dmg -volname "LuxCoreRender2.3alpha0" -fs HFS+ -srcfolder release_OSX/

echo "DMG created !"
