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

#include <iostream>
#include <boost/filesystem.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/format.hpp>

#include "luxrays/luxrays.h"
#include "luxrays/utils/ocl.h"
#include "luxcoreapp.h"

using namespace std;
using namespace luxrays;
using namespace luxcore;

/*
// Code used to convert samples/luxcoreui/resources/luxlogo_bg.png

#include <OpenImageIO/imagebufalgo.h>
#include <OpenImageIO/imagebuf.h>
#include <OpenImageIO/dassert.h>

static void ConvertImage(const string &fileName) {
	auto_ptr<ImageInput> in(ImageInput::open(fileName));
	
	const ImageSpec &spec = in->spec();
	auto_ptr<u_char> pixels(new u_char[spec.width * spec.height * spec.nchannels] );

	in->read_image(TypeDesc::UCHAR, pixels.get());

	cout << "const unsigned int imageWidth = " << spec.width << ";\n";
	cout << "const unsigned int imageHeight = " << spec.height << ";\n";
	
	cout << "const unsigned char image[] = {\n";
	for (int y = 0; y < spec.height; ++y) {
		for (int x = 0; x < spec.width; ++x) {
			const int index = (x + y * spec.width) * 3;
			cout <<
					(u_int)pixels.get()[index] << ", " <<
					(u_int)pixels.get()[index + 1] << ", " <<
					(u_int)pixels.get()[index + 2] << ", ";
		}
		cout << "\n";
	}
	cout << "};\n";

	in->close();
	in.reset();
}
*/

int main(int argc, char *argv[]) {
	try {
		// Initialize LuxCore
		luxcore::Init(LuxCoreApp::LogHandler);

		LA_LOG("LuxCoreUI v" LUXCORE_VERSION_MAJOR "." LUXCORE_VERSION_MINOR " (LuxCore demo: http://www.luxrender.net)");

		//ConvertImage("samples/luxcoreui/resources/luxlogo_bg.png");
		
		bool removeUnused = false;
		bool mouseGrabMode = false;
		bool fullScreen = false;
		Properties cmdLineProp;
		string configFileName, filmFileName, renderStateFileName;
		for (int i = 1; i < argc; i++) {
			if (argv[i][0] == '-') {
				// I should check for out of range array index...

				if (argv[i][1] == 'h') {
					LA_LOG("Usage: " << argv[0] << " [options] [configuration file]" << endl <<
							" -o [configuration file]" << endl <<
							" -f [scene file]" << endl <<
							" -w [window width]" << endl <<
							" -e [window height]" << endl <<
							" -g <enable full screen mode>" << endl <<
							" -t [halt time in secs]" << endl <<
							" -D [property name] [property value]" << endl <<
							" -d [current directory path]" << endl <<
							" -m <makes the mouse operations work in \"grab mode\">" << endl << 
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

				else if (argv[i][1] == 'g') fullScreen = true;

				else if (argv[i][1] == 'w') cmdLineProp.Set(Property("film.width")(argv[++i]));

				else if (argv[i][1] == 'f') cmdLineProp.Set(Property("scene.file")(argv[++i]));

				else if (argv[i][1] == 't') cmdLineProp.Set(Property("batch.halttime")(argv[++i]));

				else if (argv[i][1] == 'm') mouseGrabMode = true;

				else if (argv[i][1] == 'D') {
					cmdLineProp.Set(Property(argv[i + 1]).Add(argv[i + 2]));
					i += 2;
				}

				else if (argv[i][1] == 'd') boost::filesystem::current_path(boost::filesystem::path(argv[++i]));

				else if (argv[i][1] == 'c') removeUnused = true;

				else {
					LA_LOG("Invalid option: " << argv[i]);
					exit(EXIT_FAILURE);
				}
			} else {
				string s = argv[i];
				if ((s.length() >= 4) && ((s.substr(s.length() - 4) == ".cfg") || (s.substr(s.length() - 4) == ".lxs"))) {
					if (configFileName.compare("") != 0)
						throw runtime_error("Used multiple configuration files");
					configFileName = s;
				} else if ((s.length() >= 4) && ((s.substr(s.length() - 4) == ".flm"))) {
					if (filmFileName.compare("") != 0)
						throw runtime_error("Used multiple film files");
					filmFileName = s;
				} else if ((s.length() >= 4) && ((s.substr(s.length() - 4) == ".rst"))) {
					if (renderStateFileName.compare("") != 0)
						throw runtime_error("Used multiple render state files");
					renderStateFileName = s;
				} else
					throw runtime_error("Unknown file extension: " + s);
			}
		}

		// Check if we have to parse a LuxCore SDL file or a LuxRender SDL file
		RenderConfig *config;
		if (configFileName.compare("") == 0) {
			// Start without a rendering
			config = NULL;
		} else if ((configFileName.length() >= 4) && (configFileName.substr(configFileName.length() - 4) == ".lxs")) {
			// It is a LuxRender SDL file
			LA_LOG("Parsing LuxRender SDL file...");
			Properties renderConfigProps, sceneProps;
			luxcore::ParseLXS(configFileName, renderConfigProps, sceneProps);

			// For debugging
			//LA_LOG("RenderConfig: \n" << renderConfigProps);
			//LA_LOG("Scene: \n" << sceneProps);

			Scene *scene = Scene::Create(renderConfigProps.Get(Property("images.scale")(1.f)).Get<float>());
			scene->Parse(sceneProps);
			config = RenderConfig::Create(renderConfigProps.Set(cmdLineProp), scene);
			config->DeleteSceneOnExit();
		} else {
			// It is a LuxCore SDL file
			config = RenderConfig::Create(Properties(configFileName).Set(cmdLineProp));
		}

		if (config && removeUnused) {
			// Remove unused materials and textures
			config->GetScene().RemoveUnusedMeshes();
			config->GetScene().RemoveUnusedImageMaps();
			config->GetScene().RemoveUnusedMaterials();
			config->GetScene().RemoveUnusedTextures();
		}

		// Load the start film
		Film *startFilm;
		if (filmFileName.compare("") == 0)
			startFilm = NULL;
		else
			startFilm = Film::Create(filmFileName);

		// Load the start render state
		RenderState *startRenderState;
		if (renderStateFileName.compare("") == 0)
			startRenderState = NULL;
		else
			startRenderState = RenderState::Create(renderStateFileName);

		if (!config && (startFilm || startRenderState))
			throw runtime_error("You have to use also a render configuration file to resume the rendering");
		if (config && ((startFilm && !startRenderState) || (!startFilm && startRenderState)))
			throw runtime_error("You have to use both a film and render state to resume the rendering");

		if (config && (config->ToProperties().Get("renderengine.type").Get<string>() == "FILESAVER")) {
			RenderSession *session = new RenderSession(config);

			// Save the scene and exit
			session->Start();
			session->Stop();

			delete session;
			delete config;
		} else {
			LuxCoreApp app(config, startRenderState, startFilm);
			app.optMouseGrabMode = mouseGrabMode;
			app.optFullScreen = fullScreen;

			app.RunApp();
		}

		LA_LOG("Done.");

#if !defined(LUXRAYS_DISABLE_OPENCL)
	} catch (cl::Error &err) {
		LA_LOG("OpenCL ERROR: " << err.what() << "(" << oclErrorString(err.err()) << ")");
		return EXIT_FAILURE;
#endif
	} catch (runtime_error &err) {
		LA_LOG("RUNTIME ERROR: " << err.what());
		return EXIT_FAILURE;
	} catch (exception &err) {
		LA_LOG("ERROR: " << err.what());
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
