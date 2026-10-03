#!/usr/bin/env python3
"""
Benchmark comparing simplify vs simplify2 mesh simplification in LuxCore.

Uses the scenes/displacement/simplify-plane(.scn) pipeline
(mesh -> subdiv maxlevel 8 -> displacement -> simplify), which is identical
in both variants except for the shape type (simplify vs simplify2).

Runs with renderengine.type = FILESAVER so the measured time is the mesh
processing (including the simplification) without any rendering.
"""

import csv
import re
import statistics
import subprocess
import time
from pathlib import Path

REPO_ROOT = Path("/home/vincent/.masse/Documents/DevGit/LuxCore")
LUXCORE_CONSOLE = REPO_ROOT / "out/build/samples/luxcoreconsole/Release/luxcoreconsole"
OUTPUT_DIR = REPO_ROOT / "scenes/benchmark/results"
SAVE_DIR = OUTPUT_DIR / "saved"

VARIANTS = [
    {"name": "simplify", "cfg": REPO_ROOT / "scenes/displacement/simplify-plane-legacy.cfg"},
    {"name": "simplify2", "cfg": REPO_ROOT / "scenes/displacement/simplify-plane.cfg"},
]

RUNS = 3


def run_once(cfg, save_subdir):
    save_subdir.mkdir(parents=True, exist_ok=True)
    cmd = [
        str(LUXCORE_CONSOLE),
        "-D", "renderengine.type", "FILESAVER",
        "-D", "filesaver.directory", str(save_subdir),
        "-D", "filesaver.renderengine.type", "PATHOCL",
        "-D", "batch.halttime", "600",
        str(cfg),
    ]
    start = time.perf_counter()
    result = subprocess.run(
        cmd, capture_output=True, text=True, cwd=str(REPO_ROOT), timeout=600
    )
    elapsed = time.perf_counter() - start
    output = result.stdout + result.stderr
    if result.returncode != 0:
        raise RuntimeError(f"luxcoreconsole failed (exit {result.returncode}):\n{output[-2000:]}")
    return elapsed, output


SDL_TIME_RE = re.compile(r"\[SDL\]\[([\d.]+)\]")


def sdl_time(line):
    m = SDL_TIME_RE.search(line)
    return float(m.group(1)) if m else None


def parse_output(output):
    """Return (simplify_step_time, src_tris, dst_tris).

    The simplify step time is taken from the SDL log: for the legacy
    simplify, from its self reported "Simplify time: X secs"; for
    simplify2, from the [SDL][t] timestamps between "Creating simplified
    shape" and the final "Simplified shape from X to Y faces" line.
    """
    step_time = None
    src_tris = dst_tris = 0
    start_time = None
    last_subdivided = None

    for line in output.splitlines():
        if "Simplify time:" in line:
            m = re.search(r"Simplify time: ([\d.]+)secs", line)
            if m:
                step_time = float(m.group(1))
                # The legacy simplify prints its result right before, as
                # "Subdivided shape from X to Y faces"
                if last_subdivided:
                    src_tris, dst_tris = last_subdivided
        elif "Subdivided shape from" in line:
            m = re.search(r"Subdivided shape from (\d+) to (\d+)", line)
            if m:
                last_subdivided = int(m.group(1)), int(m.group(2))
        elif "Creating simplified shape" in line:
            start_time = sdl_time(line)
        elif "Simplified shape from" in line:
            m = re.search(r"Simplified shape from (\d+) to (\d+)", line)
            if m:
                src_tris, dst_tris = int(m.group(1)), int(m.group(2))
                end_time = sdl_time(line)
                if start_time is not None and end_time is not None:
                    step_time = end_time - start_time

    return step_time, src_tris, dst_tris


def main():
    print("=" * 64)
    print("LuxCore simplify vs simplify2 benchmark")
    print(f"{RUNS} runs per variant, FILESAVER engine (mesh processing only)")
    print("=" * 64)

    if not LUXCORE_CONSOLE.exists():
        raise SystemExit(f"Error: {LUXCORE_CONSOLE} not found")

    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    csv_file = OUTPUT_DIR / "benchmark_results.csv"

    rows = []
    for variant in VARIANTS:
        step_times = []
        total_times = []
        src_tris = dst_tris = 0
        for run in range(RUNS):
            save_dir = SAVE_DIR / f"{variant['name']}-run{run}"
            elapsed, output = run_once(variant["cfg"], save_dir)
            step_time, src_tris, dst_tris = parse_output(output)
            total_times.append(elapsed)
            if step_time is None:
                raise RuntimeError(f"Could not parse simplify step time for {variant['name']}:\n"
                                   + "\n".join(l for l in output.splitlines() if "Simplif" in l)[-2000:])
            step_times.append(step_time)
            print(f"  {variant['name']}: run {run + 1}/{RUNS}: step {step_time:.3f}s, "
                  f"total {elapsed:.2f}s ({src_tris} -> {dst_tris} tris)")

        step_best = min(step_times)
        step_mean = statistics.mean(step_times)
        step_stdev = statistics.stdev(step_times) if len(step_times) > 1 else 0.0
        total_mean = statistics.mean(total_times)
        print(f"{variant['name']}: step best {step_best:.3f}s, step mean {step_mean:.3f}s "
              f"(+/- {step_stdev:.3f}s), total mean {total_mean:.2f}s, "
              f"{src_tris} -> {dst_tris} tris")
        rows.append([variant["name"], f"{step_best:.3f}", f"{step_mean:.3f}",
                     f"{step_stdev:.3f}", f"{total_mean:.3f}", src_tris, dst_tris])

    with open(csv_file, "w", newline="") as f:
        writer = csv.writer(f)
        writer.writerow(["Method", "Simplify_Step_Best_s", "Simplify_Step_Mean_s",
                         "Simplify_Step_StDev_s", "Total_Mean_s",
                         "Triangles_Original", "Triangles_Result"])
        writer.writerows(rows)

    print()
    speedup = float(rows[0][2]) / float(rows[1][2])
    print(f"Speedup (simplify step mean / simplify2 step mean): {speedup:.2f}x")
    print(f"Results saved to: {csv_file}")


if __name__ == "__main__":
    main()
