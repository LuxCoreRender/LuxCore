:: Gathering and packing binaries
echo %RELEASE_BUILD%
if "%RELEASE_BUILD%" EQU "TRUE" (
    echo This is an official release build
    echo %BUILD_SOURCEVERSION%
    git tag --points-at %BUILD_SOURCEVERSION%
    for /f "tokens=2 delims=_" %%a in ('git tag --points-at %BUILD_SOURCEVERSION%') do set GITHUB_TAG=%%a
) else (
    set GITHUB_TAG=latest
)

echo %GITHUB_TAG%
if "%GITHUB_TAG%" EQU "" (
    echo ERROR: No git tag found, one is needed for an official release
    exit /b 1
)
cd ..\WindowsCompile
call create-standalone.bat

if "%1" EQU "/no-ocl" (
    set LUX_LATEST=luxcorerender-%GITHUB_TAG%-win64
) else (
    set LUX_LATEST=luxcorerender-%GITHUB_TAG%-win64-opencl
)

move %DIR% %LUX_LATEST%
.\support\bin\7za.exe a %LUX_LATEST%.zip %LUX_LATEST%
copy %LUX_LATEST%.zip %BUILD_ARTIFACTSTAGINGDIRECTORY%
copy /Y ..\LuxCore\release-notes.txt %BUILD_ARTIFACTSTAGINGDIRECTORY%\release-notes.txt

@echo ##vso[task.setvariable variable=github_tag]%GITHUB_TAG%
