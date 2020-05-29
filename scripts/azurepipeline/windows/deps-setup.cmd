:: Setting up dependencies

cd ..
git lfs install
git clone --branch master https://github.com/LuxCoreRender/WindowsCompile .\WindowsCompile

mklink /J Luxcore %SYSTEM_DEFAULTWORKINGDIRECTORY%

pip install --upgrade pip

:: setuptools 45.x has issues when packaged with PyInstaller, see:
:: https://github.com/pypa/setuptools/issues/1963
:: Next PyInstaller version will fix
pip install --upgrade "setuptools<45.0.0"

pip install wheel
pip install pyinstaller

:: pyluxcoretool will use same numpy version used to build LuxCore
pip install numpy==1.15.4

pip install PySide2

.\WindowsCompile\support\bin\wget.exe https://github.com/GPUOpen-LibrariesAndSDKs/OCL-SDK/files/1406216/lightOCLSDK.zip
.\WindowsCompile\support\bin\7z.exe x -oWindowsCompile\OCL_SDK_Light lightOCLSDK.zip

REM .\WindowsCompile\support\bin\wget.exe https://www.khronos.org/registry/OpenCL/api/2.1/cl.hpp
REM copy /Y cl.hpp WindowsCompile\OCL_SDK_Light\include\CL\cl.hpp

.\WindowsCompile\support\bin\wget.exe https://developer.download.nvidia.com/compute/cuda/10.1/Prod/network_installers/cuda_10.1.243_win10_network.exe
call .\cuda_10.1.243_win10_network.exe -s nvcc_10.1 cudart_10.1 nvrtc_10.1 nvrtc_dev_10.1
