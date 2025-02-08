# SPDX-FileCopyrightText: 2024 Howetuft
#
# SPDX-License-Identifier: Apache-2.0

import os
import tempfile
from urllib.request import urlretrieve
from pathlib import Path
from zipfile import ZipFile
import subprocess
import logging
import shutil
import textwrap
import argparse
import json

# TODO
URL = "https://github.com/howetuft/LuxCoreDeps/releases/download/v0.0.1/luxcore-deps-ubuntu-latest-2.10.zip"

# TODO
# Manage separate build folder


CONAN_APP = None

CONAN_ALL_PACKAGES = '"*"'

BUILD_DIR = os.getenv("BUILD_DIR", "./build")

CMAKE_DIR = Path(BUILD_DIR) / "cmake"

logger = logging.getLogger("LuxCoreDeps")

# TODO
VERSIONS = {
    "BOOST_VERSION": "1.84.0",
    "OIIO_VERSION": "2.5.16.0",
    "OCIO_VERSION": "2.4.0",
    "OIDN_VERSION": "2.3.1",
    "TBB_VERSION": "2021.12.0",
    "OPENEXR_VERSION": "3.3.2",
    "BLENDER_VERSION": "4.2.3",
    "OPENVDB_VERSION": "11.0.0",
    "SPDLOG_VERSION": "1.15.0",
    "EMBREE3_VERSION": "3.13.5",
    "FMT_VERSION": "11.0.2",
    "OPENSUBDIV_VERSION": "3.6.0",
    "JSON_VERSION": "3.11.3",
    "EIGEN_VERSION": "3.4.0",
    "ROBINHOOD_VERSION": "3.11.5",
    "MINIZIP_VERSION": "4.0.3",
    "PYBIND11_VERSION": "2.13.6",
    "CXX_VERSION": "20",
    "GCC_VERSION": "14",
    "BUILD_TYPE": "Release",
}


def ensure_conan_app():
    logger.info("Looking for conan")
    global CONAN_APP
    CONAN_APP = shutil.which("conan")
    logger.info(f"Conan found: '{CONAN_APP}'")

def ensure_conan_home():
    logger.info("Ensuring CONAN_HOME exists")
    global CONAN_HOME
    if not CONAN_HOME.exists():
        CONAN_HOME.mkdir()

    if not CONAN_HOME.is_dir():
        logger.critical("CONAN_HOME('{CONAN_HOME}') exists but is not a directory")
        exit(1)

    CONAN_HOME = CONAN_HOME.resolve()

    logger.info(f"CONAN_HOME exists at '{CONAN_HOME}' - OK")


def run_conan(args, **kwargs):
    conan_env = {"CONAN_HOME": CONAN_HOME}
    conan_env.update(VERSIONS)
    kwargs["env"] = conan_env
    kwargs["text"] = kwargs.get("text", True)
    args = [CONAN_APP] + args
    res = subprocess.run(args, **kwargs)
    if res.returncode:
        logger.critical("Error while executing conan")
        print(res.stdout)
        print(res.stderr)
        exit(1)
    return res


def download(url, destdir):
    """Download file from url into destdir."""
    filename = destdir / "deps.zip"
    local_filename, _ = urlretrieve(url, filename=filename)
    downloaded = ZipFile(local_filename)
    downloaded.extractall(destdir)


def install(filename, destdir):
    """Install file from local directory into destdir."""
    logger.info(f"Importing {filename}")
    zipfile = ZipFile(str(filename))
    zipfile.extractall(destdir)


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("-f", "--file", type=Path)
    args = parser.parse_args()


    logger.setLevel(logging.INFO)
    logging.basicConfig(level=logging.INFO)
    with tempfile.TemporaryDirectory() as tmpdir:

        tmpdir = Path(tmpdir)

        CONAN_HOME = tmpdir / ".conan2"

        # Initialize
        ensure_conan_app()

        # Download and unzip
        logger.info("Downloading dependencies")
        if not args.file:
            download(URL, tmpdir)
        else:
            install(args.file, tmpdir)

        # Clean
        logger.info("Cleaning local cache")
        res = run_conan(["remove", "-c", "*"], capture_output=True)
        for line in res.stderr.splitlines():
            logger.info(line)

        # Install
        logger.info("Installing")
        archive = tmpdir / "conan-cache-save.tgz"
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
        run_conan(["config", "install-pkg", "luxcoreconf/2.10.0@luxcore/luxcore"])  # TODO version as a param

        # Generate & deploy
        logger.info("Generating")
        run_conan([
            "install",
            "--requires=luxcoredeps/2.10.0@luxcore/luxcore",  # TODO version as a param
            "--build=missing",
            "--profile:all=conan-profile-Linux-X64",  # TODO compute profile name
            "--deployer=full_deploy",
            f"--deployer-folder={BUILD_DIR}",
            "--generator=CMakeToolchain",
            "--generator=CMakeDeps",
            f"--output-folder={CMAKE_DIR}",
        ])
