_mount_dir="$1"
_entitlements="./scripts/macos/entitlements.plist"

echo "Codesigning .dylib and .so libraries"
for f in $(find "${_mount_dir}" -name "*.so"); do
   codesign --remove-signature "${f}"
   codesign --timestamp --options runtime --entitlements="${_entitlements}" --sign "A1CD139B9FD66DE9D474D420C1899EA96A622B9A" "${f}"
done
