# Pre-compiling the OpenCL kernels in SPIR-V format

LuxCore can embed the OpenCL render kernels (PATHOCL, TILEPATHOCL and
RTPATHOCL, in the generic/AMD/NVIDIA variants) pre-compiled in SPIR-V format
in the binaries. At run time, the OpenCL drivers supporting the
`cl_khr_il_program` extension ingest the modules directly
(`clCreateProgramWithIL`), skipping the source compilation: the kernels are
ready in a fraction of the time and the whole OpenCL compiler can even be
absent from the driver.

The kernels are baked with the `make spirv` command, through a
dependency-free standalone project (`build-system/spirv`): no `make deps`, no
conan, no LuxCoreDeps - only a C++20 compiler with the boost headers, CMake,
Ninja and a clang able to compile OpenCL C in SPIR-V format.

The SPIR-V format is platform independent: the modules baked once can be
embedded by the build on any platform (it is what the CI does, baking them
once under Linux and serving them to all the build legs).

This document is a practical guide for **Windows**, where the bake needs a
bit of setup (on Linux, `apt install g++ clang cmake ninja-build llvm-spirv libboost-dev`
and `make spirv` usually just works, the distribution clang including the
SPIR-V back-end).

## Quick summary

From a Windows terminal:

```
> make spirv
> make luxcore        (or luxcoreui, luxcoreconsole, ...)
```

The first command bakes the modules in `out\spirv-build\baked` (~9 files of a
few MB each). The second one - like any build target - configures CMake with
`LUXRAYS_SPIRV_BAKED_DIR` pointing at the bake, and the modules are embedded
in the binaries. The configuration log confirms it:

```
-- Using the pre-baked OpenCL kernels in SPIR-V format from
   <...>\out\spirv-build\baked
```

Without any bake, the build proceeds as before (no embedded modules, source
compilation at run time). `make clear` also removes the bake.

## Prerequisites (Windows)

| Tool | Used for | Notes |
|------|----------|-------|
| Python | the `make` helper (`build-system/luxmake`) | already needed to build LuxCore |
| CMake, Ninja | the bake project (`-G Ninja`) | already needed to build LuxCore |
| C++20 compiler | the `spirv-baker` tool of the bake project | MSVC (a VS developer prompt) or any other |
| Boost (headers) | the `spirv-baker` tool includes luxrays/slg headers using them | MSYS2 `mingw-w64-ucrt-x86_64-boost`; Linux `libboost-dev`; with MSVC, provide a matching boost (the compiled `iostreams` and `serialization` libraries are needed at link time) |
| clang | compiling the kernels to SPIR-V (or to LLVM bitcode, see below) | see the next section |
| `llvm-spirv` | only if the clang lacks the SPIR-V back-end | see the next section |

The point to understand: the official Windows clang builds (LLVM.org
installer, Chocolatey) **do not include the SPIR-V back-end** (the `spirv64`
target). The bake falls back - automatically, probed at configure time - on
the two-step pipeline, which works with *any* clang:

```
clang --target=spir64-unknown-unknown -x cl -emit-llvm -c kernel.cl -o kernel.bc
llvm-spirv kernel.bc -o kernel.spv
```

The `spir64` pseudo target is available in every clang build; the
SPIRV-LLVM-Translator (`llvm-spirv.exe`) translates the LLVM bitcode to
SPIR-V. The selected mode (`direct` or `spir`) is displayed at configure
time.

### Recommended: MSYS2

MSYS2 provides both tools as native Windows binaries, version-matched:

