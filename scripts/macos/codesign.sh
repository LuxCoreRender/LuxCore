_mount_dir="$1"
_entitlements="./scripts/macos/entitlements.plist"

echo "Codesigning .dylib and .so libraries"
for f in $(find "${_mount_dir}" -name "*.so"); do
   codesign --remove-signature "${f}"
   codesign --timestamp --options runtime --entitlements="${_entitlements}" --sign "B5F0C043307E420E437C2C2A0F9F58913D1B2DFB" "${f}"
done
