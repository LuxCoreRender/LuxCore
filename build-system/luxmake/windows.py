# SPDX-FileCopyrightText: 2024 Howetuft
#
# SPDX-License-Identifier: Apache-2.0

"""This script makes a final rearrangement to a Windows wheel.

2 actions to be done:
- Rename and move oidnDenoise.exe
- Rename and move OpenImageDenoise_device_cpu.dll
"""

from pathlib import Path
import argparse
import tempfile
import shutil
import re

from .utils import pack, unpack, logger, fail

# pylint:disable=line-too-long
WHEEL_INFO_RE = re.compile(
    r"""^(?P<namever>(?P<name>[^\s-]+?)-(?P<ver>[^\s-]+?))(-(?P<build>\d[^\s-]*))?
     -(?P<pyver>[^\s-]+?)-(?P<abi>[^\s-]+?)-(?P<plat>\S+)\.whl$""",
    re.VERBOSE,
)


def _rename(basepath, org, dest):
    """Rename wheel component."""
    try:
        shutil.move(
            basepath / "pyluxcore.libs" / org,
            basepath / "pyluxcore.libs" / dest,
        )
    except FileNotFoundError:
        fail("Missing file in wheel: '%s'", org)


def win_recompose(args):
    """Recompose Windows wheel (see module docstring)."""
    wheel_path = Path(args.wheel)
    wheel_folder = wheel_path.parents[0]

    logger.info("Recomposing '%s'", wheel_path)

    with tempfile.TemporaryDirectory() as tmpdir:  # Working space
        # Unpack wheel
        unpack(path=wheel_path, dest=tmpdir)

        # Get path where to output
        parsed_filename = WHEEL_INFO_RE.match(wheel_path.name)
        namever = parsed_filename.group("namever")
        unpacked_wheel_path = Path(tmpdir) / namever

        # Rename and move OpenImageDenoise_device_cpu
        logger.info(
            "Renaming LuxOpenImageDenoise_device_cpu.pyd "
            "into LuxOpenImageDenoise_device_cpu.dll"
        )
        _rename(
            unpacked_wheel_path,
            "LuxOpenImageDenoise_device_cpu.pyd",
            "LuxOpenImageDenoise_device_cpu.dll",
        )

        # Repack wheel
        pack(
            directory=unpacked_wheel_path,
            dest_dir=wheel_folder,
        )


if __name__ == "__main__":
    # Parse arguments
    parser = argparse.ArgumentParser()
    parser.add_argument("wheelpath", type=Path)
    main_args = parser.parse_args()

    win_recompose(main_args)
