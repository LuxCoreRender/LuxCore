:: Setting up dependencies

if "%BLENDER280%" EQU "TRUE" (
    git checkout blender2.80
    git config user.email "email"
    git config user.name "name"
    git merge --no-commit origin/master
)

cd ..
git lfs install
git clone --branch master https://github.com/LuxCoreRender/WindowsCompile .\WindowsCompile
git clone --branch master https://github.com/LuxCoreRender/WindowsCompileDeps .\WindowsCompileDeps

mklink /J Luxcore %SYSTEM_DEFAULTWORKINGDIRECTORY%

pip install --upgrade pip
pip install --upgrade setuptools
pip install --upgrade pywin32
pip install wheel
pip install pyinstaller
if "%BLENDER280%" EQU "TRUE" (
    pip install numpy==1.15.4
) else (
    pip install numpy==1.12.1
)
pip install PySide2

.\WindowsCompile\support\bin\wget.exe https://github.com/GPUOpen-LibrariesAndSDKs/OCL-SDK/files/1406216/lightOCLSDK.zip
.\WindowsCompile\support\bin\7za.exe x -oWindowsCompile\OCL_SDK_Light lightOCLSDK.zip
