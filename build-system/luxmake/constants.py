# SPDX-FileCopyrightText: 2025 Authors (see AUTHORS.txt)
#
# SPDX-License-Identifier: Apache-2.0

"""Constants for luxmake."""

import os
import pathlib

# Environment variables that control the build
SOURCE_DIR = pathlib.Path(
    os.getenv(
        "LUX_SOURCE_DIR",
        os.getcwd(),
    )
)
BINARY_DIR = pathlib.Path(
    os.getenv(
        "LUX_BINARY_DIR",
        "out",
    )
)
BUILD_TYPE = os.getenv(
    "LUX_BUILD_TYPE",
    "Release",
)
WHEEL_HOOK = os.getenv(  # Hook to execute after wheel-test build
    "LUX_WHEEL_HOOK",
    "",
)

# Computed variables
BUILD_DIR = BINARY_DIR / "build"
INSTALL_DIR = BINARY_DIR / "install" / BUILD_TYPE
WHEELHOUSE_DIR = INSTALL_DIR / "wheel"  # Where the wheel will be created
