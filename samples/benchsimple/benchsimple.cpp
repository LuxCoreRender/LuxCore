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

#include "luxrays/luxrays.h"
#include "luxrays/core/context.h"
#include "luxrays/core/device.h"
#include "luxrays/utils/randomgen.h"

#define RAYBUFFERS_COUNT 10
#define TRIANGLE_COUNT 1000000

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
	luxrays::DeviceDescription::FilterOne(deviceDescs);

	if (deviceDescs.size() < 1) {
		std::cerr << "Unable to find a GPU or CPU intersection device" << std::endl;
		return (EXIT_FAILURE);
	}

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
		verts[vIndex].x = (100.f * rnd.floatValue() - 50.f) + v0.x;
		verts[vIndex].y = (100.f * rnd.floatValue() - 50.f) + v0.y;
		verts[vIndex].z = (100.f * rnd.floatValue() - 50.f) + v0.z;

		verts[vIndex + 1].x = (100.f * rnd.floatValue() - 50.f) + v1.x;
		verts[vIndex + 1].y = (100.f * rnd.floatValue() - 50.f) + v1.y;
		verts[vIndex + 1].z = (100.f * rnd.floatValue() - 50.f) + v1.z;

		verts[vIndex + 2].x = (100.f * rnd.floatValue() - 50.f) + v2.x;
		verts[vIndex + 2].y = (100.f * rnd.floatValue() - 50.f) + v2.y;
		verts[vIndex + 2].z = (100.f * rnd.floatValue() - 50.f) + v2.z;

		tris[i].v[0] = vIndex;
		tris[i].v[1] = vIndex + 1;
		tris[i].v[2] = vIndex + 2;
	}

	luxrays::TriangleMesh *mesh = new luxrays::TriangleMesh(TRIANGLE_COUNT * 3, TRIANGLE_COUNT, verts, tris);
	luxrays::DataSet *dataSet = new luxrays::DataSet(ctx);
	dataSet->Add(mesh);
	dataSet->Preprocess();
	ctx->SetCurrentDataSet(dataSet);

	//--------------------------------------------------------------------------
	// Build the ray buffers
	//--------------------------------------------------------------------------

	std::cerr << "Creating " << RAYBUFFERS_COUNT << " ray buffers" << std::endl;

	luxrays::RayBuffer *rayBuffers[RAYBUFFERS_COUNT];
	for (size_t i = 0; i < RAYBUFFERS_COUNT; ++i) {
		rayBuffers[i] = device->NewRayBuffer();

		luxrays::RayBuffer *rayBuffer = rayBuffers[i];
		luxrays::Ray *rays = rayBuffer->GetRayBuffer();
		for (size_t j = 0; j < rayBuffer->GetSize(); ++i) {
			rays[j].o = luxrays::Point(100.f * rnd.floatValue() - 50.f, 100.f * rnd.floatValue() - 50.f, 100.f * rnd.floatValue() - 50.f);

			do {
				rays[j].d = luxrays::Vector(rnd.floatValue(), rnd.floatValue(), rnd.floatValue());
			} while (rays[j].d == luxrays::Vector(0.f, 0.f, 0.f)); // Just in case ...
			rays[j].d = luxrays::Normalize(rays[j].d);

			rays[j].mint = RAY_EPSILON;
			rays[j].maxt = 1000.f;
		}
	}

	//--------------------------------------------------------------------------
	// Run the benchmark
	//--------------------------------------------------------------------------

	std::cerr << "Running the benchmark for 30 seconds..." << std::endl;

	/*double tStart = luxrays::WallClockTime();
	for (;;) {

	}
	double tStop = luxrays::WallClockTime();*/

	//--------------------------------------------------------------------------
	// Free everything
	//--------------------------------------------------------------------------

	delete ctx;
	delete[] tris;
	delete[] verts;

	std::cerr << "Done." << std::endl;

	return (EXIT_SUCCESS);
}
