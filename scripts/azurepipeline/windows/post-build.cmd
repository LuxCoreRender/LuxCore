:: Gathering and packing binaries
cd ..\WindowsCompile
call create-standalone.bat
.\support\bin\7za.exe a luxcorerender.zip %DIR%
if "%1" EQU "/no-ocl" (
    ren luxcorerender.zip luxcorerender-latest-win64.zip
) else (
    ren luxcorerender.zip luxcorerender-latest-win64-opencl.zip
)
