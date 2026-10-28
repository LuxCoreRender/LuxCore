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

### 2026-10-27: T7.1 / T7.2 measured; T7.5 asynchronous render loop (branch `gpu-opt/T7.5-async-render-loop`)

Branched from `gpu-opt/T6.5-light-eye-balance` (same render loop file).

T7.1 / T7.2 first: a temporary build timed `RenderEngine::UpdateFilm()`
(host merge of the thread films, then `Film::RunTests()`) and the render
thread film transfers. GPU only, 30 s, one film update per second:

| Scene | film | merge / update | RunTests / update | film transfer, % of kernel time |
|---|---|---|---|---|
| LuxBall | 800x800 | 2.5 ms | 22 ms | 1.0% |
| DLSC | 864x640 | ~0 ms | 41 ms | 0.9% |
| 2-glasses | 1024x1024 | 1.4 ms | 18 ms | 0.8% |
| LuxCore2.1Benchmark | 789x1071 | 6.4 ms | 28 ms | 1.0% |

The merge and the tests run on the session thread beside the GPU, so
moving them to the GPU cannot raise samples/sec: T7.1 and T7.2 are
deferred. The real host cost was the render thread itself: one full core
for the whole render. The CUDA context is created with
`CU_CTX_SCHED_YIELD`, so `FinishQueue()` after each batch of iterations
spins. `CU_CTX_SCHED_BLOCKING_SYNC` alone cut the process CPU time
(2-glasses 33.9 s to 3.9 s in 30 s), but the GPU then idles while the
woken thread queues the next batch: LuxBall 18.03 to 17.75 M samples/sec
(-1.6%, speed protocol), DLSC 20.87 to 20.97. Not kept.

T7.5 instead:

- `HardwareDevice::EnqueueMarker()` / `WaitMarker()`. CUDA: an event with
  `CU_EVENT_BLOCKING_SYNC`, so the waiting thread sleeps; the context stays
  `CU_CTX_SCHED_YIELD`. OpenCL: flush / finish (the old behavior, not
  tested).
- CUDA non-blocking `EnqueueReadBuffer()` goes through pinned memory (one
  allocation of the buffer size per buffer, at the first read): an async
  copy to pageable memory blocks until the device has run all the queued
  work. The copy to the destination runs in `WaitMarker()` /
  `FinishQueue()`, the point where an OpenCL non-blocking read is
  complete too. A blocking read finishes the queue first, so the reads
  stay in queue order. Falls back to the old copy if pinned memory can not
  be allocated.
- `PathOCLOpenCLRenderThread::RenderThreadImpl()` keeps one batch queued
  ahead: it queues batch k, then waits for batch k-1. Film reads are queued
  between two batches with their own marker and completed (sample counts,
  T6.5 light window) after the next batch is queued. Pause, the PhotonGI
  cache update and the end of the render drain the queue; the last film
  read is synchronous as before.
- The 1% film transfer budget now charges each read the time of the
  second read, measured once with nothing else queued.

Windows pitfalls found on the way: `WallClockTime()` is `GetTickCount()`,
10-16 ms resolution (the loop now uses `std::chrono::steady_clock`). On
WDDM a marker can complete for the host only together with the next batch
(a film read marker waited 92-192 ms): the first version charged that wait
to the transfer budget and read the film only every ~10-20 s, which also
delayed noise estimation and made samples/sec look 10% higher on
2-glasses. Fixed by the measured read time above.

Results (bins `bench\bin-T7.1-base` = T6.5, `bench\bin-T7.1`):

| Scene (GPU only) | render thread CPU, base | T7.5 |
|---|---|---|
| LuxBall | 100% of a core | 4% |
| DLSC | 101% | 5% |
| 2-glasses | 100% | 3% |
| 2-glasses hybrid | 99% | 3% |

(process CPU over seconds 6-26 of a 30 s render, `batch.haltspp 0`; the
rest of the process CPU is the scene load and, for LuxBall, the CPU OIDN
of its output pipeline at the end.)

