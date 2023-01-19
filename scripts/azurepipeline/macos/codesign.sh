_mount_dir="$1"
_entitlements="./scripts/azurepipeline/macos/entitlements.plist"

echo "Codesigning .dylib and .so libraries"
for f in $(find "${_mount_dir}" -name "*.dylib" -o -name "*.so"); do
   codesign --remove-signature "${f}"
   codesign --timestamp --options runtime --entitlements="${_entitlements}" --sign "26B60C66D3DABEE289D6CD90948F6F036F19C3CD" "${f}"
done
