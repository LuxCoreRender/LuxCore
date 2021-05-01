cd ..\WindowsCompile
set
call .\cmake-build-x64.bat %1 %2 %3 %4
type Build_CMake\LuxCore\CMakeFiles\CMakeError.log
if ERRORLEVEL 1 exit /B 1
cd ..
 