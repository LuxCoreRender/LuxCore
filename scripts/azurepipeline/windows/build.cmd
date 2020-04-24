cd ..\WindowsCompile
set OCL_ROOT=%CD%\OCL_SDK_Light
::set CUDA_PATH=%ProgramFiles%\NVIDIA GPU Computing Toolkit\CUDA\v10.1
::set CUDA_PATH_V10_1=%ProgramFiles%\NVIDIA GPU Computing Toolkit\CUDA\v10.1
call .\cmake-build-x64.bat %1 %2 %3 %4
if ERRORLEVEL 1 exit /B 1
cd ..