- Speed protocol, back to back: LuxBall 17.88 base, 17.94 T7.5 (+0.3%). DLSC
  20.87 (base, earlier) vs 21.02 (+0.7%). Equal within noise; the film
  copies no longer stop the GPU, but they cost under 1% before.
- Image check 256 spp vs `T6.1-base-img`: LuxBall RMSE 0.00224 (floor
  0.0168), DLSC 0.0141 (0.132), mean -0.001% / +0.041%: pass.
- Film update cadence with the scene settings (30 s, noise estimations
  base / T7.5): LuxBall 16 / 15, DLSC 23 / 28, 2-glasses 5 / 6.
- 2-glasses hybrid, 40 s: brightest 50x50 tiles T7.5 / base 0.999-1.000,
  the same as between two T7.5 seeds (light path counts and the light
  channels are read at the same queue point).

Open: TILEPATHOCL and RTPATHOCL keep their own loops with `FinishQueue()`.
The NOISE / CONVERGENCE / USER_IMPORTANCE channels are still written from
pageable memory at each film read (CUDA may synchronize such copies; no
CPU or speed effect was measured).

Next: T8.1 + T8.2 per the roadmap order.

### 2026-10-27: T8.1 kernel compile time (branch `gpu-opt/T8.1-kernel-compile`)

Branched from `gpu-opt/T7.5-async-render-loop`.

Where a cold compile goes (LuxBall, specialized kernels, fresh LuxCore
cache, `CUDA_CACHE_DISABLE=1`): 13.9 s = NVRTC source to PTX 5.4 s + driver
PTX to SASS JIT at module load 8.4 s (one thread). Warm (LuxCore PTX cache +
driver JIT cache): 20 ms.

Measuring pitfall: NVRTC 12.9 caches its own results in the driver
ComputeCache (`%APPDATA%\NVIDIA\ComputeCache`), keyed after preprocessing
(an unused `-D` still hits). For cold numbers set `CUDA_CACHE_DISABLE=1`
or change the source.

Tried and rejected:

- NVRTC straight to CUBIN (`--gpu-architecture=sm_120`, `nvrtcGetCUBIN`),
  so ptxas runs inside NVRTC with `--split-compile=0`: cold 10.0 s, warm
  28 ms. Interleaved A/B (3 x 60 s, `bench\T8.1-ab.txt`): LuxBall 18.00 /
  18.00, DLSC 21.11 / 20.83 (-1.3%, runs 21.04-21.12 vs 20.80-20.93),
  LuxCore2.1Benchmark 20.04 / 19.96. Without split the CUBIN compile takes
  15.0 s, slower than today. Split thread counts 4-12 are equal within
  noise, and the SASS changes with the split count. Rejected: a 4 s saving
  once per feature set does not pay for 1.3% on every render.
- One NVRTC program per micro-kernel, compiled in parallel (12 threads,
  CUBIN): 8.7 s with `--split-compile=1` per program, 35 s with `0`
  (oversubscription). Each program parses the whole 1.3 MB source again
  (~1 s, measured with the non micro-kernels only).

ptxas per kernel (offline, CUDA 12.8 ptxas, ~0.35 s of it parsing): the 7
heavy kernels take ~1.2 s each (DL_ILLUMINATE, RT_NEXT_VERTEX, RT_DL,
HIT_OBJECT, GENERATE_NEXT_VERTEX_RAY, SPLAT_SAMPLE, DL_SAMPLE_BSDF), the
rest ~0.05-0.2 s. Less code per kernel (T5.5) is what shortens the compile.

Kept, bug fixes:

- OpenCL split compile broken since T5.10: `NextSample()` and
  `GenerateCameraRay()` sat between two kernels, so the split programs of
  MK_SPLAT_SAMPLE, MK_NEXT_SAMPLE and MK_GENERATE_CAMERA_RAY did not see
  them. With `-D opencl.devices.select 10` (the NVIDIA OpenCL device) the
  T7.5 build fails with `PathOCL kernel AdvancePaths_MK_SPLAT_SAMPLE OpenCL
  program compilation error`; moved above the first kernel, LuxBall renders
  on OpenCL (kernel compile 21 s).
