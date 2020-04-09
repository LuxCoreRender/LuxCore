_mount_dir="./release_OSX/pyluxcore"
_entitlements="./scripts/macos/entitlements.plist"

echo ; echo "Codesigning .dylib and .so libraries"
for f in $(find "${_mount_dir}" -name "*.dylib" -o -name "*.so"); do
   codesign --remove-signature "${f}"
   codesign --timestamp --options runtime --entitlements="${_entitlements}" --sign "E3C3F1D135EB71C95141E9D3265DF35A6065C7A0" "${f}"
done
