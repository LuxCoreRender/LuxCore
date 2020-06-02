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
