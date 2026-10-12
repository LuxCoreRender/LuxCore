# SPDX-FileCopyrightText: 2026 Authors (see AUTHORS.txt)
#
# SPDX-License-Identifier: Apache-2.0

"""Spirv command.

Bake the OpenCL kernels in SPIR-V format, using the dependency-free
standalone project (build-system/spirv): no LuxCoreDeps, no conan, no
'make deps' needed, only the system C++ compiler and a way to compile
OpenCL C in SPIR-V format (a clang including the SPIR-V back-end, or - on
platforms whose clang builds lack it, e.g. Windows - any clang plus the
llvm-spirv tool, i.e. the SPIRV-LLVM-Translator).

The baked modules are used by the main build (see the config command and
the LUXRAYS_SPIRV_BAKED_DIR CMake variable): the SPIR-V format being
platform independent, they are embedded in the binaries as they are.

See docs/SPIRV_PRECOMPILATION.md for a complete how-to (in particular
under Windows, where the clang builds lack the SPIR-V back-end and the
llvm-spirv tool - shipped e.g. by MSYS2 or Intel oneAPI - is needed).
"""

from .constants import PARAMS
from .utils import run_cmake, fail, logger


def spirv_baked_dir():
    """Return the directory where the spirv command bakes the modules."""
    return PARAMS.BINARY_DIR / "spirv-build" / "baked"


def spirv(
    _,  # args (unused)
):
    """Bake the OpenCL kernels in SPIR-V format."""
    bake_build_dir = PARAMS.BINARY_DIR / "spirv-build"

    # Configure and build the standalone bake project
    run_cmake(
        [
            f"-S {PARAMS.SOURCE_DIR / 'build-system' / 'spirv'}",
            f"-B {bake_build_dir}",
            "-G Ninja",
        ]
    )
    run_cmake(
        [
            "--build",
            str(bake_build_dir),
        ]
    )

    # Check that the modules have actually been baked
    baked_dir = spirv_baked_dir()
    manifest = baked_dir / "spirvmanifest.txt"
    if not manifest.exists():
        fail(
            "SPIR-V modules have not been baked (missing '%s')",
            str(manifest.absolute()),
        )

    logger.info(
        "OpenCL kernels baked in SPIR-V format in '%s'",
        str(baked_dir.absolute()),
    )
    logger.info(
        "They will be used by the next 'make config' "
        "(see the LUXRAYS_SPIRV_BAKED_DIR CMake variable)"
    )


def get_pre_baked_dir():
    """Return the directory with the pre-baked SPIR-V modules to use in
    the main build, or None if not available.

    The directory is the one filled by the 'spirv' command. The check
    requires the manifest and, for each entry, the related module: the
    main build refuses (i.e. CMake FATAL_ERROR) incomplete directories.
    """
    baked_dir = spirv_baked_dir()
    manifest = baked_dir / "spirvmanifest.txt"
    if not manifest.is_file():
        return None

    entries = manifest.read_text(encoding="utf-8").splitlines()
    if not entries:
        return None
    for entry in entries:
        module = baked_dir / f"{entry.split()[0]}.spv"
        if not module.is_file():
            logger.warning(
                "Ignoring the pre-baked SPIR-V modules in '%s': "
                "missing '%s' (re-run 'make spirv')",
                str(baked_dir),
                module.name,
            )
            return None

    return baked_dir