- `cudaKernelPersistentCache::CompilePTX()` retried a corrupt cache file by
  calling itself while holding `cacheMutex`: with a cache file damaged on
  purpose the T7.5 build stops with `RUNTIME ERROR: resource deadlock would
  occur`; now it removes the file and compiles (tested, renders).
- `ForcedCompilePTX()` leaked the `nvrtcProgram` on a compile error.

Image check 256 spp vs `T6.1-base-img`: LuxBall RMSE 0.00147 (floor
0.0168), DLSC 0.00513 (0.132), mean +0.000% / -0.002%: pass.

Not done: rendering with a generic kernel while the specialized one
compiles (large); prebuilt caches (feature sets are combinatorial with the
specialization).

### 2026-10-27: research pass and new order

The user asked for a web research pass (university and NVIDIA papers) and
set the direction: a good, fast, stable renderer, no tasks that take
weeks, keep path guiding. Findings and links are in `PLAN.md`
"References"; new tasks T4.2 (MIS compensation for env lights), T4.3
(Owen-scrambled Sobol), T9.4 (CUDA Graphs), T9.5 (light tree); T6.3 path
guiding scoped to a GPU-native hash grid. Rejected for now: ray
reordering and NEE++ (ray tracing is 1.6% of the GPU time), EARS (after
T6.3, needs the same kind of cache).

Next: merge the verified chain into one branch and build pyluxcore + the
Blender addon, then T5.5.

### 2026-10-27: merged chain, Blender 4.5 addon build

- `gpu-optimization` now holds the whole verified chain (merge commit
  `31fd565c1`): T5.4, T5.10, T5.7, T6.1, T6.2, T6.5, T7.5, T8.1. New task
  branches start from `gpu-optimization` again.
- Addon zip `D:\REPO\LuxCoreRepo\bench\addon\BlendLuxCoreDiginaat-2.11.1-blender45-gpuopt-31fd565c1.zip`:
  the previous zip with `pyluxcore.pyd` (Python 3.11, `make.bat pyluxcore`)
  swapped inside the pyluxcore wheel and its RECORD hash rewritten
  (scratchpad script, same steps work for later builds). BlendLuxCore had no
  changes since the last zip.
- Smoke test with Python 3.11 on the unpacked wheels: CUDA + OpenCL devices
  listed, LuxBall PATHOCL 16.7 M samples/sec over 8 s with a warm cache.
  Calling the Python log handler during interpreter shutdown crashes
  (`PyThreadState_Get ... GIL is released`) unless the session is deleted
  and `SetLogHandler(None)` is called first: test script issue, Blender
  does not finalize Python while a session lives.

### 2026-10-27: T5.5 work in progress (paused by the user)

Branch `gpu-opt/T5.5-specialize` (from `gpu-optimization` at `488eaddd3`).
Committed as work in progress: not merged, the last part is built but not
image-checked yet.

Changes:

1. Light type mask: the host passes `-D PARAM_LIGHT_TYPES_MASK` (types in
   `cscene.lightDefs`); `LIGHT_TYPE_GUARD()` in the light type switches of
   `EnvLight_GetRadiance`, `Light_Illuminate`, `Light_Emit`
   (`light_funcs.cl`). Offline NVRTC 12.9 sm_120 registers: LuxBall
   HIT_NOTHING 64 -> 18, heavy kernels unchanged (248-255), cubin 3.90 ->
   3.69 MB; DLSC HIT_NOTHING 64 -> 16, GENERATE_CAMERA_RAY 60 -> 47,
   GENERATE_NEXT_VERTEX_RAY 254 -> 238, DL_SAMPLE_BSDF 254 -> 243,
   DL_ILLUMINATE 243 -> 235, cubin 6.60 -> 6.28 MB. No kernel gets under
   the 168-register occupancy step.
