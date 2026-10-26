# Agent guide: LuxCore GPU (CUDA/OptiX) optimization

This file is the entry point for any agent working on the GPU optimization
effort. Read it fully before touching code, then read
`docs/gpu-optimization/PLAN.md` (what to do) and
`docs/gpu-optimization/PROGRESS.md` (what has been done, measured and decided).

## Goal

Make PATHOCL rendering on NVIDIA CUDA/OptiX faster **without changing the
rendered image**. Target hardware: NVIDIA RTX 5070 12 GB (Blackwell, sm_120),
Windows 11, CUDA toolkit 12.8 installed.

## Hard rules

1. **Lossless only.** A change must not alter the image beyond the run-to-run
   noise floor defined in "Image check" below. No lower path depth, no
   clamping, no denoiser or adaptive-sampling default changes, no
   approximations in materials, lights or sampling. Keep the existing
   math-mode options as they are (`--use_fast_math` is already the upstream
   default for CUDA kernels, `src/slg/engines/pathoclbase/pathoclbase.cpp:100`).
2. **Measure, don't guess.** Every performance change needs before/after
   numbers from the benchmark protocol below, recorded in `PROGRESS.md`.
   A change with no measurable gain is reverted, not merged.
3. **One task per branch, small commits.** See "Git workflow".
4. **Never push, open PRs, or force-push** without explicit approval from the
   user. Commit locally only.
5. **Update memory before you stop.** Append your results, decisions and open
   questions to `PROGRESS.md` and update task status in `PLAN.md` in the same
   commit as the work (or a follow-up commit) so the next agent can continue.

## Workspace layout

| Path (relative to `D:\REPO\LuxCoreRepo`) | What | Git |
|---|---|---|
| `LuxCore/` | Render engine. All code work happens here. | yes, `origin` = github.com/Diginaat/LuxCore |
| `BlendLuxCore/` | Blender addon (uses the `pyluxcore` wheel). | yes, separate repo |
| `LuxCoreTestScenes/scenes/*/LuxCoreScene/` | Exported test scenes: `render.cfg`, `scene.scn`, `reference.png` one level up. | yes, separate repo |

## Code map (GPU path)

| Area | File |
|---|---|
| OptiX accelerator build + launch | `src/luxrays/accelerators/optixaccelhw.cpp` |
| OptiX raygen/closesthit/miss programs | `include/luxrays/accelerators/optixaccel.cl` |
| CUDA device: context, launches, buffers, streams | `src/luxrays/devices/cudadevice.cpp` |
| NVRTC compile options, PTX cache, module load | `src/luxrays/utils/cuda.cpp` |
| Engine device selection, workgroup size defaults | `src/slg/engines/oclrenderengine.cpp` |
| Task count (paths in flight) | `src/slg/engines/pathocl/pathocl.cpp` (`UpdateTaskCount`) |
| GPU render loop (trace + micro-kernels, film transfer) | `src/slg/engines/pathocl/pathoclopenclthread.cpp` |
| Micro-kernel compile + enqueue | `src/slg/engines/pathoclbase/pathoclbaseoclthreadkernels.cpp` |
| Micro-kernels (path state machine) | `include/slg/engines/pathoclbase/kernels/pathoclbase_kernels_micro.cl` |
| GPU task data types (`GPUTaskState`) | `include/slg/engines/pathoclbase/kernels/pathoclbase_datatypes.cl` |
| Kernel compile-time defines | `src/slg/kernels/kernelsources.cpp` (`GetKernelParamters`) |

Kernels are OpenCL-style `.cl` sources. On CUDA they are compiled at runtime
with NVRTC to PTX (emulation layer: `include/luxrays/devices/cudadevice_oclemul_*.cl`).
OptiX is used only for ray/triangle intersection; shading runs in the CUDA
micro-kernels. Bundled OptiX headers are **7.1** (`deps/optix-7.1.0`): newer
APIs and enums (e.g. `OPTIX_COMPILE_DEBUG_LEVEL_MINIMAL`, `optixModuleCreate`)
do not exist.

## Build (Windows)

Prerequisites (see `README.md`): Visual Studio with C++ (run from a shell where
`vcvarsall.bat` was called), Python, Conan (`pip install conan`), CMake.

```
cd D:\REPO\LuxCoreRepo\LuxCore
make.bat deps            :: first time only, downloads dependencies (slow)
make.bat luxcoreconsole  :: benchmark binary
make.bat pyluxcore       :: Python module used by Blender
```

Binaries land in `out\install\Release\bin`. Build type: env var
`LUX_BUILD_TYPE` (`Release` default, or `Debug`). Benchmark only Release.

If the build needs steps not written here, record them in `PROGRESS.md`
under "Environment".

## CUDA kernel cache (important)

