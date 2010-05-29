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

// Based on smallppm, Progressive Photon Mapping by T. Hachisuka

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <sstream>
#include <stdexcept>

#if defined (WIN32)
#include <windows.h>
#endif

#include <FreeImage.h>

#include "luxrays/luxrays.h"
#include "luxrays/core/device.h"
#include "luxrays/utils/sdl/scene.h"
#include "luxrays/utils/core/randomgen.h"
#include "luxrays/core/intersectiondevice.h"
#include "luxrays/core/pixel/framebuffer.h"

struct HitPoint {
	luxrays::Vector position;
	luxrays::Vector normal;
	luxrays::Spectrum brdf;
	unsigned int pixelIndex;

	float photonRadius2;
	unsigned int accumPhotonCount;
	luxrays::Spectrum accumReflectedFlux;
};

void FreeImageErrorHandler(FREE_IMAGE_FORMAT fif, const char *message) {
	printf("\n*** ");
	if(fif != FIF_UNKNOWN)
		printf("%s Format\n", FreeImage_GetFormatFromFIF(fif));

	printf("%s", message);
	printf(" ***\n");
}

void DebugHandler(const char *msg) {
	std::cerr << msg << std::endl;
}

void SaveFrameBuffer(const std::string &fileName, const luxrays::FrameBuffer &frameBuffer) {
	std::cerr << "Saving " << fileName << std::endl;

	FREE_IMAGE_FORMAT fif = FreeImage_GetFIFFromFilename(fileName.c_str());
	if (fif != FIF_UNKNOWN) {
		const unsigned int width = frameBuffer.GetWidth();
		const unsigned int height = frameBuffer.GetHeight();
		FIBITMAP *dib = FreeImage_Allocate(width, height, 24);

		if (dib) {
			unsigned int pitch = FreeImage_GetPitch(dib);
			BYTE *bits = (BYTE *) FreeImage_GetBits(dib);
			const float *pixels = (float *)frameBuffer.GetPixels();

			for (unsigned int y = 0; y < height; ++y) {
				BYTE *pixel = (BYTE *) bits;
				for (unsigned int x = 0; x < width; ++x) {
					const int offset = 3 * (x + y * width);
					pixel[FI_RGBA_RED] = (BYTE) (pixels[offset] * 255.f + .5f);
					pixel[FI_RGBA_GREEN] = (BYTE) (pixels[offset + 1] * 255.f + .5f);
					pixel[FI_RGBA_BLUE] = (BYTE) (pixels[offset + 2] * 255.f + .5f);
					pixel += 3;
				}

				// Next line
				bits += pitch;
			}

			if (!FreeImage_Save(fif, dib, fileName.c_str(), 0))
				std::cerr << "Failed image save: " << fileName << std::endl;

			FreeImage_Unload(dib);
		} else
			std::cerr << "Unable to allocate FreeImage image: " << fileName << std::endl;
	} else
		std::cerr << "Image type unknown: " << fileName << std::endl;
}

