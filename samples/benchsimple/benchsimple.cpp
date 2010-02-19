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

#include "luxrays/luxrays.h"
#include "luxrays/core/context.h"
#include "luxrays/core/device.h"
#include "luxrays/utils/core/randomgen.h"

#define RAYBUFFERS_COUNT 10
#define TRIANGLE_COUNT 100
#define SPACE_SIZE 1000.f

void DebugHandler(const char *msg) {
	std::cerr << msg << std::endl;
}

int main(int argc, char** argv) {
	std::cerr << "LuxRays Simple Benchmark v" << LUXRAYS_VERSION_MAJOR << "." << LUXRAYS_VERSION_MINOR << std::endl;
	std::cerr << "Usage (easy mode): " << argv[0] << std::endl;

	//--------------------------------------------------------------------------
	// Create the context
	//--------------------------------------------------------------------------

	luxrays::Context *ctx = new luxrays::Context(DebugHandler);

	// Looks for the first GPU device
	std::vector<luxrays::DeviceDescription *> deviceDescs = std::vector<luxrays::DeviceDescription *>(ctx->GetAvailableDeviceDescriptions());
	//luxrays::DeviceDescription::FilterOne(deviceDescs);

	luxrays::DeviceDescription::Filter(luxrays::DEVICE_TYPE_NATIVE_THREAD, deviceDescs);

	//luxrays::DeviceDescription::Filter(luxrays::DEVICE_TYPE_OPENCL, deviceDescs);
	//luxrays::OpenCLDeviceDescription::Filter(luxrays::OCL_DEVICE_TYPE_CPU, deviceDescs);

	if (deviceDescs.size() < 1) {
		std::cerr << "Unable to find a GPU or CPU intersection device" << std::endl;
		return (EXIT_FAILURE);
	}
	deviceDescs.resize(1);

	std::cerr << "Selected intersection device: " << deviceDescs[0]->GetName();
	ctx->AddIntersectionDevices(deviceDescs);
	luxrays::IntersectionDevice *device = ctx->GetIntersectionDevices() [0];

	//--------------------------------------------------------------------------
	// Build the data set
	//--------------------------------------------------------------------------

	std::cerr << "Creating a " << TRIANGLE_COUNT << " triangle data set" << std::endl;

	luxrays::RandomGenerator rnd;
	luxrays::Point *verts = new luxrays::Point[TRIANGLE_COUNT * 3];
	luxrays::Triangle *tris = new luxrays::Triangle[TRIANGLE_COUNT];

	rnd.init(1u);
	for (size_t i = 0; i < TRIANGLE_COUNT; ++i) {
		luxrays::Vector v0(0.1f * rnd.floatValue(), 0.1f * rnd.floatValue(), 0.1f * rnd.floatValue());
		v0.x += 0.1f * luxrays::Sgn(v0.x);
		v0.y += 0.1f * luxrays::Sgn(v0.y);
		v0.z += 0.1f * luxrays::Sgn(v0.z);

		luxrays::Vector v1, v2;
		luxrays::CoordinateSystem(v0, &v1, &v2);
		v1 *= 0.1f;
		v2 *= 0.1f;

		size_t vIndex = i * 3;
		verts[vIndex].x = (SPACE_SIZE * rnd.floatValue() - SPACE_SIZE / 2.f) + v0.x;
		verts[vIndex].y = (SPACE_SIZE * rnd.floatValue() - SPACE_SIZE / 2.f) + v0.y;
		verts[vIndex].z = (SPACE_SIZE * rnd.floatValue() - SPACE_SIZE / 2.f) + v0.z;

		verts[vIndex + 1].x = (SPACE_SIZE * rnd.floatValue() - SPACE_SIZE / 2.f) + v1.x;
		verts[vIndex + 1].y = (SPACE_SIZE * rnd.floatValue() - SPACE_SIZE / 2.f) + v1.y;
		verts[vIndex + 1].z = (SPACE_SIZE * rnd.floatValue() - SPACE_SIZE / 2.f) + v1.z;

		verts[vIndex + 2].x = (SPACE_SIZE * rnd.floatValue() - SPACE_SIZE / 2.f) + v2.x;
		verts[vIndex + 2].y = (SPACE_SIZE * rnd.floatValue() - SPACE_SIZE / 2.f) + v2.y;
		verts[vIndex + 2].z = (SPACE_SIZE * rnd.floatValue() - SPACE_SIZE / 2.f) + v2.z;

		tris[i].v[0] = vIndex;
		tris[i].v[1] = vIndex + 1;
		tris[i].v[2] = vIndex + 2;
	}

	luxrays::TriangleMesh *mesh = new luxrays::TriangleMesh(TRIANGLE_COUNT * 3, TRIANGLE_COUNT, verts, tris);
	luxrays::DataSet *dataSet = new luxrays::DataSet(ctx);
	dataSet->Add(mesh);
	dataSet->Preprocess();
	ctx->SetCurrentDataSet(dataSet);

	ctx->Start();
	device->Start();

	//--------------------------------------------------------------------------
	// Build the ray buffers
	//--------------------------------------------------------------------------

	std::cerr << "Creating " << RAYBUFFERS_COUNT << " ray buffers" << std::endl;

	luxrays::Ray ray;
	std::queue<luxrays::RayBuffer *> todoRayBuffers;
	for (size_t i = 0; i < RAYBUFFERS_COUNT; ++i) {
		luxrays::RayBuffer *rayBuffer = device->NewRayBuffer();
		todoRayBuffers.push(rayBuffer);

		for (size_t j = 0; j < rayBuffer->GetSize(); ++j) {
			ray.o = luxrays::Point(SPACE_SIZE * rnd.floatValue() - SPACE_SIZE / 2.f,
					SPACE_SIZE * rnd.floatValue() - SPACE_SIZE / 2.f,
					SPACE_SIZE * rnd.floatValue() - SPACE_SIZE / 2.f);

			do {
				ray.d = luxrays::Vector(rnd.floatValue(), rnd.floatValue(), rnd.floatValue());
			} while (ray.d == luxrays::Vector(0.f, 0.f, 0.f)); // Just in case ...
			ray.d = luxrays::Normalize(ray.d);

			ray.mint = RAY_EPSILON;
			ray.maxt = 1000.f;

			rayBuffer->AddRay(ray);
		}
	}

	//--------------------------------------------------------------------------
	// Run the benchmark
	//--------------------------------------------------------------------------

	std::cerr << "Running the benchmark for 30 seconds..." << std::endl;

	double tStart = luxrays::WallClockTime();
	double tLastCheck = tStart;
	double bufferDone = 0.0;
	bool done = false;
	while (!done) {
		while (todoRayBuffers.size() > 0) {
			device->PushRayBuffer(todoRayBuffers.front());
			todoRayBuffers.pop();

			// Check if it is time to stop
			const double tNow = luxrays::WallClockTime();
			if (tNow - tLastCheck > 1.0) {
				if (tNow - tStart > 30.0) {
					done = true;
					break;
				}

				std::cerr << int(tNow - tStart) << "/30secs" << std::endl;
				tLastCheck = tNow;
			}
		}

		todoRayBuffers.push(device->PopRayBuffer());
		bufferDone += 1.0;
	}

	while (todoRayBuffers.size() != RAYBUFFERS_COUNT) {
		todoRayBuffers.push(device->PopRayBuffer());
		bufferDone += 1.0;
	}
	double tStop = luxrays::WallClockTime();
	double tTime = tStop - tStart;

	std::cerr << "Test total time: " << tTime << std::endl;
	std::cerr << "Test total ray buffer count: " << int(bufferDone) << std::endl;
	std::cerr << "Test ray buffer size: " << todoRayBuffers.front()->GetRayCount() << std::endl;
	double raySec = (bufferDone * todoRayBuffers.front()->GetRayCount()) / tTime;
	if (raySec < 10000.0)
		std::cerr << "Test performance: " << int(raySec) <<" ray/sec" << std::endl;
	else
		std::cerr << "Test performance: " << int(raySec / 1000.0) <<"K ray/sec" << std::endl;

	//--------------------------------------------------------------------------
	// Free everything
	//--------------------------------------------------------------------------

	while (todoRayBuffers.size() > 0) {
		delete todoRayBuffers.front();
		todoRayBuffers.pop();
	}

	ctx->Stop();
	delete ctx;
	mesh->Delete();
	delete mesh;

	std::cerr << "Done." << std::endl;

	return EXIT_SUCCESS;
}
