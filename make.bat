@echo off

REM Convenience wrapper for CMake commands

REM Script command (1st parameter)
set COMMAND=%1

if "%BUILD_CMAKE_ARGS%" == "" (
    set BUILD_CMAKE_ARGS=
)

if "%BUILD_DIR%" == "" (
    set BUILD_DIR=.\build
)

set SOURCE_DIR=%cd%

if "%PYTHON%" == "" (
    set PYTHON=python3
)

:InvokeCMake
setlocal
set PRESET=%1
set TARGET=%2
cmake %BUILD_CMAKE_ARGS% --build --preset %PRESET% --target %TARGET%
endlocal
goto :EOF

:InvokeCMakeConfig
setlocal
set PRESET=%1
cmake --preset %PRESET% -S %SOURCE_DIR%
endlocal
goto :EOF

:Clean
call :InvokeCMake conan-release clean
goto :EOF

:Config
call :InvokeCMakeConfig conan-release
goto :EOF

:Luxcore
call :Config
call :InvokeCMake conan-release luxcore
goto :EOF

:PyLuxcore
call :Config
call :InvokeCMake conan-release pyluxcore
goto :EOF

:LuxcoreUI
call :Config
call :InvokeCMake conan-release luxcoreui
goto :EOF

:LuxcoreConsole
call :Config
call :InvokeCMake conan-release luxcoreconsole
goto :EOF

:Clear
rd /s /q %BUILD_DIR%
goto :EOF

:Deps
%PYTHON% cmake/make_deps.py
goto :EOF

:ListPresets
cmake --list-presets
goto :EOF

if "%COMMAND%" == "" (
    call :Luxcore
    call :PyLuxcore
    call :LuxcoreUI
    call :LuxcoreConsole
) else if "%COMMAND%" == "luxcore" (
    call :Luxcore
) else if "%COMMAND%" == "pyluxcore" (
    call :PyLuxcore
) else if "%COMMAND%" == "luxcoreui" (
    call :LuxcoreUI
) else if "%COMMAND%" == "luxcoreconsole" (
    call :LuxcoreConsole
) else if "%COMMAND%" == "config" (
    call :Config
) else if "%COMMAND%" == "clean" (
    call :Clean
) else if "%COMMAND%" == "deps" (
    call :Deps
) else if "%COMMAND%" == "list-presets" (
    call :ListPresets
) else (
    echo Command "%COMMAND%" unknown
)
