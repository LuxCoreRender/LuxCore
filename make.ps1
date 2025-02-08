# Convenience wrapper for CMake commands

# Script command (1st parameter)
param (
    [string]$COMMAND = ""
)

if (-not $env:BUILD_CMAKE_ARGS) {
    $env:BUILD_CMAKE_ARGS = $NULL
}

if (-not $env:BUILD_DIR) {
    $env:BUILD_DIR = "./build"
}

$SOURCE_DIR = pwd

if (-not $env:PYTHON) {
    $env:PYTHON = "python3"
}

function Invoke-CMake {
    param (
        [string]$Preset = "conan-release",
        [string]$Target = ""
    )
    cmake $env:BUILD_CMAKE_ARGS --build --preset $Preset --target $Target
}

function Invoke-CMakeConfig {
    param (
        [string]$Preset = "conan-release"
    )
    #cmake $env:BUILD_CMAKE_ARGS -preset $Preset -S $SOURCE_DIR
    cmake --preset $Preset -S $SOURCE_DIR
}

function Clean {
    Invoke-CMake -Target "clean"
}

function Config {
    Invoke-CMakeConfig
}

function Luxcore {
    Config
    Invoke-CMake -Target "luxcore"
}

function PyLuxcore {
    Config
    Invoke-CMake -Target "pyluxcore"
}

function LuxcoreUI {
    Config
    Invoke-CMake -Target "luxcoreui"
}

function LuxcoreConsole {
    Config
    Invoke-CMake -Target "luxcoreconsole"
}

function Clear {
    Remove-Item -Recurse -Force $env:BUILD_DIR
}

function Deps {
    & $env:PYTHON cmake/make_deps.py
}

function List-Presets {
    cmake --list-presets
}

switch ($COMMAND) {
    "" {
      Luxcore
      PyLuxcore
      LuxcoreUI
      LuxcoreConsole
    }
    "luxcore" {
      Luxcore
    }
    "pyluxcore" {
      PyLuxcore
    }
    "luxcoreui" {
      LuxcoreUI
    }
    "luxcoreconsole" {
      LuxcoreConsole
    }
    "config" {
      Config
    }
    "clean" {
      Invoke-CMake -Target "clean"
    }
    "deps" {
        Deps
    }
    "list-presets" {
        cmake --list-presets
    }
    default {
        Write-Host "Command '$COMMAND' unknown"
    }
}
