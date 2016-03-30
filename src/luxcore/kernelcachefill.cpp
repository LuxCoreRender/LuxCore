/***************************************************************************
 * Copyright 1998-2015 by authors (see AUTHORS.txt)                        *
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

#include "luxcore/luxcore.h"

using namespace std;
using namespace luxrays;
using namespace luxcore;

//------------------------------------------------------------------------------
// KernelCacheFill
//------------------------------------------------------------------------------

static void CreateBox(Scene *scene, const string &objName, const string &meshName,
		const string &matName, const bool enableUV, const BBox &bbox) {
	Point *p = Scene::AllocVerticesBuffer(24);
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

	Triangle *vi = Scene::AllocTrianglesBuffer(12);
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

	// Define the Mesh
	if (!enableUV) {
		// Define the object
		scene->DefineMesh(meshName, 24, 12, p, vi, NULL, NULL, NULL, NULL);
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
		scene->DefineMesh(meshName, 24, 12, p, vi, NULL, uv, NULL, NULL);
	}

	// Add the object to the scene
	Properties props;
	props.SetFromString(
		"scene.objects." + objName + ".shape = " + meshName + "\n"
		"scene.objects." + objName + ".material = " + matName + "\n"
		);
	scene->Parse(props);
}

static void RenderTestScene(const Properties &cfgSetUpProps, const Properties &scnSetUpProps) {
	LC_LOG("====================================================================");
	LC_LOG("Creating kernel cache entry with configurationproperties:");
	LC_LOG(cfgSetUpProps);
	LC_LOG("And scene properties:");
	LC_LOG(scnSetUpProps);

	// Build the scene to render
	Scene *scene = new Scene();

	Properties scnProps = scnSetUpProps;

	scnProps <<
			Property("scene.camera.lookat.orig")(1.f , 6.f , 3.f) <<
			Property("scene.camera.lookat.target")(0.f , 0.f , .5f) <<
			Property("scene.camera.fieldofview")(60.f);

	const string geometrySetUp = cfgSetUpProps.Get(Property("kernelcachefill.scene.geometry.type")("test")).Get<string>();
	if (geometrySetUp == "test") {
		// Define texture maps
		/*const u_int size = 256;
		vector<u_char> img(size * size * 3);
		u_char *ptr = &img[0];
		for (u_int y = 0; y < size; ++y) {
			for (u_int x = 0; x < size; ++x) {
				if ((x % 64 < 32) ^ (y % 64 < 32)) {
					*ptr++ = 255;
					*ptr++ = 0;
					*ptr++ = 0;
				} else {
					*ptr++ = 255;
					*ptr++ = 255;
					*ptr++ = 0;
				}
			}
		}

		scene->DefineImageMap<u_char>("check_texmap", &img[0], 1.f, 3, size, size, Scene::DEFAULT);
		scene->Parse(
			Property("scene.textures.map.type")("imagemap") <<
			Property("scene.textures.map.file")("check_texmap") <<
			Property("scene.textures.map.gamma")(1.f)
			);*/

		// Setup materials
		scnProps <<
				Property("scene.materials.whitelight.type")("matte") <<
				Property("scene.materials.whitelight.emission")(1000000.f, 1000000.f, 1000000.f) <<
				Property("scene.materials.mat_white.type")("matte") <<
				Property("scene.materials.mat_white.kd")(.7f, .7f, .7f) <<
				Property("scene.materials.mat_red.type")("matte") <<
				Property("scene.materials.mat_red.kd")(.7f, 0.f, 0.f);

		// Parse the scene definition properties
		scene->Parse(scnProps);

		// Create the ground
		CreateBox(scene, "ground", "mesh-ground", "mat_white", true, BBox(Point(-3.f, -3.f, -.1f), Point(3.f, 3.f, 0.f)));
		// Create the red box
		CreateBox(scene, "box01", "mesh-box01", "mat_red", false, BBox(Point(-.5f, -.5f, .2f), Point(.5f, .5f, .7f)));
		// Create the light
		CreateBox(scene, "box02", "mesh-box02", "whitelight", false, BBox(Point(-1.75f, 1.5f, .75f), Point(-1.5f, 1.75f, .5f)));
	} else {
		scnProps <<
				Property("scene.lights.skyl.type")("sky") <<
				Property("scene.lights.skyl.dir")(0.166974f, 0.59908f, 0.783085f) <<
				Property("scene.lights.skyl.turbidity")(2.2f) <<
				Property("scene.lights.skyl.gain")(0.8f, 0.8f, 0.8f) <<
				Property("scene.lights.sunl.type")("sun") <<
				Property("scene.lights.sunl.dir")(0.166974f, 0.59908f, 0.783085f) <<
				Property("scene.lights.sunl.turbidity")(2.2f) <<
				Property("scene.lights.sunl.gain")(0.8f, 0.8f, 0.8f);

		scene->Parse(scnProps);
	}
	
	// Do the render

	Properties cfgProps = cfgSetUpProps;
	cfgProps <<
			Property("film.outputs.1.type")("RGB_IMAGEPIPELINE") <<
			Property("film.outputs.1.filename")("image.png");

	RenderConfig *config = new RenderConfig(cfgProps, scene);
	RenderSession *session = new RenderSession(config);

	// Start the rendering
	session->Start();

	session->UpdateStats();

	// Save the rendered image
	//session->GetFilm().SaveOutputs();
	
	// Stop the rendering
	session->Stop();

	delete session;
	delete config;
	delete scene;

	LC_LOG("Done.");
}

