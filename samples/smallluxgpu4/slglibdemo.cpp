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

#include <iostream>

#include "slg.h"
#include "rendersession.h"

#include "luxrays/core/utils.h"
#include "luxrays/opencl/utils.h"

using namespace std;
using namespace luxrays;
using namespace luxrays::sdl;
using namespace luxrays::utils;
using namespace slg;

void DebugHandler(const char *msg) {
	cerr << "[LuxRays] " << msg << endl;
}

void SDLDebugHandler(const char *msg) {
	cerr << "[LuxRays::SDL] " << msg << endl;
}

void SLGDebugHandler(const char *msg) {
	cerr << "[SLG] " << msg << endl;
}

void FreeImageErrorHandler(FREE_IMAGE_FORMAT fif, const char *message) {
	printf("\n*** ");
	if(fif != FIF_UNKNOWN)
		printf("%s Format\n", FreeImage_GetFormatFromFIF(fif));

	printf("%s", message);
	printf(" ***\n");
}

static void CreateBox(Scene *scene, const string &objName, const string &matName,
		const bool enableUV, const BBox &bbox) {
	Point *p = new Point[24];
	// Bottom face
	p[0] = Point(bbox.pMin.x, bbox.pMin.y, bbox.pMin.z);
	p[1] = Point(bbox.pMin.x, bbox.pMax.y, bbox.pMin.z);
	p[2] = Point(bbox.pMax.x, bbox.pMax.y, bbox.pMin.z);
	p[3] = Point(bbox.pMax.x, bbox.pMin.y, bbox.pMin.z);
	// Top face
	p[4] = Point(bbox.pMin.x, bbox.pMin.y, bbox.pMax.z);
	p[5] = Point(bbox.pMax.x, bbox.pMin.y, bbox.pMax.z);
	p[6] = Point(bbox.pMax.x, bbox.pMax.y, bbox.pMax.z);
	p[7] = Point(bbox.pMin.x, bbox.pMax.y, bbox.pMax.z);
	// Side left
	p[8] = Point(bbox.pMin.x, bbox.pMin.y, bbox.pMin.z);
	p[9] = Point(bbox.pMin.x, bbox.pMin.y, bbox.pMax.z);
	p[10] = Point(bbox.pMin.x, bbox.pMax.y, bbox.pMax.z);
	p[11] = Point(bbox.pMin.x, bbox.pMax.y, bbox.pMin.z);
	// Side right
	p[12] = Point(bbox.pMax.x, bbox.pMin.y, bbox.pMin.z);
	p[13] = Point(bbox.pMax.x, bbox.pMax.y, bbox.pMin.z);
	p[14] = Point(bbox.pMax.x, bbox.pMax.y, bbox.pMax.z);
	p[15] = Point(bbox.pMax.x, bbox.pMin.y, bbox.pMax.z);
	// Side back
	p[16] = Point(bbox.pMin.x, bbox.pMin.y, bbox.pMin.z);
	p[17] = Point(bbox.pMax.x, bbox.pMin.y, bbox.pMin.z);
	p[18] = Point(bbox.pMax.x, bbox.pMin.y, bbox.pMax.z);
	p[19] = Point(bbox.pMin.x, bbox.pMin.y, bbox.pMax.z);
	// Side front
	p[20] = Point(bbox.pMin.x, bbox.pMax.y, bbox.pMin.z);
	p[21] = Point(bbox.pMin.x, bbox.pMax.y, bbox.pMax.z);
	p[22] = Point(bbox.pMax.x, bbox.pMax.y, bbox.pMax.z);
	p[23] = Point(bbox.pMax.x, bbox.pMax.y, bbox.pMin.z);

	Triangle *vi = new Triangle[12];
	// Bottom face
	vi[0] = Triangle(0, 1, 2);
	vi[1] = Triangle(2, 3, 0);
	// Top face
	vi[2] = Triangle(4, 5, 6);
	vi[3] = Triangle(6, 7, 4);
	// Side left
	vi[4] = Triangle(8, 9, 10);
	vi[5] = Triangle(10, 11, 8);
	// Side right
	vi[6] = Triangle(12, 13, 14);
	vi[7] = Triangle(14, 15, 12);
	// Side back
	vi[8] = Triangle(16, 17, 18);
	vi[9] = Triangle(18, 19, 16);
	// Side back
	vi[10] = Triangle(20, 21, 22);
	vi[11] = Triangle(22, 23, 20);

	if (!enableUV) {
		// Define the object
		scene->DefineObject("Mesh-" + objName, 24, 12, p, vi, NULL, NULL, false);

		// Add the object to the scene
		scene->AddObject(objName, "Mesh-" + objName,
				"scene.objects." + objName + ".material = " + matName + "\n"
				"scene.objects." + objName + ".useplynormals = 0\n"
			);
	} else {
		UV *uv = new UV[24];
		// Bottom face
		uv[0] = UV(0.f, 0.f);
		uv[1] = UV(1.f, 0.f);
		uv[2] = UV(1.f, 1.f);
		uv[3] = UV(0.f, 1.f);
		// Top face
		uv[4] = UV(0.f, 0.f);
		uv[5] = UV(1.f, 0.f);
		uv[6] = UV(1.f, 1.f);
		uv[7] = UV(0.f, 1.f);
		// Side left
		uv[8] = UV(0.f, 0.f);
		uv[9] = UV(1.f, 0.f);
		uv[10] = UV(1.f, 1.f);
		uv[11] = UV(0.f, 1.f);
		// Side right
		uv[12] = UV(0.f, 0.f);
		uv[13] = UV(1.f, 0.f);
		uv[14] = UV(1.f, 1.f);
		uv[15] = UV(0.f, 1.f);
		// Side back
		uv[16] = UV(0.f, 0.f);
		uv[17] = UV(1.f, 0.f);
		uv[18] = UV(1.f, 1.f);
		uv[19] = UV(0.f, 1.f);
		// Side front
		uv[20] = UV(0.f, 0.f);
		uv[21] = UV(1.f, 0.f);
		uv[22] = UV(1.f, 1.f);
		uv[23] = UV(0.f, 1.f);

		// Define the object
		scene->DefineObject("Mesh-" + objName, 24, 12, p, vi, NULL, uv, false);

		// Add the object to the scene
		scene->AddObject(objName, "Mesh-" + objName,
				"scene.objects." + objName + ".material = " + matName + "\n"
				"scene.objects." + objName + ".useplynormals = 0\n"
			);
	}
}

