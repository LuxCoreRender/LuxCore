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

#include <bit>
#include <boost/algorithm/string.hpp>
#include <oneapi/tbb.h>

#include "luxcore/luxcorelogger.h"
#include "luxcore/luxcore.h"
#include "luxcore/luxcoreimpl.h"
#include "slg/slg.h"
#include "luxrays/utils/buffer.h"

using namespace std;
using namespace luxrays;
using namespace luxcore;
using namespace luxcore::detail;

//------------------------------------------------------------------------------
// KernelCacheFill
//------------------------------------------------------------------------------

#if !defined(LUXRAYS_DISABLE_OPENCL)

static void CreateBox(
	luxcore::Scene& scene,
	const string &objName,
	const string &meshName,
	const string &matName,
	const bool enableUV,
	const BBox &bbox
) {
	VertexBuffer p = luxcore::Scene::AllocVerticesBuffer(24);
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

	TriangleBuffer vi(12);
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
		scene.DefineMesh(
			meshName,
			24,
			12,
			p.GetSubObjects().data(),
			vi.GetSubObjects().data(),
			nullptr,  // No normals
			nullptr,  // No UV
			nullptr,
			nullptr
		);
	} else {
	std::array<UV, 24> uv{
		// Bottom face
		UV(0.f, 0.f),
		UV(1.f, 0.f),
		UV(1.f, 1.f),
		UV(0.f, 1.f),
		// Top face
		UV(0.f, 0.f),
		UV(1.f, 0.f),
		UV(1.f, 1.f),
		UV(0.f, 1.f),
		// Side left
		UV(0.f, 0.f),
		UV(1.f, 0.f),
		UV(1.f, 1.f),
		UV(0.f, 1.f),
		// Side right
		UV(0.f, 0.f),
		UV(1.f, 0.f),
		UV(1.f, 1.f),
		UV(0.f, 1.f),
		// Side back
		UV(0.f, 0.f),
		UV(1.f, 0.f),
		UV(1.f, 1.f),
		UV(0.f, 1.f),
		// Side front
		UV(0.f, 0.f),
		UV(1.f, 0.f),
		UV(1.f, 1.f),
		UV(0.f, 1.f),
		};

		// Define the object
		scene.DefineMesh(
			meshName,
			24,
			12,
			p.GetSubObjects().data(),
			vi.GetSubObjects().data(),
			nullptr,
			reinterpret_cast<float *>(uv.data()),
			nullptr,
			nullptr
		);
	}

	// Add the object to the scene
	auto props = std::make_unique<Properties>();;
	props->SetFromString(
		"scene.objects." + objName + ".shape = " + meshName + "\n"
		"scene.objects." + objName + ".material = " + matName + "\n"
		);
	scene.Parse(props);
}

