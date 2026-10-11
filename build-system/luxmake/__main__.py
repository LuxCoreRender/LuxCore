# SPDX-FileCopyrightText: 2025 Authors (see AUTHORS.txt)
#
# SPDX-License-Identifier: Apache-2.0

"""This script wraps cmake calls for LuxCore build.

It is intended to get the same behaviour between Windows and *nix os
"""

import argparse

from .utils import set_logger_verbose
from .deps import deps
from .presets import list_presets
from .config import config
from .build import build_and_install, install
from .wheel import make_wheel
from .clear import clean, clear
from .spirv import spirv
from .windows import win_recompose


def main():
    """Entry point."""
    # Get command-line parameters
    parser = argparse.ArgumentParser(
        prog="make",
        add_help=False,
    )
    parser.add_argument(
        "-v",
        "--verbose",
        action="store_true",
        help=argparse.SUPPRESS,
    )
    subparsers = parser.add_subparsers()

    # Dependencies
    parser_deps = subparsers.add_parser("deps")
    parser_deps.set_defaults(func=deps)

    # List Presets
    parser_presets = subparsers.add_parser("list-presets")
    parser_presets.set_defaults(func=list_presets)

    # Config
    parser_config = subparsers.add_parser("config")
    parser_config.set_defaults(func=config)

    # Build
    parser_build_and_install = subparsers.add_parser("build-and-install")
    parser_build_and_install.add_argument("target")
    parser_build_and_install.set_defaults(func=build_and_install)

    # Install
    parser_install = subparsers.add_parser("install")
    parser_install.add_argument("target")
    parser_install.set_defaults(func=install)

    # Wheel
    parser_wheel = subparsers.add_parser("wheel-test")
    parser_wheel.set_defaults(func=make_wheel)

    # Windows wheel recomposing
    parser_wheel = subparsers.add_parser("win-recompose")
    parser_wheel.add_argument("wheel")
    parser_wheel.set_defaults(func=win_recompose)

    # Spirv
    parser_spirv = subparsers.add_parser("spirv")
    parser_spirv.set_defaults(func=spirv)

    # Clear
    parser_clear = subparsers.add_parser("clear")
    parser_clear.set_defaults(func=clear)

    # Clean
    parser_clear = subparsers.add_parser("clean")
    parser_clear.set_defaults(func=clean)

    args = parser.parse_args()
    if args.verbose:
        set_logger_verbose()
    args.func(args)


if __name__ == "__main__":
    main()
