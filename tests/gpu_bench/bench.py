# SPDX-FileCopyrightText: 2026 Authors (see AUTHORS.txt)
#
# SPDX-License-Identifier: Apache-2.0

"""GPU benchmark harness for the CUDA/OptiX optimization work.

Implements the "Benchmark protocol" of AGENTS.md with the standard library
only. Results go to <out>/<label>/ (keep <out> outside the repository).

  speed:   median samples/sec per scene (1 warm-up run + N measured runs)
  image:   fixed-SPP renders, written twice so the noise floor is known
  compare: RMSE of candidate vs baseline images against the noise floor

Examples:
  python tests/gpu_bench/bench.py speed --bin out/install/Release/bin/luxcoreconsole.exe --label main
  python tests/gpu_bench/bench.py image --bin ... --label main
  python tests/gpu_bench/bench.py compare --baseline main --candidate T1.1
"""

import argparse
import array
import json
import math
import os
import re
import statistics
import subprocess
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
DEFAULT_SCENES_DIR = REPO_ROOT.parent / "LuxCoreTestScenes" / "scenes"
DEFAULT_OUT = REPO_ROOT.parent / "bench"

SCENES = ["LuxBall", "DLSC", "LuxCore2.1Benchmark", "DanishMood", "LynxSpider"]

# Overrides shared by every run: GPU PATHOCL, fixed seed, no adaptive
# sampling, no hybrid CPU threads, no noise-threshold halt
COMMON_PROPS = {
    "renderengine.type": "PATHOCL",
    "renderengine.seed": "1",
    "sampler.sobol.adaptive.strength": "0",
    "opencl.cpu.use": "0",
    "opencl.gpu.use": "1",
    "opencl.native.threads.count": "0",
    "batch.haltthreshold": "-1",
}

# Extra film output used for image comparison: linear radiance, no pipeline
COMPARE_OUTPUT_INDEX = 99
COMPARE_FILE = "rgb.hdr"

SAMPLES_SEC_RE = re.compile(r"\[Elapsed time:\s*(\d+)/\d+sec\].*?Avg\. samples/sec\s+([\d.]+)M")
OUTPUT_FILENAME_RE = re.compile(r'^\s*(film\.outputs\.\d+\.filename)\s*=\s*"?([^"\r\n]+)"?', re.MULTILINE)


def scene_dir(scenes_dir, scene):
    return scenes_dir / scene / "LuxCoreScene"


def run_console(binary, scene, scenes_dir, run_dir, props, temp_dir):
    """Render one scene; returns the console log text.

    temp_dir becomes TEMP/TMP of the process. LuxCore keeps its kernel cache
    there and the cache key ignores NVRTC built-in options, so each build
    (label) needs its own cache or builds would reuse each other's PTX.
    """
    cfg_dir = scene_dir(scenes_dir, scene)
    run_dir.mkdir(parents=True, exist_ok=True)
    temp_dir.mkdir(parents=True, exist_ok=True)
    env = dict(os.environ, TEMP=str(temp_dir), TMP=str(temp_dir))

    all_props = dict(COMMON_PROPS)
    # Write the scene's own outputs into run_dir, not into the scenes repo
    cfg_text = (cfg_dir / "render.cfg").read_text(encoding="utf-8", errors="replace")
    for key, filename in OUTPUT_FILENAME_RE.findall(cfg_text):
        all_props[key] = str(run_dir / Path(filename.strip()).name)
    all_props.update(props)

    cmd = [str(binary)]
    for key, value in all_props.items():
        cmd += ["-D", key, value]
    cmd.append("render.cfg")

    proc = subprocess.run(cmd, cwd=cfg_dir, env=env, capture_output=True, text=True,
                          errors="replace", check=False)
    log = proc.stdout + proc.stderr
    (run_dir / "console.log").write_text(log, encoding="utf-8")
    if proc.returncode:
        raise RuntimeError(f"{scene}: luxcoreconsole exit code {proc.returncode}, see {run_dir / 'console.log'}")
    if "Optix" not in log:
        print(f"WARNING {scene}: no Optix line in log, check device selection", file=sys.stderr)
    return log


def parse_samples_sec(log, halttime):
    matches = [(int(t), float(v)) for t, v in SAMPLES_SEC_RE.findall(log)]
    if not matches:
        raise RuntimeError("no 'Avg. samples/sec' line in log")
    # The final line is printed after the halt and counts shutdown time
    # as render time, so use the last line within the halt time
    in_time = [v for t, v in matches if t <= halttime]
    return in_time[-1] if in_time else matches[-1][1]


