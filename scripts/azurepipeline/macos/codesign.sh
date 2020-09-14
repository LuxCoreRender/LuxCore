_mount_dir="$1"
_entitlements="./scripts/azurepipeline/macos/entitlements.plist"

echo "Codesigning .dylib and .so libraries"
for f in $(find "${_mount_dir}" -name "*.dylib" -o -name "*.so"); do
   codesign --remove-signature "${f}"
   codesign --timestamp --options runtime --entitlements="${_entitlements}" --sign "4B1E18D992DAAFEEFE81C7345DD259CE60F733B0" "${f}"
done
