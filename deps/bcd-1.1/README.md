# BCD - Bayesian Collaborative Denoiser for Monte-Carlo Rendering #

![](BCD.jpg)

## Overview ##

BCD allows to denoise images rendered with Monte Carlo path tracing and provided in the form of their samples statistics (average, distribution and covariance of per-pixel color samples). BCD can run in CPU (e.g., renderfarm) or GPU  (e.g., desktop) mode. It can be integrated as a library to any Monte Carlo renderer, using the provided sample accumulator to interface the Monte Carlo simulation with the BCD internals, and comes with a graphics user interface for designing interactively the denoising parameters, which can be saved in JSON format and later reused in batch. 

BCD has been designed for easy integration and low invasiveness in the host renderer, in a high spp context (production rendering). There are at least three ways to integrate BCD in a rendering pipeline, by either:
 * dumping all samples in a raw file, using the raw2bcd tool to generate the rendering statistics from this file and then running the BCD using the CLI tool;
 * exporting the mandatory statistics from the rendering loop in EXR format and running the BCD CLI tool to obtain a denoised image;
 * directly integrating the BCD library into the renderer, using the sample accumulator to post samples to BCD during the path tracing and denoising the accumulated values after rendering using the library.

Version 1.0 of BCD is the reference implementation for the paper [Bayesian Collaborative Denoising 
for Monte-Carlo Rendering](https://www.telecom-paristech.fr/~boubek/papers/BCD) by Malik Boughida and Tamy Boubekeur.

Copyright(C) 2014-2018
Malik Boughida and Tamy Boubekeur
                                                                           
All rights reserved. 

## Release Notes ##

### v1.1 ###
* New architecture: 
	* BCD is now a library, for easy integration in host renders,
	* a new sample accumulator object helps easily dumping samples during rendering for later extracting BCD inputs and denoising the final image,
	* dependency are now managed through git submodules,
	* the command line tool is still here.
* Denoising presets can now be saved/loaded in a JSON file.
* New GUI to load raw images (color and stats), configure the BCD denoiser, run and configure it interactively, load and save presets which can be reused by the core library (batch denoising, engine-integrated denoising).
* Various bug fixes.
* Now properly compiles and runs on Linux and Windows 10, in CPU and GPU mode.
* Improved build system (CMake).

### v1.0 ##
Initial version.

## Building ##

This program uses CMake. It has been tested on Linux and Windows 10. Building on MacOS has not been tested but should not cause major problem, at least for the CPU version.

This program has several dependencies that are automatically downloaded through GIT submodules:

Required:

* OpenEXR
* Eigen
* JSON
* zlib

Optional (need to be installed on the host system):

* OpenMP
* CUDA

To build on Linux, go to the directory containing this README file, then:

```
$ mkdir build
$ cd build
$ cmake ..
$ make
```

You can also use the graphical user interface for cmake (cmake-gui). 
Use the following cmake option to compile without CUDA support (multi-core CPU execution only then): 
```
-DBCD_USE_CUDA=OFF
```

## Running ##

Usage:

```
$ cd bin/
$ ./bcd_cli <arguments list>
```

Required arguments list:

* -o &lt;output&gt;          The file path to the output image
* -i &lt;input&gt;           The file path to the input image
* -h &lt;hist&gt;            The file path to the input histograms buffer
* -c &lt;cov&gt;             The file path to the input covariance matrices buffer

Optional arguments list:

* -d &lt;float&gt;           Histogram patch distance threshold (default: 1)
* -b &lt;int&gt;             Radius of search windows (default: 6)
* -w &lt;int&gt;             Radius of patches (default: 1)
* -r &lt;0/1&gt;             1 for random pixel order (in case of grid artifacts) (default: 0)
* -p &lt;0/1&gt;             1 for a spike removal prefiltering (default: 0)
* --p-factor &lt;float&gt;   Factor that is multiplied by standard deviation to get the threshold for classifying spikes during prefiltering. Put lower value to remove more spikes (default: 2)
* -m &lt;float in [0,1]&gt;  Probability of skipping marked centers of denoised patches. 1 accelerates a lot the computations. 0 helps removing potential grid artifacts (default: 1)
* -s &lt;int&gt;             Number of Scales for Multi-Scaling (default: 3)
* --ncores &lt;nbOfCores&gt; Number of cores used by OpenMP (default: environment variable OMP_NUM_THREADS)
* --use-cuda &lt;0/1&gt;     1 to use cuda, 0 not to use it (default: 1)
* -e &lt;float&gt;           Minimum eigen value for matrix inversion (default: 1e-08)

Example: 
```
$ ./bcd_cli -o filtered-rendering.exr -i noisy-rendering.exr -h noisy-rendering_hist.exr -c noisy-rendering_cov.exr
```

Precompiled MS Windows binaries are provided in the bin/win64 directory.

Only EXR images are supported. A collection of input data files is provided on the [project webpage](https://www.telecom-paristech.fr/~boubek/papers/BCD).

## Conversion from raw full sampling images to proper inputs ##

The **raw2bcd** command line tool allows to convert raw binary many-samples per pixel (i.e. all the samples that get average to the final pixel color, before averaging them) files to the 3 EXR files required by BCD (per-pixel color, distribution/histogram, covariance matrix).

Usage: 
```
$ raw2bcd <raw-input-file> <output-prefix>
```

Converts a raw file with all samples into the inputs for the Bayesian Collaborative Denoiser (bcd_cli) program.

Required arguments list:
* _raw-input-file_,  the file path to the input binary raw sample set file,
* _output-prefix_, the file path to the output image, without .exr extension.

RAW sample images follow the following header structure:
<pre><code>typedef struct {
   int version;
   int xres;
   int yres;
   int num_samples;
   int num_channels;
   float data[1];
} Header;
</code></pre>

Depending on num_channels value you might get RGB (3) or RGBA (4) values.

The input file "test.raw" is provided as an example in the data/raw directory.

Example:
```
$ raw2bcd raw/test.raw inputs/test
$ bcd_cli --use-cuda 1 --ncores 4 -o outputs/test_BCDfiltered.exr -i inputs/test.exr -h inputs/test_hist.exr -c inputs/test_cov.exr
```

## Authors

* [**Malik Boughida**](https://www.telecom-paristech.fr/~boughida/) 
* [**Tamy Boubekeur**](https://www.telecom-paristech.fr/~boubek)

See also the list of [contributors](https://github.com/superboubek/bcd/contributors) who participated in this project.

## Citation

Please cite the following paper in case you are using this code:
>**Bayesian Collaborative Denoising for Monte-Carlo Rendering.** *Malik Boughida and Tamy Boubekeur.* Computer Graphics Forum (Proc. EGSR 2017), vol. 36, no. 4, p. 137-153, 2017.

## License

This project is licensed under a BSD-like license - see the [LICENSE.txt](LICENSE.txt) file for details.