def read_hdr(path):
    """Read a Radiance RGBE (.hdr) image as a flat float array (RGB).

    LuxCore saves linear (not tonemapped) outputs only as .exr or .hdr, and
    .hdr can be decoded with the standard library. RGBE keeps about 8 bits of
    mantissa (~0.4% relative error), well below the noise at the test SPP.
    """
    data = Path(path).read_bytes()
    pos = data.index(b"\n\n") + 2
    end = data.index(b"\n", pos)
    tokens = data[pos:end].split()
    if len(tokens) != 4 or tokens[0] != b"-Y" or tokens[2] != b"+X":
        raise RuntimeError(f"{path}: unsupported HDR orientation {tokens}")
    height, width = int(tokens[1]), int(tokens[3])
    pos = end + 1

    rgbe = bytearray(width * height * 4)
    for y in range(height):
        row = y * width * 4
        if data[pos] == 2 and data[pos + 1] == 2 and (data[pos + 2] << 8 | data[pos + 3]) == width:
            # New-style RLE scanline: each of the 4 components is stored separately
            pos += 4
            for c in range(4):
                x = 0
                while x < width:
                    count = data[pos]
                    pos += 1
                    if count > 128:
                        count -= 128
                        value = data[pos]
                        pos += 1
                        for i in range(x, x + count):
                            rgbe[row + i * 4 + c] = value
                    else:
                        for i in range(x, x + count):
                            rgbe[row + i * 4 + c] = data[pos]
                            pos += 1
                    x += count
        else:
            # Flat scanline
            rgbe[row:row + width * 4] = data[pos:pos + width * 4]
            pos += width * 4

    out = array.array("f", bytes(width * height * 3 * 4))
    for i in range(width * height):
        e = rgbe[i * 4 + 3]
        if e:
            f = math.ldexp(1.0, e - 136)
            out[i * 3] = (rgbe[i * 4] + 0.5) * f
            out[i * 3 + 1] = (rgbe[i * 4 + 1] + 0.5) * f
            out[i * 3 + 2] = (rgbe[i * 4 + 2] + 0.5) * f
    return out


def rmse(a, b):
    if len(a) != len(b):
        raise RuntimeError("image size mismatch")
    return math.sqrt(sum((x - y) * (x - y) for x, y in zip(a, b)) / len(a))


def parse_props(pairs):
    return {key: value for key, value in pairs or []}


def cmd_speed(args):
    props = parse_props(args.D)
    temp_dir = args.out / args.label / "tmp"
    results = {}
    for scene in args.scenes:
        base = args.out / args.label / "speed" / scene
        # Warm-up: fills the kernel cache, not measured
        run_console(args.bin, scene, args.scenes_dir, base / "warmup",
                    {**props, "batch.halttime": "15", "batch.haltspp": "0"}, temp_dir)
        runs = []
        for i in range(args.runs):
            log = run_console(args.bin, scene, args.scenes_dir, base / f"run{i}",
                              {**props, "batch.halttime": str(args.halttime), "batch.haltspp": "0"}, temp_dir)
            runs.append(parse_samples_sec(log, args.halttime))
        results[scene] = {"runs": runs, "median": statistics.median(runs)}
        print(f"{scene}: {results[scene]['median']:.3f} M samples/sec {runs}", flush=True)

    out_file = args.out / args.label / "speed.json"
    out_file.write_text(json.dumps({"props": props, "halttime": args.halttime, "results": results}, indent=2))
    print("\n| Scene | M samples/sec (median) |\n|---|---|")
    for scene, r in results.items():
        print(f"| {scene} | {r['median']:.3f} |")


def cmd_ab(args):
    # A and B runs alternate, so GPU clock and temperature drift during a long
    # session affect both sides alike
    common = parse_props(args.D)
    sides = [(args.a[0], Path(args.a[1]), {**common, **parse_props(args.da)}),
             (args.b[0], Path(args.b[1]), {**common, **parse_props(args.db)})]
    rows = []
    for scene in args.scenes:
        runs = {label: [] for label, _, _ in sides}
        for label, binary, props in sides:
            base = args.out / "ab" / label / scene
            run_console(binary, scene, args.scenes_dir, base / "warmup",
                        {**props, "batch.halttime": "15", "batch.haltspp": "0"}, args.out / label / "tmp")
        for i in range(args.runs):
            for label, binary, props in sides:
                base = args.out / "ab" / label / scene
                log = run_console(binary, scene, args.scenes_dir, base / f"run{i}",
                                  {**props, "batch.halttime": str(args.halttime), "batch.haltspp": "0"},
                                  args.out / label / "tmp")
                runs[label].append(parse_samples_sec(log, args.halttime))
        med_a = statistics.median(runs[args.a[0]])
        med_b = statistics.median(runs[args.b[0]])
        rows.append((scene, med_a, med_b))
        print(f"{scene}: {args.a[0]} {med_a:.3f} {runs[args.a[0]]}, {args.b[0]} {med_b:.3f} {runs[args.b[0]]}", flush=True)

    print(f"\n| Scene | {args.a[0]} | {args.b[0]} | change |\n|---|---|---|---|")
    for scene, med_a, med_b in rows:
        print(f"| {scene} | {med_a:.3f} | {med_b:.3f} | {(med_b - med_a) / med_a:+.1%} |")


