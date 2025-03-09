# ****************************************************************************
# *                                                                          *
# * Copyright 2024 Howetuft <howetuft-at-gmail-dot-com>                      *
# *                                                                          *
# * Licensed under the Apache License, Version 2.0 (the "License");          *
# * you may not use this file except in compliance with the License.         *
# * You may obtain a copy of the License at                                  *
# *                                                                          *
# * http://www.apache.org/licenses/LICENSE-2.0                               *
# *                                                                          *
# * Unless required by applicable law or agreed to in writing, software      *
# * distributed under the License is distributed on an "AS IS" BASIS,        *
# * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. *
# * See the License for the specific language governing permissions and      *
# * limitations under the License.                                           *
# *                                                                          *
# ****************************************************************************

import platform
from pathlib import Path
import shutil

import importlib
from ctypes import CDLL, RTLD_GLOBAL, byref, c_int, ARRAY

from .pyluxcore import *

_LUXFOLDER = Path(pyluxcore.__file__).parent

_OIDN_PATHS = {
    "Linux": (_LUXFOLDER / ".." / "pyluxcore.oidn", "oidnDenoise"),
    "Windows": (_LUXFOLDER / ".." / "pyluxcore.libs", "oidnDenoise.exe"),
    "Darwin": (_LUXFOLDER / ".." / "pyluxcore.oidn", "oidnDenoise"),
}

CUDA_ARCHS = []
NVRTC_VERSION_MAJOR = None
NVRTC_VERSION_MINOR = None


def which_oidn():
    """Retrieve external oidn path (applying which).

    Returns path only if oidn is there and executable, None otherwise.
    """
    path, executable = _OIDN_PATHS[platform.system()]
    denoiser_path = shutil.which(executable, path=path)
    return denoiser_path


def path_to_oidn():
    """Retrieve external oidn path.

    Just return theoretical path, do not check if oidn is there, nor if
    it is executable.
    """
    path, executable = _OIDN_PATHS[platform.system()]
    return path / executable


def preload_nvrtc():
    """Preload nvidia runtime compiler."""
    # Find Python module
    try:
        nvrtc_mod = importlib.import_module("nvidia.cuda_nvrtc")
    except ModuleNotFoundError:
        print("nvrtc: Python module not found")
        return None, None, []
    modpath = Path(nvrtc_mod.__path__[0])
    # print(f"Found nvrtc Python module at {modpath}")

    # Try to load lib
    if platform.system() == "Linux":
        libpath = modpath / "lib" / "libnvrtc.so.12"
    elif platform.system() == "Windows":
        libpath = modpath / "bin" / "nvrtc64_120_0.dll"
    else:
        return None, None, []

    try:
        nvrtc = CDLL(str(libpath), RTLD_GLOBAL)
    except OSError:
        print("nvrtc: could not load library")
        return None, None, []

    # Check version
    major = c_int()
    minor = c_int()
    nvrtc.nvrtcVersion(byref(major), byref(minor))

    # Check architectures
    num_archs = c_int()
    nvrtc.nvrtcGetNumSupportedArchs(byref(num_archs))
    nvrtc.nvrtcGetSupportedArchs.argtypes = [
        ARRAY(c_int, num_archs.value),
    ]
    archs = ARRAY(c_int, num_archs.value)()
    nvrtc.nvrtcGetSupportedArchs(archs)

    return major, minor, archs


NVRTC_VERSION_MAJOR, NVRTC_VERSION_MINOR, CUDA_ARCHS = preload_nvrtc()