2. Sobol: `SobolSequence_SobolDimension()` (`sampler_sobol_funcs.cl`) is
   unrolled and branch free (32 independent loads, XOR with a bit mask).
   Bit-exact result. Found with Nsight Compute source counters (see below):
   the old loop was the top stall line of SPLAT_SAMPLE (19.5% of its warp
   stall samples, long_scoreboard), because `pass + rngPass` has ~32 bits
   set so the loop ran ~32 dependent iterations.
3. Volume gate (built as `bench\bin-T5.5c`, NOT yet image-checked or
   timed): `SCENE_HAS_VOLUMES` (`material_types.cl`, from the volume bits of
   `PARAM_MATERIAL_TYPES_MASK`). Without volumes all volume indices are
   `NULL_INDEX`, so `Scene_Intersect()` skips `Volume_Scatter()`, the volume
   priority check and `PathVolumeInfo_Update()`; `BSDF_Init()` skips the two
   eval-op calls `Material_GetInteriorVolume()`/`GetExteriorVolume()` and
   sets the four hit point volume fields to `NULL_INDEX` (what the old code
   computed); MK_DL_ILLUMINATE skips the `directLightVolInfos[gid]` copy
   (only read by the gated code). `Volume_Scatter()` got
   `MATERIAL_TYPE_GUARD()` per volume type.

Measurements (bins in `D:\REPO\LuxCoreRepo\bench`):

- Kernel profile LuxBall 20 s (`opencl.kernel.profile.enable 1`), bin-T5.5
  (light mask) vs bin-T5.5b (+ Sobol): SPLAT_SAMPLE 3890 -> 3577 ms (-8%),
  other kernels within noise.
- `bench.py ab --runs 3 --halttime 25`, T5.5 vs T5.5b: LuxBall 17.69 ->
  17.39 (-1.7%, runs 17.1-17.75 overlap: noise), DLSC 20.29 -> 21.02
  (+3.6%, all three runs higher).
- Image check T5.5b (`bench.py image`, 256 spp, vs `T6.1-base-img`):
  LuxBall RMSE 0.00224 (floor 0.0168), DLSC 0.0142 (floor 0.132): pass.

Nsight Compute source-level profile (LuxBall, bin with
`--generate-line-info` enabled temporarily in `cuda.cpp`, reverted):

- SPLAT_SAMPLE: 56 registers, 61% achieved occupancy, issue slots 3.6%
  busy (latency bound). Top stall lines after the Sobol loop: pixel filter
  `Distribution2D` binary search (`mc_funcs.cl` std_upper_bound, 6%), the
  `filmRadianceGroup[i]` pointer table in `Film_AddSampleResultColor`
  (3.9%), NaN checks and film atomics (3% each).
- The six heavy kernels together: no single hot spot. The top lines are
  each 1-2.5% of the stall samples, all long_scoreboard on per-path AoS
  struct fields (`pathStates`, `material->isDelta`, `bsdf->isVolume`,
  `sampleResult->...`). A real fix is a SoA layout of the path state, which
  is a multi-week refactor: not planned (user: no multi-week tasks).
- How to get per-line data: `ncu --section SourceCounters ...`, then
  `ncu -i <rep> --page source --print-source cuda,sass --csv`. ncu looks
  for the `.cl` files in the scene directory (the `#line` names have no
  path): copy `include/**/*.cl` there for the export and delete them after.
  A multi-kernel export has no kernel name rows; export one kernel per
  report. A full export of 7 heavy kernels takes over 10 minutes.

Next steps for T5.5:

1. Image check bin-T5.5c on LuxBall, DLSC and a volume scene (DanishMood
   with `-D path.photongi.indirect.enabled 0`, or LynxSpider): volumes must
   render as before.
