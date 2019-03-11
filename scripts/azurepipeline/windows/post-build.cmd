:: Gathering and packing binaries
echo %RELEASE_BUILD%
if "%RELEASE_BUILD%" EQU "TRUE" (
    echo This is an official release build
    echo Git commit is %BUILD_SOURCEVERSION%
    git tag --points-at %BUILD_SOURCEVERSION%
    for /f "tokens=2 delims=_" %%a in ('git tag --points-at %BUILD_SOURCEVERSION%') do set GITHUB_TAG=%%a
    echo Github tag is %GITHUB_TAG%
    if "%GITHUB_TAG%" EQU "" (
        echo An official release build must be associated with a Github tag
        exit /b 1
    )
) else (
    set GITHUB_TAG=latest
)
echo %GITHUB_TAG%
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
