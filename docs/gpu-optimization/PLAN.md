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
| T0.3 | Baseline numbers for `main` on the RTX 5070, including noise floor per scene. | done (sequential runs; superseded by A/B) | Table in `PROGRESS.md`. |
| T0.4 | Per-kernel GPU time profile: `opencl.kernel.profile.enable 1` waits for every kernel and logs the time per micro-kernel, OptiX trace and queue reset at the end of the render (Nsight Systems hangs `luxcoreconsole` on this machine, even with `--help`). | done, branch `gpu-opt/T0.4-kernel-profile` | Off by default; use for measurements only. |

## Phase 1: cheap, low risk

| ID | Task | Status | Acceptance |
|---|---|---|---|
| T1.1 | OptiX: `OPTIX_BUILD_FLAG_PREFER_FAST_TRACE`; `OPTIX_GEOMETRY_FLAG_DISABLE_ANYHIT`; ray flag `OPTIX_RAY_FLAG_DISABLE_ANYHIT` in `optixTrace`; debug level `NONE`; `usesPrimitiveTypeFlags = TRIANGLE`. Files: `optixaccelhw.cpp`, `optixaccel.cl`. | done (unverified) | Faster trace, image within noise. Check motion-blur scene still works. |
| T1.2 | Remove `--Ofast-compile=min` from NVRTC options (`cuda.cpp`). LuxCore bundles NVRTC 12.9.86, so the option was active; removing it may lengthen the cold compile (about 2.5 min with it). | done (unverified) | Speed >= baseline after clearing the kernel cache. |
| T1.3 | Upload OptiX launch params only when ray/hit buffer pointers change (`OptixKernel::EnqueueTraceRayBuffer`). | done (unverified) | No regression; image identical. |
| T1.4 | Include NVRTC built-in options (and NVRTC version) in the CUDA kernel cache key so option changes invalidate the cache (`cudaKernelCache` in `cuda.cpp`). | done (unverified), branch `gpu-opt/T1.4-cache-key` | Changing an option triggers a recompile without manual cache deletion. |
| T1.5 | Block size: sweep `opencl.gpu.workgroup.size` 32/64/128/256 per scene. If a size wins consistently, make CUDA pick it (e.g. `cuOccupancyMaxPotentialBlockSize` in `CUDADevice::GetKernelWorkGroupSize`, and stop forcing 32 for CUDA devices in `oclrenderengine.cpp`; keep 32 for OpenCL). | done (unverified), branch `gpu-opt/T1.5-blocksize`: per-kernel `cuOccupancyMaxPotentialBlockSize` | Measured gain on most scenes, none slower. |
| T1.6 | Register cap: sweep `CU_JIT_MAX_REGISTERS` via `cuModuleLoadDataEx` options (`cuda.cpp`, module load) together with T1.5. Check spills with `CU_JIT_INFO_LOG_BUFFER`. | done (unverified), branch `gpu-opt/T1.6-maxregisters`: property `cuda.kernel.maxregisters`; cap 128 A/B +0.4% to +2.5% | Measured gain; keep default if none. |
| T1.7 | Paths in flight: sweep `opencl.task.count` 512K (current cap for 12 GB) / 1M / 2M with the script's `-D`. Laine 2013 saw +5% going from 1M to 8M paths. If a larger pool wins, raise the cap in `PathOCLRenderEngine::UpdateTaskCount` for cards with enough VRAM. | done (unverified), branch `gpu-opt/T1.7-task-count`: 1M paths on cards > 10 GB | Measured gain; VRAM use stays well under 12 GB on LynxSpider. |

Phase 1 tasks T1.1-T1.3 are committed together on branch
`gpu-opt/T1.1-T1.3-optix-nvrtc`; verify them with T0.3 numbers before merging.

All implemented tasks are merged for testing on `gpu-opt/combined` (not into
`gpu-optimization` until verified).

## Phase 2: medium

