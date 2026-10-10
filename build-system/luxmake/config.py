# SPDX-FileCopyrightText: 2025 Authors (see AUTHORS.txt)
#
# SPDX-License-Identifier: Apache-2.0

"""Config command.

This command has also been extended to export compile commands for CMake
(`compile_commands.json` file), mainly for syntaxic checkers.
"""

from .constants import PARAMS
from .utils import run_cmake, fail, logger
from .spirv import get_pre_baked_dir


def config(
    _,  # args (unused)
):
    """CMake config."""
    # Check whether presets exist
    presets = PARAMS.BINARY_DIR / "build" / "generators" / "CMakePresets.json"
    if not presets.exists():
        fail(
            "Cannot find presets file ('%s'). "
            "Have you run 'make deps' beforehand?",
            str(presets.absolute()),
        )

    # Prepare and run command
    cmd = [
        "--preset conan-default",
        f"-DCMAKE_INSTALL_PREFIX={str(PARAMS.INSTALL_DIR)}",
        "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON",
        f"-S {str(PARAMS.SOURCE_DIR)}",
    ]

    # Use the pre-baked SPIR-V modules, if available (see the spirv
    # command, i.e. 'make spirv'): the main build will not need a clang
    # able to compile OpenCL C in SPIR-V format
    if pre_baked_dir := get_pre_baked_dir():
        cmd.append(f"-DLUXRAYS_SPIRV_BAKED_DIR={pre_baked_dir}")
        logger.info(
            "Using the pre-baked SPIR-V modules from '%s'",
            str(pre_baked_dir.absolute()),
        )

    run_cmake(cmd)

    # Info
    compile_commands_file = (
        PARAMS.BINARY_DIR / "build" / "compile_commands.json"
    )
    logger.info(
        "Compile commands file generated at: '%s'", compile_commands_file
    )