static void RenderTestScene(const Properties &cfgSetUpProps, const Properties &scnSetUpProps) {
	// Build the scene to render
	auto sceneptr = Scene::Create();
	auto& scene = *sceneptr;

	auto scnProps = scnSetUpProps.Clone();

	*scnProps <<
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

	scene.DefineImageMap<u_char>("image.png", &img[0], 1.f, 3, size, size, Scene::DEFAULT);

	// Define light sources
	const Property lightSetUpProp = scnSetUpProps.Get(Property("kernelcachefill.light.types")("infinite"));
	bool hasTriangleLight = false;
	for (u_int i = 0; i < lightSetUpProp.GetSize(); ++i) {
		const string lightType = lightSetUpProp.Get<string>(i);
		if (lightType == "trianglelight") {
			hasTriangleLight = true;
			continue;
		}

		*scnProps << Property("scene.lights." + lightType + "_light.type")(lightType);
		if (lightType == "mappoint")
			*scnProps << Property("scene.lights." + lightType + "_light.mapfile")("image.png");
	}

	// Parse the scene definition properties
	scene.Parse(scnProps);

	const string geometrySetUp = cfgSetUpProps.Get(Property("kernelcachefill.geometry.type")("test")).Get<string>();
	if (geometrySetUp == "test") {
		// Define materials and meshes
		if (hasTriangleLight) {
			auto props = std::make_unique<Properties>();
			*props <<
					Property("scene.materials.triangle_light.type")("matte") <<
				Property("scene.materials.triangle_light.emission")(
					1000000.f, 1000000.f, 1000000.f
				);
			scene.Parse(props);

			CreateBox(
				scene,
				"box_triangle_light",
				"mesh_box_triangle_light",
				"triangle_light",
				false,
				BBox(Point(-1.75f, 1.5f, .75f), Point(-1.5f, 1.75f, .5f))
			);
		}

		// One box for each material
		const Property materialSetUpProp = scnSetUpProps.Get(Property("kernelcachefill.material.types")("matte"));
		for (u_int i = 0; i < materialSetUpProp.GetSize(); ++i) {
			const string materialType = materialSetUpProp.Get<string>(i);

			auto props = std::make_unique<Properties>();
			*props << Property("scene.materials." + materialType + "_mat.type")(materialType);
				scene.Parse(props);

			CreateBox(scene, "mbox_" + materialType, "mesh_mbox_" + materialType, materialType + "_mat", false, BBox(Point(-1.75f, 1.5f, .75f + i), Point(-1.5f, 1.75f, .5f + i)));
		}

		// One box for each texture
		const Property textureSetUpProp = scnSetUpProps.Get(Property("kernelcachefill.texture.types")("constfloat3"));
		for (u_int i = 0; i < textureSetUpProp.GetSize(); ++i) {
			const string textureType = textureSetUpProp.Get<string>(i);

			auto props = std::make_unique<Properties>();
			*props <<
						Property("scene.textures." + textureType + "_tex.type")(textureType) <<
					Property("scene.materials." + textureType + "_tmat.type")("matte") <<
					Property("scene.materials." + textureType + "_tmat.kd")(textureType + "_tex");
			scene.Parse(props);

			CreateBox(scene, "tbox_" + textureType, "mesh_tbox_" + textureType, textureType + "_tmat", false, BBox(Point(-1.75f, 2.5f, .75f + i), Point(-1.5f, 2.75f, .5f + i)));
		}
	} else {
		if (hasTriangleLight && (lightSetUpProp.GetSize() == 1)) {
			// I can not render an empty scene with only area light sources
			return;
		}

		scene.Parse(scnProps);
	}

	// Do the render

	auto cfgProps = cfgSetUpProps.Clone();
	*cfgProps <<
			Property("film.outputs.1.type")("RGB_IMAGEPIPELINE") <<
			Property("film.outputs.1.filename")("image.png");

	RenderConfigRPtr config = RenderConfig::Create(std::move(cfgProps), sceneptr);
	auto session = RenderSession::Create(config);

	// Start the session: the kernels are compiled here (and stored in the
	// kernel cache). slg::compileOnlyMode is set so no rendering thread is
	// started.
	session->Start();

	session->UpdateStats();

	// Save the rendered image
	//session->GetFilm().SaveOutputs();

	// Stop the session
	session->Stop();

	LC_LOG("Done.");
}

// Set the slg::compileOnlyMode flag (in order to compile the kernels without
// rendering anything) and temporarily disable the LuxRays, SDL and SLG
// sub-system logs in order to hide the messages related to the dummy scenes
// used to compile the kernels.
class KernelCacheFillMode {
public:
	KernelCacheFillMode() {
		compileOnlyModeSaved = slg::compileOnlyMode;
		slg::compileOnlyMode = true;

		logLuxRaysEnabledSaved = logLuxRaysEnabled;
		logSDLEnabledSaved = logSDLEnabled;
		logSLGEnabledSaved = logSLGEnabled;

		logLuxRaysEnabled = false;
		logSDLEnabled = false;
		logSLGEnabled = false;
	}

