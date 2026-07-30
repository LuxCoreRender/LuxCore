"""Build a self-contained Windows runtime directory for camera_controller.py."""

from __future__ import annotations

import shutil
import site
import sys
from pathlib import Path


UI_DIR = Path(__file__).resolve().parent
ROOT_DIR = UI_DIR.parent
DIST_DIR = UI_DIR / "dist" / "LuxCoreController"
PYTHON_HOME = Path(sys.executable).resolve().parent
USER_SITE = Path(site.getusersitepackages())
PYLUXCORE_SOURCE = ROOT_DIR / "out" / "build" / "src" / "pyluxcore" / "Release"
LUXCORE_BIN_SOURCE = ROOT_DIR / "out" / "install" / "Release" / "bin"
CUDA_BIN = Path(r"C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v12.4\bin")
ICON_SOURCE = Path(r"D:\nPowerSoftware.com\NewImages\HexigonLogoFLat.png")

RUNTIME_FILES = (
    "camera_controller.py",
    "scene.scn",
    "render.cfg",
    "hdre_055.hdr",
)
CUDA_FILES = ("nvrtc64_120_0.dll", "nvrtc-builtins64_124.dll")


def copy_file(source: Path, destination: Path) -> None:
    if not source.is_file():
        raise FileNotFoundError(f"Required file is unavailable: {source}")
    destination.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(source, destination)


def copy_tree(source: Path, destination: Path) -> None:
    if not source.is_dir():
        raise FileNotFoundError(f"Required directory is unavailable: {source}")
    shutil.copytree(
        source, destination, dirs_exist_ok=True,
        ignore=shutil.ignore_patterns("__pycache__", "*.pyc"))


def copy_user_package(package_name: str) -> None:
    source = USER_SITE / package_name
    destination = DIST_DIR / "python" / "site-packages" / package_name
    if source.is_dir():
        copy_tree(source, destination)
    else:
        copy_file(source, destination)


def copy_matching_user_packages(pattern: str) -> None:
    for source in USER_SITE.glob(pattern):
        copy_user_package(source.name)


def write_launchers() -> None:
    (DIST_DIR / "StartLuxCore.bat").write_text(
        """@echo off
cd /d "%~dp0"
start "" "%~dp0python\\pythonw.exe" "%~dp0camera_controller.py"
""",
        encoding="ascii",
        newline="\r\n")
    (DIST_DIR / "StartLuxCore.vbs").write_text(
        '''Dim shell, fso, here, pythonw, controller
Set shell = CreateObject("WScript.Shell")
Set fso = CreateObject("Scripting.FileSystemObject")
here = fso.GetParentFolderName(WScript.ScriptFullName)
pythonw = fso.BuildPath(here, "python\\pythonw.exe")
controller = fso.BuildPath(here, "camera_controller.py")
shell.CurrentDirectory = here
shell.Run """" & pythonw & """" & " """ & controller & """", 0, False
''',
        encoding="ascii",
        newline="\r\n")


def write_readme() -> None:
    (DIST_DIR / "RUN.txt").write_text(
        """LuxCore interactive render controller

Run StartLuxCore.vbs for a windowless launch, or StartLuxCore.bat to retain
the transient command window. Both launch scripts use the bundled Python
runtime and do not require this LuxCore source tree or a system Python install.

Requirements:
- Windows x64
- A compatible NVIDIA display driver with OptiX support

The package includes LuxCore native libraries, the Python binding, Python
runtime, Pillow, NumPy, OpenEXR, TkDND, and CUDA 12.4 NVRTC. Generated settings,
logs, and rendered films are written beside this file.
""",
        encoding="utf-8",
        newline="\r\n")


def main() -> None:
    if DIST_DIR.exists():
        shutil.rmtree(DIST_DIR)
    DIST_DIR.mkdir(parents=True)

    for file_name in RUNTIME_FILES:
        copy_file(UI_DIR / file_name, DIST_DIR / file_name)
    if ICON_SOURCE.is_file():
        copy_file(ICON_SOURCE, DIST_DIR / ICON_SOURCE.name)

    # Copy the interpreter tree so Python, tkinter, and Pillow remain local.
    copy_tree(PYTHON_HOME, DIST_DIR / "python")

    # These packages are installed in the per-user site-packages directory.
    for package_name in ("numpy", "numpy.libs", "tkinterdnd2"):
        copy_user_package(package_name)
    for file_name in ("Imath.py", "OpenEXR.cp314-win_amd64.pyd"):
        copy_user_package(file_name)
    for pattern in (
            "numpy-*.dist-info", "openexr-*.dist-info",
            "tkinterdnd2-*.dist-info"):
        copy_matching_user_packages(pattern)

    copy_file(
        PYLUXCORE_SOURCE / "pyluxcore.pyd",
        DIST_DIR / "runtime" / "pyluxcore" / "pyluxcore.pyd")
    copy_tree(LUXCORE_BIN_SOURCE, DIST_DIR / "runtime" / "bin")
    for file_name in CUDA_FILES:
        copy_file(CUDA_BIN / file_name, DIST_DIR / "runtime" / "cuda" / file_name)

    write_launchers()
    write_readme()
    print(DIST_DIR)


if __name__ == "__main__":
    main()
