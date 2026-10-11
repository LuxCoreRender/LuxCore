@echo off

REM SPDX-FileCopyrightText: 2024-2025 Authors (see AUTHORS.txt)
REM
REM SPDX-License-Identifier: Apache-2.0


REM Convenience wrapper for CMake commands

REM Script command (1st parameter)

set COMMAND=%1


if "%LUX_PYTHON%" == "" (
    set LUX_PYTHON=python
)

set LUXMAKE="%LUX_PYTHON%" -u -m build-system.luxmake

if "%COMMAND%" == "" (
    call :Config
    call :BuildAndInstall luxcore
    call :BuildAndInstall luxcoreui
    call :BuildAndInstall luxcoreconsole
    call :BuildAndInstall pyluxcore
) else if "%COMMAND%" == "luxcore" (
    call :Config
    call :BuildAndInstall luxcore
) else if "%COMMAND%" == "pyluxcore" (
    call :Config
    call :BuildAndInstall luxcore
    call :BuildAndInstall pyluxcore
) else if "%COMMAND%" == "luxcoreui" (
    call :Config
    call :BuildAndInstall luxcore
    call :BuildAndInstall luxcoreui
) else if "%COMMAND%" == "luxcoreconsole" (
    call :Config
    call :BuildAndInstall luxcore
    call :BuildAndInstall luxcoreconsole
) else if "%COMMAND%" == "doc" (
    call :Config
    call :BuildAndInstall doc
) else if "%COMMAND%" == "config" (
    call :Config
) else if "%COMMAND%" == "spirv" (
    call :Spirv
) else if "%COMMAND%" == "package" (
    call :BuildAndInstall package
) else if "%COMMAND%" == "install" (
    call :Install
) else if "%COMMAND%" == "clean" (
    call :Clean
) else if "%COMMAND%" == "clear" (
    call :Clear
) else if "%COMMAND%" == "deps" (
    call :Deps
) else if "%COMMAND%" == "win-recompose" (
    call :WinRecompose %2
) else if "%COMMAND%" == "list-presets" (
    call :ListPresets
) else if "%COMMAND%" == "wheel-test" (
    call :WheelTest
) else if "%COMMAND%" == "msvc-init" (
    call :MsvcInit
) else if "%COMMAND%" == "windebug" (
    call :WinDebug
) else (
    echo Unknown command: "%COMMAND%"
)
exit /B

:Deps
call %LUXMAKE% deps
goto :EOF

:ListPresets
call %LUXMAKE% list-presets
goto :EOF

:Config
call %LUXMAKE% config
goto :EOF

:Spirv
REM Bake the OpenCL kernels in SPIR-V format (dependency-free: no 'make deps'
REM needed). The baked modules are automatically used by the next 'make config'
call %LUXMAKE% spirv
goto :EOF

:BuildAndInstall
call %LUXMAKE% build-and-install %1
goto :EOF

:Install
IF "%~1" == "" (
    %LUXMAKE% all
) else (
    %LUXMAKE% %1
)
goto :EOF

:WinRecompose
call %LUXMAKE% win-recompose %1
goto :EOF

:Clean
call %LUXMAKE% clean
goto :EOF

:WheelTest
call %LUXMAKE% wheel-test
goto :EOF

:MsvcInit
REM 'setup_x64.bat' must be in PATH
set CMAKE_CXX_COMPILER_LAUNCHER=ccache
set CMAKE_C_COMPILER_LAUNCHER=ccache
set BUILD_CMAKE_ARGS="-DCMAKE_VERBOSE_MAKEFILE=ON"
set CCACHE_DIRECT=true
set CCACHE_DEPEND=true
call setup_x64.bat
goto :EOF

:WinDebug
REM Special (quick & dirty) custom config for debugging on Windows platform
REM Run it under cmd.exe
REM 'setup_x64.bat' must be in PATH
call :MsvcInit
set LUX_BUILD_TYPE=Debug
set LUX_SANITIZE=1
set CMAKE_BUILD_PARALLEL_LEVEL=1
call :Config
call :BuildAndInstall luxcore
call :BuildAndInstall luxcoreui
call :BuildAndInstall luxcoreconsole
call :BuildAndInstall pyluxcore 

:Clear
REM rmdir /S /Q
call %LUXMAKE% clear
goto :EOF

:EOF