	~KernelCacheFillMode() {
		slg::compileOnlyMode = compileOnlyModeSaved;

		logLuxRaysEnabled = logLuxRaysEnabledSaved;
		logSDLEnabled = logSDLEnabledSaved;
		logSLGEnabled = logSLGEnabledSaved;
	}

private:
	bool compileOnlyModeSaved;
	bool logLuxRaysEnabledSaved, logSDLEnabledSaved, logSLGEnabledSaved;
};

static void KernelCacheFillImpl(
	PropertiesRPtr configPtr,
	void (*ProgressHandler)(const size_t, const size_t)
) {
	// Compile the kernels without rendering anything and hide the log
	// messages related to the dummy scenes
	KernelCacheFillMode fillMode;

	auto& config = *configPtr;

	// Extract the render engines - default to all OpenCL engines
	const Property renderEngines = config.Get(Property("kernelcachefill.renderengine.types")("PATHOCL", "TILEPATHOCL", "RTPATHOCL"));
	const size_t count = renderEngines.GetSize();

	// Log start of kernel cache filling
	LC_LOG("====================================================================");
	LC_LOG("Starting parallel kernel compilation for " << count << " OpenCL render engines");
	LC_LOG("====================================================================");

	// Prepare configuration properties for each render engine
	std::vector<std::pair<std::string, Properties>> engineConfigs;
	for (u_int renderEngineIndex = 0; renderEngineIndex < count; ++renderEngineIndex) {
		const string renderEngineType = renderEngines.Get<string>(renderEngineIndex);
		string samplerType;
		if ((renderEngineType == "TILEPATHOCL") || (renderEngineType == "RTPATHOCL"))
			samplerType = "TILEPATHSAMPLER";
		else
			samplerType = "SOBOL";
		
		Properties cfgProps;
		cfgProps <<
				Property("renderengine.type")(renderEngineType) <<
				// Native threads are of no use for kernel compilation
				Property("opencl.native.threads.count")(0u) <<
				Property("sampler.type")(samplerType) <<
				config.Get(Property("scene.epsilon.min")(DEFAULT_EPSILON_MIN)) <<
				config.Get(Property("scene.epsilon.max")(DEFAULT_EPSILON_MAX));
		
		if (config.IsDefined("opencl.devices.select"))
			cfgProps << config.Get("opencl.devices.select");

		engineConfigs.emplace_back(renderEngineType, std::move(cfgProps));
	}

	// Parallel kernel compilation using TBB
	tbb::parallel_for(tbb::blocked_range<size_t>(0, count),
		[&](const tbb::blocked_range<size_t> &range) {
			for (size_t i = range.begin(); i < range.end(); ++i) {
				const auto &[renderEngineType, cfgProps] = engineConfigs[i];
				
				LC_LOG("[" << renderEngineType << "] Compiling kernels...");
				
				// Call progress handler if provided (note: not thread-safe, but for logging it's okay)
				if (ProgressHandler) {
					ProgressHandler(i, count);
				}
				
				// Build the test scene and compile its kernels
				// (slg::compileOnlyMode is set, so nothing is rendered)
				RenderTestScene(cfgProps, Properties());
				
				LC_LOG("[" << renderEngineType << "] Kernel compilation completed");
			}
		});

	LC_LOG("====================================================================");
	LC_LOG("Parallel kernel compilation completed for all OpenCL engines");
	LC_LOG("====================================================================");
}

#endif

void luxcore::KernelCacheFill(PropertiesRPtr config, void (*ProgressHandler)(const size_t, const size_t)) {
	API_BEGIN("{}, {}", ToArgString(config),(void *)ProgressHandler);

#if !defined(LUXRAYS_DISABLE_OPENCL)
	KernelCacheFillImpl(config, ProgressHandler);
#endif
	
	API_END();
}
// vim: autoindent noexpandtab tabstop=4 shiftwidth=4