| ID | Task | Status | Acceptance |
|---|---|---|---|
| T2.1 | Structure-of-arrays for hot path state. Step 1: move `PathState` out of `GPUTaskState` into its own `uint` buffer so state checks are coalesced (about 66 `->state` uses in `pathoclbase_kernels_micro.cl` / `pathoclbase_funcs.cl`, kernel args `KERNEL_ARGS`, buffer alloc in `pathoclbaseoclthreadinit.cpp`, TILEPATHOCL shares the kernels). Step 2: profile which other `GPUTaskState` / `EyePathInfo` fields are read by many kernels (e.g. `throughput`) and split those too. Laine 2013 measured +80% total from SOA vs AOS path state; LuxCore's gain will be smaller because its kernels are not split by material, but this is the cheapest layout win. | step 1 done (unverified), on `gpu-opt/T3.1-path-queues`; step 2 deferred (registers, not layout, look like the limit) | Step 1 est. 2-5%, step 2 more. Image within noise. |
| T2.2 | Shadow rays terminate on first hit when exact: add a shadow-ray flag in `Ray.flags`, pass `OPTIX_RAY_FLAG_TERMINATE_ON_FIRST_HIT` only for those rays, and only when the scene has no shadow-transparent materials and no volumes (decide on host, pass as kernel arg or define). Requires a careful read of `MK_RT_DL` and `throughShadowTransparency` handling first; write the analysis in `PROGRESS.md` before coding. | done (unverified), branch `gpu-opt/T2.2-shadow-anyhit` | Est. 5-15% of shadow trace time. LuxBall/DLSC faster; scenes with transparency/volumes unchanged and correct. |
| T2.3 | Pinned host memory (`cuMemHostAlloc`/`cuMemHostRegister`) for film and task-stats readback buffers. | rejected: film/stats transfers are capped at 1% of kernel time by the render loop | Est. <=1%. Merge only if measurable. |
| T2.4 | Non-default CUDA stream per device plus a CUDA graph for the per-iteration sequence (1 trace + 10 micro-kernels). OptiX launch must use the same stream. | rejected for now: launches are async and already hidden; gain < 1%, and a graph needs updates on every arg change | Est. 1-3%. |
| T2.5 | Interleave the material and texture evaluation stacks: element i of thread t at `stacks[i * globalSize + t]` instead of `stacks[t * maxSize + i]`, so stack pushes/pops of a warp are coalesced. | done (unverified image), branch `gpu-opt/T2.5-evalstack-interleave`: A/B LuxBall +1.1%, DLSC +1.7%, DanishMood +18% | Lossless (addresses only). |
| T2.6 | Write `SampleResult` AOV channels only when a film channel reads them (same conditions as `Film_AddSampleResultColor` / variance clamping); clear only `radianceGroupCount` radiance groups. | done (unverified image), branch `gpu-opt/T2.6-sampleresult-channels`: A/B vs T2.5 LuxBall +2.8%, DLSC +3.7% | Lossless (film gets the same values). |
| T2.7 | Structure-of-arrays for the remaining per-path structs used by the hot kernels (`GPUTaskState` incl. `BSDF`, `GPUTask.tmpHitPoint`, `SampleResult`, `EyePathInfo`): every access of a warp to an AoS field touches up to 32 sectors. Needs a word-interleaved layout or field-by-field split; large refactor of all `->` accesses in the kernels. Cycles stores its integrator state as one array per field. | superseded by T5.3. T2.7a (private BSDF copy in `MK_GENERATE_NEXT_VERTEX_RAY`, branch `gpu-opt/T2.7-local-bsdf`): no gain, the copy still reads the AoS buffer | Profile first (T0.4); est. large on small scenes. |

## Phase 3: large (start only if Phase 1-2 results justify it)

