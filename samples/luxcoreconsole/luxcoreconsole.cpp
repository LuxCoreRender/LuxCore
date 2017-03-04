/***************************************************************************
 * Copyright 1998-2017 by authors (see AUTHORS.txt)                        *
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

#include "luxrays/luxrays.h"
#include "luxrays/utils/ocl.h"
#include "luxcore/luxcore.h"

using namespace std;
using namespace luxrays;
using namespace luxcore;

static void BatchSimpleMode(RenderConfig *config) {
	RenderSession *session = new RenderSession(config);

	const u_int haltTime = config->GetProperty("batch.halttime").Get<u_int>();
	const u_int haltSpp = config->GetProperty("batch.haltspp").Get<u_int>();
	const float haltThreshold = config->GetProperty("batch.haltthreshold").Get<float>();

	// Start the rendering
	session->Start();

	const Properties &stats = session->GetStats();
	while (!session->HasDone()) {
		boost::this_thread::sleep(boost::posix_time::millisec(1000));
		session->UpdateStats();

		// Check if periodic save is enabled
		if (session->NeedPeriodicFilmSave()) {
			// Time to save the image and film
			session->GetFilm().SaveOutputs();
		}

		const double elapsedTime = stats.Get("stats.renderengine.time").Get<double>();
		if ((haltTime > 0) && (elapsedTime >= haltTime))
			break;

		const u_int pass = stats.Get("stats.renderengine.pass").Get<u_int>();
		if ((haltSpp > 0) && (pass >= haltSpp))
			break;

		// Convergence test is update inside UpdateFilm()
		const float convergence = stats.Get("stats.renderengine.convergence").Get<float>();
		if ((haltThreshold >= 0.f) && (1.f - convergence <= haltThreshold))
			break;

		// Print some information about the rendering progress
		LC_LOG(boost::str(boost::format("[Elapsed time: %3d/%dsec][Samples %4d/%d][Convergence %f%%][Avg. samples/sec % 3.2fM on %.1fK tris]") %
				int(elapsedTime) % int(haltTime) % pass % haltSpp % (100.f * convergence) %
				(stats.Get("stats.renderengine.total.samplesec").Get<double>() / 1000000.0) %
				(stats.Get("stats.dataset.trianglecount").Get<double>() / 1000.0)));

	}

	// Stop the rendering
	session->Stop();

	// Save the rendered image
	session->GetFilm().SaveOutputs();

	delete session;
}

int main(int argc, char *argv[]) {
	// This is required to run AMD GPU profiler
	//XInitThreads();

	try {
		// Initialize LuxCore
		luxcore::Init();

		bool removeUnusedMatsAndTexs = false;
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
							" -t [halt time in secs]" << endl <<
							" -D [property name] [property value]" << endl <<
							" -d [current directory path]" << endl <<
							" -c <remove all unused materials and textures>" << endl <<
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

				else if (argv[i][1] == 't') cmdLineProp.Set(Property("batch.halttime")(argv[++i]));

				else if (argv[i][1] == 'D') {
					cmdLineProp.Set(Property(argv[i + 1]).Add(argv[i + 2]));
					i += 2;
				}

				else if (argv[i][1] == 'd') boost::filesystem::current_path(boost::filesystem::path(argv[++i]));

				else if (argv[i][1] == 'c') removeUnusedMatsAndTexs = true;

				else {
					LC_LOG("Invalid option: " << argv[i]);
					exit(EXIT_FAILURE);
				}
			} else {
				string s = argv[i];
				if ((s.length() >= 4) && ((s.substr(s.length() - 4) == ".cfg") || (s.substr(s.length() - 4) == ".lxs"))) {
					if (configFileName.compare("") != 0)
						throw runtime_error("Used multiple configuration files");
					configFileName = s;
				} else
					throw runtime_error("Unknown file extension: " + s);
			}
		}

		// Load the Scene
		if (configFileName.compare("") == 0)
			configFileName = "scenes/luxball/luxball.cfg";

		// Check if we have to parse a LuxCore SDL file or a LuxRender SDL file
		Scene *scene;
		RenderConfig *config;
		if ((configFileName.length() >= 4) && (configFileName.substr(configFileName.length() - 4) == ".lxs")) {
			// It is a LuxRender SDL file
			LC_LOG("Parsing LuxRender SDL file...");
			Properties renderConfigProps, sceneProps;
			luxcore::ParseLXS(configFileName, renderConfigProps, sceneProps);

			// For debugging
			//LC_LOG("RenderConfig: \n" << renderConfigProps);
			//LC_LOG("Scene: \n" << sceneProps);

			renderConfigProps.Set(cmdLineProp);

			scene = Scene::Create(renderConfigProps.Get(Property("images.scale")(1.f)).Get<float>());
			scene->Parse(sceneProps);
			config = RenderConfig::Create(renderConfigProps.Set(cmdLineProp), scene);
		} else {
			// It is a LuxCore SDL file
			config = RenderConfig::Create(Properties(configFileName).Set(cmdLineProp));
			scene = NULL;
		}

		if (removeUnusedMatsAndTexs) {
			// Remove unused materials and textures
			config->GetScene().RemoveUnusedMaterials();
			config->GetScene().RemoveUnusedTextures();
		}

		const bool fileSaverRenderEngine = (config->GetProperty("renderengine.type").Get<string>() == "FILESAVER");
		if (fileSaverRenderEngine) {
			RenderSession *session = new RenderSession(config);

			// Save the scene and exit
			session->Start();
			session->Stop();

			delete session;
		} else {
			// Force the film update at 2.5secs (mostly used by PathOCL)
			config->Parse(Properties().Set(Property("screen.refresh.interval")(2500)));

			BatchSimpleMode(config);
		}

		delete config;
		delete scene;

		LC_LOG("Done.");

#if !defined(LUXRAYS_DISABLE_OPENCL)
	} catch (cl::Error &err) {
		LC_LOG("OpenCL ERROR: " << err.what() << "(" << oclErrorString(err.err()) << ")");
		return EXIT_FAILURE;
#endif
	} catch (runtime_error &err) {
		LC_LOG("RUNTIME ERROR: " << err.what());
		return EXIT_FAILURE;
	} catch (exception &err) {
		LC_LOG("ERROR: " << err.what());
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
