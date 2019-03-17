:: Testing Luxcore

set GITHUB_TAG=%~1

pip install pillow

cd ..
set PYTHONPATH=%CD%\WindowsCompile\luxcorerender-%GITHUB_TAG%-win64
cd LuxCore\pyunittests
python unittests.py
