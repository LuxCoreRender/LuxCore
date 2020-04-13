_mount_dir="./release_OSX/pyluxcore"
_entitlements="./scripts/macos/entitlements.plist"

echo ; echo "Codesigning .dylib and .so libraries"
for f in $(find "${_mount_dir}" -name "*.dylib" -o -name "*.so"); do
   codesign --remove-signature "${f}"
   codesign --timestamp --options runtime --entitlements="${_entitlements}" --sign "24293BCE17D91BE0078BE5616A602479703843BA" "${f}"
done