2. Milestone `bench.py ab` bin-T8.1 (merged chain) vs bin-T5.5c on LuxBall,
   DLSC, LuxCore2.1Benchmark; record, then merge `--no-ff` into
   `gpu-optimization`.
3. Optional, small: `-D PARAM_FILM_RADIANCE_GROUP_COUNT` to drop the
   `filmRadianceGroup[]` pointer table checks in SPLAT_SAMPLE (radiance
   group i is non-null iff i < `film->GetRadianceGroupCount()`,
   `pathoclbaseoclthreadfilm.cpp:143`).
4. Then T4.2. Design note: keep the original env distribution for
   `Light_Emit()` (light tracing and hybrid back/forward need full
   coverage) and add a second, compensated distribution only for
   `Illuminate()` and the direct pdf in `GetRadiance()` (CPU
   `infinitelight.cpp`, GPU `compilelights.cpp` TYPE_IL +
   `light_funcs.cl`). Only when the light is visible to diffuse, glossy
   and specular indirect rays; fall back to the original distribution if
   the compensated one is all zero.

### 2026-10-28: T5.5 done: Sobol tables kept, light mask and volume gate rejected

Image check of the volume gate (`bin-T5.5c`, 256 spp, volume scenes with
`-D path.photongi.indirect.enabled 0` against a new `T8.1-vol-img`
baseline): LuxBall RMSE 0.00172 (floor 0.0168), DLSC 0.00385 (0.132),
DanishMood 0.00124 (0.0283), LynxSpider 0.000485 (0.0142): pass, volumes
render as before.

Speed was the problem. `bench.py ab`, T8.1 (merged chain) vs T5.5c,
3 x 40 s: LuxBall 17.99 -> 16.38 (-8.9%), DLSC 21.02 -> 21.14,
LuxCore2.1Benchmark 18.70 -> 18.61. Per step (3 x 30 s): T8.1 vs T5.5
(light mask) LuxBall 18.02 -> 18.13, DLSC 20.91 -> 21.13; T5.5b (+ Sobol)
vs T5.5c (+ volume gate) LuxBall 18.21 -> 16.50 (-9.4%), DLSC 21.93 ->
21.11 (-3.7%). The light mask and the Sobol unroll are gains, the volume
gate is a loss.

Why (LuxBall, bins and reports in `D:\REPO\LuxCoreRepo\bench\T5.5-prof`):

- Kernel profile 20 s, T5.5b vs T5.5c: RT_NEXT_VERTEX 3171 -> 5184 ms,
  while DL_SAMPLE_BSDF 2317 -> 1670, RT_DL 1686 -> 1237 and
  GENERATE_NEXT_VERTEX_RAY 4624 -> 4077 ms got faster.
- Nsight Compute, one RT_NEXT_VERTEX launch: 30% fewer instructions,
  255 -> 244 registers, no spills (ptxas: stack frame 720 -> 528 bytes),
  but 41 -> 109 warp cycles per issued instruction, half the memory
  throughput, long_scoreboard stalls per issue 33.6 -> 98. Same occupancy
  (16%). Removing code from `Scene_Intersect()` (not inlined, shared by
  RT_NEXT_VERTEX and RT_DL) made the scheduling of its loads worse; the
  exact cause was not found (no line info in these builds).
- Variant without the `BSDF_Init()` gate (T5.5d): RT_NEXT_VERTEX still
  4996 ms, so the `Scene_Intersect()` gate causes it.
- Variant with only the shadow ray volume info copy skip and the
  `Volume_Scatter()` type guards (T5.5e): RT_NEXT_VERTEX back to 3053 ms
  and DL_SAMPLE_BSDF -20%, but `ab` vs T5.5b: LuxBall +1.6%, DLSC -3.5%,
  LuxCore2.1Benchmark -3.0%. T5.5e was also unsafe: `directLightVolInfos`
  is allocated uninitialized and only written by that copy, and
  `Scene_Intersect()` reads it for shadow rays. If this is retried, call
  `PathVolumeInfo_Init()` on it in the Init kernel (reruns after every
  scene edit; the thread start allocation does not).

