/***************************************************************************
 * Copyright 1998-2020 by authors (see AUTHORS.txt)                        *
 *                                                                         *
 *   This file is part of LuxCoreRender.                                   *
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

#include <boost/foreach.hpp>
#include <boost/unordered_set.hpp>
#include <boost/algorithm/string.hpp>

#include "luxcore/luxcore.h"
#include "luxcore/luxcoreimpl.h"

using namespace std;
using namespace luxrays;
using namespace luxcore;

//------------------------------------------------------------------------------
// KernelCacheFill
//------------------------------------------------------------------------------

#if !defined(LUXRAYS_DISABLE_OPENCL)

static void CreateBox(Scene *scene, const string &objName, const string &meshName,
		const string &matName, const bool enableUV, const BBox &bbox) {
	Point *p = (Point *)Scene::AllocVerticesBuffer(24);
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

	Triangle *vi = (Triangle *)Scene::AllocTrianglesBuffer(12);
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
		scene->DefineMesh(meshName, 24, 12, (float *)p, (unsigned int *)vi, NULL, NULL, NULL, NULL);
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
		scene->DefineMesh(meshName, 24, 12, (float *)p, (unsigned int *)vi, NULL, (float *)uv, NULL, NULL);
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
	LC_LOG("Creating kernel cache entry with configuration properties:");
	LC_LOG(cfgSetUpProps);
	LC_LOG("And scene properties:");
	LC_LOG(scnSetUpProps);

	// Build the scene to render
	Scene *scene = Scene::Create();

	Properties scnProps = scnSetUpProps;

	scnProps <<
			Property("scene.camera.lookat.orig")(1.f , 6.f , 3.f) <<
			Property("scene.camera.lookat.target")(0.f , 0.f , .5f) <<
			Property("scene.camera.fieldofview")(60.f);

	// Define image maps
	const u_int size = 256;
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

	scene->DefineImageMap<u_char>("image.png", &img[0], 1.f, 3, size, size, Scene::DEFAULT);

	// Define light sources
	const Property lightSetUpProp = scnSetUpProps.Get(Property("kernelcachefill.light.types")("infinite"));
	bool hasTriangleLight = false;
	for (u_int i = 0; i < lightSetUpProp.GetSize(); ++i) {
		const string lightType = lightSetUpProp.Get<string>(i);
		if (lightType == "trianglelight") {
			hasTriangleLight = true;
			continue;
		}

		scnProps << Property("scene.lights." + lightType + "_light.type")(lightType);
		if (lightType == "mappoint")
			scnProps << Property("scene.lights." + lightType + "_light.mapfile")("image.png");
	}

	// Parse the scene definition properties
	scene->Parse(scnProps);

	const string geometrySetUp = cfgSetUpProps.Get(Property("kernelcachefill.geometry.type")("test")).Get<string>();
	if (geometrySetUp == "test") {
		// Define materials and meshes
		if (hasTriangleLight) {
			Properties props;
			props <<
				Property("scene.materials.triangle_light.type")("matte") <<
				Property("scene.materials.triangle_light.emission")(1000000.f, 1000000.f, 1000000.f);
			scene->Parse(props);

			CreateBox(scene, "box_triangle_light", "mesh_box_triangle_light", "triangle_light", false, BBox(Point(-1.75f, 1.5f, .75f), Point(-1.5f, 1.75f, .5f)));
		}

		// One box for each material
		const Property materialSetUpProp = scnSetUpProps.Get(Property("kernelcachefill.material.types")("matte"));
		for (u_int i = 0; i < materialSetUpProp.GetSize(); ++i) {
			const string materialType = materialSetUpProp.Get<string>(i);

			Properties props;
			props << Property("scene.materials." + materialType + "_mat.type")(materialType);
			scene->Parse(props);

			CreateBox(scene, "mbox_" + materialType, "mesh_mbox_" + materialType, materialType + "_mat", false, BBox(Point(-1.75f, 1.5f, .75f + i), Point(-1.5f, 1.75f, .5f + i)));
		}

		// One box for each texture
		const Property textureSetUpProp = scnSetUpProps.Get(Property("kernelcachefill.texture.types")("constfloat3"));
		for (u_int i = 0; i < textureSetUpProp.GetSize(); ++i) {
			const string textureType = textureSetUpProp.Get<string>(i);

			Properties props;
			props <<
					Property("scene.textures." + textureType + "_tex.type")(textureType) <<
					Property("scene.materials." + textureType + "_tmat.type")("matte") <<
					Property("scene.materials." + textureType + "_tmat.kd")(textureType + "_tex");
			scene->Parse(props);

			CreateBox(scene, "tbox_" + textureType, "mesh_tbox_" + textureType, textureType + "_tmat", false, BBox(Point(-1.75f, 2.5f, .75f + i), Point(-1.5f, 2.75f, .5f + i)));
		}
	} else {
		if (hasTriangleLight && (lightSetUpProp.GetSize() == 1)) {
			// I can not render an empty scene with only area light sources
			return;
		}

		scene->Parse(scnProps);
	}
	
	// Do the render

	Properties cfgProps = cfgSetUpProps;
	cfgProps <<
			Property("film.outputs.1.type")("RGB_IMAGEPIPELINE") <<
			Property("film.outputs.1.filename")("image.png");

	RenderConfig *config = RenderConfig::Create(cfgProps, scene);
	RenderSession *session = RenderSession::Create(config);

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

static void KernelCacheFillImpl(const Properties &config, void (*ProgressHandler)(const size_t, const size_t)) {
	// Extract the render engines
	const Property renderEngines = config.Get(Property("kernelcachefill.renderengine.types")("PATHOCL", "TILEPATHOCL", "RTPATHOCL"));
	const size_t count = renderEngines.GetSize();
	
	// For each render engine type
	for (u_int renderEngineIndex = 0; renderEngineIndex < renderEngines.GetSize(); ++renderEngineIndex) {
		const string renderEngineType = renderEngines.Get<string>(renderEngineIndex);
		string samplerType;
		if ((renderEngineType == "TILEPATHOCL") || (renderEngineType == "RTPATHOCL"))
			samplerType = "TILEPATHSAMPLER";
		else
			samplerType = "SOBOL";
		
		Properties cfgProps;
		cfgProps << 
				Property("renderengine.type")(renderEngineType) <<
				Property("sampler.type")(samplerType) <<
				config.Get(Property("scene.epsilon.min")(DEFAULT_EPSILON_MIN)) <<
				config.Get(Property("scene.epsilon.max")(DEFAULT_EPSILON_MAX));
		
		if (config.IsDefined("opencl.devices.select"))
			cfgProps << config.Get("opencl.devices.select");

		// Run the rendering
		LC_LOG("====================================================================");
		if (ProgressHandler)
			ProgressHandler(renderEngineIndex, count);
		LC_LOG("Step: " << renderEngineIndex << "/" << count);
		RenderTestScene(cfgProps, Properties());
	}

	LC_LOG("====================================================================");
}

#endif

void luxcore::KernelCacheFill(const Properties &config, void (*ProgressHandler)(const size_t, const size_t)) {
#if !defined(LUXRAYS_DISABLE_OPENCL)
	KernelCacheFillImpl(config, ProgressHandler);
#endif
}
