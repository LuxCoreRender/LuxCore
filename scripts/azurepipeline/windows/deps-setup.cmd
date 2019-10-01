:: Setting up dependencies

if "%BLENDER280%" EQU "TRUE" (
    git checkout blender2.80
    git config user.email "email"
    git config user.name "name"
    git merge --no-commit origin/%BUILD_SOURCEBRANCHNAME%
)

cd ..
git lfs install
git clone --branch master https://github.com/LuxCoreRender/WindowsCompile .\WindowsCompile
.\WindowsCompile\support\bin\wget https://github.com/LuxCoreRender/WindowsCompileDeps/releases/download/luxcorerender_v2.2/WindowsCompileDeps.7z
.\WindowsCompile\support\bin\7z x -oWindowsCompileDeps WindowsCompileDeps.7z

mklink /J Luxcore %SYSTEM_DEFAULTWORKINGDIRECTORY%

pip install --upgrade pip
pip install --upgrade setuptools
pip install --upgrade pywin32
pip install wheel
pip install pyinstaller

:: pyluxcoretool will use same numpy version used to build LuxCore
for /F "tokens=2" %%a in ('python --version') do set PPP=%%a
for /F "tokens=1,2 delims=." %%a in ("%PPP%") do (
    set PPP1=%%a
    set PPP2=%%b
)
if "%PPP1%.%PPP2%" EQU "3.5" (
    pip install numpy==1.12.1
) else (
    pip install numpy==1.15.4
)

pip install PySide2

.\WindowsCompile\support\bin\wget.exe https://github.com/GPUOpen-LibrariesAndSDKs/OCL-SDK/files/1406216/lightOCLSDK.zip
.\WindowsCompile\support\bin\7za.exe x -oWindowsCompile\OCL_SDK_Light lightOCLSDK.zip
