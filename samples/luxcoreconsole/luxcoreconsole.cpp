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

#include <boost/filesystem.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/format.hpp>
#include <boost/thread.hpp>
#include <boost/filesystem.hpp>
#include <boost/algorithm/string/case_conv.hpp>

#include "luxrays/utils/oclerror.h"
#include "luxcore/luxcore.h"

using namespace std;
using namespace luxrays;
using namespace luxcore;

static string GetFileNameExt(const string &fileName) {
	return boost::algorithm::to_lower_copy(boost::filesystem::path(fileName).extension().string());
}

static void BatchRendering(RenderConfig *config, RenderState *startState, Film *startFilm) {
	RenderSession *session = RenderSession::Create(config, startState, startFilm);

	const unsigned int haltTime = config->GetProperty("batch.halttime").Get<unsigned int>();
	const unsigned int haltSpp = config->GetProperty("batch.haltspp").Get<unsigned int>();

	// Start the rendering
	session->Start();

	const Properties &stats = session->GetStats();
	while (!session->HasDone()) {
		boost::this_thread::sleep(boost::posix_time::millisec(1000));
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
	}

	// Stop the rendering
	session->Stop();

	const string renderEngine = config->GetProperty("renderengine.type").Get<string>();
	if (renderEngine != "FILESAVER") {
		// Save the rendered image
		session->GetFilm().SaveOutputs();
	}

	delete session;
}

int main(int argc, char *argv[]) {
	// This is required to run AMD GPU profiler
	//XInitThreads();

	try {
		// Initialize LuxCore
		luxcore::Init();

		bool removeUnused = false;
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

				else if (argv[i][1] == 'd') boost::filesystem::current_path(boost::filesystem::path(argv[++i]));

				else if (argv[i][1] == 'c') removeUnused = true;

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

		// Load the Scene
		if (configFileName.compare("") == 0)
			throw runtime_error("You must specify a file to render");

		// Check if we have to parse a LuxCore SDL file or a LuxRender SDL file
		Scene *scene = NULL;
		RenderConfig *config;
		RenderState *startRenderState = NULL;
		Film *startFilm = NULL;
		
		if (configFileName.compare("") != 0) {
			// Clear the file name resolver list
			luxcore::ClearFileNameResolverPaths();
			// Add the current directory to the list of place where to look for files
			luxcore::AddFileNameResolverPath(".");
			// Add the .cfg directory to the list of place where to look for files
			boost::filesystem::path path(configFileName);
			luxcore::AddFileNameResolverPath(path.parent_path().generic_string());
		}
		
		const string configFileNameExt = GetFileNameExt(configFileName);
		if (configFileNameExt == ".lxs") {
			// It is a LuxRender SDL file
			LC_LOG("Parsing LuxRender SDL file...");
			Properties renderConfigProps, sceneProps;
			luxcore::ParseLXS(configFileName, renderConfigProps, sceneProps);

			// For debugging
			//LC_LOG("RenderConfig: \n" << renderConfigProps);
			//LC_LOG("Scene: \n" << sceneProps);

			renderConfigProps.Set(cmdLineProp);

			scene = Scene::Create();
			scene->Parse(sceneProps);
			config = RenderConfig::Create(renderConfigProps.Set(cmdLineProp), scene);
		} else if (configFileNameExt == ".cfg") {
			// It is a LuxCore SDL file
			config = RenderConfig::Create(Properties(configFileName).Set(cmdLineProp));
		} else if (configFileNameExt == ".bcf") {
			// It is a LuxCore RenderConfig binary archive
			config = RenderConfig::Create(configFileName);
			config->Parse(cmdLineProp);
		} else if (configFileNameExt == ".rsm") {
			// It is a rendering resume file
			config = RenderConfig::Create(configFileName, &startRenderState, &startFilm);
			config->Parse(cmdLineProp);
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
			config->Parse(Properties().Set(Property("screen.refresh.interval")(2500)));
		}
		
		BatchRendering(config, startRenderState, startFilm);

		delete config;
		delete scene;

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
