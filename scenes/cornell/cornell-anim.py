#!/usr/bin/python
# -*- coding: utf-8 -*-

import os
import sys

# To avoid .pyc files
sys.dont_write_bytecode = True

# For the develop environment and Linux workaround for https://github.com/LuxCoreRender/LuxCore/issues/80
sys.path.append(".")
sys.path.append("./lib")
sys.path.append("./lib/pyluxcoretools.zip")

import pyluxcoretools.pyluxcoreconsole.cmd as consoleCmd

if __name__ == '__main__':
	imageWidth = "512"
	imageHeight = "512"
	haltSpp = "256"

	framesCount =  5 * 30
	frameTime = 1.0 / framesCount

	for frame in range(framesCount):
		frameFileName = "frame{:03d}.png".format(frame)
		print("Rendering frame: {}".format(frameFileName))

		shutterOpen = frame * frameTime
		shutterClose = shutterOpen + frameTime

		cmdArgv = ["cornell-anum.py", "scenes/cornell/cornell-anim.cfg", \
				   "-D", "renderengine.type", "PATHOCL", \
				   "-D", "sampler.type", "SOBOL", \
				   "-w", imageWidth, \
				   "-e", imageHeight, \
				   "-D", "batch.haltspp", haltSpp, \
				   "-t", str(shutterOpen), str(shutterClose)]

		consoleCmd.main(cmdArgv)

		os.rename("denoised.png", frameFileName)

# python3 scenes/cornell/cornell-anim.py
# ffmpeg -framerate 30 -i frame%03d.png -vcodec libx264 -crf 5 cornell.mp4