int main(int argc, char *argv[]) {
	luxrays::sdl::LuxRaysSDLDebugHandler = SDLDebugHandler;

	try {
		// Initialize FreeImage Library
		FreeImage_Initialise(TRUE);
		FreeImage_SetOutputMessage(FreeImageErrorHandler);

		//----------------------------------------------------------------------
		// Build the scene to render
		//----------------------------------------------------------------------

		Scene *scene = new Scene();

		// Setup the camera
		scene->CreateCamera(
			"scene.camera.lookat = 1.0 6.0 3.0  0.0 0.0 0.5\n"
			"scene.camera.fieldofview = 60.0\n"
			);

		// Define texture maps
		const u_int size = 500;
		float *img = new float[size * size * 3];
		float *ptr = img;
		for (u_int y = 0; y < size; ++y) {
			for (u_int x = 0; x < size; ++x) {
				if ((x % 50 < 25) ^ (y % 50 < 25)) {
					*ptr++ = 1.f;
					*ptr++ = 0.f;
					*ptr++ = 0.f;
				} else {
					*ptr++ = 1.f;
					*ptr++ = 1.f;
					*ptr++ = 0.f;
				}
			}
		}

		scene->DefineImageMap("check_texmap", new ImageMap(img, 1.f, 3, size, size));
		scene->DefineTextures(
			"scene.textures.map.type = imagemap\n"
			"scene.textures.map.file = check_texmap\n"
			"scene.textures.map.gamma = 1.0\n"
			);

		// Setup materials
		scene->DefineMaterials(
			"scene.materials.whitelight.type = matte\n"
			"scene.materials.whitelight.emission = 200.0 200.0 200.0\n"
			"scene.materials.mat_white.type = matte\n"
			"scene.materials.mat_white.kd = map\n"
			"scene.materials.mat_red.type = matte\n"
			"scene.materials.mat_red.kd = 0.75 0.0 0.0\n"
			"scene.materials.mat_glass.type = glass\n"
			"scene.materials.mat_glass.kr = 0.9 0.9 0.9\n"
			"scene.materials.mat_glass.kt = 0.9 0.9 0.9\n"
			"scene.materials.mat_glass.ioroutside = 1.0\n"
			"scene.materials.mat_glass.iorinside = 1.4\n"
			);

		// Create the ground
		CreateBox(scene, "ground", "mat_white", true, BBox(Point(-3.f,-3.f,-.1f), Point(3.f, 3.f, 0.f)));
		// Create the red box
		CreateBox(scene, "box01", "mat_red", false, BBox(Point(-.5f,-.5f, .2f), Point(.5f, .5f, 0.7f)));
		// Create the glass box
		CreateBox(scene, "box02", "mat_glass", false, BBox(Point(1.5f, 1.5f, .3f), Point(2.f, 1.75f, 1.5f)));
		// Create the light
		CreateBox(scene, "box03", "whitelight", false, BBox(Point(-1.75f, 1.5f, .75f), Point(-1.5f, 1.75f, .5f)));

		/*// Create an InfiniteLight loaded from file
		scene->AddInfiniteLight(
				"scene.infinitelight.file = scenes/simple-mat/arch.exr\n"
				"scene.infinitelight.gamma = 1.0\n"
				"scene.infinitelight.gain = 3.0 3.0 3.0\n"
				);*/

		// Create a SkyLight & SunLight
		scene->AddSkyLight(
				"scene.skylight.dir = 0.166974 0.59908 0.783085\n"
				"scene.skylight.turbidity = 2.2\n"
				"scene.skylight.gain = 0.8 0.8 0.8\n"
				);
		scene->AddSunLight(
				"scene.sunlight.dir = 0.166974 0.59908 0.783085\n"
				"scene.sunlight.turbidity = 2.2\n"
				"scene.sunlight.gain = 0.8 0.8 0.8\n"
				);

		//----------------------------------------------------------------------
		// Do the render
		//----------------------------------------------------------------------

		RenderConfig *config = new RenderConfig(
				"renderengine.type = PATHCPU\n"
				"sampler.type = INLINED_RANDOM\n"
				"opencl.platform.index = -1\n"
				"opencl.cpu.use = 0\n"
				"opencl.gpu.use = 1\n"
				"batch.halttime = 10\n",
				*scene);
		RenderSession *session = new RenderSession(config);
		RenderEngine *engine = session->renderEngine;

		const unsigned int haltTime = config->cfg.GetInt("batch.halttime", 0);
		const unsigned int haltSpp = config->cfg.GetInt("batch.haltspp", 0);
		const float haltThreshold = config->cfg.GetFloat("batch.haltthreshold", -1.f);

		// Start the rendering
		session->Start();
		const double startTime = WallClockTime();

		double lastFilmUpdate = startTime;
		char buf[512];
		for (;;) {
			boost::this_thread::sleep(boost::posix_time::millisec(1000));

			// Film update may be required by some render engine to
			// update statistics, convergence test and more
			if (WallClockTime() - lastFilmUpdate > 5.0) {
				session->renderEngine->UpdateFilm();
				lastFilmUpdate =  WallClockTime();
			}

			const double now = WallClockTime();
			const double elapsedTime = now - startTime;
			if ((haltTime > 0) && (elapsedTime >= haltTime))
				break;

			const unsigned int pass = engine->GetPass();
			if ((haltSpp > 0) && (pass >= haltSpp))
				break;

			// Convergence test is update inside UpdateFilm()
			const float convergence = engine->GetConvergence();
			if ((haltThreshold >= 0.f) && (1.f - convergence <= haltThreshold))
				break;

			// Print some information about the rendering progress
			sprintf(buf, "[Elapsed time: %3d/%dsec][Samples %4d/%d][Convergence %f%%][Avg. samples/sec % 3.2fM on %.1fK tris]",
					int(elapsedTime), int(haltTime), pass, haltSpp, 100.f * convergence, engine->GetTotalSamplesSec() / 1000000.0,
					config->scene->dataSet->GetTotalTriangleCount() / 1000.0);

			SLG_LOG(buf);
		}

		// Stop the rendering
		session->Stop();

		// Save the rendered image
		session->SaveFilmImage();

		delete session;
		SLG_LOG("Done.");

#if !defined(LUXRAYS_DISABLE_OPENCL)
	} catch (cl::Error err) {
		SLG_LOG("OpenCL ERROR: " << err.what() << "(" << luxrays::utils::oclErrorString(err.err()) << ")");
		return EXIT_FAILURE;
#endif
	} catch (runtime_error err) {
		SLG_LOG("RUNTIME ERROR: " << err.what());
		return EXIT_FAILURE;
	} catch (exception err) {
		SLG_LOG("ERROR: " << err.what());
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
