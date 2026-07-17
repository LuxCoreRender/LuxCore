# SPDX-FileCopyrightText: 2025 Authors (see AUTHORS.txt)
#
# SPDX-License-Identifier: Apache-2.0

"""This script downloads and installs dependencies for LuxCore build."""

import os
import tempfile
from urllib.request import urlretrieve
from urllib.parse import urlparse
from pathlib import Path
from zipfile import ZipFile
import subprocess
import shutil
import argparse
import json
import platform
from functools import cache

from .constants import PARAMS
from .utils import logger, fail, Colors, set_logger_verbose
from .check import check_requirements

CONAN_ALL_PACKAGES = '"*"'

CONAN_ENV = {}

URL_SUFFIXES = {
    "Linux-X64": "ubuntu-latest",
    "Windows-X64": "windows-2022",
    "Windows-ARM64": "windows-11-arm",
    "macOS-ARM64": "macos-15-arm64",
    "macOS-X64": "macos-15-intel",
}


def logger_step(step):
    """Log a new step."""
    length = 12
    stars = "*" * length
    logger.info("")
    logger.info("%s  %s  %s", stars, step, stars)


def find_platform():
    """Find current platform."""
    system = platform.system()
    if system == "Linux":
        res = "Linux-X64"
    elif system == "Windows":
        machine = platform.machine()
        if machine.lower() == "arm64":
            res = "Windows-ARM64"
        elif machine.lower() in ("amd64", "x86_64"):
            res = "Windows-X64"
        else:
            raise RuntimeError(f"Unknown machine for Windows: '{machine}'")
    elif system == "Darwin":
        machine = platform.machine()
        if machine == "arm64":
            res = "macOS-ARM64"
        elif machine == "x86_64":
            res = "macOS-X64"
        else:
            raise RuntimeError(f"Unknown machine for MacOS: '{machine}'")
    else:
        raise RuntimeError(f"Unknown system '{system}'")
    return res


def build_url(
    user,
    release,
):
    """Build the url to download from."""
    suffix = URL_SUFFIXES[find_platform()]
    if not user:
        user = "LuxCoreRender"

    url = (
        "https://github.com",
        user,
        "LuxCoreDeps",
        "releases",
        "download",
        f"v{release}",
        f"luxcore-deps-{suffix}.zip",
    )

    return "/".join(url)


def get_profile_name():
    """Get the profile file name, based on platform."""
    return f"conan-profile-{find_platform()}"


@cache
def ensure_conan_app():
    """Ensure Conan is installed."""
    if not (res := shutil.which("conan")):
        fail("Conan not found!")
    return Path(res)


def run_conan(
    args,
    **kwargs,
):
    """Run conan statement."""
    conan_app = ensure_conan_app()
    kwargs["env"] = os.environ.update(CONAN_ENV)
    kwargs["text"] = kwargs.get("text", True)
    args = [conan_app] + args
    logger.debug(args)
    lines = []
    with subprocess.Popen(
        args,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        bufsize=1,
        universal_newlines=True,
        env=kwargs["env"],
    ) as proc:
        for line in proc.stdout:
            line = line.rstrip()
            logger.info(line)
            lines += [line]

        if proc.returncode:
            fail("Error while executing conan...")

        return lines


def download(
    url,
    destdir,
):
    """Download file from url into destdir."""
    # Download artifact
    destdir = Path(destdir)
    filename = urlparse(url).path.split("/")[-1]
    filepath = destdir / filename
    (
        local_filename,
        _,
    ) = urlretrieve(
        url,
        filename=filepath,
    )

    # Check attestation
    logger.info(
        "Checking '%s'",
        local_filename,
    )

    if not (gh_app := shutil.which("gh")):
        msg = (
            Colors.WARNING,
            "SIGNATURE CHECKING ERROR",
            Colors.ENDC,
        )
        msg = "".join(msg)
        logger.warning(msg)
        msg = (
            Colors.WARNING,
            "Cannot find 'gh' application - ",
            "Dependencies origin cannot be checked.",
            Colors.ENDC,
        )
        msg = "".join(msg)
        logger.warning(msg)
    else:
        gh_cmd = [
            gh_app,
            "attestation",
            "verify",
            "-oLuxCoreRender",
            "--format",
            "json",
            filepath,
        ]
        errormsg = f"{Colors.WARNING}SIGNATURE CHECKING ERROR{Colors.ENDC}"
        try:
            gh_output = subprocess.check_output(
                gh_cmd,
                text=True,
            )
        except subprocess.CalledProcessError as err:
            logger.warning(errormsg)
            logger.warning(
                "gh return code: %s",
                err.returncode,
            )
            logger.warning(err.output)
        except OSError as err:
            logger.warning(errormsg)
            logger.warning(str(err))
        else:
            msg = f"{Colors.OKGREEN}'%s': found certificate - OK{Colors.ENDC}"
            logger.info(
                msg,
                filename,
            )
            (
                signature,
                *_,
            ) = json.loads(gh_output)
            certificate = signature["verificationResult"]["signature"][
                "certificate"
            ]
            logger.debug(
                json.dumps(
                    certificate,
                    indent=2,
                )
            )

    # Unzip
    with ZipFile(local_filename) as downloaded:
        downloaded.extractall(destdir)