int main(int argc, char *argv[]) {
	std::cerr << "LuxRays SmallPPMGPU v" << LUXRAYS_VERSION_MAJOR << "." << LUXRAYS_VERSION_MINOR << std::endl;
	std::cerr << "Usage (easy mode): " << argv[0] << std::endl;

	try {
		//----------------------------------------------------------------------
		// Parse command line options
		//----------------------------------------------------------------------

		std::cerr << "Usage: " << argv[0] << " [options] [scene file]" << std::endl <<
				" -w [window width]" << std::endl <<
				" -e [window height]" << std::endl <<
				" -h <display this help and exit>" << std::endl;

		unsigned int width = 640;
		unsigned int height = 480;
		std::string sceneFileName = "scenes/simple/simple.scn";
		for (int i = 1; i < argc; i++) {
			if (argv[i][0] == '-') {
				// I should check for out of range array index...

				if (argv[i][1] == 'h') exit(EXIT_SUCCESS);

				else if (argv[i][1] == 'e') height = atoi(argv[++i]);

				else if (argv[i][1] == 'w') width = atoi(argv[++i]);

				else {
					std::cerr << "Invalid option: " << argv[i] << std::endl;
					exit(EXIT_FAILURE);
				}
			} else {
				std::string s = argv[i];
				if ((s.length() >= 4) && (s.substr(s.length() - 4) == ".scn")) {
					sceneFileName = s;
				} else
					throw std::runtime_error("Unknow file extension: " + s);
			}
		}
		const unsigned int pixelCount = width * height;

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
		std::vector<luxrays::IntersectionDevice *> devices = ctx->AddIntersectionDevices(deviceDescs);
		luxrays::IntersectionDevice *device = devices[0];

		//----------------------------------------------------------------------
		// Read the scene
		//----------------------------------------------------------------------

		luxrays::sdl::Scene *scene = new luxrays::sdl::Scene(ctx, sceneFileName);
		scene->camera->Update(width, height);

		// Set the Luxrays SataSet
		ctx->SetDataSet(scene->dataSet);
		ctx->Start();

		//----------------------------------------------------------------------
		// Allocate frame buffer
		//----------------------------------------------------------------------

		luxrays::FrameBuffer frameBuffer(width, height);

		//----------------------------------------------------------------------
		// Build the HitPoint list
		//----------------------------------------------------------------------

		luxrays::RayBuffer *rayBuffer = device->NewRayBuffer();
		luxrays::RandomGenerator rndGen;
		rndGen.init(7);

		std::vector<HitPoint> hitPoints(pixelCount);
		unsigned int hitPointIndex = 0;
		std::cerr << "Building hitPoints list:" << std::endl;
		for (unsigned int y = 0; y < height; ++y) {
			if (y % 25 == 0)
				std::cerr << "  " << y << "/" << height << std::endl;

			for (unsigned int x = 0; x < width; ++x) {
				if (rayBuffer->IsFull()) {
					// Trace the rays
					device->PushRayBuffer(rayBuffer);
					rayBuffer = device->PopRayBuffer();

					for (unsigned int i = 0; i < rayBuffer->GetRayCount(); ++i) {
						const luxrays::RayHit *rayHit = &rayBuffer->GetHitBuffer()[i];
						if (rayHit->Miss())
							frameBuffer.SetPixel(hitPointIndex++, luxrays::Spectrum());
						else {
							// Something was hit
							const unsigned int currentTriangleIndex = rayHit->index;

							// Get the triangle
							const luxrays::ExtTriangleMesh *mesh = scene->objects[scene->dataSet->GetMeshID(currentTriangleIndex)];
							const luxrays::Triangle &tri = mesh->GetTriangles()[scene->dataSet->GetMeshTriangleID(currentTriangleIndex)];

							frameBuffer.SetPixel(hitPointIndex++, InterpolateTriColor(tri, mesh->GetColors(), rayHit->b1, rayHit->b2));
						}
					}

					rayBuffer->Reset();
				}

				luxrays::Ray eyeRay;
				scene->camera->GenerateRay(x, y, width, height, &eyeRay,
						rndGen.floatValue(), rndGen.floatValue(), rndGen.floatValue());
				rayBuffer->AddRay(eyeRay);
			}
		}

		// Trace all left rays
		if (rayBuffer->GetRayCount() > 0) {
			device->PushRayBuffer(rayBuffer);
			rayBuffer = device->PopRayBuffer();

			for (unsigned int i = 0; i < rayBuffer->GetRayCount(); ++i) {
				const luxrays::RayHit *rayHit = &rayBuffer->GetHitBuffer()[i];
				if (rayHit->Miss())
					frameBuffer.SetPixel(hitPointIndex++, luxrays::Spectrum());
				else {
					// Something was hit
					const unsigned int currentTriangleIndex = rayHit->index;

					// Get the triangle
					const luxrays::ExtTriangleMesh *mesh = scene->objects[scene->dataSet->GetMeshID(currentTriangleIndex)];
					const luxrays::Triangle &tri = mesh->GetTriangles()[scene->dataSet->GetMeshTriangleID(currentTriangleIndex)];

					frameBuffer.SetPixel(hitPointIndex++, InterpolateTriColor(tri, mesh->GetColors(), rayHit->b1, rayHit->b2));
				}
			}

			rayBuffer->Reset();
		}

		//----------------------------------------------------------------------
		// Save the frame buffer
		//----------------------------------------------------------------------

		SaveFrameBuffer("image.ppm", frameBuffer);

		//----------------------------------------------------------------------
		// Clean up
		//----------------------------------------------------------------------

		ctx->Stop();
		delete scene;
		delete rayBuffer;
		delete ctx;

#if !defined(LUXRAYS_DISABLE_OPENCL)
	} catch (cl::Error err) {
		std::cerr << "OpenCL ERROR: " << err.what() << "(" << err.err() << ")" << std::endl;
#endif
	} catch (std::runtime_error err) {
		std::cerr << "RUNTIME ERROR: " << err.what() << std::endl;
	} catch (std::exception err) {
		std::cerr << "ERROR: " << err.what() << std::endl;
	}

	return EXIT_SUCCESS;
}
