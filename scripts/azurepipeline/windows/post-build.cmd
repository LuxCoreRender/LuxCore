:: Gathering and packing binaries
cd ..\WindowsCompile
call create-standalone.bat
.\support\bin\7za.exe a luxcorerender.zip %DIR%
if "%1" EQU "/no-ocl" (
    move luxcorerender.zip luxcorerender-latest-win64.zip
) else (
    move luxcorerender.zip luxcorerender-latest-win64-opencl.zip
)
