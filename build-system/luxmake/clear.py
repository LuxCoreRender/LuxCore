# SPDX-FileCopyrightText: 2025 Authors (see AUTHORS.txt)
#
# SPDX-License-Identifier: Apache-2.0

"""Clear and clean commands."""

import shutil
import sys
import subprocess

from .constants import PARAMS
from .utils import logger
from .presets import get_presets, PresetType
from .utils import run_cmake


def clean(
    _,
):
    """CMake clean."""
    for preset in get_presets(PresetType.BUILD):
        logger.info(
            "Cleaning preset '%s'",
            preset,
        )
        cmd = [
            "--build",
            f"--preset {preset}",
            "--target clean",
        ]
        run_cmake(cmd)


def win_rmdir(directory):
    """(Windows only) Remove directory, using DOS command."""
    if sys.platform == "win32":
        try:
            # Use /s /q to remove directory and all subdirectories/files,
            # quietly
            subprocess.run(
                ["cmd", "/c", "rmdir", "/s", "/q", directory],
                check=True,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
            )
        except subprocess.CalledProcessError:
            # Continue silently if directory does not exist or command fails
            pass


def clear(
    _,
):
    """Clear binary directory."""
    # We just remove the subdirectories, in order to avoid
    # unwanted removals if BINARY_DIR points to a wrong directory
    for subdir in (
        "build",
        "dependencies",
        "install",
        "spirv-build",
    ):
        if not PARAMS.BINARY_DIR:
            raise RuntimeError("Invalid binary directory")
        directory = PARAMS.BINARY_DIR / subdir
        logger.info(
            "Removing '%s'",
            directory,
        )
        try:
            win_rmdir(directory)
            shutil.rmtree(
                directory,
                ignore_errors=True,
            )
        except FileNotFoundError:
            # Do not fail if not found
            logger.debug(
                "'%s' not found",
                directory,
            )
