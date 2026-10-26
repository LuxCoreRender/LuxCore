# GPU (CUDA/OptiX) optimization plan

Rules, build, benchmark protocol and git workflow: see `/AGENTS.md`.
Results and decisions: see `PROGRESS.md`.

Status values: `todo`, `in progress`, `done (unverified)`, `done`, `rejected`.
Gain estimates are unmeasured guesses from code reading; replace them with
measured numbers in `PROGRESS.md`.

## Current architecture (why it is slow)

- PATHOCL is a wavefront path tracer. Each iteration: one OptiX launch traces
  all rays, then 10 micro-kernels (`AdvancePaths_MK_*`) advance the path state
  machine.
- Every micro-kernel is launched over **all** `taskCount` paths (512K on 12 GB
  cards). Each thread reads its state and returns if it does not match
  (`pathoclbase_kernels_micro.cl`, e.g. line 52). Most threads in most
  launches do no work.
- The state is the first field of `GPUTaskState`, which also contains a large
  `BSDF`. Reading it is uncoalesced: about 32 bytes of traffic per thread per
  kernel, roughly 168 MB per iteration just for state checks.
- One generic kernel is compiled for every scene. Only engine type and epsilon
  are compile-time defines (`kernelsources.cpp`). All material, texture and
  light code is in every kernel, which raises register pressure and lowers
  occupancy.
- CUDA work goes to the legacy default stream 0; launches use a fixed block
  size of 32 (`CUDADevice::GetKernelWorkGroupSize` returns 32 and
  `opencl.gpu.workgroup.size` defaults to 32).
- Shadow rays (`MK_RT_DL`) find the closest hit even when any hit would do.

## Phase 0: baseline and harness

| ID | Task | Status | Acceptance |
|---|---|---|---|
| T0.1 | Release build of `luxcoreconsole` and `pyluxcore` on `main` (baseline). Record toolchain versions (MSVC, CUDA, NVRTC actually used, driver, Conan) and any extra build steps. | done (luxcoreconsole; pyluxcore not built yet) | Builds; LuxBall renders on CUDA+OptiX; log confirms OptiX accelerator. |
| T0.2 | Benchmark script (Python) implementing the AGENTS.md protocol: runs the scene set, parses samples/sec, renders fixed-SPP images, computes RMSE vs baseline and noise floor, prints a markdown table. Lives in `tests/gpu_bench/`. | done | One command produces the table for a given build dir. |
| T0.3 | Baseline numbers for `main` on the RTX 5070, including noise floor per scene. | todo | Table in `PROGRESS.md`. |

## Phase 1: cheap, low risk

| ID | Task | Status | Acceptance |
|---|---|---|---|
| T1.1 | OptiX: `OPTIX_BUILD_FLAG_PREFER_FAST_TRACE`; `OPTIX_GEOMETRY_FLAG_DISABLE_ANYHIT`; ray flag `OPTIX_RAY_FLAG_DISABLE_ANYHIT` in `optixTrace`; debug level `NONE`; `usesPrimitiveTypeFlags = TRIANGLE`. Files: `optixaccelhw.cpp`, `optixaccel.cl`. | done (unverified) | Faster trace, image within noise. Check motion-blur scene still works. |
| T1.2 | Remove `--Ofast-compile=min` from NVRTC options (`cuda.cpp`). LuxCore bundles NVRTC 12.9.86, so the option was active; removing it may lengthen the cold compile (about 2.5 min with it). | done (unverified) | Speed >= baseline after clearing the kernel cache. |
| T1.3 | Upload OptiX launch params only when ray/hit buffer pointers change (`OptixKernel::EnqueueTraceRayBuffer`). | done (unverified) | No regression; image identical. |
| T1.4 | Include NVRTC built-in options (and NVRTC version) in the CUDA kernel cache key so option changes invalidate the cache (`cudaKernelCache` in `cuda.cpp`). | todo | Changing an option triggers a recompile without manual cache deletion. |
| T1.5 | Block size: sweep `opencl.gpu.workgroup.size` 32/64/128/256 per scene. If a size wins consistently, make CUDA pick it (e.g. `cuOccupancyMaxPotentialBlockSize` in `CUDADevice::GetKernelWorkGroupSize`, and stop forcing 32 for CUDA devices in `oclrenderengine.cpp`; keep 32 for OpenCL). | todo | Measured gain on most scenes, none slower. |
| T1.6 | Register cap: sweep `CU_JIT_MAX_REGISTERS` via `cuModuleLoadDataEx` options (`cuda.cpp`, module load) together with T1.5. Check spills with `CU_JIT_INFO_LOG_BUFFER`. | todo | Measured gain; keep default if none. |