Compiled PTX is cached in `%TEMP%\luxcorerender.org\cuda_kernel_cache`.
The cache key hashes only the kernel source and the `-D` parameters, **not**
NVRTC's built-in options in `cudaKernelCache::ForcedCompilePTX`
(`src/luxrays/utils/cuda.cpp`). After changing those options, delete the cache
directory or the old PTX is reused and your change has no effect. (Plan task
T1.4 fixes this.)

Kernel compile time is not render time: always warm the cache with one run
before measuring. A cold PathOCL kernel compile takes about 2.5 minutes.

`tests/gpu_bench/bench.py` gives each build label its own `TEMP` (so its own
kernel cache) under `<out>/<label>/tmp`, which avoids this problem when
comparing builds.

## Benchmark protocol

Scene set (from `LuxCoreTestScenes/scenes`), chosen to cover features:

| Scene | Why |
|---|---|
| `LuxBall` | simple, no volumes, no transparency |
| `DLSC` | 622 meshes, no volumes/transparency (cfg is PATHCPU: override engine) |
| `LuxCore2.1Benchmark` | many transparent materials |
| `DanishMood` | interior, volumes, textures |
| `LynxSpider` | heavy: 871 meshes, volumes, transparency |

Run from the scene's `LuxCoreScene` directory. Common overrides:

```
-D renderengine.type PATHOCL
-D renderengine.seed 1
-D sampler.sobol.adaptive.strength 0
-D opencl.cpu.use 0 -D opencl.gpu.use 1
-D opencl.native.threads.count 0
```

Check the log once to confirm the CUDA device is used with OptiX
(`cuda.optix.devices.select` / device names in the log) and record the exact
device-selection overrides in `PROGRESS.md`.

**Speed:** `-D batch.halttime 60`. Read the last
`[Avg. samples/sec ...M ...]` line of `luxcoreconsole`. One warm-up run, then
3 measured runs; report the median. Close other GPU apps (Blender, browsers
with GPU video) while measuring.

**Image check:** `-D batch.haltspp 256` with the same seed, saving a linear
`RGB` film output as `.hdr` (LuxCore writes linear outputs only as `.exr` or
`.hdr`). Render the baseline twice: the RMSE between the two baseline images
is the noise floor. A candidate passes if RMSE(candidate, baseline) <= 1.1 x
noise floor and the diff image shows no structure (edges, shadows, banding).
Record both numbers. Observed on LuxBall at 16 spp: two baseline runs were
bit-identical (noise floor 0), so a pass then means an identical image.

**Script:** `tests/gpu_bench/bench.py` implements this protocol (standard
library only). Results go to `D:\REPO\LuxCoreRepo\bench\<label>\`.

```
python tests/gpu_bench/bench.py speed   --bin <bin>\luxcoreconsole.exe --label main
python tests/gpu_bench/bench.py image   --bin <bin>\luxcoreconsole.exe --label main
python tests/gpu_bench/bench.py image   --bin <bin>\luxcoreconsole.exe --label T1.1 --repeat 1
python tests/gpu_bench/bench.py compare --baseline main --candidate T1.1
```

Add `--scenes LuxBall ...` to limit scenes, `-D <prop> <value>` for sweeps.
Copy each build's `out\install\Release\bin` to `bench\bin-<label>` before
benchmarking so later builds don't overwrite it.

## Git workflow

- Integration branch: `gpu-optimization` (branched from upstream `main` at
  `ce1d64155`). It holds the docs and merged, verified tasks. `main` stays
  untouched as the baseline.
- One branch per task, from `gpu-optimization`: `gpu-opt/<task-id>-<slug>`,
  e.g. `gpu-opt/T2.1-pathstate-array`.
- Commit messages: Conventional Commits, scope = area, body includes the
  measured result, e.g.

  ```
  perf(optix): disable any-hit and prefer fast trace

  LuxBall 12.3 -> 13.1 M samples/sec (+6.5%), image RMSE within noise.
  ```

- Merge into `gpu-optimization` with `git merge --no-ff` only after the task
  passes speed and image checks and `PROGRESS.md` is updated.
- A task that fails stays on its branch; note the result in `PROGRESS.md`
  so nobody retries it blindly.
- Do not commit build output (`out/`), kernel caches, or rendered images.
  Put benchmark images outside the repo (e.g. `D:\REPO\LuxCoreRepo\bench\`).

## Memory

The repository docs are the shared memory between agents:

- `docs/gpu-optimization/PLAN.md`: task list, status, acceptance criteria.
  Update the status column when you start (`in progress`) and finish a task.
- `docs/gpu-optimization/PROGRESS.md`: append-only log. Benchmarks, decisions,
  dead ends, environment notes. Newest entry at the bottom, dated (YYYY-MM-DD).

Write facts a cold-start agent needs; do not repeat what `git log` already says.
