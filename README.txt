LuxCore library
===============

LuxCore is the new LuxRender v2.x C++ and Python API. It is released under Apache Public
License v2.0 and can be freely used in open source and commercial applications. You can
find more information about the API at http://www.luxrender.net/wiki/LuxCore

SLG library
===========

SLG is an implementation of LuxCore API and it can be safely ignored if you are
not interested in internal LuxRender development.

LuxRays library
===============

LuxRays is the part of LuxRender dedicated to accelerate the ray intersection
process by using CPUs or GPUs. You can find more information at
http://www.luxrender.net/wiki/index.php?title=LuxRays
If you don't have any specific interest in the ray/triangle intersection topic
or internal LuxRender development, you can safely ignore this library.

LuxCoreUI
=========

It is the most complete example of LuxCore API usage and it is available inside
samples/luxcoreui directory.
Just run luxcoreui from the root directory with:

./bin/luxcoreui scenes/luxball/luxball-hdr.cfg

to check how it works.

LuxCoreConsole
==============

It a simple example of command line renderer written using LuxCore API and it is
available inside samples/luxcoreconsole directory.
Just run luxcoreconsole from the root directory with:

./bin/luxcoreconsole -D batch.halttime 10 scenes/luxball/luxball-hdr.cfg

Authors
=======

See AUTHORS.txt file.

Credits
=======

A special thanks goes to:

- Alain "Chiaroscuro" Ducharme for Blender 2.5 exporter and several scenes provided;
- Sladjan "lom" Ristic for several scenes provided;
- Riku "rikb" Walve for source patches;
- David "livuxman" Rodr�guez for source patches;
- Daniel "ZanQdo" Salazar (http://www.3developer.com/) for Sala scene and Michael "neo2068" Klemm for SLG2 adaptation;
- Mourelas Konstantinos "Moure" (http://moure-portfolio.blogspot.com/) for Room Scene;
- Diego Nehab for PLY reading/writing library;
- http://www.hdrlabs.com/sibl/archive.html and http://shtlab.blogspot.com/2009/08/hdri-panoramic-skies-for-free.html for HDR maps;
- http://chronosphere.home.comcast.net/~chronosphere/radiosity.htm for Cornell Blender scene;
- libPNG authors http://www.libpng.org;
- zlib authors http://www.zlib.net/ (not used anymore);
- OpenEXR authors http://www.openexr.com/ (not used anymore);
- FreeImage open source image library. See http://freeimage.sourceforge.net for details;
- Tomas Davidovic (http://www.davidovic.cz and http://www.smallvcm.com) for SmallVCM, an endless source of hints;
- GLFW authors (http://www.glfw.org);
- ImGUI authors (https://github.com/ocornut/imgui);
- Cycles authors (https://www.blender.org/) for HSV/RGB conversion code.

This software is released under Apache License Version 2.0 (see COPYING.txt file).
