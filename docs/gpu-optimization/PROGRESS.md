# GPU optimization progress log

Append-only. Newest entry at the bottom. Date every entry (YYYY-MM-DD).
Record: what was done, measured numbers, decisions, dead ends, open questions.

## Environment

- GPU: NVIDIA RTX 5070 12 GB (Blackwell, compute capability 12.0).
- OS: Windows 11 IoT Enterprise LTSC 2024.
- CUDA toolkit 12.8 at `C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v12.8`.
- CUDA driver reported by LuxCore: 13.50. OptiX available (headers 7.1).
- Bundled NVRTC (used for kernels): 12.9.86, from LuxCoreDeps 2.2.4.
- CMake 4.4.3, Conan 2.33.0, Python 3.14.8.
- MSVC: VS 2022 BuildTools (`C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools`),
  matching the windows-2022 prebuilt deps. VS 18 Community is also installed; not used.
- Build steps that worked (from `cmd`, `make.bat` must be called by full path
  because the bare name is not found from this shell):

  ```
  call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
  cd /d D:\REPO\LuxCoreRepo\LuxCore
  D:\REPO\LuxCoreRepo\LuxCore\make.bat deps
  D:\REPO\LuxCoreRepo\LuxCore\make.bat luxcoreconsole
  ```

- Agent sandbox note: network access (pip, Conan downloads) and GPU access
  need commands run outside the sandbox.

## Baseline

Not measured yet (T0.3).

| Scene | samples/sec (M) | noise floor RMSE |
|---|---|---|
| LuxBall | | |
| DLSC | | |
| LuxCore2.1Benchmark | | |
| DanishMood | | |
| LynxSpider | | |

## Log

### 2026-10-26: code review and Phase 1 edits

