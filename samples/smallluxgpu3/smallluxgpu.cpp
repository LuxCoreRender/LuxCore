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

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <sstream>
#include <stdexcept>

#include <boost/thread.hpp>
#include <boost/filesystem.hpp>

// Required when using XInitThread()
//#include <X11/Xlib.h>

#include "luxrays/core/device.h"
#include "luxrays/opencl/utils.h"

#include "smalllux.h"
#include "displayfunc.h"
#include "rendersession.h"
#include "pathocl/pathocl.h"
#include "telnet.h"

string SLG_LABEL = "SmallLuxGPU v" SLG_VERSION_MAJOR "." SLG_VERSION_MINOR " (LuxRays demo: http://www.luxrender.net)";

RenderSession *session = NULL;

// Mouse "grab" mode. This is the natural way cameras are usually manipulated
// The flag is off by default but can be turned on by using the -m switch

bool mouseGrabMode = false;

void DebugHandler(const char *msg) {
	cerr << "[LuxRays] " << msg << endl;
}

void SDLDebugHandler(const char *msg) {
	cerr << "[LuxRays::SDL] " << msg << endl;
}

void SLGDebugHandler(const char *msg) {
	cerr << "[SLG] " << msg << endl;
}

#if defined(__GNUC__) && !defined(__CYGWIN__)
#include <execinfo.h>
#include <stdio.h>
#include <stdlib.h>
#include <typeinfo>
#include <cxxabi.h>

static string Demangle(const char *symbol) {
	size_t size;
	int status;
	char temp[128];
	char* result;

	if (1 == sscanf(symbol, "%*[^'(']%*[^'_']%[^')''+']", temp)) {
		if (NULL != (result = abi::__cxa_demangle(temp, NULL, &size, &status))) {
			string r = result;
			return r + " [" + symbol + "]";
		}
	}

	if (1 == sscanf(symbol, "%127s", temp))
		return temp;

	return symbol;
}

void SLGTerminate(void) {
	SLG_LOG("=========================================================");
	SLG_LOG("Unhandled exception");

	void *array[32];
	size_t size = backtrace(array, 32);
	char **strings = backtrace_symbols(array, size);

	SLG_LOG("Obtained " << size << " stack frames.");

	for (size_t i = 0; i < size; i++)
		SLG_LOG("  " << Demangle(strings[i]));

	free(strings);
}
#endif

void FreeImageErrorHandler(FREE_IMAGE_FORMAT fif, const char *message) {
	printf("\n*** ");
	if(fif != FIF_UNKNOWN)
		printf("%s Format\n", FreeImage_GetFormatFromFIF(fif));

	printf("%s", message);
	printf(" ***\n");
}

static int BatchMode(const double stopTime, const unsigned int stopSPP, const float haltthreshold) {
	RenderConfig *config = session->renderConfig;
	RenderEngine *engine = session->renderEngine;

	// Force the film update at 2.5secs (mostly used by PathOCL)
	config->SetScreenRefreshInterval(2500);

	// Start the rendering
	session->Start();
	const double startTime = WallClockTime();

	double lastFilmUpdate = WallClockTime();
	char buf[512];
	for (;;) {
		boost::this_thread::sleep(boost::posix_time::millisec(1000));

		// Check if periodic save is enabled
		if (session->NeedPeriodicSave()) {
			// Time to save the image and film
			session->SaveFilmImage();
			lastFilmUpdate =  WallClockTime();
		} else {
			// Film update may be required by some render engine to
			// update statistics, convergence test and more
			if (WallClockTime() - lastFilmUpdate > 5.0) {
				session->renderEngine->UpdateFilm();
				lastFilmUpdate =  WallClockTime();
			}
		}

		const double now = WallClockTime();
		const double elapsedTime = now - startTime;
		if ((stopTime > 0) && (elapsedTime >= stopTime))
			break;

		const unsigned int pass = engine->GetPass();
		if ((stopSPP > 0) && (pass >= stopSPP))
			break;

		// Convergence test is update inside UpdateFilm()
		const float convergence = engine->GetConvergence();
		if ((haltthreshold >= 0.f) && (1.f - convergence <= haltthreshold))
			break;

		// Print some information about the rendering progress
		sprintf(buf, "[Elapsed time: %3d/%dsec][Samples %4d/%d][Convergence %f%%][Avg. samples/sec % 3.2fM on %.1fK tris]",
				int(elapsedTime), int(stopTime), pass, stopSPP, 100.f * convergence, engine->GetTotalSamplesSec() / 1000000.0,
				config->scene->dataSet->GetTotalTriangleCount() / 1000.0);

		SLG_LOG(buf);
	}

	// Stop the rendering
	session->Stop();

	// Save the rendered image
	session->SaveFilmImage();

	delete session;
	SLG_LOG("Done.");

	return EXIT_SUCCESS;
}

