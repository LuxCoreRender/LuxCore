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
