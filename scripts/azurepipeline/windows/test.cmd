:: Testing Luxcore

set VERSION_STRING=%~1

pip install pillow

cd ..
set PYTHONPATH=%CD%\WindowsCompile\luxcorerender-%VERSION_STRING%-win64
cd LuxCore\pyunittests
python unittests.py
