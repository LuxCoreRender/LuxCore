[![Build Status](https://dev.azure.com/LuxCoreRender/LuxCoreRender/_apis/build/status/LuxCoreRender.LuxCore)](https://dev.azure.com/LuxCoreRender/LuxCoreRender/_build/latest?definitionId=1)

### LuxCoreRender

![LuxCoreRender alt text](https://luxcorerender.org/wp-content/uploads/2017/12/wallpaper_lux_05_rend1b.jpg)

LuxCoreRender is a physically correct, unbiased rendering engine. It is built on
physically based equations that model the transportation of light. This allows
it to accurately capture a wide range of phenomena which most other rendering
programs are simply unable to reproduce.

You can find more information about at https://www.luxcorerender.org


### Building

#### Tool requirements

Ensure the following software is installed and available in the PATH:

- Git
- Conan (`pip install conan`)
- Python 3
- CMake
- Ninja

#### Quick build

```
git clone https://github.com/LuxCoreRender/LuxCore.git

cd LuxCore
git checkout for_v2.10

make deps
make
make install
```

This will download LuxCore source code, LuxCore precompiled dependencies,
configure CMake and build.

Last statement can be adapted to build a specific target.

Examples: `make luxcore` `make pyluxcore` `make luxcoreconsole` `make luxcoreui`

#### Other commands

- `make clean`: clean build tree (delete intermediate files)
- `make clear`: remove build tree
- `make config`: configure/reconfigure project
- `make deps`: update dependencies: `make deps`


### LuxCore library

LuxCore is the new LuxCoreRender v2.x C++ and Python API. It is released under Apache Public
License v2.0 and can be freely used in open source and commercial applications.

You can find more information about the API at https://wiki.luxcorerender.org/LuxCore_API

### LuxCoreUI

This is the most complete example of LuxCore API usage and it is available in
the [`samples/luxcoreui`](samples/luxcoreui) directory.

To see how it works, just run `luxcoreui` from the root directory:

```
./bin/luxcoreui scenes/cornell/cornell.cfg
```

### LuxCoreConsole

This is a simple example of a command line renderer written using LuxCore API and it is
available in the [`samples/luxcoreconsole`](samples/luxcoreconsole) directory.
Just run `luxcoreconsole` from the root directory with:

```
./bin/luxcoreconsole -D batch.halttime 10 scenes/cornell/cornell.cfg
```

### Authors

See AUTHORS.txt file.

### Credits

A special thanks goes to:

- Alain "Chiaroscuro" Ducharme for Blender 2.5 exporter and several scenes provided;
- Sladjan "lom" Ristic for several scenes provided;
- Riku "rikb" Walve for source patches;
- David "livuxman" Rodriguez for source patches;
- [Daniel "ZanQdo" Salazar](http://www.3developer.com) for Sala scene and Michael "neo2068" Klemm for SLG2 adaptation;
- [Mourelas Konstantinos "Moure"](http://moure-portfolio.blogspot.com) for Room Scene;
- Diego Nehab for PLY reading/writing library;
- [HDR Labs sIBL archive](http://www.hdrlabs.com/sibl/archive.html) and
  [SHT Lab](http://shtlab.blogspot.com/2009/08/hdri-panoramic-skies-for-free.html) for HDR maps;
- [Chronosphere](http://chronosphere.home.comcast.net/~chronosphere/radiosity.htm) for Cornell Blender scene;
- [libPNG](http://www.libpng.org) authors;
- [zlib](http://www.zlib.net) authors;
- [OpenEXR](http://www.openexr.com) authors;
- [OpenImageIO](http://www.openimageio.org) authors;
- [Tomas Davidovic](http://www.davidovic.cz) for [SmallVCM](http://www.smallvcm.com), an endless source of hints;
- [GLFW](http://www.glfw.org) authors;
- [ImGUI](https://github.com/ocornut/imgui) authors;
- [Cycles](https://www.blender.org) authors for HSV/RGB conversion code;
- [OpenVDB](http://www.openvdb.org) authors;
- [Eigen](http://eigen.tuxfamily.org) authors;
- Yangli Hector Yee, Steven Myint and Jeff Terrace for [perceptualdiff](https://github.com/myint/perceptualdiff);
- Michael Labbe for [Native File Dialog](https://github.com/mlabbe/nativefiledialog);
- Sven Forstmann's [quadric mesh simplification code](https://github.com/sp4cerat/Fast-Quadric-Mesh-Simplification).
- [SpdLog](https://github.com/gabime/spdlog) authors

### License

This software is released under Apache License Version 2.0 (see COPYING.txt file).