The volume gate is dropped.

T5.5b (light mask + Sobol unroll) was not a clear win either. `ab`
T8.1 vs T5.5b: LuxBall +1.3%, DLSC +3.9%, LuxCore2.1Benchmark -4.8%
(with `-D path.photongi.indirect.enabled 0`, 5 runs: 17.57 vs 16.73;
with PhotonGI this scene varies 15.6-18.8 between runs, too noisy to
use). Kernel profile on LuxCore2.1Benchmark (sun + sky2): DL_ILLUMINATE
T8.1 1624 ms, T5.5 (mask) 2144, T5.5b (+ unroll) 2457. Nsight Compute,
one DL_ILLUMINATE launch: the mask drops it from 241 to 48 registers
(40 with the unroll), so occupancy rises 16% -> 57% -> 84%; at 84% the
unrolled Sobol (32 loads per dimension) saturates the load/store queue:
lg_throttle stalls per issue 0.6 -> 13 -> 80, long_scoreboard 8.6 ->
39 -> 100, L1 hit rate 77% -> 71%, with half the instructions.

Fix for the Sobol part: GPU Sobol tables. For each dimension, 8 tables of
16 entries: entry n of table k is the XOR of the direction vectors 4k..4k+3
selected by the bits of n (`GenerateSobolTables()` in
`pathoclbaseoclthreadinit.cpp`, `SOBOL_TABLE_SIZE` = 128 uint per
dimension instead of 32). `SobolSequence_SobolDimension()` does 8
independent loads. Bit-exact: image check 256 spp vs `T6.1-base-img`
LuxBall RMSE 0.00383 (floor 0.0168), DLSC 0.0153 (0.132), pass.

`ab` vs T8.1 (3 x 30 s; LuxCore2.1Benchmark and DanishMood without
PhotonGI):

| Scene | T5.5f (mask + tables) | T5.5g (tables only) |
|---|---|---|
| LuxBall | +2.0% | +1.3% |
| DLSC | +2.0% | +3.9% |
| LuxCore2.1Benchmark | -1.4% | +0.2% |
| DanishMood | -2.0% | +1.8% |

Decision: T5.5 = Sobol tables only (T5.5g). The light type mask is
dropped too: it raises occupancy where the kernel then gets memory bound
and costs the heavier scenes; its gain was compile time and cubin size
(-5%), not speed. Not done: `PARAM_FILM_RADIANCE_GROUP_COUNT` (estimated
at most 0.7%: 3.9% of the SPLAT_SAMPLE stalls, SPLAT_SAMPLE about 18% of
the kernel time, below the `ab` noise).

T5.5 conclusion: compiling code out of these kernels does not reliably
help. The heavy kernels are memory-latency bound; fewer registers raise
occupancy, which can make them slower (cache and load/store queue
pressure). Specialization beyond the material/texture types (T3.2) is not
worth more work.

Lesson: register counts do not predict these effects. Check every
kernel-code change with a 20 s kernel profile (`opencl.kernel.profile.enable
1`) per micro-kernel, not only the total.

Tool note: `ptxas` 12.8 on a cached PTX file (`<TEMP>\luxcorerender.org\
cuda_kernel_cache`): strip the 4-byte header (`tail -c +5`) and change
`.version 8.8` (NVRTC 12.9) to `8.7`; `-e <kernel>` limits it to one entry.

### 2026-10-28: T4.2 MIS compensation for `infinite` lights (opt-in)

Branch `gpu-opt/T4.2-mis-compensation`. New property
`scene.lights.<name>.miscompensation.enable` (default 0) for `infinite`
lights, CPU and GPU (Karlik et al. 2019). Illuminate() and the direct
light pdf use a second Distribution2D built from max(0, L - average L)
in solid angle (sin(theta) per row), mixed with 10% of a distribution
proportional to L so that every emitting direction keeps a non zero pdf
(paths without MIS: last vertex, shadow transparency, visibility flags,
BIDIRCPU, DLSC). Emit() and the emission pdf keep the original
distribution. GPU: `InfiniteLightParam.illuminateDistributionOffset`
(equal to `distributionOffset` when the option is off).

