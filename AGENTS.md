# Project instructions

## Limits
- Never use git reset without explicit agreement from user.
- Never interact with external repos without explicit user agreement

# Build
- To build LuxCore, enter (plain) `make` in command line. `make` acts as a
  build-system wrapper, which in turn will call cmake. Do not try to use cmake
  directly.
- The wrapper is in @build-system/luxmake subfolder.
- `make` provides a few specialized targets, but if no argument is given, it builds all.
- To check buildability, plain `make` (with no arguments) is enough
- To pass environment variable to build system, on Linux, you'll have to export the env var (set is not enough)
- The build type is controlled by env var `LUX_BUILD_TYPE`. Possible values (not exhaustive): `Release` | `Debug`
- `make wheel-test` builds a Python wheel for testing. It is especially useful for blender integration testing and debugging (BlendLuxCore)
- By default, the testing wheel is compiled in Debug mode. If you want it in release (eg. for profiling), export LUX_BUILD_TYPE=Release
- If you test on scenes, do not forget to provide a halt condition. Genuine pathtracing is a never-ending algorithm. Don't forget `halt.enable = True`
