cd ..\WindowsCompile
call .\cmake-build-x64.bat %1 %2 %3 %4
if ERRORLEVEL 1 exit /B 1
cd ..
 