Default image unchanged (`bin-T4.2`, 256 spp vs `T6.1-base-img`):
LuxBall RMSE 0.00367 (floor 0.0168), DLSC 0.00877 (0.132); DanishMood
without PhotonGI vs `T8.1-vol-img` 0.00411 (0.0283).

`bench.py equaltime` (new): RMSE vs a 2048 spp reference (`ref-T4.2`,
option off) after 20 s, 3 runs (seeds 1-3), PhotonGI indirect off, the
scenes' own clamping:

| Scene | RMSE off | RMSE on | MSE ratio |
|---|---|---|---|
| OilPuddle | 0.0328 | 0.0333 | 1.032 |
| OrangeJuice | 1.397 | 1.406 | 1.014 (fireflies dominate) |
| DanishMood | 0.0194 | 0.0172 | 0.789 |
| LynxSpider | 0.0143 | 0.0142 | 0.982 |

So it helps the interior lit by the environment through windows
(DanishMood: 21% less MSE at equal time) and not the outdoor scenes:
it stays off by default, a per-scene option.

Pitfall (fixed in `bench.py`): `luxcoreconsole -D` sets render config
properties only; `scene.*` properties in it never reach the scene loaded
from `scene.file`. The first equaltime run compared "off" with "off"
(MSE ratio 1.000 everywhere). `run_console()` now writes `scene.camera.`,
`scene.lights.`, `scene.materials.` ... overrides into a copy of the
scene file in the run directory and passes `-D scene.file`.

Mean check (image mean vs option off, 3 seeds, fixed spp): with the
scenes' clamping, on shifts the mean by +0.3% (OilPuddle), -0.35%
(OrangeJuice), +0.23% (LynxSpider), 0 (DanishMood), seed spread 0.01-0.04%.
With `-D path.clamping.variance.maxvalue 0` OilPuddle matches (clamping
bias depends on the sampling), LynxSpider still +0.26% (also on PATHCPU,
and with Russian roulette off). Cause found upstream, not in T4.2:
upstream code with only the env distribution changed (data x sin(theta),
option off, experiment build) shifts LynxSpider by +0.10%, exactly
(to 0.001%) what the option's code path gives with the same distribution
(defensive fraction 1). So the upstream PATH estimator on LynxSpider
depends on the env light pdf: some MIS weights there do not sum to 1, or
a technique misses some paths. Not RR, not clamping, CPU and GPU alike;
LynxSpider differs from OilPuddle/DanishMood by alpha transparency on 43
materials, a shadow-transparent area light material, a sun and a sphere
light. Open: find it (see PLAN T4.4); the default image has the same
bias, of unknown size.

### 2026-10-28: Blender 4.5 addon build with T5.5 + T4.2

- Addon zip `D:\REPO\LuxCoreRepo\bench\addon\BlendLuxCoreDiginaat-2.11.1-blender45-gpuopt-27b2798db.zip`
  (`gpu-optimization` at `27b2798db`), same steps as 2026-10-27
  (`make.bat pyluxcore` with Python 3.11, pyd swapped into the stock wheel).
- Smoke test with Python 3.11 on the unpacked wheels: CUDA + OptiX, LuxBall
  PATHOCL 17.1 M samples/sec over 15 s with a warm cache, image correct.
- MIS compensation (T4.2) is not in the BlendLuxCore UI: set
  `scene.lights.<name>.miscompensation.enable` by hand or add an addon
  option later.

### 2026-10-28: T6.3 path guiding (in progress, committed on the branch, not merged)

Branch `gpu-opt/T6.3-path-guiding`. Opt-in: `path.guiding.enable` (default
off), `path.guiding.trainingspp` (64), `path.guiding.cellscale` (.03).
PATHOCL only (`UsePathGuiding()`), kernels behind `-D PARAM_PATH_GUIDING`.

