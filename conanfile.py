# SPDX-FileCopyrightText: 2024 Howetuft
#
# SPDX-License-Identifier: Apache-2.0

from pathlib import Path
import io
import os

from conan import ConanFile
from conan.tools.cmake import CMake, CMakeDeps, CMakeToolchain, cmake_layout
from conan.tools.env import VirtualBuildEnv


class LuxCore(ConanFile):
    name = "luxcore"
    version = "2.10.0"
    user = "luxcore"
    channel = "luxcore"

    requires = "luxcoredeps/2.10.0@luxcore/luxcore"
    tool_requires = "ninja/[*]"
    settings = "os", "compiler", "build_type", "arch"

    def generate(self):
        tc = CMakeToolchain(self)
        tc.absolute_paths = True
        tc.variables["CMAKE_COMPILE_WARNING_AS_ERROR"] = False

        # OIDN denoiser executable
        oidn = self.dependencies["luxcoredeps"].dependencies["oidn"]
        oidn_info = oidn.cpp_info
        oidn_version = oidn.ref.version
        oidn_bindir = Path(oidn_info.bindirs[0])
        if self.settings.os == "Windows":
            denoise_path = oidn_bindir / "oidnDenoise.exe"
        else:
            denoise_path = oidn_bindir / "oidnDenoise"
        tc.variables["LUX_OIDN_DENOISE_PATH"] = denoise_path.as_posix()

        # OIDN denoiser cpu (for Linux)
        oidn_libdir = Path(oidn_info.libdirs[0])
        tc.variables["LUX_OIDN_DENOISE_LIBS"] = oidn_libdir.as_posix()
        tc.variables["LUX_OIDN_DENOISE_BINS"] = oidn_bindir.as_posix()
        tc.variables["LUX_OIDN_VERSION"] = oidn_version
        if self.settings.os == "Linux":
            denoise_cpu = (
                oidn_libdir / f"libOpenImageDenoise_device_cpu.so.{oidn_version}"
            )
        elif self.settings.os == "Windows":
            denoise_cpu = oidn_bindir / "OpenImageDenoise_device_cpu.dll"
        elif self.settings.os == "Macos":
            denoise_cpu = (
                oidn_libdir / f"OpenImageDenoise_device_cpu.{oidn_version}.pylib"
            )
        tc.variables["LUX_OIDN_DENOISE_CPU"] = denoise_cpu.as_posix()

        # nvrtc
        if self.settings.os in ("Linux", "Windows"):
            nvrtc = self.dependencies["luxcoredeps"].dependencies["nvrtc"]
            nvrtc_info = nvrtc.cpp_info
            if self.settings.os == "Linux":
                nvrtc_dir = Path(nvrtc_info.libdirs[0])
            else:
                nvrtc_dir = Path(nvrtc_info.bindirs[0])
                tc.cache_variables["LUX_NVRTC_BINS"] = nvrtc_dir

            nvrtc_libs = [
                f.as_posix()
                for f in nvrtc_dir.iterdir()
                if f.is_file()
            ]
            tc.cache_variables["LUX_NVRTC"] = ";".join(nvrtc_libs)


        # CMAKE_OSX_ARCHITECTURES
        if self.settings.os == "Macos" and self.settings.arch == "armv8":
            tc.cache_variables["CMAKE_OSX_ARCHITECTURES"] = "arm64"


        # Bison/Flex
        # FLEX_EXECUTABLE and BISON_EXECUTABLE are recognized by
        # standard CMake FindFlex and FindBison
        if self.settings.os == "Windows":
            # winflexbison
            winflexbison = self.dependencies["luxcoredeps"].dependencies["winflexbison"]

            lex_path = os.path.join(winflexbison.package_folder, "bin", "win_flex.exe").replace("\\", "/")
            tc.cache_variables["FLEX_EXECUTABLE"] = lex_path

            yacc_path = os.path.join(winflexbison.package_folder, "bin", "win_bison.exe").replace("\\", "/")
            tc.cache_variables["BISON_EXECUTABLE"] = yacc_path
        else:
            # *nix Flex and Bison
            # CMake variables for CMake to find Bison & Flex (relying on
            # standard FindBison and FindFlex
            flex = self.dependencies["luxcoredeps"].dependencies["flex"]
            flex_path = os.path.join(flex.package_folder, "bin", "flex").replace("\\", "/")
            tc.cache_variables["FLEX_EXECUTABLE"] = flex_path

            bison = self.dependencies["luxcoredeps"].dependencies["bison"]
            bison_path = os.path.join(bison.package_folder, "bin", "bison").replace("\\", "/")
            tc.cache_variables["BISON_EXECUTABLE"] = bison_path
            bison.cpp_info.set_property("cmake_find_mode", "none")  # Force use of standard CMake FindBISON

            # Environment variables for Bison/Flex/m4 to work together
            buildenv = VirtualBuildEnv(self)

            bison_root = bison.package_folder.replace("\\", "/")
            buildenv.environment().define_path("CONAN_BISON_ROOT", bison_root)

            pkgdir = os.path.join(bison.package_folder, "res", "bison")
            buildenv.environment().define_path("BISON_PKGDATADIR", pkgdir)

            # https://github.com/conda-forge/bison-feedstock/issues/7
            m4 = self.dependencies["luxcoredeps"].dependencies["m4"]
            m4_path = os.path.join(m4.package_folder, "bin", "m4").replace("\\", "/")
            tc.cache_variables["CONAN_M4_PATH"] = m4_path
            buildenv.environment().define_path("M4", m4_path)


            buildenv.generate()
            tc.presets_build_environment = buildenv.environment()

        tc.cache_variables["SPDLOG_FMT_EXTERNAL_HO"] = True

        tc.generate()

        cd = CMakeDeps(self)

        cd.generate()

    def layout(self):
        cmake_layout(self, generator="Ninja")
