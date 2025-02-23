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

        if self.settings.os == "Macos" and self.settings.arch == "armv8":
            tc.cache_variables["CMAKE_OSX_ARCHITECTURES"] = "arm64"

        # Bison/Flex
        # FLEX_EXECUTABLE and BISON_EXECUTABLE are recognized by
        # standard FindFlex and FindBison
        if self.settings.os == "Windows":
            # winflexbison
            winflexbison = self.dependencies["luxcoredeps"].dependencies["winflexbison"]

            lex_path = os.path.join(winflexbison.package_folder, "bin", "win_flex.exe").replace("\\", "/")
            tc.cache_variables["FLEX_EXECUTABLE"] = lex_path

            yacc_path = os.path.join(winflexbison.package_folder, "bin", "win_bison.exe").replace("\\", "/")
            tc.cache_variables["BISON_EXECUTABLE"] = yacc_path
        # else:
            # # *nix Flex and Bison
            # flex = self.dependencies["flex"]
            # flex_path = os.path.join(flex.package_folder, "bin", "flex").replace("\\", "/")
            # tc.cache_variables["FLEX_EXECUTABLE"] = flex_path


            # bison = self.dependencies["bison"]
            # bison_path = os.path.join(bison.package_folder, "bin", "bison").replace("\\", "/")
            # tc.cache_variables["BISON_EXECUTABLE"] = bison_path
            # bison.cpp_info.set_property("cmake_find_mode", "none")  # Force use of standard CMake FindBISON

            # # *nix Flex and Bison
            # flex = self.dependencies["luxcoredeps"].dependencies["flex"]
            # flex_path = os.path.join(flex.package_folder, "bin", "flex").replace("\\", "/")
            # tc.cache_variables["FLEX_EXECUTABLE"] = flex_path


            # bison = self.dependencies["luxcoredeps"].dependencies["bison"]
            # bison_path = os.path.join(bison.package_folder, "bin", "bison").replace("\\", "/")
            # tc.cache_variables["BISON_EXECUTABLE"] = bison_path
            # bison.cpp_info.set_property("cmake_find_mode", "none")  # Force use of standard CMake FindBISON

            # bison_root = bison.package_folder.replace("\\", "/")
            # self.buildenv_info.define_path("CONAN_BISON_ROOT", bison_root)

            # pkgdir = os.path.join(bison.package_folder, "res", "bison")
            # self.buildenv_info.define_path("BISON_PKGDATADIR", pkgdir)

            # # yacc is a shell script, so requires a shell (such as bash)
            # yacc = os.path.join(bison.package_folder, "bin", "yacc").replace("\\", "/")
            # self.conf_info.define("user.bison:yacc", yacc)

        tc.cache_variables["SPDLOG_FMT_EXTERNAL_HO"] = True

        tc.generate()

        cd = CMakeDeps(self)

        cd.generate()
