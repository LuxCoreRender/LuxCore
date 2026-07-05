# SPDX-FileCopyrightText: 2025 Authors (see AUTHORS.txt)
#
# SPDX-License-Identifier: Apache-2.0

"""Check whether requirements are installed."""

import shutil
import subprocess
import re
from dataclasses import dataclass
from enum import Enum

from .utils import logger, fail, Colors


@dataclass
class Require:
    """Requirement."""

    name: str
    min_version: tuple
    mandatory: bool


class Status(Enum):
    """Requirement status."""

    OK = 0
    WARN = 1
    ERROR = 2


REQUIREMENTS = (
    Require("conan", (2, 18), True),
    Require("wheel", None, True),
    Require("cmake", (3, 29), True),
    Require("git", None, True),
    Require("act", None, False),
    Require("gh", None, False),
    Require("repairwheel", None, False),
)


def _version_tuple(version_str):
    """Translate version into a tuple of int."""
    if not version_str:
        return None
    if "-" in version_str:
        version_str, *_ = version_str.rpartition("-")
    try:
        return tuple(int(i) for i in version_str.split("."))
    except ValueError:
        return None


def check(name, min_version=None, mandatory=True):
    """Check whether an app exists and whether its version is correct."""

    # Prepare display
    display_name = name if mandatory else f"{name} (optional)"
    prefix = f"Looking for '{display_name}' - "

    def error(*args):
        if mandatory:
            log = logger.error
            color = Colors.FAIL
            status = Status.ERROR
        else:
            log = logger.warning
            color = Colors.WARNING
            status = Status.WARN

        msg, *others = args
        msg = f"{color}{prefix}{msg}{Colors.ENDC}"
        log(msg, *others)
        return status

    # Existence
    if not (app := shutil.which(name)):
        return error("'%s' is missing!", display_name)

    # Get version
    result = subprocess.run(
        [app, "--version"], capture_output=True, text=True, check=False
    )
    output = result.stdout.strip() or result.stderr.strip()
    # Match version patterns like 1.2.3, 4.5, 2.0.1-alpha, etc.
    if match := re.search(r"\d+\.\d+(?:\.\d+)?(?:[-.\w]*)?", output):
        version_str = match.group(0)
    else:
        version_str = None

    # Check Version, if necessary
    if min_version:
        min_version_str = ".".join(str(i) for i in min_version)

        # Translate version
        if not (version := _version_tuple(version_str)):
            return error("Cannot read '%s' version", display_name)
        if version < min_version:
            return error(
                "'%s': installed version ('%s') is lower than required ('%s')",
                display_name,
                version_str,
                min_version_str,
            )
    if version_str:
        logger.info(
            "%sFound '%s', version '%s'",
            prefix,
            app,
            version_str,
        )
    else:
        logger.info("%sFound '%s'", prefix, app)

    return True


def check_requirements():
    """Check all requirements."""
    logger.info("Checking requirements:")
    checks = [
        check(req.name, req.min_version, req.mandatory) for req in REQUIREMENTS
    ]
    if all(c == Status.OK for c in checks):
        logger.info("Requirements - OK")

    if any(c == Status.ERROR for c in checks):
        fail("Some mandatory requirements are missing. Please check...")

    logger.info("Requirements - OK (some warnings)")