void luxcore::KernelCacheFill(const Properties &config) {
#if !defined(LUXRAYS_DISABLE_OPENCL)
	// Extract the render engines
	const Property renderEngines = config.Get(Property("kernelcachefill.renderengine.type")("PATHOCL", "BIASPATHOCL", "RTPATHOCL", "RTBIASPATHOCL"));

	// Extract the samplers
	const Property samplers = config.Get(Property("kernelcachefill.sampler.type")("RANDOM", "SOBOL", "METROPOLIS"));

	// Extract the cameras
	const Property cameras = config.Get(Property("kernelcachefill.camera.type")("orthographic", "perspective"));

	// Extract the cameras
	const Property geometrySetUpOptions = config.Get(Property("kernelcachefill.scene.geometry.type")("empty", "test"));

	// For each render engine type
	for (u_int renderEngineIndex = 0; renderEngineIndex < renderEngines.GetSize(); ++renderEngineIndex) {
		Properties cfgProps, scnProps;
		const string renderEngineType = renderEngines.Get<string>(renderEngineIndex);

		cfgProps << 
				config <<
				Property("renderengine.type")(renderEngineType);

		// For each camera type
		for (u_int cameraIndex = 0; cameraIndex < cameras.GetSize(); ++cameraIndex) {
			const string cameraType = cameras.Get<string>(cameraIndex);
			
			scnProps << Property("scene.camera.type")(cameraType); 

			// For each scene geometry setup
			for (u_int geometrySetUpIndex = 0; geometrySetUpIndex < geometrySetUpOptions.GetSize(); ++geometrySetUpIndex) {
				const string geometrySetUp = geometrySetUpOptions.Get<string>(geometrySetUpIndex);
				
				cfgProps << Property("kernelcachefill.scene.geometry.type")(geometrySetUp);

				// For each render sampler (if applicable)
				if ((renderEngineType == "PATHOCL") || (renderEngineType == "RTPATHOCL")) {
					for (u_int samplerIndex = 0; samplerIndex < samplers.GetSize(); ++samplerIndex) {
						const string samplerType = samplers.Get<string>(samplerIndex);

						cfgProps << Property("sampler.type")(samplerType);

						// Run the rendering
						RenderTestScene(cfgProps, scnProps);
					}
				} else {
					// Run the rendering
					RenderTestScene(cfgProps, scnProps);
				}
			}
		}
	}

	LC_LOG("====================================================================");
#endif
}
