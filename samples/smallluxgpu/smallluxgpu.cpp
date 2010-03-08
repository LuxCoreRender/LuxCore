/***************************************************************************
 *   Copyright (C) 1998-2010 by authors (see AUTHORS.txt )                 *
 *                                                                         *
 *   This file is part of LuxRays.                                         *
 *                                                                         *
 *   LuxRays is free software; you can redistribute it and/or modify       *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   LuxRays is distributed in the hope that it will be useful,            *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 *                                                                         *
 *   LuxRays website: http://www.luxrender.net                             *
 ***************************************************************************/

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <sstream>
#include <stdexcept>

#include <boost/thread.hpp>

#include "displayfunc.h"
#include "renderconfig.h"
#include "path.h"
#include "luxrays/core/device.h"

#if defined(__GNUC__)
#include <execinfo.h>
#include <stdio.h>
#include <stdlib.h>
#include <typeinfo>
#include <cxxabi.h>

static string Demangle(const char *symbol) {
	size_t size;
	int status;
	char temp[128];
	char* result;

	if (1 == sscanf(symbol, "%*[^'(']%*[^'_']%[^')''+']", temp)) {
		if (NULL != (result = abi::__cxa_demangle(temp, NULL, &size, &status))) {
			string r = result;
			return r + " [" + symbol + "]";
		}
	}

	if (1 == sscanf(symbol, "%127s", temp))
		return temp;

	return symbol;
}

void SLGTerminate(void) {
	cerr << "=========================================================" << endl;
	cerr << "Unhandled exception" << endl;

	void *array[32];
	size_t size = backtrace(array, 32);
	char **strings = backtrace_symbols(array, size);

	cerr << "Obtained " << size << " stack frames." << endl;

	for (size_t i = 0; i < size; i++)
		cerr << "  " << Demangle(strings[i]) << endl;

	free(strings);
}
#endif

static int BatchMode(double stopTime, unsigned int stopSPP) {
	const double startTime = WallClockTime();

	double sampleSec = 0.0;
	char buff[512];
	const vector<IntersectionDevice *> interscetionDevices = config->GetIntersectionDevices();
	for (;;) {
		boost::this_thread::sleep(boost::posix_time::millisec(1000));
		double elapsedTime = WallClockTime() - startTime;

		unsigned int pass = 0;
		const vector<RenderThread *> &renderThreads = config->GetRenderThreads();
		for (size_t i = 0; i < renderThreads.size(); ++i)
			pass += renderThreads[i]->GetPass();

		if ((stopTime > 0) && (elapsedTime >= stopTime))
			break;
		if ((stopSPP > 0) && (pass >= stopSPP))
			break;

		double raysSec = 0.0;
		for (size_t i = 0; i < interscetionDevices.size(); ++i)
			raysSec += interscetionDevices[i]->GetPerformance();

		sampleSec = config->scene->camera->film->GetAvgSampleSec();
		sprintf(buff, "[Elapsed time: %3d/%dsec][Samples %4d/%d][Avg. samples/sec % 4dK][Avg. rays/sec % 4dK on %.1fK tris]",
				int(elapsedTime), int(stopTime), pass, stopSPP, int(sampleSec/ 1000.0),
				int(raysSec / 1000.0), config->scene->dataSet->GetTotalTriangleCount() / 1000.0);
		std::cerr << buff << std::endl;
	}

	std::string fileName = config->cfg.GetString("image.filename", "image.ppm");
	std::cerr << "Saving " << fileName << std::endl;
	if ((fileName.length() >= 4) && (fileName.substr(fileName.length()-4) == ".png")) {
		std::cerr << "Using PNG file format" << std::endl;
		config->scene->camera->film->SavePNG(fileName);
	} else if ((fileName.length() >= 4) && (fileName.substr(fileName.length()-4) == ".ppm")) {
		std::cerr << "Using PPM file format" << std::endl;
		config->scene->camera->film->SavePPM(fileName);
	} else {
		std::cerr << "Unknown image format extension, using PPM" << std::endl;
		config->scene->camera->film->SavePPM(fileName);
	}

	sprintf(buff, "LuxMark index: %.3f", sampleSec / 1000000.0);
	std::cerr << buff << std::endl;

	delete config;
	std::cerr << "Done." << std::endl;

	return EXIT_SUCCESS;
}

