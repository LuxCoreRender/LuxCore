# SPDX-FileCopyrightText: 2024 Howetuft
#
# SPDX-License-Identifier: Apache-2.0

import os
from pathlib import Path
import argparse
import tempfile
import shutil

from wheel.wheelfile import WheelFile
from wheel.cli.unpack import unpack
from wheel.cli.pack import pack

def main():
    # Parse arguments
    parser = argparse.ArgumentParser()
    parser.add_argument("wheelpath", type=Path)
    args = parser.parse_args()

    wheel_path = args.wheelpath
    wheel_folder = wheel_path.parents[0]

    print(f"Recomposing {wheel_path}")

    with tempfile.TemporaryDirectory() as tmpdir:  # Working space
        # Unpack wheel
        unpack(path=args.wheelpath, dest=tmpdir)
        with WheelFile(args.wheelpath) as wf:
            namever = wf.parsed_filename.group("namever")
            unpacked_wheel_path = Path(tmpdir) / namever

        # Rename and move oidnDenoise
        print("Rename oidnDenoise.pyd into oidnDenois.exe")
        shutil.move(
            unpacked_wheel_path / "pyluxcore.libs" / "oidnDenoise.pyd",
            unpacked_wheel_path / "pyluxcore.libs" / "oidnDenoise.exe",
        )

        # Rename and move OpenImageDenoise_device_cpu
        print("Rename OpenImageDenoise_device_cpu.pyd into OpenImageDenoise_device_cpu.dll")
        shutil.move(
            unpacked_wheel_path / "pyluxcore.libs" / "OpenImageDenoise_device_cpu.pyd",
            unpacked_wheel_path / "pyluxcore.libs" / "OpenImageDenoise_device_cpu.dll",
        )

        # Repack wheel
        pack(
            directory=unpacked_wheel_path,
            dest_dir=wheel_folder,
            build_number=None
        )


if __name__ == "__main__":
    main()