| ID | Task | Status | Acceptance |
|---|---|---|---|
| T3.1 | Per-state work queues (stream compaction), as in Laine 2013 section 4.4 and Padilla 2026 section 3.1: the kernel that sets a path's next state appends the path index to that state's queue (one counter + index array per state, preallocated, 4 bytes x taskCount each). Each micro-kernel then reads `queue[gid]` and exits if `gid >= count`. CUDA has no indirect dispatch, so keep launching `taskCount` threads: idle threads only read the counter, not the path state. Aggregate the atomics per warp (ballot + one `atomic_add` per warp); Laine measured +40% from this, though recent NVCC/NVRTC may already do it automatically, so check the PTX first. Changes the render loop, kernel args and every micro-kernel. | done (unverified), branch `gpu-opt/T3.1-path-queues`; A/B DLSC +5.5%, DanishMood +16% | Est. 10-25%. Design note in `PROGRESS.md` first. |
| T3.2 | Scene-feature kernel specialization: compile-time defines for used material/texture/light types, volumes, PhotonGI, etc., with `#if` guards in kernel code. Cost: recompiles when the feature set changes; must keep `kernelcachefill` working. Alternative from Laine 2013 section 4.3: split `MK_HIT_OBJECT` / BSDF evaluation into per-material kernels fed by per-material queues (needs T3.1), with expensive materials in their own kernel and simple ones grouped. | done (unverified), branch `gpu-opt/T3.2-kernel-specialization`: opt-in `opencl.kernel.specialize.enable` | Est. 10-30% depending on scene. Design note first. |
| T3.3 | Material-coherent shading: before `MK_HIT_OBJECT` (and `MK_DL_ILLUMINATE` / `MK_DL_SAMPLE_BSDF`, which also evaluate the BSDF), reorder that state's queue by material index so a warp evaluates one material. Counting sort over the T3.1 queue: per-material counters (atomics), prefix sum, scatter into a second index array. Cycles X ("shader sorting") and pbrt-v4 (per-material queues) both do this. Only the processing order changes, so the image is bit-identical up to float summation order of the film. Skip the sort when the scene has one material, or when the queue is small (sort cost > gain). | open | Gain on DLSC / LynxSpider / DanishMood, no loss on LuxBall. Image within noise. |

## Phase 4: less noise per second (unbiased variance reduction)

The user accepts less noise as long as the image is unbiased and has no
visible structure. These tasks change the noise, not the expected image, so
the image check differs from Phases 1-3: compare against a high-spp
reference of the baseline (e.g. 16K spp). Pass = mean difference within the
seed-to-seed spread and lower RMSE to the reference at equal render time,
with no structure in the diff image.

| ID | Task | Status | Acceptance |
|---|---|---|---|
| T4.1 | Path guiding with SD-trees (Mueller, Gross, Novak 2017, `cgf13227-sup-0002-s1.pdf` has the sampling and refinement pseudocode). Training passes of doubling length: the GPU records radiance per vertex (position, direction, value) into a buffer, the CPU builds the spatial binary tree + directional quadtrees between passes and uploads them flattened, `MK_GENERATE_NEXT_VERTEX_RAY` samples a mix of BSDF and guide (one-sample MIS, BSDF fraction 0.5, never 0 so the estimator stays unbiased). Only the last pass, or inverse-variance weighted passes, go to the film. Optional follow-up: variance-aware target density from Rath et al. 2020 (`a151-rath/`). Off for specular-only bounces. New property, off by default until it wins. | open | Unbiased (mean within seed spread vs reference); lower RMSE at equal time on DanishMood / LynxSpider (indirect-heavy). Large: design note in `PROGRESS.md` first. |

## Phase 5: memory-bound kernels (roadmap from Nsight Compute, 2026-10-26)

Goal set by the user: Octane-class speed on the RTX 5070. Octane is not
installed here, so parity cannot be measured directly; progress is tracked
in M samples/sec and kernel ms per M samples on the bench scenes.

Measured on LuxBall with Nsight Compute (`gpu-opt/combined`, details in
`PROGRESS.md`): the heavy kernels issue an instruction in only 6-9% of
cycles. Warps wait on memory (`long_scoreboard` stall 14-28 cycles per issued
instruction, everything else under 3). Three causes, each a multiplier:

1. Few warps to hide latency: 247-255 registers per thread, 12-16% achieved
   occupancy (about 7 of 48 warps per SM).
2. Uncoalesced per-path structs: 4-11 sectors per global load request and
   16-32 per store request (ideal for 4-byte fields: 4). Kernels that mainly
   write state are LSU-throttled (`MK_GENERATE_CAMERA_RAY` `lg_throttle`
   150, `MK_SPLAT_SAMPLE` 252).
3. Partly empty warps: 4.8 (`MK_RT_DL`), 15.8 (`MK_RT_NEXT_VERTEX`) and
   9-26 (queued kernels) active threads per warp instruction out of 32.

OptiX traversal is 1.5% of GPU time and the Sobol sampler about 2.5%
(RANDOM sampler A/B): neither is a lever.

