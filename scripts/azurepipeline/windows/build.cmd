cd ..\WindowsCompile
set OCL_ROOT=%CD%\OCL_SDK_Light
call .\cmake-build-x64.bat %1 %2 %3
cd ..
