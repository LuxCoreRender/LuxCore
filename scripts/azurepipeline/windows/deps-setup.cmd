:: Setting up dependencies
cd ..
git lfs install
git clone --branch master https://github.com/LuxCoreRender/WindowsCompile .\WindowsCompile
git clone --branch master https://github.com/LuxCoreRender/WindowsCompileDeps .\WindowsCompileDeps

mklink /J Luxcore .\s

pip install --upgrade setuptools
pip install --upgrade pywin32
pip install pyinstaller
pip install numpy==1.12.1
.\WindowsCompile\support\bin\wget.exe --user-agent="Mozilla/5.0" https://download.lfd.uci.edu/pythonlibs/h2ufg7oq/PySide-1.2.4-cp35-cp35m-win_amd64.whl -O PySide-1.2.4-cp35-cp35m-win_amd64.whl
pip install PySide-1.2.4-cp35-cp35m-win_amd64.whl

.\WindowsCompile\support\bin\wget.exe https://github.com/GPUOpen-LibrariesAndSDKs/OCL-SDK/files/1406216/lightOCLSDK.zip
.\WindowsCompile\support\bin\7za.exe x -oWindowsCompile\OCL_SDK_Light lightOCLSDK.zip
set OCL_ROOT=%CD%\WindowsCompile\OCL_SDK_Light
