:: Gathering and packing binaries
echo %VERSION_STRING%
if "%VERSION_STRING%" EQU "" (
    set VERSION_STRING=latest
)
cd ..\WindowsCompile
call create-standalone.bat

if "%1" EQU "/no-ocl" (
    set LUX_LATEST=luxcorerender-%VERSION_STRING%-win64
) else (
    set LUX_LATEST=luxcorerender-%VERSION_STRING%-win64-opencl
)

move %DIR% %LUX_LATEST%
.\support\bin\7za.exe a %LUX_LATEST%.zip %LUX_LATEST%
copy %LUX_LATEST%.zip %BUILD_ARTIFACTSTAGINGDIRECTORY%
copy /Y ..\LuxCore\release-notes.txt %BUILD_ARTIFACTSTAGINGDIRECTORY%\release-notes.txt

@echo ##vso[task.setvariable variable=version_string]%VERSION_STRING%
