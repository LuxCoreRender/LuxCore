/***************************************************************************
 * Copyright 1998-2013 by authors (see AUTHORS.txt)                        *
 *                                                                         *
 *   This file is part of LuxRender.                                       *
 *                                                                         *
 * Licensed under the Apache License, Version 2.0 (the "License");         *
 * you may not use this file except in compliance with the License.        *
 * You may obtain a copy of the License at                                 *
 *                                                                         *
 *     http://www.apache.org/licenses/LICENSE-2.0                          *
 *                                                                         *
 * Unless required by applicable law or agreed to in writing, software     *
 * distributed under the License is distributed on an "AS IS" BASIS,       *
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.*
 * See the License for the specific language governing permissions and     *
 * limitations under the License.                                          *
 ***************************************************************************/

#include <cstdlib>
#include <queue>
#include <iostream>
#include <iomanip>

#include "luxrays/luxrays.h"
#include "luxrays/core/context.h"
#include "luxrays/core/intersectiondevice.h"
#include "luxrays/core/oclintersectiondevice.h"
#include "luxrays/core/randomgen.h"

#define RAYBUFFERS_COUNT 10
#define TRIANGLE_COUNT 500
#define SPACE_SIZE 1000.f

void DebugHandler(const char *msg) {
	std::cerr << msg << std::endl;
}

