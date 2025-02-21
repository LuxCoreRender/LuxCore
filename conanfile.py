# SPDX-FileCopyrightText: 2024 Howetuft
#
# SPDX-License-Identifier: Apache-2.0

from conan import ConanFile

from conan.tools.cmake import CMake, CMakeDeps, CMakeToolchain, cmake_layout
from pathlib import Path

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
        tc.preprocessor_definitions["OIIO_STATIC_DEFINE"] = True
        tc.variables["CMAKE_COMPILE_WARNING_AS_ERROR"] = False

        # OIDN denoiser executable
        oidn = self.dependencies["luxcoredeps"].dependencies["oidn"]
        oidn_info = oidn.cpp_info
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
        tc.variables["LUX_OIDN_VERSION"] = oidn.ref.version
        if self.settings.os == "Linux":
            denoise_cpu = (
                oidn_libdir / f"libOpenImageDenoise_device_cpu.so.{OIDN_VERSION}"
            )
        elif self.settings.os == "Windows":
            denoise_cpu = oidn_bindir / "OpenImageDenoise_device_cpu.dll"
        elif self.settings.os == "Macos":
            denoise_cpu = (
                oidn_libdir / f"OpenImageDenoise_device_cpu.{OIDN_VERSION}.pylib"
            )
        tc.variables["LUX_OIDN_DENOISE_CPU"] = denoise_cpu.as_posix()

        if self.settings.os == "Macos" and self.settings.arch == "armv8":
            tc.cache_variables["CMAKE_OSX_ARCHITECTURES"] = "arm64"

        if self.settings.os == "Macos":
            buildenv = VirtualBuildEnv(self)

            bisonbrewpath = io.StringIO()
            self.run("brew --prefix bison", stdout=bisonbrewpath)
            bison_root = os.path.join(bisonbrewpath.getvalue().rstrip(), "bin")
            buildenv.environment().define("BISON_ROOT", bison_root)

            flexbrewpath = io.StringIO()
            self.run("brew --prefix flex", stdout=flexbrewpath)
            flex_root = os.path.join(flexbrewpath.getvalue().rstrip(), "bin")
            buildenv.environment().define("FLEX_ROOT", flex_root)

            buildenv.generate()
            tc.presets_build_environment = buildenv.environment()

        tc.cache_variables["SPDLOG_FMT_EXTERNAL_HO"] = True

        tc.generate()

        cd = CMakeDeps(self)

        cd.generate()
