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
#include <boost/lexical_cast.hpp>

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

static int BatchTileMode(const unsigned int stopSPP, const float haltThreshold) {
	RenderConfig *config = session->renderConfig;
	RenderEngine *engine = session->renderEngine;

	// batch.halttime condition doesn't make sense in the context
	// of tile rendering
	if (config->cfg.IsDefined("batch.halttime") && (config->cfg.GetInt("batch.halttime", 0) > 0))
		throw runtime_error("batch.halttime parameter can not be used with batch.tile");
	// image.subregion condition doesn't make sense in the context
	// of tile rendering
	if (config->cfg.IsDefined("image.subregion"))
		throw runtime_error("image.subregion parameter can not be used with batch.tile");

	// Force the film update at 2.5secs (mostly used by PathOCL)
	config->SetScreenRefreshInterval(2500);

	const u_int filterBorder = 2;

	// Original film size
	const u_int originalFilmWidth = session->film->GetWidth();
	const u_int originalFilmHeight = session->film->GetHeight();

	// Allocate a film where to merge all tiles
	Film mergeTileFilm(originalFilmWidth, originalFilmWidth,
			session->film->HasPerPixelNormalizedBuffer(), session->film->HasPerScreenNormalizedBuffer(),
			true);
	mergeTileFilm.CopyDynamicSettings(*(session->film));
	mergeTileFilm.EnableOverlappedScreenBufferUpdate(false);
	mergeTileFilm.Init(originalFilmWidth, originalFilmWidth);

	// Get the tile size
	vector<int> tileSize = config->cfg.GetIntVector("batch.tile", "256 256");
	if (tileSize.size() != 2)
		throw runtime_error("Syntax error in batch.tile (required 2 parameters)");
	tileSize[0] = Max<int>(64, Min<int>(tileSize[0], originalFilmWidth));
	tileSize[1] = Max<int>(64, Min<int>(tileSize[1], originalFilmHeight));
	SLG_LOG("Tile size: " << tileSize[0] << " x " << tileSize[1]);

	// Get the loop count
	int loopCount = config->cfg.GetInt("batch.tile.loops", 1);

	// Start the rendering
	session->Start();

	for (int loopIndex = 0; loopIndex < loopCount; ++loopIndex) {
		u_int tileX = 0;
		u_int tileY = 0;
		const double startTime = WallClockTime();

		// To setup new rendering parameters
		session->BeginEdit();

		for (;;) {
			SLG_LOG("Rendering tile offset: (" << tileX << "/" << originalFilmWidth  + 2 * filterBorder << ", " <<
					tileY << "/" << originalFilmHeight + 2 * filterBorder << ")");

			// Set the film subregion to render
			u_int filmSubRegion[4];
			filmSubRegion[0] = tileX;
			filmSubRegion[1] = Min(tileX + tileSize[0] - 1, originalFilmWidth - 1) + 2 * filterBorder;
			filmSubRegion[2] = tileY;
			filmSubRegion[3] = Min(tileY + tileSize[1] - 1, originalFilmHeight - 1) + 2 * filterBorder;
			SLG_LOG("Tile subregion: " << 
					boost::lexical_cast<string>(filmSubRegion[0]) << " " << 
					boost::lexical_cast<string>(filmSubRegion[1]) << " " <<
					boost::lexical_cast<string>(filmSubRegion[2]) << " " <<
					boost::lexical_cast<string>(filmSubRegion[3]));

			// Update the camera and resize the film
			session->renderConfig->scene->camera->Update(
					originalFilmWidth + 2 * filterBorder,
					originalFilmHeight + 2 * filterBorder,
					filmSubRegion);
			session->editActions.AddAction(CAMERA_EDIT);

			session->film->Init(session->renderConfig->scene->camera->GetFilmWeight(),
					session->renderConfig->scene->camera->GetFilmHeight());
			session->editActions.AddAction(FILM_EDIT);

			session->EndEdit();

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

				const unsigned int pass = engine->GetPass();
				if ((stopSPP > 0) && (pass >= stopSPP))
					break;

				// Convergence test is update inside UpdateFilm()
				const float convergence = engine->GetConvergence();
				if ((haltThreshold >= 0.f) && (1.f - convergence <= haltThreshold))
					break;

				// Print some information about the rendering progress
				const double now = WallClockTime();
				const double elapsedTime = now - startTime;
				sprintf(buf, "[Loop step: %d/%d][Elapsed time: %3d][Samples %4d/%d][Convergence %f%%][Avg. samples/sec % 3.2fM on %.1fK tris]",
						loopIndex + 1, loopCount,
						int(elapsedTime), pass, stopSPP, 100.f * convergence, engine->GetTotalSamplesSec() / 1000000.0,
						config->scene->dataSet->GetTotalTriangleCount() / 1000.0);

				SLG_LOG(buf);
			}

			// Splat the current tile on the merge film
			session->renderEngine->UpdateFilm();

			{
				boost::unique_lock<boost::mutex> lock(session->filmMutex);
				mergeTileFilm.AddFilm(*(session->film), filterBorder, filterBorder,
						filmSubRegion[1] - filmSubRegion[0] - 2 * filterBorder + 1,
						filmSubRegion[3] - filmSubRegion[2] - 2 * filterBorder + 1,
						tileX, tileY);
			}

			// Save the merge film
			const string fileName = config->cfg.GetString("image.filename", "image.png");
			SLG_LOG("Saving merged tiles to: " << fileName);
			mergeTileFilm.UpdateScreenBuffer();
			mergeTileFilm.SaveScreenBuffer(fileName);

			// Advance to the next tile
			tileX += tileSize[0];
			if (tileX >= originalFilmWidth) {
				tileX = 0;
				tileY += tileSize[1];

				if (tileY >= originalFilmHeight) {
					// Rendering done
					break;
				}
			}

			// To setup new rendering parameters
			session->BeginEdit();
		}
	}

	// Stop the rendering
	session->Stop();

	delete session;
	SLG_LOG("Done.");

	return EXIT_SUCCESS;
}

static int BatchSimpleMode(const double stopTime, const unsigned int stopSPP, const float haltThreshold) {
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
		if ((haltThreshold >= 0.f) && (1.f - convergence <= haltThreshold))
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

			// Check if I have to do tile rendering
			if (config->cfg.IsDefined("batch.tile"))
				return BatchTileMode(haltspp, haltthreshold);
			else
				return BatchSimpleMode(halttime, haltspp, haltthreshold);
		} else {
			// It is important to initialize OpenGL before OpenCL
			// (for OpenGL/OpenCL inter-operability)
			u_int width, height;
			config->GetScreenSize(&width, &height);
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
