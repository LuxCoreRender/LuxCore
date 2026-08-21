# SPDX-FileCopyrightText: 2025 Authors (see AUTHORS.txt)
#
# SPDX-License-Identifier: Apache-2.0

"""Utilities for make wrapper."""

import sys
import functools
import logging
import shutil
import subprocess
import tempfile
import pathlib
import re
from dataclasses import dataclass

# Logger
logger = logging.getLogger("LuxCore")


def set_logger_verbose():
    """Set logger in verbose mode (show debug messages)."""
    logger.setLevel(logging.DEBUG)
    logger.debug("Verbose mode")


def fail(*args):
    """Fails gracefully."""
    print(end=Colors.FAIL, flush=True)
    logger.error(*args)
    print(end=Colors.ENDC, flush=True)
    sys.exit(1)


@dataclass
class Colors:
    """Colors for terminal output."""

    HEADER = "\033[95m"
    OKBLUE = "\033[94m"
    OKCYAN = "\033[96m"
    OKGREEN = "\033[92m"
    WARNING = "\033[93m"
    WARNING2 = "\033[33m"
    WARNING3 = "\033[4;33m"
    FAIL = "\033[91m"
    ENDC = "\033[0m"
    BOLD = "\033[1m"
    UNDERLINE = "\033[4m"


# Cmake
@functools.cache
def ensure_cmake_app():
    """Ensure cmake is installed."""
    logger.debug("Looking for cmake")
    if not (res := shutil.which("cmake")):
        fail("CMake not found!")
    logger.debug(
        "CMake found: '%s'",
        res,
    )
    return res


def run_cmake(
    args,
    **kwargs,
):
    """Run cmake statement."""
    cmake_app = ensure_cmake_app()
    args = [cmake_app] + args
    logger.debug(" ".join(args))
    # print(" ".join(args))  # Debug
    res = subprocess.run(
        args,
        shell=False,
        check=False,
        **kwargs,
    )
    if res.returncode:
        fail("Error while executing cmake")
    return res


_CMAKE_FIND_PACKAGE_SNIPPET = """\
cmake_minimum_required(VERSION 3.20)
project(find)
find_package({0})
message(STATUS "@@${{{0}_VERSION}}@@")
"""

_TOOLCHAIN = pathlib.Path(
    "out", "build", "generators", "conan_toolchain.cmake"
)


def _run_find_package(dep):
    """Run a find_package in cmake."""
    with tempfile.TemporaryDirectory() as folder:
        folder = pathlib.Path(folder)
        with open(
            folder / "CMakeLists.txt", "w", encoding="utf-8"
        ) as cmakelists:
            cmakelists.write(_CMAKE_FIND_PACKAGE_SNIPPET.format(dep))
            cmakelists.close()
            statement = [
                "-S",
                f"{folder}",
                "-B",
                f'{folder / "build"}',
                f"-DCMAKE_TOOLCHAIN_FILE={str(_TOOLCHAIN.absolute())}",
                "-DCMAKE_BUILD_TYPE=Debug",
            ]
            res = run_cmake(
                statement,
                text=True,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
            )
            res.check_returncode()
            return res.stdout


def get_dep_version(dep):
    """Get the version of a given dependency, as foundable by cmake.

    Important: this function assumes it is run in the root directory
    of the projet.
    """

    # Run cmake and parse output
    cmake_result = _run_find_package(dep)
    if not (versions := re.findall(r"@@([A-Za-z0-9.]+)@@", cmake_result)):
        raise ValueError(f"No dependency '{dep}' found")
    version, *_ = versions

    return version


def unpack(path, dest):
    """Unpack a wheel."""
    args = [
        sys.executable,
        "-m",
        "wheel",
        "unpack",
        f"--dest={dest}",
        str(path),
    ]
    try:
        output = subprocess.check_output(
            args, text=True, stderr=subprocess.STDOUT
        )
    except subprocess.CalledProcessError as err:
        fail(err.output)
    logger.info(output)


def pack(directory, dest_dir):
    """(Re)pack a wheel."""
    args = [
        sys.executable,
        "-m",
        "wheel",
        "pack",
        f"--dest-dir={dest_dir}",
        str(directory),
    ]
    try:
        output = subprocess.check_output(
            args, text=True, stderr=subprocess.STDOUT
        )
    except subprocess.CalledProcessError as err:
        fail(err.output)
    logger.info(output)


def run_module(module, args, hide_output=False):
    """Run a Python module on command line.

    Args:
    module -- Name of the module (str)
    args -- List of the arguments (list of str)
    hide_output -- Do not display output

    Returns:
    Output from stdout & stderr
    """
    module = str(module)
    if not module:
        raise ValueError("Module cannot be empty")
    hide_output = bool(hide_output)

    command = [sys.executable, "-m"] + [module] + [str(a) for a in args]

    try:
        output = subprocess.check_output(
            command, text=True, stderr=subprocess.STDOUT
        )
    except subprocess.CalledProcessError as err:
        raise RuntimeError(err.output) from err

    if not hide_output:
        logger.info(output)
    return output


# Initialization
# Set-up logger
logger.setLevel(logging.INFO)
logging.basicConfig(level=logging.INFO)