int main(int argc, char *argv[]) {
#if defined(__GNUC__) && !defined(__CYGWIN__)
	set_terminate(SLGTerminate);
#endif

	// This is required to run AMD GPU profiler
    //XInitThreads();

	luxrays::sdl::LuxRaysSDLDebugHandler = SDLDebugHandler;

	try {

		// Initialize FreeImage Library
		FreeImage_Initialise(TRUE);
		FreeImage_SetOutputMessage(FreeImageErrorHandler);

		bool batchMode = false;
		bool telnetServerEnabled = false;
		Properties cmdLineProp;
		string configFileName;
		for (int i = 1; i < argc; i++) {
			if (argv[i][0] == '-') {
				// I should check for out of range array index...

				if (argv[i][1] == 'h') {
					SLG_LOG("Usage: " << argv[0] << " [options] [configuration file]" << endl <<
							" -o [configuration file]" << endl <<
							" -f [scene file]" << endl <<
							" -w [window width]" << endl <<
							" -e [window height]" << endl <<
							" -t [halt time in secs]" << endl <<
							" -T <enable the telnet server>" << endl <<
							" -D [property name] [property value]" << endl <<
							" -d [current directory path]" << endl <<
							" -m Makes the mouse operations work in \"grab mode\"" << endl << 
							" -h <display this help and exit>");
					exit(EXIT_SUCCESS);
				}
				else if (argv[i][1] == 'o') {
					if (configFileName.compare("") != 0)
						throw runtime_error("Used multiple configuration files");

					configFileName = string(argv[++i]);
				}

				else if (argv[i][1] == 'e') cmdLineProp.SetString("image.height", argv[++i]);

				else if (argv[i][1] == 'w') cmdLineProp.SetString("image.width", argv[++i]);

				else if (argv[i][1] == 'f') cmdLineProp.SetString("scene.file", argv[++i]);

				else if (argv[i][1] == 't') cmdLineProp.SetString("batch.halttime", argv[++i]);

				else if (argv[i][1] == 'T') telnetServerEnabled = true;

				else if (argv[i][1] == 'm') mouseGrabMode = true;

				else if (argv[i][1] == 'D') {
					cmdLineProp.SetString(argv[i + 1], argv[i + 2]);
					i += 2;
				}

				else if (argv[i][1] == 'd') boost::filesystem::current_path(boost::filesystem::path(argv[++i]));

				else {
					SLG_LOG("Invalid option: " << argv[i]);
					exit(EXIT_FAILURE);
				}
			} else {
				string s = argv[i];
				if ((s.length() >= 4) && (s.substr(s.length() - 4) == ".cfg")) {
					if (configFileName.compare("") != 0)
						throw runtime_error("Used multiple configuration files");
					configFileName = s;
				} else
					throw runtime_error("Unknown file extension: " + s);
			}
		}

		if (configFileName.compare("") == 0)
			configFileName = "scenes/luxball/render-fast.cfg";

		RenderConfig *config = new RenderConfig(configFileName, &cmdLineProp);

		const unsigned int halttime = config->cfg.GetInt("batch.halttime", 0);
		const unsigned int haltspp = config->cfg.GetInt("batch.haltspp", 0);
		const float haltthreshold = config->cfg.GetFloat("batch.haltthreshold", -1.f);
		if ((halttime > 0) || (haltspp > 0) || (haltthreshold >= 0.f))
			batchMode = true;
		else
			batchMode = false;

		if (batchMode) {
			session = new RenderSession(config);
			return BatchMode(halttime, haltspp, haltthreshold);
		} else {
			// It is important to initialize OpenGL before OpenCL
			// (for OpenGL/OpenCL interoperability)
			unsigned int width = config->cfg.GetInt("image.width", 640);
			unsigned int height = config->cfg.GetInt("image.height", 480);

			InitGlut(argc, argv, width, height);

			session = new RenderSession(config);

			// Start the rendering
			session->Start();

			if (telnetServerEnabled) {
				TelnetServer telnetServer(18081, session);
				RunGlut();
			} else
				RunGlut();
		}
	} catch (cl::Error err) {
		SLG_LOG("OpenCL ERROR: " << err.what() << "(" << luxrays::utils::oclErrorString(err.err()) << ")");
	} catch (runtime_error err) {
		SLG_LOG("RUNTIME ERROR: " << err.what());
	} catch (exception err) {
		SLG_LOG("ERROR: " << err.what());
	}

	return EXIT_SUCCESS;
}