int main(int argc, char *argv[]) {
#if defined(__GNUC__)
	set_terminate(SLGTerminate);
#endif

	try {
		std::cerr << "Usage (easy mode): " << argv[0] << std::endl;
		std::cerr << "Usage (benchmark mode): " << argv[0] << " <native thread count> <use CPU device (0 or 1)> <use GPU device (0 or 1)> <GPU workgroup size (0=default value or anything > 0)>" << std::endl;
		std::cerr << "Usage (interactive mode): " << argv[0] << " <low latency mode enabled (0 or 1)> <native thread count> <use CPU device (0 or 1)> <use GPU device (0 or 1)> <GPU workgroup size (0=default value or anything > 0)> <window width> <window height> <scene file>" << std::endl;
		std::cerr << "Usage (batch mode): " << argv[0] << " <low latency mode enabled (0 or 1)> <native thread count> <use CPU device (0 or 1)> <use GPU device (0 or 1)> <GPU workgroup size (0=default value or anything > 0)> <window width> <window height> <halt time in secs> <scene file>" << std::endl;
		std::cerr << "Usage (with configuration file mode): " << argv[0] << " <configuration file name>" << std::endl;

		// It is important to initialize OpenGL before OpenCL
		unsigned int width;
		unsigned int height;
		if (argc == 10) {
			width = atoi(argv[6]);
			height = atoi(argv[7]);
			const bool lowLatencyMode = (atoi(argv[1]) == 1);
			config = new RenderingConfig(lowLatencyMode, argv[9], width, height, atoi(argv[2]), (atoi(argv[3]) == 1), (atoi(argv[4]) == 1), atoi(argv[5]));
			return BatchMode(atoi(argv[8]), 0);
		} else if (argc == 9) {
			width = atoi(argv[6]);
			height = atoi(argv[7]);
		} else if (argc == 5) {
			config = new RenderingConfig(false, "scenes/luxball/luxball.scn", 640, 480, atoi(argv[1]), (atoi(argv[2]) == 1), (atoi(argv[3]) == 1), atoi(argv[4]));
			return BatchMode(180.0, 0);
		} else if (argc == 2) {
			config = new RenderingConfig(argv[1]);
			width = config->cfg.GetInt("image.width", 640);
			height = config->cfg.GetInt("image.height", 480);

			const unsigned int halttime = config->cfg.GetInt("batch.halttime", 0);
			const unsigned int haltspp = config->cfg.GetInt("batch.haltspp", 0);
			if ((halttime > 0) || (haltspp > 0)) {
				config->Init();
				return BatchMode(halttime, haltspp);
			}
		} else  if (argc == 1) {
			width = 640;
			height = 480;
		} else
			exit(-1);

		InitGlut(argc, argv, width, height);

		if (argc == 9) {
			const bool lowLatencyMode = (atoi(argv[1]) == 1);
			config = new RenderingConfig(lowLatencyMode, argv[8], width, height, atoi(argv[2]), (atoi(argv[3]) == 1), (atoi(argv[4]) == 1), atoi(argv[5]));

			if (!lowLatencyMode)
				config->screenRefreshInterval = 2000;
		} else if (argc == 2) {
			config->Init();
		} else if (argc == 1)
			config = new RenderingConfig(true, "scenes/simple.scn", width, height, boost::thread::hardware_concurrency(), false, true, 0);
		else
			exit(-1);

		RunGlut();
	} catch (cl::Error err) {
		std::cerr << "OpenCL ERROR: " << err.what() << "(" << err.err() << ")" << std::endl;
	} catch (std::runtime_error err) {
		std::cerr << "ERROR: " << err.what() << std::endl;
	}

	return EXIT_SUCCESS;
}
