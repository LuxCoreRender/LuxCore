LuxRays
=======

LuxRays is the part of LuxRender dedicated to accelerate the ray intersection
process by using GPUs. You can find more information about the ongoing effort of
integrating OpenCL support in LuxRender at http://www.luxrender.net/wiki/index.php?title=Luxrender_and_OpenCL
and at http://www.luxrender.net/wiki/index.php?title=LuxRays

LuxCore
=======

LuxCore is the new LuxRender v2.x C++ and Python API. It is released under Apache Public
License v2.0 and can be freely used in open source and commercial applications. You can
find more information about the API at http://www.luxrender.net/wiki/LuxCore

LuxCoreUI
=========

It is the most complete example of LuxCore API usage and it is available inside
samples/luxcoreui directory.
Just run luxcoreui from the root directory with:

./bin/luxcoreui scenes/luxball/luxball-hdr.cfg

to check how it works.

SLG (SmallLuxGPU)
=================

SmallLuxGPU was a LuxCore/LuxRays demo and was a LuxRender v0.8 "proof of concept". You can find
more information about SLG at http://www.luxrender.net/wiki/index.php?title=SLG
It is now replaced by LuxCoreUI.

Donate
======

LuxRender is now part of the Software Freedom Conservancy (http://sfconservancy.org/), which allows us to receive
donations to foster the development and cover the expenses of the LuxRender project.
For each donation you'll do, a small amount will go to the Conservancy so that it can
benefit all the member projects, and the rest will be made available to LuxRender.
In the United States, you can benefit from tax deductions according to the
Conservancy 501(c)(3) not for profit organization status.

You can donate at http://www.luxrender.net/en_GB/donate

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
- Nate Robins for glut32.dll;
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
