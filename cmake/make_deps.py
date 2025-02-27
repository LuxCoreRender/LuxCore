# SPDX-FileCopyrightText: 2024 Howetuft
#
# SPDX-License-Identifier: Apache-2.0

"""This script downloads and installs dependencies for LuxCore build."""

import os
import tempfile
from urllib.request import urlretrieve
from pathlib import Path
from zipfile import ZipFile
import subprocess
import logging
import shutil
import argparse
import json
import platform
import sys
from functools import cache


CONAN_ALL_PACKAGES = '"*"'

BUILD_DIR = os.getenv("BUILD_DIR", "./build")

CMAKE_DIR = Path(BUILD_DIR) / "cmake"

logger = logging.getLogger("LuxCore Dependencies")

CONAN_ENV = {}

URL_SUFFIXES = {
    "Linux-X64": "ubuntu-latest",
    "Windows-X64": "windows-latest",
    "macOS-ARM64": "macos-14",
    "macOS-X64": "macos-13",
}

LUX_GENERATOR = os.getenv("LUX_GENERATOR", "")


def find_platform():
    """Find current platform."""
    system = platform.system()
    if system == "Linux":
        res = "Linux-X64"
    elif system == "Windows":
        res = "Windows-X64"
    elif system == "Darwin":
        machine = platform.machine()
        if machine == "arm64":
            res = "macOS-ARM64"
        elif machine == "x86_64":
            res = "macOS-X64"
        else:
            raise RuntimeError(f"Unknown machine for MacOS: '{machine}'")
    else:
        raise RuntimeError(f"Unknown system '{system}'")
    return res


def build_url(user, release):
    """Build the url to download from."""
    suffix = URL_SUFFIXES[find_platform()]

    if not user:
        user = "LuxCoreRender"

    url = (
        "https://github.com",
        user,
        "LuxCoreDeps",
        "releases",
        "download",
        release,
        f"luxcore-deps-{suffix}.zip"
    )

    return "/".join(url)


def get_profile_name():
    """Get the profile file name, based on platform."""
    return f"conan-profile-{find_platform()}"


@cache
def ensure_conan_app():
    """Ensure Conan is installed."""
    logger.info("Looking for conan")
    if not (res := shutil.which("conan")):
        logger.error("Conan not found!")
        sys.exit(1)
    logger.info("Conan found: '%s'", res)
    return res


def run_conan(args, **kwargs):
    """Run conan statement."""
    conan_app = ensure_conan_app()
    if "env" not in kwargs:
        kwargs["env"] = CONAN_ENV
    else:
        kwargs["env"].update(CONAN_ENV)
    kwargs["env"].update(os.environ)
    kwargs["text"] = kwargs.get("text", True)
    args = [conan_app] + args
    logger.debug(args)
    res = subprocess.run(args, shell=False, check=True, **kwargs)
    if res.returncode:
        logger.error("Error while executing conan")
        print(res.stdout)
        print(res.stderr)
        sys.exit(1)
    return res


def download(url, destdir):
    """Download file from url into destdir."""
    filename = destdir / "deps.zip"
    local_filename, _ = urlretrieve(url, filename=filename)
    with ZipFile(local_filename) as downloaded:
        downloaded.extractall(destdir)


def install(filename, destdir):
    """Install file from local directory into destdir."""
    logger.info("Importing %s", filename)
    with ZipFile(str(filename)) as zipfile:
        zipfile.extractall(destdir)


def conan_home():
    """Get Conan home path."""
    conan_app = ensure_conan_app()
    res = subprocess.run(
        [conan_app, "config", "home"],
        capture_output=True,
        text=True,
        check=True,
    )
    if res.returncode:
        logger.error("Error while executing conan")
        print(res.stdout)
        print(res.stderr)
        sys.exit(1)
    return Path(res.stdout.strip())


def copy_conf(dest):
    """Copy global.conf into conan tree."""
    home = conan_home()
    source = home / "global.conf"
    logger.info("Copying %s to %s", source, dest)
    shutil.copy(source, dest)



def main():
    """Entry point."""

    # Set-up logger
    logger.setLevel(logging.INFO)
    logging.basicConfig(level=logging.INFO)
    logger.info("BEGIN")

    # Get settings
    logger.info("Reading settings")
    with open("luxcore.json", encoding="utf-8") as f:
        settings = json.load(f)
    logger.info("Build directory: %s", BUILD_DIR)

    # Get optional command-line parameters
    parser = argparse.ArgumentParser()
    parser.add_argument("-l", "--local", type=Path)
    parser.add_argument("-v", "--verbose", action="store_true")
    args = parser.parse_args()
    if args.verbose:
        logger.setLevel(logging.DEBUG)

    # Process
    with tempfile.TemporaryDirectory() as tmpdir:

        tmpdir = Path(tmpdir)

        _conan_home = tmpdir / ".conan2"

        CONAN_ENV.update(
            {
                "CONAN_HOME": str(_conan_home),
                "GCC_VERSION": str(settings["Build"]["gcc"]),
                "CXX_VERSION": str(settings["Build"]["cxx"]),
                "BUILD_TYPE": "Release",  # TODO Command line parameter
            }
        )

        # Initialize
        url = build_url(
            settings["Dependencies"]["user"],
            settings["Dependencies"]["release"],
        )

        # Download and unzip
        if not args.local:
            logger.info("Downloading dependencies (url='%s')", url)
            download(url, tmpdir)

        # Clean
        logger.info("Cleaning local cache")
        res = run_conan(["remove", "-c", "*"], capture_output=True)
        for line in res.stderr.splitlines():
            logger.info(line)
        copy_conf(_conan_home)  # Copy global.conf in current conan home

        # Install
        logger.info("Installing")
        if not (local := args.local):
            archive = tmpdir / "conan-cache-save.tgz"
        else:
            archive = local
        res = run_conan(
            ["cache", "restore", archive],
            capture_output=True,
        )
        for line in res.stderr.splitlines():
            logger.info(line)

        # Check
        logger.info("Checking integrity")
        res = run_conan(["cache", "check-integrity", "*"], capture_output=True)
        logger.info("Integrity check: OK")

        # Installing profiles
        logger.info("Installing profiles")
        run_conan(
            ["config", "install-pkg", "luxcoreconf/2.10.0@luxcore/luxcore"]
        )  # TODO version as a param

        # Generate & deploy
        logger.info("Generating")
        main_block = [
            "install",
            "--build=missing",
            f"--profile:all={get_profile_name()}",
            "--deployer=full_deploy",
            f"--deployer-folder={BUILD_DIR}",
            f"--output-folder={CMAKE_DIR}",
            "--conf:all=tools.cmake.cmaketoolchain:generator=Ninja",
        ]
        statement = main_block + ["."]
        run_conan(statement)

    logger.info("END")


if __name__ == "__main__":
    main()