def get_existing_config(config_file):
    """Get existing configuration from existing global.conf"""
    config_file = Path(config_file)
    if not config_file.exists():
        logger.info("No global.conf found")
        return []
    with config_file.open(encoding="utf-8") as f:
        # Minimal filtering: remove blank lines and lines starting with '#'
        # (comments...)
        return [
            l.rstrip()
            for l in f.readlines()
            if l.rstrip() and not l.startswith("#")
        ]


def show_build_info(destdir):
    """Show build information that should be bundled with the archive."""
    file_path = destdir / "build-info.json"
    try:
        with open(file_path, "r", encoding="utf-8") as f:
            data = json.load(f)
    except FileNotFoundError:
        logger.debug("Error: The file '%s' does not exist.", file_path)
        return
    except json.JSONDecodeError:
        logger.debug(
            "Error: The file '%s' is not a valid JSON file.", file_path
        )
        return

    missing = "<missing>"
    timestamp = data.get("TIMESTAMP", missing)
    system = data.get("SYSTEM", missing)
    system_name = data.get("SYSTEM_NAME", missing)
    compiler_id = data.get("CXX_COMPILER_ID", missing)
    compiler_version = data.get("CXX_COMPILER_VERSION", missing)
    compiler_architecture = data.get("CXX_COMPILER_ARCHITECTURE_ID", missing)

    logger.info("Build information:")
    logger.info(" - Build date: %s", timestamp)
    logger.info(" - System: %s (%s)", system_name, system)
    logger.info(" - Compiler: %s - version %s", compiler_id, compiler_version)
    logger.info(" - Target architecture: %s", compiler_architecture)


def install(
    filename,
    destdir,
):
    """Install file from local directory into destdir."""
    logger.info(
        "Importing %s",
        filename,
    )
    with ZipFile(str(filename)) as zipfile:
        zipfile.extractall(destdir)


def conan_home():
    """Get Conan home path."""
    res = run_conan(
        ["config", "home"],
        text=True,
    )
    if not res:
        raise RuntimeError("Cannot find conan home")
    return Path(res[0].strip())


def copy_global_conf(
    dest,
):
    """Copy global.conf into conan tree."""
    home = conan_home()
    source = home / "global.conf"
    logger.info(
        "Copying %s to %s",
        source,
        dest,
    )
    shutil.copy(
        source,
        dest,
    )


def set_global_conf(cache_dir, existing_config):
    """Set global.conf file."""
    global_conf = conan_home() / "global.conf"
    global_conf.touch()

    logger.info("Writing configuration file: '%s'", str(global_conf))
    with global_conf.open("w+", encoding="utf-8") as conf:

        def write(entry):
            logger.info(" - %s", entry)
            conf.write(entry.rstrip())
            conf.write("\n")

        write(f"core.cache:storage_path={cache_dir}")
        write(f"core.download:download_cache={cache_dir}")
        write("core:non_interactive=True")
        for line in existing_config:
            write(line)