Phase 1 tasks T1.1-T1.3 are committed together on branch
`gpu-opt/T1.1-T1.3-optix-nvrtc`; verify them with T0.3 numbers before merging.

## Phase 2: medium

| ID | Task | Status | Acceptance |
|---|---|---|---|
| T2.1 | Move `PathState` out of `GPUTaskState` into its own `uint` buffer so state checks are coalesced. Touches about 66 `->state` uses in `pathoclbase_kernels_micro.cl` / `pathoclbase_funcs.cl`, kernel args (`KERNEL_ARGS`), buffer alloc in `pathoclbaseoclthreadinit.cpp`, and TILEPATHOCL which shares the kernels. Skip if T3.1 is done first (it subsumes this). | todo | Est. 2-5%. Image within noise. |
| T2.2 | Shadow rays terminate on first hit when exact: add a shadow-ray flag in `Ray.flags`, pass `OPTIX_RAY_FLAG_TERMINATE_ON_FIRST_HIT` only for those rays, and only when the scene has no shadow-transparent materials and no volumes (decide on host, pass as kernel arg or define). Requires a careful read of `MK_RT_DL` and `throughShadowTransparency` handling first; write the analysis in `PROGRESS.md` before coding. | todo | Est. 5-15% of shadow trace time. LuxBall/DLSC faster; scenes with transparency/volumes unchanged and correct. |
| T2.3 | Pinned host memory (`cuMemHostAlloc`/`cuMemHostRegister`) for film and task-stats readback buffers. | todo | Est. <=1%. Merge only if measurable. |
| T2.4 | Non-default CUDA stream per device plus a CUDA graph for the per-iteration sequence (1 trace + 10 micro-kernels). OptiX launch must use the same stream. | todo | Est. 1-3%. |

## Phase 3: large (start only if Phase 1-2 results justify it)

| ID | Task | Status | Acceptance |
|---|---|---|---|
| T3.1 | Per-state work queues (stream compaction): each micro-kernel runs only on paths in its state, using atomic-append queues or a prefix sum. Changes the render loop, kernel args and every micro-kernel. | todo | Est. 10-25%. Design note in `PROGRESS.md` first. |
| T3.2 | Scene-feature kernel specialization: compile-time defines for used material/texture/light types, volumes, PhotonGI, etc., with `#if` guards in kernel code. Cost: recompiles when the feature set changes; must keep `kernelcachefill` working. | todo | Est. 10-30% depending on scene. Design note first. |

## Out of scope

- Anything that changes the image (fast math, clamping, depth, denoiser
  defaults, adaptive sampling defaults).
- `CU_CTX_SCHED_*` flags, OptiX log level: no measurable render-time effect
  expected.
- `--use_fast_math`: already the upstream default for CUDA kernels
  (`pathoclbase.cpp:100`), so it is not an available lever.
- Task count: 12 GB cards already get the 512K maximum (`UpdateTaskCount`).

## User-side settings (no code)

In Blender, hybrid CPU+GPU rendering (`use_native_cpu`) can slow GPU renders.
Test with it off. Adaptive sampling and the OptiX denoiser give large speedups
but change the image, so they are the user's choice, not part of this plan.
