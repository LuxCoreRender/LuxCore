import subprocess
import sys
import re
import os

if __name__ == "__main__":
    # if environment variable is set...
    if "CONAN_PRESET" in os.environ and (conan_preset:= os.environ["CONAN_PRESET"]):
        print(conan_preset)
        exit(0)

    try:
        res = subprocess.run(
            ["cmake", "--list-presets"],
            capture_output=True,
            text=True,
            check=True,
        )
    except FileNotFoundError as err:
        print(str(err), file=sys.stderr)
        exit(1)
    except subprocess.CalledProcessError as err:
        print(err.stderr, file=sys.stderr)
        exit(1)

    lines = res.stdout.splitlines()

    try:
        line = lines[2]
    except IndexError:
        print("ERROR: bad cmake output:", res.stdout, file=sys.stderr)
        print(res.stderr, file=sys.stderr)
        exit(1)

    res = re.findall('"([^"]*)"', line)

    for preset in res:
        print(preset)
        break
    else:
        print("ERROR: no preset found in cmake output", line)