int main(int argc, char** argv) {
	try {
		std::cerr << "LuxRays Simple IntersectionDevice Benchmark v" << LUXRAYS_VERSION_MAJOR << "." << LUXRAYS_VERSION_MINOR << std::endl;
		std::cerr << "Usage: " << argv[0] << std::endl;

		//--------------------------------------------------------------------------
		// Create the context
		//--------------------------------------------------------------------------

		luxrays::Context *ctx = new luxrays::Context(DebugHandler);

		// Get the list of all devices available
		std::vector<luxrays::DeviceDescription *> deviceDescs = std::vector<luxrays::DeviceDescription *>(ctx->GetAvailableDeviceDescriptions());

		// Use the first device available
		//luxrays::DeviceDescription::FilterOne(deviceDescs);

		// Use the first native C++ device available
		luxrays::DeviceDescription::Filter(luxrays::DEVICE_TYPE_NATIVE_THREAD, deviceDescs);

		// Use the first OpenCL device available
		//luxrays::DeviceDescription::Filter(luxrays::DEVICE_TYPE_OPENCL_ALL, deviceDescs);
		//luxrays::DeviceDescription::FilterOne(deviceDescs);

		// Use the first OpenCL CPU device available
		//luxrays::DeviceDescription::Filter(luxrays::DEVICE_TYPE_OPENCL_ALL, deviceDescs);
		//luxrays::OpenCLDeviceDescription::Filter(luxrays::DEVICE_TYPE_OPENCL_CPU, deviceDescs);

		// Use the first OpenCL GPU device available
		//luxrays::DeviceDescription::Filter(luxrays::DEVICE_TYPE_OPENCL_ALL, deviceDescs);
		//luxrays::OpenCLDeviceDescription::Filter(luxrays::DEVICE_TYPE_OPENCL_GPU, deviceDescs);

		// Use all GPU devices available (for virtual device)
		//luxrays::DeviceDescription::Filter(luxrays::DEVICE_TYPE_OPENCL_ALL, deviceDescs);
		//luxrays::OpenCLDeviceDescription::Filter(luxrays::DEVICE_TYPE_OPENCL_GPU, deviceDescs);
		
		if (deviceDescs.size() < 1) {
			std::cerr << "Unable to find an usable intersection device" << std::endl;
			return (EXIT_FAILURE);
		}

		// Single device
		deviceDescs.resize(1);
		std::cerr << "Selected intersection device: " << deviceDescs[0]->GetName();
		luxrays::IntersectionDevice *device = ctx->AddIntersectionDevices(deviceDescs)[0];

		// Multiple devices
		//ctx->AddVirtualIntersectionDevices(deviceDescs);
		//luxrays::IntersectionDevice *device = ctx->GetIntersectionDevice()[0];
		

		// If it is a NativeThreadIntersectionDevice, you can set the number of threads
		// to use. The default is to use one for each core available.
		//((luxrays::NativeThreadIntersectionDevice *)device)->SetThreadCount(1);

		//--------------------------------------------------------------------------
		// Build the data set
		//--------------------------------------------------------------------------

		std::cerr << "Creating a " << TRIANGLE_COUNT << " triangle data set" << std::endl;

		luxrays::RandomGenerator rnd(1u);
		luxrays::Point *verts = new luxrays::Point[TRIANGLE_COUNT * 3];
		luxrays::Triangle *tris = new luxrays::Triangle[TRIANGLE_COUNT];

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
		ctx->SetDataSet(dataSet);

		//--------------------------------------------------------------------------
		// Build the ray buffers
		//--------------------------------------------------------------------------

		std::cerr << "Creating " << RAYBUFFERS_COUNT << " ray buffers" << std::endl;

		luxrays::Ray ray;
		std::queue<luxrays::RayBuffer *> todoRayBuffers;
		std::vector<luxrays::RayBuffer *> rayBuffers;
		for (size_t i = 0; i < RAYBUFFERS_COUNT; ++i) {
			luxrays::RayBuffer *rayBuffer = device->NewRayBuffer();
			todoRayBuffers.push(rayBuffer);
			rayBuffers.push_back(rayBuffer);

			for (size_t j = 0; j < rayBuffer->GetSize(); ++j) {
				ray.o = luxrays::Point(SPACE_SIZE * rnd.floatValue() - SPACE_SIZE / 2.f,
						SPACE_SIZE * rnd.floatValue() - SPACE_SIZE / 2.f,
						SPACE_SIZE * rnd.floatValue() - SPACE_SIZE / 2.f);

				do {
					ray.d = luxrays::Vector(rnd.floatValue(), rnd.floatValue(), rnd.floatValue());
				} while (ray.d == luxrays::Vector(0.f, 0.f, 0.f)); // Just in case ...
				ray.d = luxrays::Normalize(ray.d);

				ray.mint = luxrays::MachineEpsilon::E(ray.o);
				ray.maxt = 1000.f;

				rayBuffer->AddRay(ray);
			}
		}

		//--------------------------------------------------------------------------
		// Run the serial benchmark. This simulate one single thread pushing
		// rays to trace.
		//--------------------------------------------------------------------------

		{
			// The number of queues used
			device->SetQueueCount(1);
			// You have to set the max. number of buffers you can push between 2 pop.
			device->SetBufferCount(RAYBUFFERS_COUNT);

			ctx->Start();

			std::cerr << "Running the serial benchmark for 15 seconds..." << std::endl;
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
						if (tNow - tStart > 15.0) {
							done = true;
							break;
						}
	
						std::cerr << int(tNow - tStart) << "/15secs" << std::endl;
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
	
			ctx->Stop();

			std::cerr << "Test total time: " << tTime << std::endl;
			std::cerr << "Test total ray buffer count: " << int(bufferDone) << std::endl;
			std::cerr << "Test ray buffer size: " << todoRayBuffers.front()->GetRayCount() << std::endl;
			double raySec = (bufferDone * todoRayBuffers.front()->GetRayCount()) / tTime;
			if (raySec < 10000.0)
				std::cerr << "Test performance: " << std::setiosflags(std::ios::fixed) << std::setprecision(2) <<
						raySec <<" rays/sec" << std::endl;
			else if (raySec < 1000000.0)
				std::cerr << "Test performance: " << std::setiosflags(std::ios::fixed) << std::setprecision(2) <<
						(raySec / 1000.0) <<"K rays/sec" << std::endl;
			else
				std::cerr << "Test performance: " << std::setiosflags(std::ios::fixed) << std::setprecision(2) <<
						(raySec / 1000000.0) <<"M rays/sec" << std::endl;
		}

		//--------------------------------------------------------------------------
		// Run the parallel benchmark. This simulate multiple threads pushing
		// rays to trace.
		//--------------------------------------------------------------------------

		{
			// The number of queues used
			device->SetQueueCount(RAYBUFFERS_COUNT);
			// You have to set the max. number of buffers you can push between 2 pop.
			device->SetBufferCount(1);

			ctx->Start();

			std::cerr << "Running the parallel benchmark for 15 seconds..." << std::endl;
			double tStart = luxrays::WallClockTime();

			for (u_int i = 0; i < RAYBUFFERS_COUNT; ++i)
				device->PushRayBuffer(rayBuffers[i], i);

			double tLastCheck = tStart;
			double bufferDone = 0.0;
			bool done = false;
			u_int queueIndex = 0;
			while (!done) {
				device->PopRayBuffer(queueIndex);
				bufferDone += 1.0;
	
				device->PushRayBuffer(rayBuffers[queueIndex], queueIndex);
	
				// Check if it is time to stop
				const double tNow = luxrays::WallClockTime();
				if (tNow - tLastCheck > 1.0) {
					if (tNow - tStart > 15.0) {
						done = true;
						break;
					}
	
					std::cerr << int(tNow - tStart) << "/15secs" << std::endl;
					tLastCheck = tNow;
				}
	
				queueIndex = (queueIndex + 1) % RAYBUFFERS_COUNT;
			}

			for (u_int i = 0; i < RAYBUFFERS_COUNT; ++i)
				device->PopRayBuffer(i);
			bufferDone += RAYBUFFERS_COUNT;

			double tStop = luxrays::WallClockTime();
			double tTime = tStop - tStart;

			ctx->Stop();

			std::cerr << "Test total time: " << tTime << std::endl;
			std::cerr << "Test total ray buffer count: " << int(bufferDone) << std::endl;
			std::cerr << "Test ray buffer size: " << todoRayBuffers.front()->GetRayCount() << std::endl;
			double raySec = (bufferDone * todoRayBuffers.front()->GetRayCount()) / tTime;
			if (raySec < 10000.0)
				std::cerr << "Test performance: " << std::setiosflags(std::ios::fixed) << std::setprecision(2) <<
						raySec <<" rays/sec" << std::endl;
			else if (raySec < 1000000.0)
				std::cerr << "Test performance: " << std::setiosflags(std::ios::fixed) << std::setprecision(2) <<
						(raySec / 1000.0) <<"K rays/sec" << std::endl;
			else
				std::cerr << "Test performance: " << std::setiosflags(std::ios::fixed) << std::setprecision(2) <<
						(raySec / 1000000.0) <<"M rays/sec" << std::endl;
		}

		//--------------------------------------------------------------------------
		// Free everything
		//--------------------------------------------------------------------------

		while (todoRayBuffers.size() > 0) {
			delete todoRayBuffers.front();
			todoRayBuffers.pop();
		}

		delete ctx;
		delete dataSet;
		mesh->Delete();
		delete mesh;

		std::cerr << "Done." << std::endl;
#if !defined(LUXRAYS_DISABLE_OPENCL)
	} catch (cl::Error err) {
		std::cout << "OpenCL ERROR: " << err.what() << "(" << luxrays::oclErrorString(err.err()) << ")" << std::endl;
		return EXIT_FAILURE;
#endif
	} catch (std::runtime_error err) {
		std::cout << "RUNTIME ERROR: " << err.what() << std::endl;
		return EXIT_FAILURE;
	} catch (std::exception err) {
		std::cout << "ERROR: " << err.what() << std::endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