Done: T5.1, T5.2, T5.8, T5.9. No gain: T5.3. Next: T5.5, T5.4, T5.6,
T5.7 (= T3.3). All lossless
(same math, different memory layout, schedule or dead code removal).

| ID | Task | Status | Acceptance |
|---|---|---|---|
| T5.1 | Kernel-local temporaries on CUDA: `GPUTask.tmpHitPoint`, `tmpBsdf` and `tmpPathDepthInfo` are only used inside one micro-kernel call (light sample point, shadow-ray transparency, volume textures, DL BSDF sampling). Declare them as kernel locals on CUDA (local memory is interleaved per thread, so coalesced, and cached in L1); keep the global buffer on OpenCL, whose `__global` pointers cannot point to private memory. | done, merged: LuxBall kernel 81.8 -> 70.5 ms per M samples | Kernel profile: `MK_DL_ILLUMINATE`, `MK_RT_DL`, `MK_RT_NEXT_VERTEX` faster; image identical. |
| T5.2 | Queues for `MK_RT_NEXT_VERTEX` and `MK_RT_DL`: double-buffered across iterations (the kernel that sets the state appends to the next iteration's queue; pass-through hits that keep tracing re-append). Today both run over all paths and their warps are 50-85% empty. | done, merged: built by a scan after each iteration (`QueueRTPaths`, replaced by T5.9) instead of double buffers; 70.5 -> 67.2 | Lanes per warp instruction up; kernel time down; image identical. |
| T5.3 | Transposed storage for the persistent per-path structs on CUDA (`GPUTaskState` incl. `BSDF`, `EyePathInfo`, `SampleResult`, `GPUTaskDirectLight`): word `w` of path `p` at `buf[w * taskCount + p]`. A kernel copies the words it uses into a private struct at entry (coalesced: 4 sectors per warp load) and writes back only what it changes. The struct code stays as is. Replaces T2.7. | no gain (branch `gpu-opt/T5.3-transposed-taskstate`, `GPUTaskState` only): 65.2 -> 81.6, 70.4 with T5.9 queues. The full copy reads all 97 words, AoS only the fields a kernel uses. Would need per-field strided access in all BSDF code | Sectors per request near 4; large gain on all heavy kernels; image identical. |
| T5.4 | Smaller `HitPoint`: `localToWorld` (128 B, a third of `BSDF`) is per-mesh data; look it up or recompute it from mesh index and ray time where it is used instead of storing it per path. Check every reader first. | todo | Bit-identical transform values; less traffic in `MK_RT_NEXT_VERTEX`. |
| T5.5 | Feature specialization beyond material/texture types (extends T3.2): compile out light types, volumes, PhotonGI, bake maps, shadow catcher, holdout, hybrid back/forward and unused AOVs when the scene does not use them. Goal: fewer registers, higher occupancy; then re-sweep `cuda.kernel.maxregisters` (T1.6). | todo | Registers per heavy kernel well under 255 on LuxBall/DLSC; speed up; image identical. |
| T5.6 | Fuse `MK_HIT_OBJECT` + `MK_DL_ILLUMINATE` + `MK_DL_SAMPLE_BSDF` (they always run back to back in one iteration on the same paths) into one kernel that loads the BSDF once. Same code and sample order, so the image is unchanged. | todo | Fused time < sum of the three (30 ms per M samples on LuxBall). |
| T5.7 | Material sorting (T3.3) for the BSDF kernels: 9-26 active lanes per warp instruction today. | todo | See T3.3. |
| T5.8 | Material and texture evaluation stacks as kernel locals on CUDA, sized at compile time (`PARAM_MATERIAL_EVAL_STACK_SIZE`, `PARAM_TEXTURE_EVAL_STACK_SIZE`) when kernels are specialized; the global stack buffers shrink to 1 float. | done, merged: 67.2 -> 65.8; with T5.1+T5.2 +17-36% M samples/sec on the 5 scenes | Image identical; kernel time down. |
| T5.9 | Build each path queue by scanning the path states right before its micro-kernel (`QueuePaths`, one launch per state) instead of pushing in `PATH_STATE_SET()`: queues in ascending path order, so a warp touches neighbouring rays, hits and sample results. | done, merged: all queues scanned except `MK_GENERATE_NEXT_VERTEX_RAY` (pushed, keeps completion order); +1.9 to +8.3% vs T5.8, DanishMood -1.7% within noise | Kernel time down net of the scans; image identical. |

## References

PDFs are in `D:\REPO\LuxCoreRepo\papers\` (outside git; do not commit them).
Extract text with `pdftotext -layout <file> <out.txt>` (available in Git Bash).

- `laine2013hpg_paper.pdf`: Laine, Karras, Aila, "Megakernels Considered
  Harmful: Wavefront Path Tracing on GPUs", HPG 2013. The design LuxCore's
  micro-kernels follow, minus the queues. Key measured effects: SOA path state
  +80% total vs AOS (T2.1), queues so each kernel runs only on its paths
  (T3.1), warp-coalesced queue atomics +40% (T3.1), per-material kernels (T3.2),
  path pool 1M to 8M +5% (T1.7). Pool of 1M paths at 212 bytes each.
- `2605.27323v1.pdf`: Padilla, Webster, Kim, "Megakernel vs Wavefront GPU Path
  Tracing", arXiv 2605.27323, 2026. Vulkan + RT cores on an RTX 3060 Ti:
  wavefront +16% over megakernel, attributed to cache locality (higher L2 and
  VRAM throughput). Section 3.1: launching every bounce over all pixels
  wastes threads and memory traffic; they compact active paths with atomic
  index lists (T3.1). Neither design saturated any GPU unit, so latency,
  memory traffic and synchronization are the limits, which supports layout
  and queue work over more arithmetic tuning.

- Mueller, Gross, Novak, "Practical Path Guiding for Efficient
  Light-Transport Simulation", CGF 36(4), EGSR 2017 (doi 10.1111/cgf.13227).
  SD-tree: binary tree over space, quadtree over directions, trained in
  passes of doubling sample count. Unbiased when mixed with BSDF sampling.
  Basis of T4.1. Supplementary pseudocode: `cgf13227-sup-0002-s1.pdf`.
- `a151-rath/`: Rath, Grittmann, Herholz, Vevoda, Slusallek, Krivanek,
  "Variance-Aware Path Guiding", SIGGRAPH 2020 (supplemental, code under
  `supplemental/path-guiding/code`). Guides toward the variance-optimal
  density instead of the radiance product; refinement for T4.1.
- `125-134.pdf`, `bekaert_colorpage.pdf`: Bekaert, Sbert, Halton,
  "Accelerating Path Tracing by Re-Using Paths", EGWR 2002. Unbiased, about
  10x on indirect light, but pixels in a tile share paths: correlated,
  blocky noise (their figure 5). Rejected: visible structure.
- Laine, Karras, Aila 2013 (above) and pbrt-v4 chapter 15 "Wavefront
  Rendering on GPUs": per-material queues and kernels (T3.3 alternative).
- Cycles X (code.blender.org 2021-04 and 2021-11): wavefront kernels with
  per-state queues (done in T3.1), shader sorting (T3.3), decoupled shadow
  kernel (LuxCore already has `MK_RT_DL`), adaptive sample batching for
  occupancy (LuxCore already adapts 1-128 iterations per batch), scrambling
  distance (+1-5%, rejected: correlates samples between pixels), hair
  transparency baking (not applicable, LuxCore has no hair transparency
  per shadow hit). The Cycles X slides (Google Docs) and the OTOY
  performance page (HTTP 403) could not be read.
- Blender 2019 "Accelerating Cycles using NVIDIA RTX": OptiX for traversal
  (LuxCore already does this) and the OptiX denoiser (out of scope).
- ReSTIR DI/GI and GRIS (NVIDIA 2020-2022): spatiotemporal resampling for
  real-time. Rejected for final renders: spatial reuse correlates
  neighbouring pixels (structured noise at low spp), the practical variants
  are biased, and temporal reuse needs animation frames.
- Interactive path tracing of sparse volumes (NVIDIA 2021): NanoVDB +
  unbiased tracking + reconstruction. Rejected: the reconstruction is a
  denoiser, and replacing LuxCore's ray-marched heterogeneous volumes with
  tracking estimators changes the converged image.

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
