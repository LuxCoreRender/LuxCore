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

:: pywin32 v. 225 and newer cause error in PyInstaller exe:
:: [11676] Failed to execute script pyi_rth_win32comgenpy
:: Under investigation, reverting to v. 224 for the moment
pip install --upgrade pywin32==224

pip install wheel
pip install pyinstaller

:: pyluxcoretool will use same numpy version used to build LuxCore
pip install numpy==1.15.4

pip install PySide2

.\WindowsCompile\support\bin\wget.exe https://github.com/GPUOpen-LibrariesAndSDKs/OCL-SDK/files/1406216/lightOCLSDK.zip
.\WindowsCompile\support\bin\7z.exe x -oWindowsCompile\OCL_SDK_Light lightOCLSDK.zip