def cmd_image(args):
    props = parse_props(args.D)
    props.update({
        "batch.halttime": "0",
        "batch.haltspp": str(args.haltspp),
        f"film.outputs.{COMPARE_OUTPUT_INDEX}.type": "RGB",
    })
    for scene in args.scenes:
        for i in range(args.repeat):
            run_dir = args.out / args.label / "image" / scene / f"run{i}"
            # Run i uses seed + i: the noise floor is the difference between
            # two independent renders, so a candidate that gives another
            # (equally valid) noise pattern is not flagged
            run_console(args.bin, scene, args.scenes_dir, run_dir,
                        {**props, "renderengine.seed": str(args.seed + i),
                         f"film.outputs.{COMPARE_OUTPUT_INDEX}.filename": str(run_dir / COMPARE_FILE)},
                        args.out / args.label / "tmp")
        print(f"{scene}: {args.repeat} image(s) at {args.haltspp} spp", flush=True)


def cmd_compare(args):
    rows = []
    ok_all = True
    for scene in args.scenes:
        def img(label, i):
            return read_hdr(args.out / label / "image" / scene / f"run{i}" / COMPARE_FILE)

        base0 = img(args.baseline, 0)
        base1 = img(args.baseline, 1)
        cand_img = img(args.candidate, 0)
        noise = rmse(base0, base1)
        cand = rmse(cand_img, base0)
        ok = cand <= args.tolerance * noise if noise > 0 else cand == 0
        ok_all &= ok
        # Mean brightness change relative to the baseline: shows a bias that
        # RMSE can hide in the noise
        mean0 = statistics.fmean(base0)
        noise_bias = (statistics.fmean(base1) - mean0) / mean0
        cand_bias = (statistics.fmean(cand_img) - mean0) / mean0
        rows.append((scene, noise, cand, noise_bias, cand_bias, ok))

    print(f"\n| Scene | noise floor RMSE | candidate RMSE | seed mean diff | candidate mean diff | pass (<= {args.tolerance}x) |")
    print("|---|---|---|---|---|---|")
    for scene, noise, cand, noise_bias, cand_bias, ok in rows:
        print(f"| {scene} | {noise:.6g} | {cand:.6g} | {noise_bias:+.3%} | {cand_bias:+.3%} | {'yes' if ok else 'NO'} |")
    print("\nAlso inspect diff images for structure; RMSE alone can hide local errors.")
    return 0 if ok_all else 1


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--out", type=Path, default=DEFAULT_OUT, help="results root (outside the repo)")
    sub = parser.add_subparsers(dest="command", required=True)

    def add_render_args(p):
        p.add_argument("--scenes", nargs="+", default=SCENES)
        p.add_argument("--bin", type=Path, required=True, help="luxcoreconsole executable")
        p.add_argument("--label", required=True, help="name of this build, e.g. main or T1.1")
        p.add_argument("--scenes-dir", type=Path, default=DEFAULT_SCENES_DIR)
        p.add_argument("-D", nargs=2, action="append", metavar=("PROP", "VALUE"),
                       help="extra property override, e.g. -D opencl.gpu.workgroup.size 128")

    p = sub.add_parser("speed")
    add_render_args(p)
    p.add_argument("--runs", type=int, default=3)
    p.add_argument("--halttime", type=int, default=60)

    p = sub.add_parser("image")
    add_render_args(p)
    p.add_argument("--haltspp", type=int, default=256)
    p.add_argument("--repeat", type=int, default=2, help="2 for a baseline (noise floor), 1 for a candidate")
    p.add_argument("--seed", type=int, default=1, help="seed of the first run; run i uses seed + i")

    p = sub.add_parser("ab", help="interleaved speed runs of two builds or settings")
    p.add_argument("--scenes", nargs="+", default=SCENES)
    p.add_argument("--scenes-dir", type=Path, default=DEFAULT_SCENES_DIR)
    p.add_argument("--a", nargs=2, required=True, metavar=("LABEL", "BIN"))
    p.add_argument("--b", nargs=2, required=True, metavar=("LABEL", "BIN"))
    p.add_argument("-D", nargs=2, action="append", metavar=("PROP", "VALUE"), help="override for both sides")
    p.add_argument("--da", nargs=2, action="append", metavar=("PROP", "VALUE"), help="override for side A only")
    p.add_argument("--db", nargs=2, action="append", metavar=("PROP", "VALUE"), help="override for side B only")
    p.add_argument("--runs", type=int, default=3)
    p.add_argument("--halttime", type=int, default=30)

    p = sub.add_parser("compare")
    p.add_argument("--scenes", nargs="+", default=SCENES)
    p.add_argument("--baseline", required=True)
    p.add_argument("--candidate", required=True)
    p.add_argument("--tolerance", type=float, default=1.1)

    args = parser.parse_args()
    if args.command == "speed":
        cmd_speed(args)
    elif args.command == "ab":
        cmd_ab(args)
    elif args.command == "image":
        cmd_image(args)
    else:
        sys.exit(cmd_compare(args))


if __name__ == "__main__":
    main()
