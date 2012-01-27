#!/bin/sh
./slg2 -D renderengine.type 4 -D opencl.cpu.use 1 -D opencl.gpu.use 1 -D screen.refresh.interval 50 -D path.sampler.type METROPOLIS scenes/luxball/render-fast.cfg
