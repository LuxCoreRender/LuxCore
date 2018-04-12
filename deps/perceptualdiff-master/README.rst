==============
perceptualdiff
==============

A program that compares two images using a perceptually based image metric.

.. image:: https://travis-ci.org/myint/perceptualdiff.svg?branch=master
    :target: https://travis-ci.org/myint/perceptualdiff
    :alt: Build status

.. image:: https://scan.coverity.com/projects/1561/badge.svg
    :target: https://scan.coverity.com/projects/1561
    :alt: Static analysis status

Copyright (C) 2006-2011 Yangli Hector Yee

Copyright (C) 2011-2016 Steven Myint, Jeff Terrace

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; either version 2 of the License, or (at your option) any later
version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE.  See the GNU General Public License for more details in the
file ``LICENSE``.


Build Instructions
==================

#. Download CMake from http://www.cmake.org if you do not already have it on
   your system.

#. Download FreeImage from https://sourceforge.net/projects/freeimage.
    - On OS X with MacPorts: ``port install freeimage``
    - On OS X with Brew: ``brew install freeimage``
    - On Ubuntu: ``apt-get install libfreeimage-dev``

#. Type::

    $ make

#. To specify the install directory, use::

    $ make install DESTDIR="/home/me/mydist"


Usage
=====

Command line::

    Usage: perceptualdiff image1 image2

    Compares image1 and image2 using a perceptually based image metric.

    Options:
      --verbose         Turn on verbose mode
      --fov deg         Field of view in degrees [0.1, 89.9] (default: 45.0)
      --threshold p     Number of pixels p below which differences are ignored
      --gamma g         Value to convert rgb into linear space (default: 2.2)
      --luminance l     White luminance (default: 100.0 cdm^-2)
      --luminance-only  Only consider luminance; ignore chroma (color) in the
                        comparison
      --color-factor    How much of color to use [0.0, 1.0] (default: 1.0)
      --down-sample     How many powers of two to down sample the image
                        (default: 0)
      --scale           Scale images to match each other's dimensions
      --sum-errors      Print a sum of the luminance and color differences
      --output o        Write difference to the file o
      --version         Print version


Check that perceptualdiff is built with OpenMP support::

    $ ./perceptualdiff | grep -i openmp
    OpenMP status: enabled


Credits
=======

- Hector Yee, project administrator and originator - hectorgon.blogspot.com.
- Scott Corley, for png file IO code.
- Tobias Sauerwein, for make install, package_source Cmake configuration.
- Cairo Team, for bugfixes.
- Jim Tilander, rewrote the IO to use FreeImage.
- Steven Myint, for OpenMP support and bug fixes.
- Jeff Terrace, for better FreeImage support and new command-line options.


Version History
===============

- 1.0 - Initial distribution
- 1.0.1 - Fixed off by one convolution error and libpng interface to 1.2.8
- 1.0.2 - [jt] Converted the loading and saving routines to use FreeImage
- 1.1 - Added colorfactor and downsample options. Also always output
  difference file if requested. Always print out differing pixels even if the
  test passes.
- 1.1.1 - Turn off color test in low lighting conditions.
- 1.1.2 - Add OpenMP parallel processing support and fix bugs.
- 1.2 - Add ``--sum-errors``, use more standard option style, and fix bugs.
- 1.3 - Add MSVC compatibility.
- 1.4 - Detect differences due to the alpha channel. This was lost in 1.0.2
  when FreeImage was introduced.
- 2.0 - Support usage as a library.
- 2.1 - Allow accessing stats directly when used as a library.


Usage as a library
==================

.. code:: cpp

    #include <perceptualdiff/metric.h>
    #include <perceptualdiff/rgba_image.h>

    int main()
    {
        const auto a = pdiff::read_from_file("a.png");
        const auto b = pdiff::read_from_file("b.png");

        const bool same = pdiff::yee_compare(*a, *b);
    }


Links
=====

* Coveralls_

.. _`Coveralls`: https://coveralls.io/r/myint/perceptualdiff