- Reviewed the CUDA/OptiX path; findings are in `PLAN.md` ("Current
  architecture").
- Implemented T1.1-T1.3 (OptiX flags, removed `--Ofast-compile=min`, params
  upload only on change). Not built, not measured. Committed on branch
  `gpu-opt/T1.1-T1.3-optix-nvrtc`.
- Found: CUDA kernel cache key ignores NVRTC built-in options; clear
  `%TEMP%\luxcorerender.org\cuda_kernel_cache` after option changes (T1.4).
- Found: CUDA block size is always 32 (`CUDADevice::GetKernelWorkGroupSize`
  returns 32, and `opencl.gpu.workgroup.size` forces 32 by default) (T1.5).
- Decided: `--use_fast_math` rejected (risk of NaN/fireflies; violates the
  lossless rule).
- Next: T0.1 build, T0.2 script, T0.3 baseline, then verify T1.1-T1.3.

### 2026-10-26: baseline build and benchmark script

- T0.1: `luxcoreconsole` built from `main` (`ce1d64155`), copied to
  `D:\REPO\LuxCoreRepo\bench\bin-main`. Log confirms CUDA device
  "NVIDIA GeForce RTX 5070" with "Optix enabled: 1", accelerator OPTIX.
  No device-selection overrides needed: with a CUDA device present, LuxCore
  uses only CUDA devices.
- T0.2: `tests/gpu_bench/bench.py` written and smoke-tested on LuxBall.
- Smoke numbers (not a baseline, 1 run of 20 s): LuxBall 11.43 M samples/sec.
- Two baseline renders of LuxBall at 16 spp were bit-identical (noise floor 0).
- Found: `--use_fast_math` is already on for CUDA kernels upstream
  (`pathoclbase.cpp:100`), so the "rejected" decision above is moot.
- Found: cold PathOCL kernel compile takes 147 s (with `--Ofast-compile=min`).
- Found: OptiX logs at level 4 (`cudadevice.cpp:174`), e.g. KNOBS/DISKCACHE lines.
- Found: linear outputs can only be `.exr`/`.hdr`; the script uses `.hdr`.

### 2026-10-26: papers reviewed

- Read `papers/laine2013hpg_paper.pdf` and `papers/2605.27323v1.pdf`
  (summaries and task mapping in `PLAN.md`, "References").
- LuxCore PATHOCL already is a wavefront path tracer, but without the two
  things Laine 2013 found most valuable: queues (every micro-kernel runs over
  all paths) and SOA path state (`GPUTaskState` is one AOS struct with an
  embedded `BSDF`).
- Plan changes: added T1.7 (path pool size sweep); T2.1 widened from the
  state field to SOA for hot path state; T3.1 got a concrete queue design
  (no indirect dispatch on CUDA: launch `taskCount`, exit on `gid >= count`);
  T3.2 got the per-material-kernel alternative.
- Priority after Phase 1: T2.1, then T3.1. Both papers point to memory
  layout and wasted threads, not arithmetic, as the limit.

### 2026-10-26: first measurements, bench fixes, register report

- T0.3 (sequential runs, 60 s, median of 3, M samples/sec), `main`:
  LuxBall 11.93, DLSC 12.40, LuxCore2.1Benchmark 13.49, DanishMood 10.54,
  LynxSpider 5.54. Run-to-run spread up to about 2%.
- T1.1-T1.3 measured right after (same session): LuxBall 11.33, DLSC 12.18,
  LuxCore2.1Benchmark 13.43, DanishMood 9.96, LynxSpider 5.31 (-1% to -5%).
  Sequential runs over ~1 h are affected by GPU clock/temperature drift, so
  this is not conclusive. Re-measured with interleaved A/B runs (below).
- Image check flaw found: two baseline renders with the **same seed** differ
  only by float ordering, so their RMSE (the old noise floor) is far below
  real sampling noise. T1.1-T1.3 "failed" on LuxBall/DLSC/DanishMood with
  RMSE 2.5-4.6x that floor, which says nothing about bias: any change to the
  generated code changes the noise pattern. Fix in `bench.py`: baseline run
  i uses seed 1+i (noise floor = two independent renders); compare also
  prints the mean brightness change (bias check). User: less noise than the
  baseline is fine; the requirement is no bias and no visible structure.
- `bench.py speed` used the final samples/sec line, which is printed after
  the halt and counts shutdown time (e.g. 12.04 at 20 s vs 11.47 final).
  Now uses the last line within the halt time.
- New `bench.py ab`: interleaved runs of two builds/settings.
- Register report (`ptxas -v -arch=sm_120` on the cached PTX; strip the
  4-byte header of the cache file and set `.version 8.7` because the
  bundled NVRTC 12.9 emits PTX 8.8 and toolkit 12.8 ptxas reads up to 8.7):

  | Kernel | regs | stack/spill |
  |---|---|---|
  | MK_RT_NEXT_VERTEX | 246 | 176 B stack |
  | MK_HIT_NOTHING | 64 | 80 B |
  | MK_HIT_OBJECT | 255 | 192 B, 24 B spill |
  | MK_RT_DL | 248 | 176 B |
  | MK_DL_ILLUMINATE | 244 | 144 B |
  | MK_DL_SAMPLE_BSDF | 254 | 160 B |
  | MK_GENERATE_NEXT_VERTEX_RAY | 244 | 176 B |
  | MK_SPLAT_SAMPLE | 40 | 160 B |
  | MK_NEXT_SAMPLE | 39 | 16 B |
  | MK_GENERATE_CAMERA_RAY | 59 | 0 |

  ~250 registers per thread allows about 8 resident warps per SM: the heavy
  kernels are latency bound. Register pressure (T1.6 cap, T3.2
  specialization) is likely a bigger lever than wasted threads.
- T3.1 (queues) first A/B: LuxBall 11.87 -> 11.89 (+0.2%), DLSC
  12.44 -> 13.13 (+5.5%). Remaining scenes pending.

### 2026-10-26: all tasks implemented, fast A/B of `gpu-opt/combined`

- `gpu-opt/combined` = T1.1-T1.7, T2.1 step 1, T2.2, T3.1, T3.2 (plus docs).
  T2.3 and T2.4 rejected by analysis (see PLAN.md).
- Fast A/B, `main` vs combined with `opencl.kernel.specialize.enable 1`
  (`bench.py ab --runs 2 --halttime 20`, M samples/sec):

  | Scene | main | combined | change |
  |---|---|---|---|
  | LuxBall | 12.16 | 11.47 | -5.7% |
  | DLSC | 12.52 | 13.07 | +4.4% |
  | LuxCore2.1Benchmark | 9.52 | 9.51 | 0% |
  | DanishMood | 5.24 | 6.13 | +17% (main runs 5.72 / 4.76) |
  | LynxSpider | 5.59 | 8.31 | +49% |

- `cuda.kernel.maxregisters 128` on top of combined: mixed within noise
  (LuxBall +6%, DanishMood -15% with one run at 4.05). Keep default 0.
- LuxBall is slower with combined. T3.1 alone was neutral on LuxBall and
  the first sequential T1.1-T1.3 run was also -5% on LuxBall: isolate
  T1.1-T1.3 (PREFER_FAST_TRACE, `--Ofast-compile` removal), T1.5 and T1.7
  on LuxBall next.
- Absolute numbers drift a lot during a long session (main DanishMood
  10.54 at the start, 5.2-5.7 here; LuxCore2.1Benchmark 13.49 -> 9.5).
  Only interleaved A/B numbers are comparable.
- Image check at 128 spp (`compare main-img combined-img`): all scenes pass
  the RMSE test. Mean brightness: LuxCore2.1Benchmark -1.85% vs +0.98%
  between seeds. Cause: the scene uses the PhotonGI indirect cache, rebuilt
  every run; the same build and seed shifts by -0.5% between two runs, spread
  over the whole image (not fireflies). With
  `-D path.photongi.indirect.enabled 0`: candidate mean +0.001%, RMSE
  0.0083 vs noise floor 0.0835. No bias.
- For bias checks of PhotonGI scenes, disable the indirect cache.

### 2026-10-26: pyluxcore for Blender 4.5 (Python 3.11)

- Blender 4.5 embeds Python 3.11 (no headers/libs), Blender 5.2 embeds 3.13.
  The pyluxcore module must be built against the same minor version.
- No Python 3.11 was installed; the Adobe Substance 3D Designer SDK has a
  full 3.11.6 (`C:\Program Files\Adobe\Adobe Substance 3D Designer\plugins\pythonsdk`).
  Build: `cmake out/build "-DPython_EXECUTABLE=<that>\python.exe"
  -DPython_FIND_REGISTRY=NEVER -U_Python_EXECUTABLE
  -UPYBIND11_PYTHON_EXECUTABLE_LAST`, then `make.bat pyluxcore`.
  Note: this changes the cached Python for later `make.bat` runs.
- Installed over the existing wheel in
  `%APPDATA%\Blender Foundation\Blender\4.5\extensions\.local\lib\python3.11\site-packages`:
  `pyluxcore\pyluxcore.pyd` plus plain-named `tbb12`, `tbbmalloc`, `embree4`,
  `LuxOpenImageDenoise(_core)` DLLs and `LuxOpenImageDenoise_device_cpu.pyd`
  in `pyluxcore.libs`. Backup of the original wheel files:
  `D:\REPO\LuxCoreRepo\bench\blender45-pyluxcore-backup`.
- Pitfall: without delvewheel renaming, Windows does not resolve the plain
  `tbb12.dll` as a dependency of `embree4.dll` from the `os.add_dll_directory`
  dir (error 126), although it loads by name. Workaround in the installed
  `pyluxcore\__init__.py`: preload the five DLLs by full path in dependency
  order. Proper fix: a repaired wheel (`make.bat wheel-test`, needs
  `repairwheel`/delvewheel).
- Check with Blender's Python: import OK, CUDA + OptiX enabled, RTX 5070
  listed; LuxBall PATHOCL GPU render correct, task count 1048576.

### 2026-10-26: Blender 4.5 addon zip, T3.2 on by default

- Patching site-packages does not survive a Blender restart: Blender's
  extension manager reinstalls the wheels listed in the addon manifest.
- Install zip `D:\REPO\LuxCoreRepo\bench\addon\BlendLuxCoreDiginaat-2.11.1-blender45.zip`
  (display name BlendLuxCoreDiginaat, id still `blendluxcore`): BlendLuxCore
  `HEAD` + `wheels/` (stock nvrtc wheel, stock pyluxcore wheel with the local
  `pyluxcore.pyd`, plain-named DLLs and the preload patch, RECORD rewritten)
  + manifest `wheels` entry + `pyluxcore_installation_info.txt` holding
  sha256("pyluxcore==2.11.2") so luxloader skips its PyPI download.
- `opencl.kernel.specialize.enable` now defaults to true (commit on
  `gpu-opt/T3.2-kernel-specialization`): the addon never sets it.
- CUDA workgroup size: since T1.5 CUDA no longer uses 32; the occupancy API
  picks 256-1024 per kernel unless `opencl.gpu.workgroup.size` is set.
  Forcing 64 was requested and not done (smaller blocks; LuxBall 32 vs auto
  was within noise).

### 2026-10-26: where the GPU time goes on a small scene (T0.4), T2.5, T2.6

- Nsight Systems 2026.1.3 hangs `luxcoreconsole` (even `--help`), so T0.4
  adds `opencl.kernel.profile.enable` (sync + wall time per kernel).
- LuxBall, combined branch, 1M paths, 11.5 M samples/sec (profiling costs
  about 1%):

  | Kernel | share | registers |
  |---|---|---|
  | GENERATE_NEXT_VERTEX_RAY | 21.1% | 255 |
  | DL_ILLUMINATE | 17.7% | 255 (12 B spill) |
  | RT_NEXT_VERTEX | 13.3% | 252 |
  | GENERATE_CAMERA_RAY | 13.2% | 60 |
  | DL_SAMPLE_BSDF | 11.3% | 255 |
  | RT_DL | 9.6% | 252 |
  | HIT_OBJECT | 6.9% | 255 (376 B spill) |
  | SPLAT_SAMPLE | 3.4% | 40 |
  | NEXT_SAMPLE | 1.8% | 46 |
  | OptiX trace | **1.6%** | - |

- `nvidia-smi dmon` during LuxBall: SM 100%, memory controller 37%, 120 W
  of 250 W at full clocks. Ray tracing is almost free; the time is in the
  shading/bookkeeping kernels, which are latency-bound (few warps per SM,
  uncoalesced array-of-structs accesses), not bandwidth-bound.
- GENERATE_CAMERA_RAY has 60 registers and still costs 13%: it cleared
  ~340 B of `SampleResult` per sample (8 radiance groups + 19 AOVs) with
  uncoalesced writes. T2.6 removes the writes of unused channels.
- T2.5 (interleaved eval stacks), A/B vs combined: LuxBall +1.1%, DLSC
  +1.7%, DanishMood +18.4%. T2.6 vs T2.5: LuxBall +2.8%, DLSC +3.7%,
  DanishMood +16% (DanishMood is noisy: T2.5 alone measured 6.36 and 5.59
  in two A/Bs, likely its PhotonGI cache build inside the 20 s).
- Next lever (T2.7): structure-of-arrays for `GPUTaskState`/`BSDF`,
  `tmpHitPoint`, `SampleResult`.

### 2026-10-26: Nsight Compute profile, new roadmap (Phase 5)

- User goal: Octane-class speed on the RTX 5070. Octane is not installed,
  so parity is not measurable here; tracked via samples/sec instead.
- T2.7a (private BSDF copy in `MK_GENERATE_NEXT_VERTEX_RAY`, CUDA only):
  kernel 18.01 -> 18.10 ms per M samples, no gain. Kept on branch
  `gpu-opt/T2.7-local-bsdf`, not merged.
- Sobol vs RANDOM sampler (kernel profile, LuxBall): 83.1 vs 81.0 ms per M
  samples in total. The sampler costs about 2.5%: not a lever.
- Nsight Compute works once GPU performance counters are allowed for all
  users (NVIDIA Control Panel, Developer, Manage GPU Performance Counters;
  the user enabled it). Without it: `ERR_NVGPUCTRPERM`. Command used, from
  the scene dir, with `TEMP`/`TMP` set to `bench\T2.6\tmp` so the kernel
  cache is warm:

  ```
  "C:\Program Files\NVIDIA Corporation\Nsight Compute 2025.1.1\target\windows-desktop-win7-x64\ncu.exe"
    --target-processes all -k "regex:MK_(GENERATE_NEXT_VERTEX_RAY|DL_ILLUMINATE|...)"
    --launch-skip 160 --launch-count 8 --set full --import-source no
    -o bench\ncu\luxball-full <bin>\luxcoreconsole.exe <usual -D overrides> render.cfg
  ```

  Read reports with `extras\python\ncu_report.py` (the CSV export mixes
  locale number formats). Report: `bench\ncu\luxball-full.ncu-rep`.
- LuxBall, `bin-T2.6` (combined), one launch per kernel after 20
  iterations. occ = achieved occupancy, lanes = active threads per warp
  instruction, ld/st = sectors per global load/store request:

  | Kernel | ms | regs | occ % | issue % | lanes | ld | st | top stall (cycles/issue) |
  |---|---|---|---|---|---|---|---|---|
  | RT_NEXT_VERTEX | 2.00 | 247 | 15.7 | 7.7 | 15.8 | 8.5 | 16.3 | long_scoreboard 19.6 |
  | HIT_OBJECT | 0.94 | 255 | 14.5 | 7.1 | 9.2 | 4.4 | 6.8 | long_scoreboard 19.5 |
  | RT_DL | 1.27 | 250 | 12.5 | 8.9 | 4.8 | 3.7 | 4.3 | long_scoreboard 14.0 |
  | DL_ILLUMINATE | 2.54 | 252 | 16.1 | 6.6 | 26.4 | 6.8 | 29.1 | long_scoreboard 22.0 |
  | DL_SAMPLE_BSDF | 1.42 | 254 | 15.2 | 6.7 | 16.4 | 11.3 | 18.6 | long_scoreboard 22.1 |
  | GENERATE_NEXT_VERTEX_RAY | 3.14 | 255 | 16.1 | 5.7 | 18.1 | 10.1 | 17.5 | long_scoreboard 28.1 |
  | SPLAT_SAMPLE | 0.36 | 40 | 86.6 | 1.7 | 31.4 | 9.0 | 25.6 | long_scoreboard 326, lg_throttle 253 |
  | GENERATE_CAMERA_RAY | 1.40 | 60 | 59.6 | 2.7 | 26.0 | 4.4 | 31.9 | lg_throttle 150, long_scoreboard 108 |

- Reading: the kernels are memory-latency bound with too few warps
  (registers), uncoalesced per-path structs and half-empty warps. DRAM is
  not saturated (26-45% in the heavy kernels; `RT_NEXT_VERTEX` moves 542 MB
  per launch, L2 at 67%). SASS: each heavy kernel is 94-102K instructions
  with about 1600 calls (cuobjdump of an earlier combined cubin).
- New plan: Phase 5 in `PLAN.md` (T5.1-T5.7). T2.7 is replaced by T5.3.

### 2026-10-26: T5.1, T5.2, T5.8 (merged), T5.3 (no gain), T5.9

Kernel profile = `kprof.py` (one 30 s LuxBall run per build after a warm-up
run, ms of kernel time per M samples; single runs vary by about 1 ms per
kernel). A/B = 3 runs of 20 s per scene, median M samples/sec.

- T5.1 (`gpu-opt/T5.1-local-temporaries`): on CUDA the `GPUTask`
  temporaries (`tmpHitPoint`, `tmpBsdf`, `tmpPathDepthInfo`) are kernel
  locals (`GPUTASK_TMP` in `pathoclbase_kernels_micro.cl`). They are written
  and read back within one kernel: in global memory the reads go to L2
  (stores evict L1), in local memory they hit L1. LuxBall kernel total
  81.8 -> 70.5, `DL_ILLUMINATE` 16.0 -> 5-8.
- T5.2 (`gpu-opt/T5.2-rt-queues`): queues for `MK_RT_NEXT_VERTEX` and
  `MK_RT_DL`, built by `QueueRTPaths` after each iteration. 70.5 -> 67.2.
  `RT_NEXT_VERTEX` warps are fuller (15.8 -> 26.5 lanes, half the
  instructions) but the kernel stays L2-bound (72%, 31.5 sectors per store
  request, 506 MB DRAM per launch from the AoS `BSDF` stores).
- T5.8 (`gpu-opt/T5.8-local-evalstacks`): with `kernel specialization` on
  CUDA, the material/texture eval stacks are kernel locals sized by
  `PARAM_MATERIAL_EVAL_STACK_SIZE`/`PARAM_TEXTURE_EVAL_STACK_SIZE`
  (`evalstack_funcs.cl`); the global stack buffers shrink to 1 float.
  67.2 -> 65.8. Nsight table: `bench\ncu\tab-T5.8.txt`, report
  `bench\ncu\luxball-T5.8.ncu-rep`.
- Image check T5.8 (T5.1+T5.2+T5.8), 128 spp vs `main`, all pass:

  | Scene | noise floor RMSE | candidate RMSE | same-seed RMSE vs T2.6 |
  |---|---|---|---|
  | LuxBall | 0.0252 | 0.0059 | 0.0059 |
  | DLSC | 0.191 | 0.052 | 0.015 |
  | LuxCore2.1Benchmark | 0.066 | 0.011 | 0.019 |
  | DanishMood | 0.033 | 0.0055 | 0.0075 |
  | LynxSpider | 0.021 | 0.0035 | 0.0028 |

- A/B T2.6 (combined) vs T5.8, M samples/sec:

  | Scene | T2.6 | T5.8 | change |
  |---|---|---|---|
  | LuxBall | 11.36 | 14.76 | +29.9% |
  | DLSC | 13.84 | 17.53 | +26.7% |
  | LuxCore2.1Benchmark | 9.12 | 10.69 | +17.2% |
  | DanishMood | 5.42 | 7.37 | +36.0% (noisy: 4.0-7.6) |
  | LynxSpider | 8.17 | 10.31 | +26.2% |

- Paths in flight sweep on T5.8 (LuxBall): 1M, 256K, 128K, 64K all give
  15.0-15.4 M samples/sec. DRAM bandwidth is not the limit.
- T5.3 (`gpu-opt/T5.3-transposed-taskstate`, not merged): `GPUTaskState`
  stored transposed on CUDA (word i of path gid at `[i * taskCount + gid]`),
  each kernel copies it to a local struct and writes back the words it may
  change. 65.2 -> 81.6: the queues hand a warp scattered path indices, so
  the transposed words of a warp fall in different sectors. With
  scan-built queues (below) 70.4: still a loss, the full copy reads all 97
  words while AoS reads only the fields a kernel uses. A transposed layout
  only pays with per-field strided access through all the BSDF code.
- T5.9 (`gpu-opt/T5.9-scan-queues`): `QueuePaths` scans the path states
  right before each micro-kernel (one launch per state, state passed as an
  argument) instead of pushing in `PATH_STATE_SET()`. Queues are then in
  ascending path order and neighbouring lanes touch neighbouring rays,
  hits and sample results. Scans cost 1.1-1.5 ms per M samples (push was
  0.1-0.3). Scanning every state: LuxBall +9.5%, DLSC +9.0%, but
  LynxSpider -9.5%: its `MK_GENERATE_NEXT_VERTEX_RAY` went 17.3 -> 28.3 ms.
  Pushing in completion order groups paths of similar cost (warps that
  finish together), a weak material sort the scan loses. So
  `MK_GENERATE_NEXT_VERTEX_RAY` is still pushed in `PATH_STATE_SET()`, all
  other states are scanned. Image check passes on all 5 scenes. A/B vs T5.8:

  | Scene | T5.8 | T5.9 | change |
  |---|---|---|---|
  | LuxBall | 15.27 | 15.86 | +3.9% |
  | DLSC | 17.42 | 18.86 | +8.3% |
  | LuxCore2.1Benchmark | 10.74 | 10.94 | +1.9% |
  | DanishMood | 6.63 | 6.52 | -1.7% (noisy: 6.3-7.7) |
  | LynxSpider | 10.04 | 10.55 | +5.1% |

- Hint for T5.7: the completion-order effect above shows material
  coherence matters a lot in `MK_GENERATE_NEXT_VERTEX_RAY` on LynxSpider.
- Next: branch `gpu-opt/ridiculous` from `gpu-opt/combined`. Lossless work
  first (T5.5, T5.4, T5.6, T5.7); lossy options later, only behind config
  properties that default to off (user decision 2026-10-26).

### 2026-10-26: T5.4, T5.10, T5.7 (on `gpu-opt/ridiculous`)

Kernel profile = `kprof.py` as above (ms of kernel time per M samples, one
30 s run per build after a warm-up run). `gpu-opt/ridiculous` =
`gpu-opt/combined` + these three tasks.

- T5.4 (`gpu-opt/T5.4-hitpoint-no-transform`): `HitPoint` no longer stores
  `localToWorld` (128 B per path). The local 3D texture mappings and the
  wireframe texture recompute it with `ExtMesh_GetLocal2World()` from
  `meshIndex`, `triangleIndex` and the new `HitPoint.time`. LuxBall kernel
  65.6 -> 61.4, `MK_RT_NEXT_VERTEX` 10.7 -> 8.3. Image check passes on all
  5 scenes. A/B vs T5.9b (M samples/sec):

  | Scene | T5.9b | T5.4 | change |
  |---|---|---|---|
  | LuxBall | 15.63 | 16.71 | +6.9% |
  | DLSC | 18.99 | 19.98 | +5.2% |
  | LuxCore2.1Benchmark | 10.96 | 11.68 | +6.6% |
  | DanishMood | 6.29 | 7.80 | +24.0% (noisy) |
  | LynxSpider | 10.31 | 10.49 | +1.7% |

- T5.10 (`gpu-opt/T5.10-fuse-path-tail`): `MK_SPLAT_SAMPLE` also does the
  `MK_NEXT_SAMPLE` and `MK_GENERATE_CAMERA_RAY` steps. LuxBall splat + next
  sample + camera ray + queues 14.7 -> 11.8. Image check passes on all 5
  scenes. The A/B vs T5.4 is missing: the first one overlapped a kernel
  profile run (numbers unusable), the user cancelled the rerun.
- T5.7 (`gpu-opt/T5.7-material-sort`): the queues of `MK_HIT_OBJECT`,
  `MK_DL_SAMPLE_BSDF` and `MK_GENERATE_NEXT_VERTEX_RAY` can be built by a
  counting sort on `bsdf.materialIndex` (`CountPathsByMaterial`,
  `ScanPathsByMaterial`, `QueuePathsByMaterial`; the count pass stores the
  key, so the queue pass reads a path order array). `MK_GENERATE_NEXT_VERTEX_RAY`
  is no longer pushed in `PATH_STATE_SET()`.
- Always sorting: LynxSpider 89.2 -> 69.1 (`DL_SAMPLE_BSDF` 16.9 -> 3.6,
  `GENERATE_NEXT_VERTEX_RAY` 17.2 -> 10.6), but LuxBall 58.5 -> 70.4
  (`DL_ILLUMINATE` 5.5 -> 10.0, `DL_SAMPLE_BSDF` 6.1 -> 9.0). A warp then
  takes its paths from a much wider range of path indices: the locality the
  T5.9 scans gave is lost. Material count does not predict the winner
  (LuxBall 9, LynxSpider 23 materials).
- So the order is calibrated per state at run time: iterations 16-47 after
  each render thread start alternate sorted and path order, time queue +
  micro-kernel with `FinishQueue()` and `std::chrono::steady_clock`, and keep
  the faster one (logged as `Material sort <state>: ...`). `WallClockTime()`
  is `GetTickCount()` on Windows (15.6 ms steps): fine for the kernel profile
  totals, useless for timing one launch. `MK_DL_ILLUMINATE` (only
  `BSDF_IsDelta()` + light sampling) lost with the sort on all 5 scenes and
  is never sorted.
- Calibration choices (sorted / path order) and kernel totals, T5.10 vs T5.7:

  | Scene | HIT_OBJECT | DL_SAMPLE_BSDF | GENERATE_NEXT_VERTEX_RAY | T5.10 | T5.7 | change |
  |---|---|---|---|---|---|---|
  | LuxBall | path | path | path | 57.9 | 56.2 | -2.9% (noise) |
  | DLSC | path | sorted | path | 49.1 | 48.2 | -1.9% (noise) |
  | LuxCore2.1Benchmark | sorted | sorted | sorted | 46.8 | 46.2 | -1.4% (noise) |
  | DanishMood | sorted | sorted | sorted | 45.7 | 46.4 | +1.6% (noise) |
  | LynxSpider | sorted | sorted | sorted | 88.8 | 69.4 | -21.9% |

  No A/B in M samples/sec (the user skipped it). Image check passes on all
  5 scenes (RMSE 0.004-0.043 vs noise floors 0.021-0.19).

- Image check caveat found on the way: LuxCore2.1Benchmark and DanishMood
  use the PhotonGI indirect cache, built on the CPU before the GPU render.
  Its result varies per run, and the whole image moves with it: the mean
  differs from `main` by -1.7% to +5.3% between builds and up to 1.2%
  between two runs of the same build and seed. With
  `-D path.photongi.indirect.enabled 0`, T5.7 vs `main` is -0.000%
  (LuxCore2.1Benchmark) and -0.096% (DanishMood), seed spread about 0.02%:
  the GPU code is unbiased. Variance clamping is not the cause: DLSC and
  LynxSpider clamp too and stay within 0.25%. Use that override when a
  mean difference on those two scenes needs explaining.
- Next: T5.5 (registers), T5.6 (fuse the BSDF kernels; with T5.7 the three
  kernels may use different orders, so fusing now needs one order for all).

### 2026-10-26: GPU-first roadmap, T5.5 baseline, T6.1 started

- The user set the direction: this fork is a GPU-first, unbiased renderer.
  `PLAN.md` has the roadmap (Phases 6-9) with the list of what a GPU render
  still does on the CPU, and the T6.1 design.
- The user reported noisy caustics and chose GPU light tracing (unbiased)
  over speeding up the PhotonGI caustic cache (blurs). In Blender both
  "Add Light Tracing" and "Use Caustic Cache" are off by default, so by
  default caustics come only from GPU path tracing.
- T5.5 baseline, offline compile of the current kernels (kernel dump from
  `-D opencl.kernel.writetofile 1`, NVRTC 12.9 + ptxas sm_120, script
  `nvrtc_regs.py` in the session scratchpad): LuxBall with only 3 material
  and 3 texture types compiled in still uses 255 / 254 / 255 / 242 / 242 /
  253 registers in RT_NEXT_VERTEX / HIT_OBJECT / RT_DL / DL_ILLUMINATE /
  DL_SAMPLE_BSDF / GENERATE_NEXT_VERTEX_RAY. The offline numbers match the
  cached PTX within 3 registers. Kernel dumps: `bench\kdump\*-T5.7.cl`.
- NVRTC 13 (asked by the user, not tested): LuxCore's loader tries
  `nvrtc64_130_0.dll` before `nvrtc64_120_0.dll` (`deps/cuew/src/cuew.cpp:625`),
  so it can be tested by copying the DLLs into a bin copy. The driver JIT
  (CUDA 13.5) makes the final SASS, so expect a small effect.

### 2026-10-26: T6.1 GPU light tracing, work in progress (branch `gpu-opt/T6.1-gpu-light-tracing`)

The user asked to implement first and run fewer A/B benchmarks: build the
feature end to end, check it with image and bias checks, measure at the end.

State: device code written, nothing built or run yet. All new kernel code is
under `#if defined(PARAM_LIGHT_TRACING)`, a define not set yet, so the eye
path kernels compile as before. The structs changed for all kernels (fields
added at the end, no image change expected): `HitPoint.fromLight`,
`CameraBase.dir`, `PerspectiveCamera.pixelArea`,
`ProjectionLightParam.lightProjectionInv` / `area`.

Done (device side):

- Adjoint BSDF. Convention: GPU materials keep computing the eye path value;
  `BSDF_Evaluate()` swaps eye/light directions with `fromLight` and applies
  the CPU shading normal factor; `BSDF_Sample()` multiplies by
  `|cos fixedDir / cos sampledDir|` (local frame) and the CPU geometry
  normal factor. For reciprocal BSDFs this equals the CPU `fromLight`
  branches (checked: matte, roughmatte, matte/roughmatte translucent,
  velvet, metal2, glossy2, glossycoating, glossytranslucent, carpaint,
  disney, mix, twosided). Refractive leaves take their own branch with
  `HitPoint_IsFromLight()`: glass (no eta^2, Fresnel of the incoming
  direction), archglass (Fresnel side), roughglass (Fresnel of
  `cosThetaOH`). Differences from CPU, on purpose: no adjoint factors for
  volumes (CPU `BSDF::Sample()` applies the geometry factor to volumes too,
  looks like a CPU bug); cloth gets the cosine factor (CPU cloth `fromLight`
  sample has none, also looks like a CPU bug).
- Perspective camera: `PerspectiveCamera_SampleLens()`,
  `PerspectiveCamera_GetSamplePosition()` (clipping plane, motion blur),
  `PerspectiveCamera_GetFluxToRadianceFactor()` in `camera_funcs.cl`.
  Other camera types: GPU light tracing off, the CPU native threads keep
  doing it (as today).
- `Light_Emit()` for all 14 light types in `light_funcs.cl`. Emission maps
  (map point, map sphere, triangle lights with IES/image map) are sampled
  uniformly over the sphere: the CPU samples their distributions, which the
  GPU doesn't have (same expected value, more noise for such lights).

Design of the remaining parts (decided, from the code):

- Light task (`LightPathTask` in `pathoclbase_datatypes.cl`): state
  (GENERATE, RT_NEXT_VERTEX, RT_EYE_CONNECTION), seed, light ID, time, lens
  point, flux, depth, volume, `isNearlyS`, `lastBSDFEvent`, stored next
  vertex ray, pending eye connection (radiance, film x/y, volume), finished
  path count. One ray in flight per light task, in the eye path rays buffer
  after the `taskCount` eye rays, so the existing OptiX launch traces both
  (`raysBuff` / `hitsBuff` sized `taskCount + lightTaskCount`). Idle slots
  get a ray with mint = maxt = 0.
- Kernel `AdvanceLightPaths` in a new `pathoclbase_kernels_lighttracing.cl`,
  mirroring `PathTracer::RenderLightSample()` / `ConnectToEye()`: connect a
  vertex to the camera only if the connection is caustic
  (`isNearlyS && depth >= 1 && !IsNearlySpecular(eval event)`, not delta,
  not camera invisible), trace the connection with `LIGHT_RAY | CAMERA_RAY`,
  then continue with the stored next vertex ray. Stop the path as soon as
  `isNearlyS` is false (no caustic vertex can follow). Russian roulette and
  max depth as the CPU. Random sampler (Tauswothe seed per light task).
  A path is counted when it ends (also failed emissions).
- Temporaries (BSDF, HitPoint): kernel locals on CUDA, `tasks[gid].tmp*`
  on OpenCL, so `lightTaskCount <= taskCount`.
- Light picking: a GPU copy of the emit light strategy distribution
  (`GetEmitLightStrategy()`, LOG_POWER by default), compiled next to
  `lightsDistribution`.
- Film: the thread film keeps `RADIANCE_PER_SCREEN_NORMALIZED` (today
  removed in `ThreadFilm::Init()`), one float3 buffer per radiance group,
  atomic splats with the filter LUTs of `FilmSampleSplatter` (built on the
  host with `FilterLUTs(filter, max(4, max(xWidth, yWidth) + 1))`, queried
  at the cell centers, uploaded as data + offset/width/height). Recv/Send
  and clear like the other channels. Render loop:
  `SetSampleCount(eye, eye, lightPathCount)`.
- Light task count: start with taskCount / 2, to tune after it works.

Still to check in the code: GPU equivalents of `BSDF::IsCameraInvisible()`
(`sceneObjs[meshIndex].cameraInvisible`) and of the CPU pass-through
shadow transparency override test in `RenderLightSample()`.

### 2026-10-26: T6.1 GPU light tracing, done (branch `gpu-opt/T6.1-gpu-light-tracing`)

With `path.hybridbackforward.enable` on, PATHOCL GPU threads now trace light
paths for the caustics (before: only the CPU native threads did, with
Metropolis). New property `path.hybridbackforward.gpulighttracing.enable`
(default true; acts only with hybrid on and a perspective camera, otherwise
the engine logs it and only the native threads trace light paths, as
before). Without hybrid nothing changes: the light tracing code is under
`PARAM_LIGHT_TRACING`, set only when the light task count is not 0.

What was built (on top of the device code of the WIP entry above):

- `AdvanceLightPaths` (end of `pathoclbase_kernels_micro.cl`, body under
  `#if defined(PARAM_LIGHT_TRACING)` because the OpenCL split compile cuts
  the source at each `__kernel`), `InitLightPaths` and
  `LightTracing_ClearFilm` plus helpers (`LightTracing_GenerateLightPath`,
  `LightTracing_SplatSample`, `LightTracing_IsNearlySpecular`) at the end of
  `pathoclbase_funcs.cl`. Same decisions as `PathTracer::RenderLightSample()`
  / `ConnectToEye()`: stop on a miss and on a not black shadow transparency
  without override (`pathtracer.cpp:875`); caustic connection only
  (`isNearlyS && depth > 0`, not camera invisible, not delta, eval event not
  nearly specular); the path stops at once when the vertex can't be nearly
  specular (the CPU traces one more vertex there only to "stabilize"
  Metropolis, those samples are dropped by `addonlycaustics`).
- Light tasks: `LightPathTask` (203 bytes), state machine GENERATE /
  RT_NEXT_VERTEX / RT_EYE_CONNECTION, one ray each after the eye rays in
  `raysBuff` / `hitsBuff`, so the one OptiX launch traces both. Idle slots
  are masked rays. A path is counted when it ends (failed emissions too);
  counts per task in `lightPathCountsBuff`, summed on the host into
  `Film::SetSampleCount(eye, eye, lightPathCount)`.
- Light picking: GPU copy of the emit light strategy distribution
  (`emitLightsDistribution`, from `DistributionLightStrategy` or
  `LightStrategyDLSCache`).
- Film: the GPU thread film keeps `RADIANCE_PER_SCREEN_NORMALIZED` (one
  float3 buffer per radiance group); atomic splats with the
  `FilmSampleSplatter` filter LUTs (built on the host by
  `CompileLightFilterLUTs()` in `pathocl.cpp`). Note: the CPU splatter
  misaligns its LUT at the right edge of the film sub-region, the GPU
  indexes it as intended.
- `Material.passThroughShadowTransparencyOverride` added to the GPU
  material struct. `HitPoint.fromLight` of the WIP entry is now a bit of
  `HitPoint.flags` (see the speed section below).
- Light task count = task count (1M on the RTX 5070). Memory: +208 MB
  `LightPathTask`, +68 MB rays/hits.

Bug found on the way: `CompileCamera()` copied only `.m` of the perspective
camera transforms, `mInv` stayed zero, so `PerspectiveCamera_GetSamplePosition()`
returned NaN film positions (splats in the top-left corner, values up to
1e12). Now `mInv` of `rasterToCamera` / `cameraToWorld` is copied too. The
eye paths don't use `mInv`, so no image change.

Verification (`run.sh`-style runs: PATHOCL, SOBOL, seed 1, CUDA only,
images in `D:\REPO\LuxCoreRepo\bench\T6.1`; "light part" = image mean minus
the hybrid run without any light tracing):

- Eye-only renders unchanged (no hybrid): LuxBall / DLSC RMSE vs T5.7c
  0.0037 / 0.024, noise floors 0.0168 / 0.132. DLSC mean over 5 seeds
  +0.031% +- 0.037% (1 sigma): the kernel change acts like a new Sobol
  realization (pixel assignment depends on kernel order since T5.7), not a
  bias.
- 2-glasses (point light, glasses, 60 s): GPU light part / CPU light part
  1.014 (seed 1), 0.987 (seed 2). Seed spread of the light part: CPU
  Metropolis 2.3%, GPU 0.4%. GPU light tracing plus 12 CPU native threads
  (Blender "use native CPU": both write the light channel, counts add up):
  0.996 of GPU only. Task count change (taskCount / 2 to taskCount): 0.998.
- 2-glasses noise, caustic tiles (50x50 tiles where the 600 s reference
  has > 10% of the max light part), two seeds, 60 s:
  eye paths only 6.7e-5; CPU light tracing (12 native threads, Metropolis,
  plus the GPU eye paths) 3.51e-4; GPU light tracing, GPU only:
  3.40e-4 with taskCount / 2, 2.53e-4 with taskCount (-28% vs CPU, with
  the CPU idle). Eye spp at 60 s: 454 / 514 without light tracing, 461 /
  471 with it (taskCount).
- Profile (2-glasses, taskCount / 2, `opencl.kernel.profile.enable`):
  `LIGHT_TRACING` 8% of kernel time. With temporary counters: 2.1e8 light
  paths in 15 s, 1.3e5 caustic connections (0.06%): most light paths hit
  the table first and end. This is the T6.2 lever.
- OrangeJuice: no light traced caustics at all, on CPU or GPU: every glass
  material has a not black `transparency.shadow`, so light paths stop there
  (`pathtracer.cpp:875`). GPU light part -0.0002, CPU light only
  (`path.hybridbackforward.partition 0`) -0.0011, image mean 1.649: noise.
  Careful: with the default partition 0.8 (eye:light sample ratio) the
  native threads also trace eye paths, and in OrangeJuice CPU eye paths are
  brighter than GPU eye paths on the jug rim, handle and table, without
  hybrid too (+0.020 image mean with 12 native threads, centre 125x125 tile
  +0.62). Pre-existing CPU/GPU eye path difference, not T6.1: open.
- Prism: unusable for this check, the GPU eye paths already miss the direct
  volumetric beam (pre-existing).

Known differences from the CPU light tracing:

- Random sampler (Tausworthe per light task), not Metropolis: unbiased, no
  adaptivity; its mean has a smaller seed spread at 60 s (see above).
- Light samples are not added to the BCD denoiser statistics.
- No volume adjoint factors (see the WIP entry).
- Emission maps sampled uniformly over the sphere (see the WIP entry).
- Camera type checked at engine start; after a scene edit to a non
  perspective camera the kernel stops connecting (no caustics until
  restart).
- OpenCL (non-CUDA) path not tested: light tasks use the `GPUTask`
  temporaries with the same index (hence lightTaskCount <= taskCount).
- RTPATHOCL / TILEPATHOCL (Blender viewport) don't use it.

Default (no hybrid) speed, `bench.py speed` (median of 3, M samples/sec):

| Build | LuxBall | DLSC |
|---|---|---|
| T5.7c (base) | 17.39 / 18.17 (two sessions) | 21.18 / 20.99 (two sessions) |
| T6.1 with a separate `HitPoint.fromLight` int | 17.54 | 20.57 / 20.58 (two sessions) |
| T6.1 with `fromLight` removed (experiment) | | 21.05 |
| T6.1 final (`HitPoint.flags`) | 18.15 | 21.10 |

The separate `fromLight` field grew `HitPoint` by 4 bytes (GPUTaskState
264 to 268 bytes per task) and slowed every eye path kernel by about 1.5%
(material sort calibration timings) even with no light tracing code
compiled in. Fix: `throughShadowTransparency` and `fromLight` share one
`HitPoint.flags` int (`HITPOINT_THROUGH_SHADOW_TRANSPARENCY`,
`HITPOINT_FROM_LIGHT`, read through `HitPoint_IsThroughShadowTransparency()`
/ `HitPoint_IsFromLight()`), so the HitPoint layout equals the base one.
The other struct additions (`Material` +4 bytes, `CameraBase.dir`,
`PerspectiveCamera.pixelArea`, `ProjectionLightParam`) cost nothing
measurable. The first LuxBall base number (17.39) was GPU clock drift: the
same base binary gave 18.17 later.

Image check after the fix (`bench.py image`, 256 spp, vs `T6.1-base-img`):
LuxBall RMSE 0.0030 (noise floor 0.0168), DLSC 0.0125 (0.132), mean
+0.000% / +0.031% (seed spread -0.005% / +0.029%): pass. 2-glasses light
part after the fix: 0.9987 of the run before it.

Next: T6.2 (light path efficiency). Open from this task: the OrangeJuice
CPU/GPU eye path difference above; a pyluxcore build for Blender with this
branch.

### 2026-10-27: T6.2 guided light emission (branch `gpu-opt/T6.2-light-path-efficiency`)

The GPU light tracer splats only caustic connections, so a light path is
useful only if its first vertex is on a "caster", an object whose material
can be nearly specular. T6.2 emits most light paths toward the casters.

- Casters: `CompiledScene::CompileLightTracingCasters()` (compilelights.cpp),
  objects whose material has SPECULAR events or GLOSSY with glossiness <=
  `path.hybridbackforward.glossinessthreshold` (the same per material
  constants the kernel tests, so the set is exact), minus NULL materials
  (always pass-through) and materials with a not black shadow transparency
  and no override (light paths stop there). World bounding boxes,
  clustered to at most 32 bounding spheres (median split of the largest).
  Recompiled on geometry and material edits, uploaded as
  `lightCastersBuff`.
- Emission (`LightEmitGuide` in `light_funcs.cl`): with probability
  `path.hybridbackforward.gpulighttracing.guidedprob` (default 0.9, 0
  disables) the second half of the emission is sampled toward a caster:
  lights with a position (point, map point, spot, sphere, map sphere,
  triangle) sample the direction uniformly in the cone toward a caster
  sphere, picked by solid angle; environment lights (constant, infinite,
  sky2, sun, distant, sharp distant) sample the ray origin on the emission
  disk uniformly in the shadow of a caster sphere, picked by area. The pdf
  is the mixture of the light pdf and the guided pdf (number of cones /
  shadows holding the sample over their total), so the estimate is
  unbiased for any caster set. Cones use 1 - cos and a sine test to stay
  accurate for small cones; the sampled cone always counts itself.
  Not guided: projection, laser, triangle lights with emission theta 0.
  The constant infinite light now samples a direction plus a disk origin
  (same distribution as its former 2 sphere points).
- No change without hybrid: all the code is under `PARAM_LIGHT_TRACING`.

Test setup warning: `opencl.native.threads.count` must be 0 for GPU-only
hybrid runs. Without it luxcoreconsole starts CPU native threads that trace
eye paths and Metropolis light paths too (the first T6.2 checks were
useless because of this).

Verification: light-only images from a temporary debug build (eye
radiance cleared before `Sampler_SplatSample()`, not committed), 2-glasses
and 9 variants with other lights (scenes generated into
`bench\T6.2\scenes`; env variants without the walls and ceiling),
PATHOCL, SOBOL, GPU only, 40 s per run, images in `bench\T6.2`:

| Light | mean guided / plain | caustic tile noise plain | guided | eye spp plain / guided |
|---|---|---|---|---|
| point | 1.0001 | 2.97e-4 | 2.94e-5 | 369 / 346 |
| spot (aimed at glasses) | 1.0004 | 0.0838 | 0.0381 | 493 / 476 |
| sphere | 0.9984 | | | 375 / 335 |
| mesh (bulb, triangle lights) | 0.9988 | 1.80 | 0.221 | 562 / 503 |
| mesh, emission theta 30 | 1.0005 | | | 567 / 500 |
| sun | 0.9999 | 0.379 | 0.0245 | 686 / 684 |
| sky2 | 1.0019 | 1.65 | 0.0732 | 760 / 797 |
| constant infinite | 1.0049 | 0.217 | 0.0110 | 775 / 800 |
| distant (theta 2) | 1.0011 | | | 701 / 691 |
| sharp distant | 1.0017 | | | 700 / 687 |

Noise = RMSE between seeds 1 and 2 / sqrt(2) over the 50x50 tiles with
more than 10% of the max tile light; the brightest tiles agree within
noise for every light. Full images (eye + light, GPU only) agree too:
2-glasses light part (image minus the eye-only run) plain 1.5456e-4 /
1.5403e-4, guided 1.5431e-4 / 1.5431e-4 (two seeds), T6.1 600 s reference
1.5467e-4. guidedprob 0.99: noise within 1% of 0.9 (point, sky2), so 0.9
stays: it bounds the damage if a caster were ever missed.

Cost: guided light paths are longer, so the light kernel takes a larger
share of each iteration (eye spp column). LuxCore2.1Benchmark (sun +
sky2, only archglass casters: their transmission is pass-through, so no
caustic at all, light part 0 with plain and guided): eye spp 678 plain,
586 / 583 guided. Moved to T6.5 (light/eye work balance).

Default (no hybrid) image check, `bench.py image` 256 spp vs
`T6.1-base-img`: LuxBall RMSE 0.0025 (noise floor 0.0168), DLSC 0.0249
(0.132), mean +0.002% / +0.073%: pass (eye kernels compile from the same
preprocessed code as T6.1).

Next: T6.5 (work balance), then T7.1 + T7.2 per the roadmap order.

### 2026-10-27: T6.5 light tracing duty cycle (branch `gpu-opt/T6.5-light-eye-balance`)

Branched from `gpu-opt/T6.2-light-path-efficiency`. The GPU render loop
(`pathoclopenclthread.cpp`) now runs the light kernel, and traces the light
rays, only every `lightPeriod` iterations. The light rays follow the eye
rays in the ray buffer, so on the other iterations only `taskCount` rays
are traced and the light rays wait, untouched, for the next light
iteration. After each film read, once 8 x lightTaskCount light paths have
been counted since the last check, the loop sums the light tracing
(RADIANCE_PER_SCREEN_NORMALIZED) channels: no new energy sets the period
to 16, new energy sets it back to 1. The light channels are normalized by
the light path count, so the period changes the noise, not the image.
Only host code changed; the kernels are the T6.2 ones.

Speed (GPU only, 40 s, warm kernel cache, eye spp):

| Scene | no GPU light tracing | T6.2 | T6.5 |
|---|---|---|---|
| LuxCore2.1Benchmark (photongi off) | 857 | 599 | 838 |
| 2-glasses | | 297 | 300 |

LuxCore2.1Benchmark switches to period 16 at 1.1 s (the first window,
1.4e7 light paths). 2-glasses never leaves period 1.

Measuring pitfall: `batch.halttime` includes the kernel compile. The
first T6.5 runs with a cold cache lost 21-27 s of 40 to it (392 spp
instead of 857). The T6.2 table runs had warm caches; only
`lo_bench_g0` (the odd 211 spp) did not.

Check of the skipped iterations: a debug build with the period forced to
16 and the eye radiance cleared (light-only, not committed,
`bench\bin-T6.5-p16lo`) vs the T6.2 light-only 2-glasses images: light
mean ratio 0.9937 / 0.9948 (seeds 1 / 2) at 40 s, 0.9988 / 0.9988 at
160 s, every bright tile the same. The gap falls as 1/time, so it is not
the duty cycle: it is the count of light paths at the end (T6.6). A path
is counted when it is done, and the paths still in flight are more often
the long caustic ones, so the light part reads low by about
Cov(contribution, length) / (mean length x mean contribution x paths per
task). With 16x fewer paths the gap is 16x larger. At the normal period
it is about -0.03% of the light part at 40 s on 2-glasses (the T6.2
light-only reference itself). In normal use the period is 16 only while
the light paths add nothing, so T6.5 does not make it larger.

Next: T7.1 + T7.2 per the roadmap order (T6.6 is low priority).
