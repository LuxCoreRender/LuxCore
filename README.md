## LuxCore, CUDA/OptiX GPU-optimized fork (Diginaat)

This is a fork of [LuxCoreRender](https://github.com/LuxCoreRender/LuxCore)
that makes GPU rendering on NVIDIA cards (CUDA + OptiX, `PATHOCL` engine)
faster. The default image stays the same as upstream: the renderer is
still unbiased, and every change that would alter the image is an opt-in
property that is off by default.

- Tested on Windows 11, NVIDIA RTX 5070 12 GB, CUDA 12.8, OptiX.
- Blender addon: [Diginaat/BlendLuxCore](https://github.com/Diginaat/BlendLuxCore)
  (download the zip from its Releases). **For now the addon works only with
  Blender 4.5 LTS on Windows x64**; Blender 5.x is not supported yet.
- Plan, measurements and decisions: [`docs/gpu-optimization/`](docs/gpu-optimization/).

### Speed compared to the original LuxCore

Million samples per second on an RTX 5070 (`luxcoreconsole`, `PATHOCL`,
CUDA + OptiX, median of 3 runs of 30 s, kernels already compiled, adaptive
sampling off). Original = upstream LuxCore `ce1d64155`.

| Scene | Original LuxCore | This fork | Change |
|---|---|---|---|
| LuxBall | 11.94 | 18.00 | +51% |
| DLSC | 12.37 | 21.59 | +75% |
| LuxCore2.1Benchmark | 11.16 | 16.00 | +43% |
| DanishMood | 7.68 | 13.14 | +71% |
| LynxSpider | 5.39 | 15.38 | +185% |

Inside Blender 4.5 (LuxMark LuxBall, final render, GPU): 24.67 to 35.12
million samples per second (+42%). The Blender viewport uses another
engine (`RTPATHOCL`); its speed-up is not measured yet.

### What changed

Faster by default (same image):
- OptiX: fast-trace BVH build, any-hit disabled, shadow rays stop at the
  first hit when that is exact.
- CUDA kernels: full NVRTC optimization, block size per kernel from the
  occupancy API, 1M paths in flight on cards with more than 10 GB.
- Wavefront path tracer: per-state work queues built by scans, material
  sorting where it is faster (chosen at run time), fused path tail kernel.
- Kernel specialization on by default: kernels are compiled for the
  material and texture types the scene uses; evaluation stacks and
  temporaries live in CUDA local memory.
- Less memory traffic per path: path state in its own array, smaller
  `HitPoint`, AOV channels written only when the film uses them, GPU Sobol
  lookup tables.
- Render loop: queues one batch ahead and reads the film asynchronously
  (the render thread uses 3-5% of a CPU core instead of a full core).
- Fixes: the CUDA kernel cache key includes the NVRTC options and version,
  OpenCL split compile, a kernel cache deadlock on a corrupt cache file.

New, opt-in:
- GPU light tracing for caustics: with `path.hybridbackforward.enable` the
  light paths are traced on the GPU too, with guided emission toward
  specular objects.
- Path guiding on the GPU: `path.guiding.enable` (experimental, `PATHOCL`).
- MIS compensation for HDRI lights: `scene.lights.<name>.miscompensation.enable`.
- Tuning and profiling: `cuda.kernel.maxregisters`, `opencl.kernel.profile.enable`.

---

### LuxCoreRender

![LuxCoreRender alt text](https://luxcorerender.org/wp-content/uploads/2017/12/wallpaper_lux_05_rend1b.jpg)

LuxCoreRender is a physically correct, unbiased rendering engine. It is built on
physically based equations that model the transportation of light. This allows
it to accurately capture a wide range of phenomena which most other rendering
programs are simply unable to reproduce.

You can find more information about at https://www.luxcorerender.org


### Building

#### Build documentation
Complete build documentation can be found in the wiki: https://wiki.luxcorerender.org/Building_LuxCoreRender.

Here is a short extract, please refer to link above for more information.


#### Tool requirements
[Mandatory] First, ensure you have a suitable toolchain:
- Windows: MSVC >= 194x _latest version_
- Linux: gcc 14
- MacOS Intel: XCode 15.2
- MacOS Arm: XCode 15.4

[Mandatory] Then, ensure the following software is also installed and available in
the PATH:
- Git
- Python 3
- Conan (`pip install conan`)
- CMake
- patchelf (for Linux and MacOS)

[Optional, but recommended] In addition, you may install the following
software:
- [Github CLI](https://cli.github.com/) (for dependency signature checking)
- [repairwheel](https://pypi.org/project/repairwheel/) (to build test wheels)
- [nektos/act](https://github.com/nektos/act) (to test Github scripts locally)
- [Doxygen](https://www.doxygen.nl/download.html) (to generate development documentation)

[Mandatory, Windows only] For Windows, ensure the command line is configured
for building (`vcvarsall.bat`).


#### Quick build

```
git clone https://github.com/LuxCoreRender/LuxCore.git

cd LuxCore
git checkout for_v2.10

make deps
make
```

This will download LuxCore source code and LuxCore precompiled dependencies,
configure CMake and start the build.

Nota: second `make` statement can also name a specific target. Examples:
`make luxcore` `make pyluxcore` `make luxcoreconsole` `make luxcoreui`

#### Build type

Build type can be controlled by environment variable `LUX_BUILD_TYPE`.
Available build types are `Release` and `Debug` (case sensitive). Default is
`Release`.

#### Other commands

- `make clean`: clean build tree (delete intermediate files)
- `make clear`: remove build tree
- `make config`: configure/reconfigure project
- `make deps`: update dependencies
- `make doc`: build Doxygen documentation



### LuxCore library

LuxCore is the new LuxCoreRender v2.x C++ and Python API. It is released under
Apache Public License v2.0 and can be freely used in open source and commercial
applications.

You can find more information about the API at
https://wiki.luxcorerender.org/LuxCore_API

### LuxCoreUI

This is the most complete example of LuxCore API usage and it is available in
the [`samples/luxcoreui`](samples/luxcoreui) directory.

To see how it works, just run `luxcoreui` from the root directory:

Linux/MacOS:
```
./out/install/Release/bin/luxcoreui scenes/cornell/cornell.cfg
```

Windows:
```
out\install\Release\bin\luxcoreui scenes\cornell\cornell.cfg
```

(assuming you selected Release as a build type)

### LuxCoreConsole

This is a simple example of a command line renderer written using LuxCore API
and it is available in the [`samples/luxcoreconsole`](samples/luxcoreconsole)
directory.
Just run `luxcoreconsole` from the root directory with:

Linux/MacOS:
```
./out/install/Release/bin/luxcoreconsole -D batch.halttime 10 scenes/cornell/cornell.cfg
```

Windows:
```
out\install\Release\bin\luxcoreconsole -D batch.halttime 10 scenes\cornell\cornell.cfg
```

(assuming you selected Release as a build type)
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
