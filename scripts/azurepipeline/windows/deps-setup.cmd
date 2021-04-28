:: Setting up dependencies

cd ..
git lfs install
git clone --branch master https://github.com/LuxCoreRender/WindowsCompile .\WindowsCompile

mklink /J Luxcore %SYSTEM_DEFAULTWORKINGDIRECTORY%

.\WindowsCompile\support\bin\wget.exe https://developer.download.nvidia.com/compute/cuda/10.1/Prod/network_installers/cuda_10.1.243_win10_network.exe
call .\cuda_10.1.243_win10_network.exe -s nvcc_10.1 cudart_10.1 nvrtc_10.1 nvrtc_dev_10.1 visual_studio_integration_10.1

set CUDA_PATH=%ProgramFiles%\NVIDIA GPU Computing Toolkit\CUDA\v10.1
set CUDA_PATH_V10_1=%ProgramFiles%\NVIDIA GPU Computing Toolkit\CUDA\v10.1
echo "##vso[task.setvariable variable=cuda.path]%CUDA_PATH%"
echo "##vso[task.setvariable variable=cuda.path.v10.1]%CUDA_PATH_V10_1%"

pip install --upgrade pip

pip install --upgrade setuptools

pip install wheel
pip install pyinstaller

:: pyluxcoretool will use same numpy version used to build LuxCore
pip install numpy==1.15.4

pip install PySide2
