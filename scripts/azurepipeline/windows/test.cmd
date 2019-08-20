:: Testing Luxcore

echo %VERSION_STRING%

pip install pillow

cd ..
set PYTHONPATH=%CD%\WindowsCompile\luxcorerender-%VERSION_STRING%-win64
cd LuxCore\pyunittests
python unittests.py