Design (`kernels/pathguiding_funcs.cl`):
- Hash table of 2^16 cells: key from the position quantized by a power of 2
  cell size (distance to the camera x cellscale) plus the dominant axis and
  side of the geometric normal. 8 probes, insert with `atomic_cmpxchg`, new
  cells only while training.
- Per cell 16 x 16 equal area direction bins (cos(theta) x phi, world space).
- Training: guidable vertices are recorded in `MK_GENERATE_NEXT_VERTEX_RAY`
  (up to 8 per path); at the splat (R_final - R_vertex) / (throughputY * pdf)
  goes to the cell bin with `AtomicAdd`. `PathGuiding_Rebuild` (one thread
  per cell) turns histograms with >= 32 samples into CDFs with a 1% floor
  at 2, 4, 8, ... spp up to trainingspp, then the training stops.
- Sampling: one-sample mix, BSDF with probability .5, the cell CDF
  otherwise; throughput f / pdf_mix. Double-buffered CDFs: a path pins the
  buffer at `MK_HIT_OBJECT` so DL and the next vertex use the same CDF.
- The guided branch uses `Material_Evaluate` only on the directions
  `BSDF_Sample` can return (`PathGuiding_EvaluateBSDF`), not `BSDF_Evaluate`
  (its geometric normal tests reject some of them). The MIS pdf
  (`PathGuiding_MISPdf`) is the mix only where both BSDF_Sample and
  BSDF_Evaluate reach the direction, the BSDF pdf elsewhere, the same on
  the DL side and the hit side.
- Not guided: volumes, specular, glossiness < .05, shadow catcher, the
  shadow-transparency pass-through vertex, Disney (T4.5).
- Smoke test: LuxBall 17.76 M samples/sec with guiding on (off 17.1-17.9).

Image mean, guiding on vs off (fixed spp, PhotonGI indirect off, clamping
0, seeds 1-3, spread 0.001-0.03%): the difference is mostly upstream bugs,
each found by bisecting the on/off difference:
- Disney Evaluate f * cos^2 (T4.5): LuxBall -1.2%; with Disney -> matte
  +0.02%. Guiding now skips Disney.
- Schlick microfacet pdf missing cos(theta_h) (T4.6): DanishMood with only
  glossy2 guided +1.6%, +0.5% with the pdf fixed (experiment build). A
  Python replica of the GPU Glossy2 confirms it: without the fix
  E[f/pdf] is 3-12% below the integral of Evaluate, with it equal.
- RR factor only in the DL MIS weight (T4.7): DanishMood glossy2 + Schlick
  fix +0.5% -> +0.15% with RR off.
- What is left is small and not explained: synthetic GlossyRoom (bench\synth,
  glossy2 floor and wall, DanishMood HDRI) with both fixes +0.07%; all matte
  with the env light hidden from BSDF hits (no MIS at all) +0.07%; with a
  uniform guide -0.3%; with BSDF fraction 1 identical to off. The guide pdf
  code and the material pairs check out; not bump mapping, alpha
  transparency, volumes, the visibility map or the env mirror transform.
  User decision 2026-10-28: mean shifts under 2% do not block, so the hunt
  stopped here.
- T4.4 (LynxSpider pdf-dependent mean): not Disney (+0.22% with Disney ->
  matte), not the Schlick pdf (+0.24% with the fix), and RR off was already
  tested there; still open.

State of the commit: the experiment hooks (`LUX_PG_DEFINES`, `PG_EXP_*`)
are removed and the Schlick pdf experiment reverted; this exact source was
not rebuilt (the last build, bench\bin-T6.3h, had the hooks). Next: build,
smoke test, speed and default image checks (guiding off must equal the
tip), equal-time RMSE on DanishMood, LynxSpider, OilPuddle, OrangeJuice
with guiding on vs off, then merge as opt-in if it wins.
