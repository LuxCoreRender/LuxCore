:: Gathering and packing binaries
echo %VERSION_STRING%
if "%VERSION_STRING%" EQU "" (
    set VERSION_STRING=latest
)
if "%BLENDER280%" EQU "TRUE" (
    if x%VERSION_STRING:blender=% EQU x%VERSION_STRING% (
::      Don't do anything if VERSION_STRING already has blender2.80 tag
        set VERSION_STRING=%VERSION_STRING%-blender2.80
    )
)
echo %VERSION_STRING%

cd ..\WindowsCompile

if "%FINAL%" EQU "TRUE" (
    call create-sdk.bat
    if "%1" EQU "/no-ocl" (
        set LUX_LATEST=luxcorerender-%VERSION_STRING%-win64-sdk
    ) else (
        set LUX_LATEST=luxcorerender-%VERSION_STRING%-win64-opencl-sdk
    )
) else (
    call create-standalone.bat
    if "%1" EQU "/no-ocl" (
        set LUX_LATEST=luxcorerender-%VERSION_STRING%-win64
    ) else (
        set LUX_LATEST=luxcorerender-%VERSION_STRING%-win64-opencl
    )
)

move %DIR% %LUX_LATEST%
.\support\bin\7z.exe a %LUX_LATEST%.zip %LUX_LATEST%
copy %LUX_LATEST%.zip %BUILD_ARTIFACTSTAGINGDIRECTORY%

@echo ##vso[task.setvariable variable=version_string]%VERSION_STRING%
