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

#include <cstdlib>
#include <queue>
#include <iostream>
#include <iomanip>

#include "luxrays/luxrays.h"
#include "luxrays/core/context.h"
#include "luxrays/core/pixeldevice.h"
#include "luxrays/utils/core/randomgen.h"

#define WIDTH 640
#define HEIGHT 480

void DebugHandler(const char *msg) {
	std::cerr << msg << std::endl;
}

static void RunAddSampleBench(luxrays::PixelDevice *device, const luxrays::FilterType type) {
	std::cerr << "Running the benchmark for 10 seconds..." << std::endl;

	double tStart = luxrays::WallClockTime();
	double tLastCheck = tStart;
	double bufferDone = 0.0;
	double sampleCount = 0.0;
	bool done = false;
	const luxrays::Spectrum white(1.f, 1.f, 1.f);
	while (!done) {
		luxrays::SampleBuffer *sb = device->GetFreeSampleBuffer();

		// Fill the sample buffer
		for (unsigned int i = 0; i < sb->GetSize(); ++i)
			sb->SplatSample(i % WIDTH, i / WIDTH, white);
		sampleCount += sb->GetSampleCount();

		device->AddSampleBuffer(type, sb);

		// Check if it is time to stop
		const double tNow = luxrays::WallClockTime();
		if (tNow - tLastCheck > 1.0) {
			if (tNow - tStart > 10.0) {
				done = true;
				break;
			}

			std::cerr << int(tNow - tStart) << "/10secs" << std::endl;
			tLastCheck = tNow;
		}
		bufferDone += 1.0;
	}

	double tStop = luxrays::WallClockTime();
	double tTime = tStop - tStart;

	std::cerr << "Test total time: " << tTime << std::endl;
	std::cerr << "Test total sample buffer count: " << int(bufferDone) << std::endl;
	std::cerr << "[" << device->GetName() << "][Samples/sec " << std::setiosflags(std::ios::fixed) << std::setprecision(2) <<
			(device->GetPerformance() / 1000000.0) << "M]" << std::endl;

	double sampleSec = sampleCount / tTime;
	if (sampleSec < 10000.0)
		std::cerr << "Test performance: " << std::setiosflags(std::ios::fixed) << std::setprecision(2) <<
				sampleSec <<" sample/sec" << std::endl;
	else if (sampleSec < 1000000.0)
		std::cerr << "Test performance: " << std::setiosflags(std::ios::fixed) << std::setprecision(2) <<
				(sampleSec / 1000.0) <<"K sample/sec" << std::endl;
	else
		std::cerr << "Test performance: " << std::setiosflags(std::ios::fixed) << std::setprecision(2) <<
				(sampleSec / 1000000.0) <<"M sample/sec" << std::endl;
}

int main(int argc, char** argv) {
	std::cerr << "LuxRays Simple PixelDevice Benchmark v" << LUXRAYS_VERSION_MAJOR << "." << LUXRAYS_VERSION_MINOR << std::endl;
	std::cerr << "Usage (easy mode): " << argv[0] << std::endl;
	std::cerr << "Usage (select native device): " << argv[0] << " -1" << std::endl;
	std::cerr << "Usage (select opencl device): " << argv[0] << " <OpenCL device index>" << std::endl;

	int devIndex = -1;
	if (argc > 1)
		devIndex = atoi(argv[1]);

	//--------------------------------------------------------------------------
	// Create the context
	//--------------------------------------------------------------------------

	luxrays::Context *ctx = new luxrays::Context(DebugHandler);

	// Looks for the first GPU device
	std::vector<luxrays::DeviceDescription *> deviceDescs = std::vector<luxrays::DeviceDescription *>(ctx->GetAvailableDeviceDescriptions());

	if (argc > 1) {
		const int devIndex = atoi(argv[1]);
		if (devIndex == -1) {
			// Use native device
			luxrays::DeviceDescription::Filter(luxrays::DEVICE_TYPE_NATIVE_THREAD, deviceDescs);
		} else {
			// Use selected device
			luxrays::DeviceDescription::Filter(luxrays::DEVICE_TYPE_OPENCL, deviceDescs);
			if ((devIndex < 0) || (devIndex >= int(deviceDescs.size()))) {
				std::cerr << "Wrong OpenCL device index" << std::endl;
				return (EXIT_FAILURE);
			}
			luxrays::DeviceDescription *device = deviceDescs[devIndex];
			deviceDescs.resize(1);
			deviceDescs[0] = device;
		}
	} else {
		// Use default device
		luxrays::DeviceDescription::FilterOne(deviceDescs);
	}

	if (deviceDescs.size() < 1) {
		std::cerr << "Unable to find a GPU or CPU pixel device" << std::endl;
		return (EXIT_FAILURE);
	}
	deviceDescs.resize(1);

	std::cerr << "Selected pixel device: " << deviceDescs[0]->GetName() << std::endl;
	std::vector<luxrays::PixelDevice *> devices = ctx->AddPixelDevices(deviceDescs);
	luxrays::PixelDevice *device = devices[0];
	device->Init(WIDTH, HEIGHT);

	ctx->Start();

	//--------------------------------------------------------------------------
	// Run the benchmark
	//--------------------------------------------------------------------------

	std::cerr << "AddSample[FILTER_NONE] Benchmark" << std::endl;
	RunAddSampleBench(device, luxrays::FILTER_NONE);
	std::cerr << "AddSample[FILTER_PREVIEW] Benchmark" << std::endl;
	RunAddSampleBench(device, luxrays::FILTER_PREVIEW);
	std::cerr << "AddSample[FILTER_GAUSSIAN] Benchmark" << std::endl;
	RunAddSampleBench(device, luxrays::FILTER_GAUSSIAN);

	//--------------------------------------------------------------------------
	// Free everything
	//--------------------------------------------------------------------------

	ctx->Stop();
	delete ctx;

	std::cerr << "Done." << std::endl;

	return EXIT_SUCCESS;
}