1. Install [MSYS2](https://www.msys2.org) (e.g. in `C:\msys64`).

2. From an MSYS2 shell (the `UCRT64` one is a good default):

   ```
   pacman -Syu
   pacman -S mingw-w64-ucrt-x86_64-clang mingw-w64-ucrt-x86_64-spirv-llvm-translator mingw-w64-ucrt-x86_64-boost
   ```

   (the `mingw64` and `clang64` environments provide the same packages, pick
   one and stay consistent with it).

3. Make the MSYS2 environment visible to the LuxCore build. From `cmd.exe`:

   ```
   set PATH=C:\msys64\ucrt64\bin;%PATH%
   ```

   (make it permanent with `setx` or the system settings if you prefer).

Note about the compiler of the bake project: the `spirv-baker` tool needs
boost (see the table above). With the MSYS2 packages above, build it with
the MSYS2 compiler - from the MSYS2 `UCRT64` shell `g++` is picked up
automatically - or, from any other shell, configure the bake project with
`-DCMAKE_CXX_COMPILER=C:/msys64/ucrt64/bin/g++.exe`. With MSVC, a separately
provided boost is required instead (e.g. the LuxCoreDeps one, but the
compiled libraries must then match the toolset/flags of the bake project,
which is not the case out of the box).

Keep the whole pair (`clang` + `llvm-spirv`) from the same MSYS2
environment: their versions must match (LLVM bitcode format), and MSYS2
guarantees it within an environment. The configure-time probe verifies the
complete chain anyway and reports a clear error if something is off.

Note: the `llvm-spirv.exe` of MSYS2 depends on DLLs of its environment: it
must be run with the environment `bin` directory in the PATH, as above.

### Alternative: Intel oneAPI

The oneAPI packages ship a standalone `llvm-spirv.exe` for Windows. It works
with any clang (e.g. the Chocolatey/LLVM.org one), but you are then
responsible for the version pairing between the clang producing the bitcode
and the translator: a mismatch is detected by the probe and reported as a
warning (`... can not translate the LLVM bitcode produced by ...
(versions mismatch?)`).

### Not needed for the LuxCore build itself

The bake only needs the tools above; the usual LuxCore build prerequisites
(`make deps` etc.) are not involved. Conversely, having baked modules changes
nothing for the build: same compilers, same dependencies - only the embedded
modules are added.

## Baking

From the LuxCore source root, in a terminal with the prerequisites available
(the MSYS2 `UCRT64` shell is the simplest: run
`python -m build-system.luxmake spirv` there; from any other shell, see the
compiler note above):

```
> make spirv
```

Expected output (the interesting lines):

```
-- Baking the OpenCL kernels in SPIR-V format with C:/msys64/ucrt64/bin/clang.exe (spir mode)
[...]
[xx/xx] Compiling pathocl_nvidia in SPIR-V format
[...]
INFO:LuxCore:OpenCL kernels baked in SPIR-V format in '<...>\out\spirv-build\baked'
INFO:LuxCore:They will be used by the next 'make config' (see the LUXRAYS_SPIRV_BAKED_DIR CMake variable)
```

`direct mode` appears instead of `spir mode` if the clang includes the
SPIR-V back-end (not the case of the official Windows builds). The MSYS2
clang runs in `direct mode`, but it drives the `llvm-spirv` tool as its
back-end helper: the tool must be installed in both cases.

The bake takes a few minutes (9 monolithic sources of ~1.2 MB each,
compiled to SPIR-V in parallel). It writes, in `out\spirv-build\baked`:

- 9 `.spv` modules (`<engine>_<platform>.spv`, with the engine in
  PATHOCL/TILEPATHOCL/RTPATHOCL and the platform in generic/amd/nvidia);
- the matching `.cl` sources and `.params` parameter files (debugging aid);
- `spirvmanifest.txt`, the list of the modules (used by the build to check
  the completeness of the directory).

The modules are baked with the exact parameters used at run time (including
the default `scene.epsilon.min/max` and the fast-math OpenCL options), so
they match the kernel cache hash: they are used as they are, on any platform,
without recompilation.

## Building with the baked modules

Nothing special to do: the next `make <target>` reconfigures CMake and picks
the bake up automatically (through the `LUXRAYS_SPIRV_BAKED_DIR` CMake
variable). Check the configuration log:

```
-- Using the pre-baked OpenCL kernels in SPIR-V format from ...
```

The same rule applies to the whole build (including `make clear`, which
removes the bake).

## Using modules baked elsewhere

`LUXRAYS_SPIRV_BAKED_DIR` (CMake variable or environment variable) can point
to any directory containing a complete bake. This is how the CI works (baked
once under Linux, served to all the platforms), and it allows two convenient
Windows alternatives to a local toolchain:

- **WSL**: bake in the Linux side (`make spirv` inside WSL, where the distro
  clang includes the SPIR-V back-end), then either let WSL write into a
  Windows-visible directory or copy `out/spirv-build/baked` across, and set
  `LUXRAYS_SPIRV_BAKED_DIR` to it;

- **CI artifacts**: download the bake artifact from a wheels/samples builder
  run and set `LUXRAYS_SPIRV_BAKED_DIR` to the extracted directory.

## Verifying at run time

The OpenCL kernel compilation log tells what was used, e.g.:

```
[PathOCL kernel] Compilation completed in 135 ms (translated from embedded SPIR-V)
[PathOCL kernel AdvancePaths_MK_RT_NEXT_VERTEX] Compilation completed in 135 ms (cached)
[PathOCL kernel AdvancePaths_MK_RT_DL] Compilation completed in 227 ms (compiled from source)
```

Caveat: the NVIDIA OpenCL devices and the AMD devices under the
Windows OpenCL drivers do not support the ingestion (`cl_khr_il_program` is
absent from the driver, whatever the version): on such a device the embedded
modules can not be used, a message is emitted and the kernels are compiled
from source. It is expected: on NVIDIA hardware, use
the CUDA path, which has its own pre-compilation.

The first compilation of a kernel with an embedded module populates the
persistent kernel cache (`~/.config/luxcorerender.org/ocl_kernel_cache` on
Linux); the next runs show `(cached)`. Deleting the cache is the way to
re-test the SPIR-V ingestion.

## Troubleshooting

| Symptom | Cause / fix |
|---------|-------------|
| `No way to compile OpenCL C in SPIR-V format: neither a clang including the SPIR-V back-end nor a clang + llvm-spirv chain is available` (fatal, at `make spirv` configure time) | `clang` or the `clang + llvm-spirv` pair is not visible: install the MSYS2 packages and check the PATH |
| `... can not translate the LLVM bitcode produced by ... (versions mismatch?)` (warning, configure time) | The clang and `llvm-spirv` versions do not match: use both from the same MSYS2 environment |
| `The embedded pre-compiled SPIR-V modules don't match these kernels ...` (warning, at render time) | The bake is stale (kernel sources changed after it) or the kernel parameters differ (e.g. non-default `scene.epsilon.*`), or the device doesn't support the ingestion: re-run `make spirv` then `make <target>` |
| `(compiled from source)` on an AMD/Intel device that supports the ingestion | Same as above: check the warning preceding it in the log |
| `fatal error C1083: Cannot open include file: 'boost/...'` (building `spirv-baker`) | The bake compiler has no boost available: use the MSYS2 compiler + the MSYS2 `boost` package, or provide a matching boost to the compiler of your choice |
| `LNK1104`/`LNK1319`/`LNK1120` linking `spirv-baker` with the LuxCoreDeps boost | The LuxCoreDeps boost libraries don't match the bake project build (toolset/flags): use the MSYS2 compiler + the MSYS2 `boost` package |
| `Running '/usr/bin/ninja.exe' '--version' failed with: no such file or directory` (fatal, at `make spirv` configure time) | `out/spirv-build` holds a CMake cache created from another environment (e.g. an MSYS2 shell): delete `out/spirv-build/CMakeCache.txt` and `out/spirv-build/CMakeFiles`, then re-run |
