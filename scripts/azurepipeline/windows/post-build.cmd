:: Gathering and packing binaries
echo %VERSION_STRING%
if "%VERSION_STRING%" EQU "" (
    set VERSION_STRING=latest
)
echo %VERSION_STRING%

cd ..\WindowsCompile

:: Variable FINAL is made available to this script only when
:: 'LuxCoreRenderWindowsSDK' job is running (see 'azure-pipelines.yml')
if "%FINAL%" EQU "TRUE" (
    call create-sdk.bat %1
    set LUX_LATEST=luxcorerender-%VERSION_STRING%-win64-sdk
) else (
    call create-standalone.bat %1
    set LUX_LATEST=luxcorerender-%VERSION_STRING%-win64
)

move %DIR% %LUX_LATEST%
.\support\bin\7z.exe a %LUX_LATEST%.zip %LUX_LATEST%
copy %LUX_LATEST%.zip %BUILD_ARTIFACTSTAGINGDIRECTORY%

@echo ##vso[task.setvariable variable=version_string]%VERSION_STRING%
