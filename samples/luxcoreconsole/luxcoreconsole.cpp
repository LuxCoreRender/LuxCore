/***************************************************************************
 * Copyright 1998-2018 by authors (see AUTHORS.txt)                        *
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

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <sstream>
#include <stdexcept>

#include <filesystem>
#include <boost/lexical_cast.hpp>
#include <boost/format.hpp>
#include <thread>
#include <filesystem>
#include <boost/algorithm/string/case_conv.hpp>

#include "luxrays/utils/oclerror.h"
#include "luxcore/luxcore.h"

using namespace std;
using namespace luxrays;
using namespace luxcore;

static string GetFileNameExt(const string &fileName) {
	return boost::algorithm::to_lower_copy(std::filesystem::path(fileName).extension().string());
}

static void BatchRendering(
	const RenderConfigRPtr & config,
	RenderStateRPtr startState,
	const std::unique_ptr<Film>& startFilm,
	const bool showDevicesStats
) {
	auto session = startFilm ?
		RenderSession::Create(config, startState, *startFilm) :
		RenderSession::Create(config);

	const unsigned int haltTime = config->GetProperty("batch.halttime").Get<unsigned int>();
	const unsigned int haltSpp = config->GetProperty("batch.haltspp").Get<unsigned int>(0);

	// Start the rendering
	session->Start();

	const Properties &stats = *session->GetStats();
	while (!session->HasDone()) {
		std::this_thread::sleep_for(1000ms);
		session->UpdateStats();

		const double elapsedTime = stats.Get("stats.renderengine.time").Get<double>();
		const unsigned int pass = stats.Get("stats.renderengine.pass").Get<unsigned int>();
		// Convergence test is update inside UpdateFilm()
		const float convergence = stats.Get("stats.renderengine.convergence").Get<float>();

		// Print some information about the rendering progress
		LC_LOG(boost::str(boost::format("[Elapsed time: %3d/%dsec][Samples %4d/%d][Convergence %f%%][Avg. samples/sec % 3.2fM on %.1fK tris]") %
				int(elapsedTime) % int(haltTime) % pass % haltSpp % (100.f * convergence) %
				(stats.Get("stats.renderengine.total.samplesec").Get<double>() / 1000000.0) %
				(stats.Get("stats.dataset.trianglecount").Get<double>() / 1000.0)));
		
		if (showDevicesStats) {
			// Intersection devices
			const Property &deviceNames = stats.Get("stats.renderengine.devices");

			double minPerf = numeric_limits<double>::infinity();
			double totalPerf = 0.0;
			for (unsigned int i = 0; i < deviceNames.GetSize(); ++i) {
				const string deviceName = deviceNames.Get<string>(i);

				const double perf = stats.Get("stats.renderengine.devices." + deviceName + ".performance.total").Get<double>();
				minPerf = Min(minPerf, perf);
				totalPerf += perf;
			}

			for (unsigned int i = 0; i < deviceNames.GetSize(); ++i) {
				const string deviceName = deviceNames.Get<string>(i);

				LC_LOG(boost::str(boost::format("  %s: [Rays/sec %dK (%dK + %dK)][Prf Idx %.2f][Wrkld %.1f%%][Mem %dM/%dM]") %
						deviceName %
						int(stats.Get("stats.renderengine.devices." + deviceName + ".performance.total").Get<double>() / 1000.0) %
						int(stats.Get("stats.renderengine.devices." + deviceName + ".performance.serial").Get<double>() / 1000.0) %
						int(stats.Get("stats.renderengine.devices." + deviceName + ".performance.dataparallel").Get<double>() / 1000.0) %
						(stats.Get("stats.renderengine.devices." + deviceName + ".performance.total").Get<double>() / minPerf) %
						(100.0 * stats.Get("stats.renderengine.devices." + deviceName + ".performance.total").Get<double>() / totalPerf) %
						int(stats.Get("stats.renderengine.devices." + deviceName + ".memory.used").Get<double>() / (1024 * 1024)) %
						int(stats.Get("stats.renderengine.devices." + deviceName + ".memory.total").Get<double>() / (1024 * 1024))));
			}
		}
	}

	// Stop the rendering
	session->Stop();

	const string renderEngine = config->GetProperty("renderengine.type").Get<string>();
	if (renderEngine != "FILESAVER") {
		// Save the rendered image
		session->GetFilm().SaveOutputs();
	}

	session.reset();
}

int main(int argc, char *argv[]) {
	// This is required to run AMD GPU profiler
	//XInitThreads();

	try {
		// Initialize LuxCore
		luxcore::Init();

		bool removeUnused = false;
		bool showDevicesStats = false;
		bool fillKernelCaches = false;
		Properties cmdLineProp;
		string configFileName;
		for (int i = 1; i < argc; i++) {
			if (argv[i][0] == '-') {
				// I should check for out of range array index...

				if (argv[i][1] == 'h') {
					LC_LOG("Usage: " << argv[0] << " [options] [configuration file]" << endl <<
							" -o [configuration file]" << endl <<
							" -f [scene file]" << endl <<
							" -w [film width]" << endl <<
							" -e [film height]" << endl <<
							" -D [property name] [property value]" << endl <<
							" -d [current directory path]" << endl <<
							" -c <remove all unused meshes, materials, textures and image maps>" << endl <<
							" -s <show devices stats>" << endl <<
							" -k <fill kernel caches>" << endl <<
							" -h <display this help and exit>");
					exit(EXIT_SUCCESS);
				}
				else if (argv[i][1] == 'o') {
					if (configFileName.compare("") != 0)
						throw runtime_error("Used multiple configuration files");

					configFileName = string(argv[++i]);
				}

				else if (argv[i][1] == 'e') cmdLineProp.Set(Property("film.height")(argv[++i]));

				else if (argv[i][1] == 'w') cmdLineProp.Set(Property("film.width")(argv[++i]));

				else if (argv[i][1] == 'f') cmdLineProp.Set(Property("scene.file")(argv[++i]));

				else if (argv[i][1] == 'D') {
					cmdLineProp.Set(Property(argv[i + 1]).Add(argv[i + 2]));
					i += 2;
				}

				else if (argv[i][1] == 'd') std::filesystem::current_path(std::filesystem::path(argv[++i]));

				else if (argv[i][1] == 'c') removeUnused = true;
				
				else if (argv[i][1] == 's') showDevicesStats = true;

				else if (argv[i][1] == 'k') fillKernelCaches = true;

				else {
					LC_LOG("Invalid option: " << argv[i]);
					exit(EXIT_FAILURE);
				}
			} else {
				const string fileName = argv[i];
				const string ext = GetFileNameExt(argv[i]);
				if ((ext == ".cfg") ||
						(ext == ".lxs") ||
						(ext == ".bcf") ||
						(ext == ".rsm")) {
					if (configFileName.compare("") != 0)
						throw runtime_error("Used multiple configuration files");
					configFileName = fileName;
				} else
					throw runtime_error("Unknown file extension: " + fileName);
			}
		}

		// Handle fill kernel caches option (doesn't require a config file)
		if (fillKernelCaches) {
			LC_LOG("Clearing kernel caches...");
			luxcore::ClearAllKernelCaches();

			LC_LOG("Filling kernel caches...");
			// Create properties for kernel cache filling - include all OpenCL engines
			auto fillProps = std::make_unique<Properties>();
			fillProps->Set(cmdLineProp);
			// Explicitly specify all OpenCL render engines to ensure kernels are compiled for each
			fillProps->Set(Property("kernelcachefill.renderengine.types")("PATHOCL", "TILEPATHOCL", "RTPATHOCL"));
			luxcore::KernelCacheFill(fillProps, nullptr);

			LC_LOG("Kernel caches filled successfully");
			return EXIT_SUCCESS;
		}

		// Load the Scene
		if (configFileName.compare("") == 0)
			throw runtime_error("You must specify a file to render");

		// Check if we have to parse a LuxCore SDL file or a LuxRender SDL file
		std::unique_ptr<Scene> scene;
		RenderConfigRPtr config;
		RenderStateRPtr startRenderState;
		FilmUPtr startFilm;

		if (configFileName.compare("") != 0) {
			// Clear the file name resolver list
			luxcore::ClearFileNameResolverPaths();
			// Add the current directory to the list of place where to look for files
			luxcore::AddFileNameResolverPath(".");
			// Add the .cfg directory to the list of place where to look for files
			std::filesystem::path path(configFileName);
			luxcore::AddFileNameResolverPath(path.parent_path().generic_string());
		}

		const string configFileNameExt = GetFileNameExt(configFileName);
		if (configFileNameExt == ".lxs") {
			// It is a LuxRender SDL file
			LC_LOG("Parsing LuxRender SDL file...");
			auto renderConfigProps = std::make_unique<Properties>();
			auto sceneProps = std::make_unique<Properties>();
			luxcore::ParseLXS(configFileName, renderConfigProps, sceneProps);

			// For debugging
			//LC_LOG("RenderConfig: \n" << renderConfigProps);
			//LC_LOG("Scene: \n" << sceneProps);

			renderConfigProps->Set(cmdLineProp);

			scene = luxcore::Scene::Create();
			scene->Parse(sceneProps);
			renderConfigProps->Set(cmdLineProp);
			config = RenderConfig::Create(std::move(renderConfigProps), std::move(scene));
		} else if (configFileNameExt == ".cfg") {
			// It is a LuxCore SDL file
			auto props = std::make_unique<Properties>(std::move(configFileName));
			props->Set(cmdLineProp);
			config = RenderConfig::Create(std::move(props));
		} else if (configFileNameExt == ".bcf") {
			// It is a LuxCore RenderConfig binary archive
			config = RenderConfig::Create(configFileName);
			auto props = std::make_unique<Properties>(std::move(cmdLineProp));
			config->Parse(props);
		} else if (configFileNameExt == ".rsm") {
			// It is a rendering resume file
			config = RenderConfig::Create(configFileName, startRenderState, startFilm);
			auto props = std::make_unique<Properties>(std::move(cmdLineProp));
			config->Parse(props);
		} else
			throw runtime_error("Unknown file extension: " + configFileName);

		if (removeUnused) {
			// Remove unused Meshes, Image maps, materials and textures
			config->GetScene().RemoveUnusedMeshes();
			config->GetScene().RemoveUnusedImageMaps();
			config->GetScene().RemoveUnusedMaterials();
			config->GetScene().RemoveUnusedTextures();
		}

		const bool fileSaverRenderEngine = (config->GetProperty("renderengine.type").Get<string>() == "FILESAVER");
		if (!fileSaverRenderEngine) {
			// Force the film update at 2.5secs (mostly used by PathOCL)
			auto props = std::make_unique<Properties>();
			props->Set(Property("screen.refresh.interval")(2500));
			config->Parse(props);
		}

		BatchRendering(config, startRenderState, startFilm, showDevicesStats);


		LC_LOG("Done.");
	} catch (runtime_error &err) {
		LC_LOG("RUNTIME ERROR: " << err.what());
		return EXIT_FAILURE;
	} catch (exception &err) {
		LC_LOG("ERROR: " << err.what());
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
// vim: autoindent noexpandtab tabstop=4 shiftwidth=4