def main(
    call_args=None,
):
    """Entry point."""
    output_dir = os.getenv(
        "output_dir",
        "out",
    )
    output_dir = Path(output_dir).absolute()

    # Set-up logger
    msg = f"{Colors.OKBLUE}BEGIN{Colors.ENDC}"
    logger.info(msg)

    # Get settings & ensure requirements
    logger_step("Checking requirements")
    with open(
        "build-system/build-settings.json",
        encoding="utf-8",
    ) as f:
        settings = json.load(f)
    logger.info(
        "Output directory: %s",
        output_dir,
    )
    ensure_conan_app()

    # Command-line parameters for standalone execution (debug)
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "-l",
        "--local",
        type=Path,
        help="Use local dependency set, located at LOCAL path",
    )
    parser.add_argument(
        "-u",
        "--user",
        type=str,
        help="Specify dependencies user (override build-settings.json)",
    )
    parser.add_argument(
        "-r",
        "--release",
        type=str,
        help="Specify dependencies release (override build-settings.json)",
    )
    parser.add_argument(
        "-o",
        "--output",
        type=Path,
        default=PARAMS.BINARY_DIR,
        help="Output directory",
    )
    parser.add_argument(
        "-v",
        "--verbose",
        action="store_true",
        help="Print additional information",
    )
    parser.add_argument(
        "-e",
        "--extended",
        action="store_true",
        help="Extended presets (including RelWithDebInfo & MinSizeRel)",
    )
    parser.add_argument(
        "-b",
        "--build-type",
        help="Build type from deps",
        default="Release",
    )
    if call_args is None:
        args = parser.parse_args()  # Parse command line
    else:
        args = parser.parse_args(call_args)
    if args.verbose:
        set_logger_verbose()
    if args.output:
        output_dir = args.output

    # Check requirements
    check_requirements()

    # Workaround: in Windows, conan home and deployment cannot be in
    # different drives...
    preset_dir = output_dir.absolute() if os.name == "nt" else None
    output_dir.mkdir(parents=True, exist_ok=True)

    # We need to get the existing configuration, from the existing
    # global.conf.
    logger_step("Getting existing config")
    config_file = conan_home() / "global.conf"
    logger.info("Reading %s", str(config_file))
    if not (existing_config := get_existing_config(config_file)):
        logger.info("<empty>")
    else:
        for line in existing_config:
            logger.info(" - %s", line)

    # Process
    # We download in a separate, temporary environment
    # and then we deploy in project output directory
    with tempfile.TemporaryDirectory(dir=preset_dir) as tmpdir:

        tmpdir = Path(tmpdir)

        _conan_home = tmpdir / ".conan2"
        _conan_home.mkdir(parents=True, exist_ok=True)

        CONAN_ENV.update(
            {
                "CONAN_HOME": str(_conan_home),
                "GCC_VERSION": str(settings["Build"]["gcc"]),
                "CXX_VERSION": str(settings["Build"]["cxx"]),
                "BUILD_TYPE": "Release",
            }
        )
        os.putenv("CONAN_HOME", str(_conan_home))
        del _conan_home

        logger_step("Checking conan home")
        logger.info("Temporary conan home is %s", str(conan_home()))

        # Set global.conf
        logger_step("Setting conan configuration")
        set_global_conf(tmpdir, existing_config)

        # Initialize
        user = args.user or settings["Dependencies"]["user"]
        release = args.release or settings["Dependencies"]["release"]
        url = build_url(
            user,
            release,
        )

        # Download and unzip
        logger_step("Retrieving dependencies")
        if not args.local:
            logger.info(
                "Downloading dependencies (url='%s')",
                url,
            )
            download(
                url,
                tmpdir,
            )
        else:
            logger.info(
                "Using local dependency set ('%s')",
                args.local,
            )
            # Unzip
            with ZipFile(args.local) as downloaded:
                downloaded.extractall(tmpdir)

        # Retrieve deps build information
        show_build_info(tmpdir)

        # Clean
        logger_step("Cleaning local cache")
        res = run_conan(["remove", "-c", "*"])

        # Install
        logger_step("Installing")
        res = run_conan(
            [
                "cache",
                "restore",
                tmpdir / "conan-cache-save.tgz",
            ],
        )

        # Check
        logger_step("Checking integrity")
        res = run_conan(["cache", "check-integrity", "*"])
        logger.info("Integrity check: OK")

        # Install profiles in conan home
        logger_step("Installing profiles")
        run_conan(["cache", "path", f"luxcoreconf/{release}@luxcore/luxcore"])

        run_conan(
            [
                "config",
                "install-pkg",
                "-vvv",
                f"luxcoreconf/{release}@luxcore/luxcore",
            ]
        )

        # Install compatibility plugin so MSVC consumers can find clang-built
        # packages on Windows ARM64 (e.g. Embree built with clang). luxcoreconf
        # ships its own compatibility.py (deployed to conan_home()/profiles by
        # config install-pkg above); we just relocate it to the path Conan
        # actually reads plugins from.
        logger_step("Installing compatibility plugin")
        plugins_dir = conan_home() / "extensions" / "plugins" / "compatibility"
        plugins_dir.mkdir(parents=True, exist_ok=True)
        bundled_compat = conan_home() / "profiles" / "compatibility.py"
        dest = plugins_dir / "compatibility.py"

        if bundled_compat.exists():
            shutil.copy(bundled_compat, dest)
            logger.info("Installed compatibility plugin to %s", dest)

            # This CONAN_HOME is a temp dir deleted when this script exits,
            # but the later build step runs separately against
            # output_dir/.conan2 -- deploy the plugin there too, same as
            # profiles are deployed below.
            persistent_plugins_dir = (
                output_dir.absolute()
                / ".conan2"
                / "extensions"
                / "plugins"
                / "compatibility"
            )
            persistent_plugins_dir.mkdir(parents=True, exist_ok=True)
            shutil.copy(dest, persistent_plugins_dir / "compatibility.py")
            logger.info(
                "Deployed compatibility plugin to %s", persistent_plugins_dir
            )
        else:
            logger.warning(
                "No compatibility.py found in luxcoreconf; ARM64 clang "
                "fallback will not be available"
            )

        # Install profiles into destination ("deploy")
        src_profile_dir = conan_home() / "profiles"
        profile_dir = output_dir.absolute() / ".conan2" / "profiles"
        profile_dir.mkdir(parents=True, exist_ok=True)
        logger.info("")
        logger.info(
            "Deploying profiles from %s to %s", src_profile_dir, profile_dir
        )
        run_conan(
            [
                "config",
                "install",
                "--type=dir",
                f"--target-folder={str(profile_dir)}",
                "-vvv",
                src_profile_dir,
            ]
        )

        # Generate & deploy
        logger_step("Generating...")
        # About release/debug mixing, see
        # https://github.com/conan-io/conan/issues/12656
        generator = "Ninja Multi-Config"
        # Next line is a workaround to replace {{profile_dir}}, which is
        # not well handled by deployer...
        CONAN_ENV["LUX_PROFILE_DIR"] = str(profile_dir)
        build_type = args.build_type or "Release"

        logger.info("Conan profile directory is %s", profile_dir)
        main_block = [
            "install",
            "--build=missing",
            f"--profile:all={get_profile_name()}",
            "--deployer=full_deploy",
            f"--deployer-folder={output_dir}/dependencies",
            f"--output-folder={output_dir}",
            f"--settings=build_type={build_type}",
            f"--conf:all=tools.cmake.cmaketoolchain:generator={generator}",
        ]
        if not os.getenv("DEPS_WITHOUT_SUDO"):
            main_block += [
                "--conf:all=tools.system.package_manager:sudo=true",
            ]
        else:
            logger.info("Sudo deactivated for package manager")
        build_types = [
            "Debug",
            "Release",
        ]
        if args.release:
            main_block += [f"--options:all=&:deps_version={args.release}"]
        if args.extended:
            build_types += [
                "RelWithDebInfo",
                "MinSizeRel",
            ]
        for build_type in build_types:
            logger_step(f"Generating '{build_type}'")
            end_block = [
                f"--settings=&:build_type={build_type}",
                Path(
                    "build-system",
                    "conan",
                    "conanfile.py",
                ),
            ]

            # conan is called here
            run_conan(main_block + end_block)

        # Show presets
        res = subprocess.run(
            [
                "cmake",
                "--list-presets=build",
            ],
            check=False,
            capture_output=True,
        )
        for line in res.stderr.splitlines():
            logger.info(line)
        print(
            "",
            flush=True,
        )

    msg = Colors.OKBLUE + "END" + Colors.ENDC
    logger.info(msg)


def deps(
    _,
):
    """Install dependencies."""
    main([f"--output={PARAMS.BINARY_DIR}"])


# Historically, this module was independent; there are some remnants of this
# previous situation (main entry point, with ability to parse command line)
if __name__ == "__main__":
    main()
