# Subdivision regression tests

Configure with `-DLUXCORE_SUBDIVISION_TESTS=ON`, build the
`subdivision_regression` target, then run `ctest -C Release --output-on-failure`
from the build directory. Runtime dependency libraries must be on the library path.

The synthetic meshes cover both subdivision modes, missing and multiple attribute
layers, vertex AOVs without a matching alpha layer, triangle AOV ancestry, repeated
subdivision and mesh copies, isolated vertices with varying attributes, and crease
invariance under vertex renumbering. The texture test replaces a referenced texture
while keeping the retired object alive, detecting stale references without relying
on a use-after-free crash.

`subdivision_regression creases` and `subdivision_regression textures` run focused
checks. No external scenes, Blender installation, or GPU are needed.
