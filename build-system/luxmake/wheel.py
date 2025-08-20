# SPDX-FileCopyrightText: 2025 Authors (see AUTHORS.txt)
#
# SPDX-License-Identifier: Apache-2.0

"""Make wheel command."""

import sys
import json
import subprocess
import tempfile
import shutil
import platform
import os
import shlex
from pathlib import Path

from .constants import SOURCE_DIR, INSTALL_DIR, BINARY_DIR, WHEELHOUSE_DIR, WHEEL_HOOK
from .utils import logger, pack, fail, Colors
from .build import build_and_install
from .windows import win_recompose

_WHEEL_SNIPPET = """\
Wheel-Version: 1.0
Generator: fake 0.0.0
Root-Is-Purelib: false
Tag: {}
"""

_METADATA_SNIPPET = """\
Metadata-Version: 2.2
Name: pyluxcore
Version: {}
Summary: LuxCore Python bindings
Keywords: raytracing,ray tracing,rendering,pbr,physical based rendering,path tracing
Author: LuxCoreRender
Requires-Python: >=3.9
Requires-Dist: numpy>=2
Requires-Dist: nvidia-cuda-nvrtc-cu12; sys_platform != "darwin"
"""


def _compute_platform_tag():
    """Compute tag.

    This tag may not be totally correct. Do not use in production.
    https://packaging.python.org/en/latest/specifications/platform-compatibility-tags
    """
    system, machine = platform.system(), platform.machine()
    if system == "Linux":
        return "linux_x86_64"
    if system == "Windows":
        return "win_amd64"
    if system == "Darwin" and machine == "x86_64":
        return "macosx_13_0"
    if system == "Darwin" and machine == "arm64":
        return "macosx_14_2"

    # Failed:
    return fail("Unknown platform/system: '%s' / '%s'", platform, machine)


def _get_lib_paths():
    """Get library paths for dependencies."""
    base = BINARY_DIR / "dependencies" / "full_deploy" / "host"
    paths = (str(p.absolute()) for p in base.rglob("**/bin"))
    result = os.pathsep.join(paths)
    return result


def make_wheel(args):
    """Build a wheel."""
    logger.warning(
        f"{Colors.WARNING}"
        "This command builds a TEST wheel, "
        "not fully compliant to standard "
        "and only intended for test. "
        "DO NOT USE IN PRODUCTION."
        f"{Colors.ENDC}"
    )
    # Build and install pyluxcore
    args.target = "pyluxcore"
    build_and_install(args)

    # Compute version
    build_settings_file = Path("build-system", "build-settings.json")
    with open(build_settings_file, encoding="utf-8") as in_file:
        default_version = json.load(in_file)["DefaultVersion"]
    version = ".".join(default_version[i] for i in ("major", "minor", "patch"))

    # Compute tag
    vinfo = sys.version_info
    python_tag = f"cp{vinfo.major}{vinfo.minor}"
    abi_tag = python_tag
    platform_tag = _compute_platform_tag()
    tag = f"{python_tag}-{abi_tag}-{platform_tag}"

    logger.info("Making wheel for version '%s' and tag '%s'", version, tag)
    with (
        tempfile.TemporaryDirectory() as wheeltree,
        tempfile.TemporaryDirectory() as raw_wheel,
    ):
        # We create an install tree, with all the wheel components, then we
        # pack it into a raw wheel and eventually we repair it.
        #
        # Additionnally, on Windows, we recompose the wheel (restablishing
        # oidnDenoise.exe and OpenImageDenoise_device_cpu.dll)

        # Set destination folders
        wheeltree = Path(wheeltree)
        raw_wheel_dir = Path(raw_wheel)

        # Create dist-info folder
        dist_info = wheeltree / f"pyluxcore-{version}.dist-info"
        dist_info.mkdir(exist_ok=True)

        # Export WHEEL file
        with open(dist_info / "WHEEL", "w", encoding="utf-8") as f:
            f.write(_WHEEL_SNIPPET.format(tag))

        # Export METADATA file
        with open(dist_info / "METADATA", "w", encoding="utf-8") as f:
            f.write(_METADATA_SNIPPET.format(version))

        # Copy subfolders into tree
        shutil.copytree(
            SOURCE_DIR / "python" / "pyluxcore",
            wheeltree / "pyluxcore",
            dirs_exist_ok=True,
        )
        shutil.copytree(
            INSTALL_DIR / "pyluxcore",
            wheeltree / "pyluxcore",
            dirs_exist_ok=True,
        )
        shutil.copytree(
            INSTALL_DIR / "pyluxcore.libs",
            wheeltree / "pyluxcore.libs",
            dirs_exist_ok=True,
        )
        shutil.copytree(
            INSTALL_DIR / "pyluxcore.oidn",
            wheeltree / "pyluxcore.oidn",
            dirs_exist_ok=True,
        )

        # Pack wheel
        logger.info("Packing wheel")
        pack(wheeltree, raw_wheel)
        wheelname = f"pyluxcore-{version}-{tag}.whl"

        # Then repair
        wheel_lib_dir = INSTALL_DIR / "lib"
        logger.info("Repairing wheel")
        input_path = raw_wheel_dir / wheelname
        cmd = [
            sys.executable,
            "-m",
            "repairwheel",
            "-l",
            wheel_lib_dir,
            "-l",
            _get_lib_paths(),
            "-o",
            WHEELHOUSE_DIR,
            input_path,
        ]
        try:
            result = subprocess.check_output(cmd, text=True)
        except subprocess.CalledProcessError as err:
            fail(err)
        logger.info(result)

        # And, for Windows, recompose
        if platform.system() == "Windows":
            args.wheel = WHEELHOUSE_DIR / wheelname
            win_recompose(args)

        # Finally, execute hook if exists
        if WHEEL_HOOK:
            logger.info("Executing hook: " + WHEEL_HOOK)
            try:
                result = subprocess.check_output(
                    shlex.split(WHEEL_HOOK), text=True
                )
            except subprocess.CalledProcessError as err:
                fail(err)
            logger.info(result)
