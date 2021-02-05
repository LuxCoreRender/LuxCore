_mount_dir="$1"
_entitlements="./scripts/macos/entitlements.plist"

echo "Codesigning .dylib and .so libraries"
for f in $(find "${_mount_dir}" -name "*.so"); do
   codesign --remove-signature "${f}"
   codesign --timestamp --options runtime --entitlements="${_entitlements}" --sign "2975307890748B8B3E44D93B00E03A80F546847F" "${f}"
done
