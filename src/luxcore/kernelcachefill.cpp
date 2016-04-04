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

#if !defined(LUXRAYS_DISABLE_OPENCL)

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
	LC_LOG("Creating kernel cache entry with configuration properties:");
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

		const Property materialSetUpProp = scnSetUpProps.Get(Property("kernelcachefill.material.types")("matte"));
		for (u_int i = 0; i < materialSetUpProp.GetSize(); ++i) {
			const string materialType = materialSetUpProp.Get<string>(i);

			Properties props;
			props << Property("scene.materials." + materialType + "_mat.type")(materialType);
			scene->Parse(props);

			CreateBox(scene, "box_" + materialType, "mesh_box_" + materialType, materialType + "_mat", false, BBox(Point(-1.75f, 1.5f, .75f + i), Point(-1.5f, 1.75f, .5f + i)));
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

template<class T> static void CombinationsImpl(const vector<T> &elems, const u_int size, vector<vector<T> > &result,
		vector<T> &tmp, const u_int start, const u_int end, const u_int index) {
	if (index == size) {
		// Done
		result.push_back(tmp);
	} else {
		for (u_int i = start; i <= end && (end - i + 1 >= size - index); i++) {
			tmp[index] = elems[i];
			CombinationsImpl(elems, size, result, tmp, i + 1, end, index + 1);
		}
	}
}

template<class T> static void Combinations(const vector<T> &elems, const u_int size, vector<vector<T> > &result) {
	vector<T> tmp(size);

	CombinationsImpl(elems, size, result, tmp, 0, elems.size() - 1, 0);
}

template<class T> static void Combinations(const vector<T> &elems, vector<vector<T> > &result) {
	for (u_int i = 1; i <= elems.size(); ++i)
		Combinations(elems, i, result);
}

template<class T> static void Combinations(const Property prop, vector<vector<T> > &combs) {
	vector<T> types;
	for (u_int i = 0; i < prop.GetSize(); ++i)
		types.push_back(prop.Get<T>(i));
	Combinations(types, combs);
}

template<class T> static void PrintCombinationsTest(const vector<vector<T> > &result) {
	BOOST_FOREACH(const vector<T> &r, result) {
		BOOST_FOREACH(const T &e, r) {
			cout << e << " ";
		}
		cout << endl;
	}
}

/*static void CombinationsTest() {
	vector<string> elems;
	elems.push_back("A");
	elems.push_back("B");
	elems.push_back("C");
	elems.push_back("D");
	elems.push_back("E");

	for (u_int i = 1; i <= elems.size(); ++i) {
		vector<vector<string> > result;
		Combinations(elems, i, result);
		cout << "Result " << i << ":" << endl;
		PrintCombinationsTest(result);
	}
}*/

static int KernelCacheFillImpl(const Properties &config, const bool doRender, const size_t count,
		void (*ProgressHandler)(const size_t, const size_t)) {
	// Extract the render engines
	const Property renderEngines = config.Get(Property("kernelcachefill.renderengine.types")("PATHOCL", "BIASPATHOCL", "RTPATHOCL", "RTBIASPATHOCL"));

	// Extract the samplers
	const Property samplers = config.Get(Property("kernelcachefill.sampler.types")("RANDOM", "SOBOL", "METROPOLIS"));

	// Extract the cameras
	const Property cameras = config.Get(Property("kernelcachefill.camera.types")("orthographic", "perspective"));

	// Extract the scene geometry setup
	const Property geometrySetUpOptions = config.Get(Property("kernelcachefill.geometry.types")("empty", "test"));

	// Extract the light sources
	Property defaultLights("kernelcachefill.light.types");
	defaultLights.Add("infinite").Add("sky").Add("sun").Add("triangle").
			Add("point").Add("mappoint").Add("spot").Add("projection").
			Add("constantinfinite").Add("sharpdistant").Add("distant").
			Add("sky2").Add("laser");
	const Property lights = config.Get(defaultLights);
	vector<vector<string> > lightTypeCombinations;
	Combinations(lights, lightTypeCombinations);

	// Extract the materials
	Property defaultMeterials("kernelcachefill.material.types");
	defaultMeterials.Add("matte").Add("roughmatte").Add("mirror").Add("glass").
			Add("archglass").Add("null").Add("roughmattetranslucent").Add("glossy2").
			Add("metal2").Add("roughglass").Add("velvet").
			Add("cloth").Add("carpaint").Add("glossytranslucent");
	const Property materials = config.Get(defaultMeterials);
	vector<vector<string> > materialTypeCombinations;
	Combinations(materials, materialTypeCombinations);

	// For each render engine type
	size_t step = 1;
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

			// For each light type
			BOOST_FOREACH(const vector<string> &lights, lightTypeCombinations) {
				Property lightProp("kernelcachefill.light.types");
				BOOST_FOREACH(const string &light, lights)
					lightProp.Add(light);

				scnProps << lightProp;

				// For each material type
				BOOST_FOREACH(const vector<string> &materials, materialTypeCombinations) {
					Property materialProp("kernelcachefill.material.types");
					BOOST_FOREACH(const string &material, materials)
						materialProp.Add(material);
					scnProps << materialProp;

					// For each scene geometry setup
					for (u_int geometrySetUpIndex = 0; geometrySetUpIndex < geometrySetUpOptions.GetSize(); ++geometrySetUpIndex) {
						const string geometrySetUp = geometrySetUpOptions.Get<string>(geometrySetUpIndex);

						cfgProps << Property("kernelcachefill.geometry.type")(geometrySetUp);

						// For each render sampler (if applicable)
						if ((renderEngineType == "PATHOCL") || (renderEngineType == "RTPATHOCL")) {
							for (u_int samplerIndex = 0; samplerIndex < samplers.GetSize(); ++samplerIndex) {
								const string samplerType = samplers.Get<string>(samplerIndex);

								cfgProps << Property("sampler.type")(samplerType);

								// Run the rendering
								if (doRender) {
									LC_LOG("====================================================================");
									if (ProgressHandler)
										ProgressHandler(step, count);
									LC_LOG("Step: " << step << "/" << count);
									RenderTestScene(cfgProps, scnProps);
								}

								++step;
							}
						} else {
							if (doRender) {
								// Run the rendering
								LC_LOG("====================================================================");
								if (ProgressHandler)
									ProgressHandler(step, count);
								LC_LOG("Step: " << step << "/" << count);
								RenderTestScene(cfgProps, scnProps);
							}

							++step;
						}
					}
				}
			}
		}
	}

	if (doRender)
		LC_LOG("====================================================================");

	return step - 1;
}

#endif

void luxcore::KernelCacheFill(const Properties &config, void (*ProgressHandler)(const size_t, const size_t)) {
#if !defined(LUXRAYS_DISABLE_OPENCL)
	const size_t count = KernelCacheFillImpl(config, false, 0, NULL);

	KernelCacheFillImpl(config, true, count, ProgressHandler);
#endif
}
