Dependencies
============

List of python packages required to run the tests and demos:

- Pillow (to install "pip install pillow" (Linux) or "easy_install pillow" (Windows))

Compile perceptualdiff on Linux:

cd deps/perceptualdiff-master
cmake .
make

Compile perceptualdiff on Windows:

cd deps/perceptualdiff-master
cmake -G "Visual Studio 15 2017" -T v141,host=x64 -A x64 -D FREEIMAGE_ROOT=..\..\..\WindowsCompileDeps .
set CL=/DFREEIMAGE_LIB
msbuild /toolsversion:15.0 /p:Configuration=Release /p:Platform=x64 perceptualdiff.sln
copy Release\perceptualdiff.exe .

Unit tests
==========

To run the tests:

python3 unittests.py

To run only a set of tests:

python3 unittests.py --filter PATHCPU

To have the list of options:

python3 unittests.py --help

To run the test with a custom configuration:

python3 unittests.py --config testconfigs/dade.cfg

Add the following properties to --config file to enable
https://github.com/LuxCoreRender/LuxCoreTestScenes test scenes:

luxcoretestscenes.directory = /home/david/projects/luxcorerender/LuxCoreTestScenes

Reference images
================

To generate reference images, just run the tests once and they will automatically
fill to referenceimages directory.
