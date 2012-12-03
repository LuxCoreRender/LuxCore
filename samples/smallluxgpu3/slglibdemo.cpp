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

static void CreateBox(Scene *scene, const string &objName, const string &matName, const BBox &bbox) {
	Point *v = new Point[8];
	// Bottom face
	v[0] = Point(bbox.pMin.x, bbox.pMin.y, bbox.pMin.z);
	v[1] = Point(bbox.pMax.x, bbox.pMin.y, bbox.pMin.z);
	v[2] = Point(bbox.pMax.x, bbox.pMax.y, bbox.pMin.z);
	v[3] = Point(bbox.pMin.x, bbox.pMax.y, bbox.pMin.z);

	// Top face
	v[4] = Point(bbox.pMin.x, bbox.pMin.y, bbox.pMax.z);
	v[5] = Point(bbox.pMax.x, bbox.pMin.y, bbox.pMax.z);
	v[6] = Point(bbox.pMax.x, bbox.pMax.y, bbox.pMax.z);
	v[7] = Point(bbox.pMin.x, bbox.pMax.y, bbox.pMax.z);

	Triangle *vi = new Triangle[4];
	// Bottom face
	vi[0] = Triangle(0, 1, 2);
	vi[1] = Triangle(2, 3, 0);

	// Top face
	vi[2] = Triangle(4, 5, 6);
	vi[3] = Triangle(6, 7, 4);

	// Define the object
	scene->extMeshCache->DefineExtMesh(objName, 8, 4, v, vi, NULL, NULL, true);

	// Add the object to the scene
	scene->AddObject(objName, matName,
		"scene.objects." + matName + "." + objName + ".useplynormals = 1\n"
		);
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
			"scene.camera.lookat = 2.0 3.0 6.0  0.0 0.0 1.0\n"
			"scene.camera.fieldofview = 60.0\n"
			);

		// Setup materials
		scene->AddMaterials(
			"scene.materials.matte.mat_white = 0.75 0.75 0.75\n"
			"scene.materials.matte.mat_red = 0.75 0.0 0.0\n"
			);

		// Create the ground
		CreateBox(scene, "ground", "mat_white", BBox(Point(-10.f,-10.f,-.1f), Point(10.f, 10.f, 0.f)));

		delete scene;

		//----------------------------------------------------------------------
		// Do the render
		//----------------------------------------------------------------------

		const string cfgFilrName = "scenes/luxball/render-fast-hdr.cfg";
		RenderConfig *config = new RenderConfig(&cfgFilrName, NULL);
		RenderSession *session = new RenderSession(config);
		RenderEngine *engine = session->renderEngine;

		const unsigned int haltTime = 10;
		const unsigned int haltSpp = config->cfg.GetInt("batch.haltspp", 0);
		const float haltThreshold = config->cfg.GetFloat("batch.haltthreshold", -1.f);

		// Start the rendering
		session->Start();
		const double startTime = WallClockTime();

		double lastFilmUpdate = WallClockTime();
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
