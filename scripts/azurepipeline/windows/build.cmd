:: Setting up dependencies
cd ..
git lfs install
git clone --branch master https://github.com/LuxCoreRender/WindowsCompile .\WindowsCompile
git clone --branch master https://github.com/LuxCoreRender/WindowsCompileDeps .\WindowsCompileDeps

mklink /J Luxcore .\s

pip --version
pip install pyinstaller
pip install numpy==1.12.1
.\WindowsCompile\support\bin\wget.exe https://download.lfd.uci.edu/pythonlibs/h2ufg7oq/PySide-1.2.4-cp35-cp35m-win_amd64.whl -O PySide-1.2.4-cp35-cp35m-win_amd64.whl
pip install PySide-1.2.4-cp35-cp35m-win_amd64.whl

cd WindowsCompile
call .\cmake-build-x64.cmd /cmake-only /no-ocl
::mv target-64-sse2/LuxCore.tar.bz2 target-64-sse2/luxcorerender-latest-linux64.tar.bz2
cd ..
