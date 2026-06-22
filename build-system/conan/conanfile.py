# SPDX-FileCopyrightText: 2024-2025 Authors (see AUTHORS.txt)
#
# SPDX-License-Identifier: Apache-2.0

"""Conan recipe for LuxCore."""


from pathlib import Path
import os
import json

from conan import ConanFile
from conan.tools.cmake import CMakeDeps, CMakeToolchain, cmake_layout
from conan.tools.env import VirtualBuildEnv

THIS_FILE = Path(__file__)
BUILD_SETTINGS_FILE = THIS_FILE.parent / ".." / "build-settings.json"

with open(BUILD_SETTINGS_FILE, encoding="utf-8") as config:
    config_json = json.load(config)
    LUXDEPS_VERSION = config_json["Dependencies"]["release"]


class LuxCore(ConanFile):
    """Conan recipe."""

    name = "luxcore"
    version = "2.11.0-a.4"
    user = "luxcore"
    channel = "luxcore"

    tool_requires = "ninja/[*]"
    settings = "os", "compiler", "build_type", "arch"

    options = {
        "deps_version": ["ANY"]  # Override json file (for tests)
    }
    default_options = {
        "deps_version": ""
    }

    def requirements(self):
        luxversion = self.options.deps_version or LUXDEPS_VERSION
        self.requires(
            f"luxcoredeps/{luxversion}@luxcore/luxcore"
        )

    def _generate_oidn(self, toolchain):
        """Generate toolchain part related to oidn."""
        self_settings_os = self.settings.os  # pylint: disable=no-member

        # OIDN denoiser executable
        oidn = self.dependencies["luxcoredeps"].dependencies["oidn"]
        oidn_version = oidn.ref.version
        oidn_bindir = Path(oidn.cpp_info.bindirs[0])
        if self_settings_os == "Windows":
            denoise_path = oidn_bindir / "oidnDenoise.exe"
        else:
            denoise_path = oidn_bindir / "oidnDenoise"
        toolchain.variables["LUX_OIDN_DENOISE_PATH"] = denoise_path.as_posix()

        # OIDN denoiser cpu (for Linux)
        oidn_libdir = Path(oidn.cpp_info.libdirs[0])
        toolchain.variables["LUX_OIDN_DENOISE_LIBS"] = oidn_libdir.as_posix()
        toolchain.variables["LUX_OIDN_DENOISE_BINS"] = oidn_bindir.as_posix()
        toolchain.variables["LUX_OIDN_VERSION"] = oidn_version
        if self_settings_os == "Linux":
            denoise_cpu = (
                oidn_libdir
                / f"libOpenImageDenoise_device_cpu.so.{oidn_version}"
            )
            denoise_core = (
                oidn_libdir / f"libOpenImageDenoise_core.so.{oidn_version}"
            )
        elif self_settings_os == "Windows":
            denoise_cpu = oidn_bindir / "OpenImageDenoise_device_cpu.dll"
            denoise_core = oidn_bindir / "OpenImageDenoise_core.dll"
        elif self_settings_os == "Macos":
            denoise_cpu = (
                oidn_libdir
                / f"OpenImageDenoise_device_cpu.{oidn_version}.pylib"
            )
            denoise_core = (
                oidn_libdir / f"OpenImageDenoise_core.{oidn_version}.pylib"
            )
        else:
            raise RuntimeError(f"OIDN: Unhandled os ({self_settings_os})")
        toolchain.variables["LUX_OIDN_DEVICE_CPU"] = denoise_cpu.as_posix()
        toolchain.variables["LUX_OIDN_CORE"] = denoise_core.as_posix()

    def _generate_nvrtc(self, toolchain):
        """Generate toolchain part related to nvrtc."""
        if not (
            self_settings_os := self.settings.os  # pylint: disable=no-member
        ) in (
            "Linux",
            "Windows",
        ):
            return

        nvrtc = self.dependencies["luxcoredeps"].dependencies["nvrtc"]
        nvrtc_info = nvrtc.cpp_info
        if self_settings_os == "Linux":
            nvrtc_dir = Path(nvrtc_info.libdirs[0])
        else:
            nvrtc_dir = Path(nvrtc_info.bindirs[0])
            toolchain.cache_variables["LUX_NVRTC_BINS"] = nvrtc_dir.as_posix()

        nvrtc_libs = [f.as_posix() for f in nvrtc_dir.iterdir() if f.is_file()]
        toolchain.cache_variables["LUX_NVRTC"] = ";".join(nvrtc_libs)

    def _generate_bison_flex(self, toolchain):
        """Generate toolchain part related to bison and flex."""
        # FLEX_EXECUTABLE and BISON_EXECUTABLE are recognized by
        # standard CMake FindFlex and FindBison
        if self.settings.os == "Windows":  # pylint: disable=no-member
            # winflexbison
            winflexbison = self.dependencies["luxcoredeps"].dependencies[
                "winflexbison"
            ]

            lex_path = os.path.join(
                winflexbison.package_folder, "bin", "win_flex.exe"
            ).replace("\\", "/")
            toolchain.cache_variables["FLEX_EXECUTABLE"] = lex_path

            yacc_path = os.path.join(
                winflexbison.package_folder, "bin", "win_bison.exe"
            ).replace("\\", "/")
            toolchain.cache_variables["BISON_EXECUTABLE"] = yacc_path
        else:
            # *nix Flex and Bison
            # CMake variables for CMake to find Bison & Flex (relying on
            # standard FindBison and FindFlex
            flex = self.dependencies["luxcoredeps"].dependencies["flex"]
            flex_path = os.path.join(
                flex.package_folder, "bin", "flex"
            ).replace("\\", "/")
            toolchain.cache_variables["FLEX_EXECUTABLE"] = flex_path

            bison = self.dependencies["luxcoredeps"].dependencies["bison"]
            bison_path = os.path.join(
                bison.package_folder, "bin", "bison"
            ).replace("\\", "/")
            toolchain.cache_variables["BISON_EXECUTABLE"] = bison_path
            bison.cpp_info.set_property(
                "cmake_find_mode", "none"
            )  # Force use of standard CMake FindBISON

            # Environment variables for Bison/Flex/m4 to work together
            buildenv = VirtualBuildEnv(self)

            bison_root = bison.package_folder.replace("\\", "/")
            buildenv.environment().define_path("CONAN_BISON_ROOT", bison_root)

            pkgdir = os.path.join(bison.package_folder, "res", "bison")
            buildenv.environment().define_path("BISON_PKGDATADIR", pkgdir)

            # https://github.com/conda-forge/bison-feedstock/issues/7
            m4_path = os.path.join(
                self.dependencies["luxcoredeps"]
                .dependencies["m4"]
                .package_folder,
                "bin",
                "m4",
            ).replace("\\", "/")
            toolchain.cache_variables["CONAN_M4_PATH"] = m4_path
            buildenv.environment().define_path("M4", m4_path)

            buildenv.generate()
            toolchain.presets_build_environment = buildenv.environment()

    def generate(self):
        """Generate toolchain and dependencies."""
        toolchain = CMakeToolchain(self)
        toolchain.absolute_paths = True

        # OIDN
        self._generate_oidn(toolchain)

        # Nvidia runtime compiler (nvrtoolchain)
        self._generate_nvrtc(toolchain)

        # Bison/Flex
        self._generate_bison_flex(toolchain)

        # Variables
        self_settings_os = self.settings.os  # pylint: disable=no-member
        self_settings_arch = self.settings.arch  # pylint: disable=no-member

        toolchain.variables["CMAKE_COMPILE_WARNING_AS_ERROR"] = False
        if self_settings_os == "Macos" and self_settings_arch == "armv8":
            toolchain.cache_variables["CMAKE_OSX_ARCHITECTURES"] = "arm64"
        toolchain.cache_variables["SPDLOG_FMT_EXTERNAL_HO"] = True
        toolchain.cache_variables["LUXCOREDEPS_VERSION"] = str(
            self.dependencies["luxcoredeps"].ref.version
        )

        toolchain.generate()

        deps = CMakeDeps(self)
        deps.generate()

    def layout(self):
        """Define package layout."""
        # Mandatory to get a CMakeUserPresets.json, see
        # https://docs.conan.io/2/reference/tools/cmake/cmaketoolchain.html
        cmake_layout(
            self, src_folder=Path("..", ".."), generator="Ninja Multi-Config"
        )
