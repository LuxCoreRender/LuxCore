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

#include "luxmarkcfg.h"
#include "luxmarkapp.h"
#include "renderconfig.h"
#include "luxrays/utils/film/film.h"
#include "path/path.h"
#include "sppm/sppm.h"
#include "pathgpu/pathgpu.h"

void FreeImageErrorHandler(FREE_IMAGE_FORMAT fif, const char *message) {
	printf("\n*** ");
	if(fif != FIF_UNKNOWN)
		printf("%s Format\n", FreeImage_GetFormatFromFIF(fif));

	printf("%s", message);
	printf(" ***\n");
}

LuxMarkApp::LuxMarkApp(int argc, char **argv) : QApplication(argc, argv) {
	// Initialize FreeImage Library, it must be done after QApplication() in
	// order to avoid a crash
	FreeImage_Initialise(TRUE);
	FreeImage_SetOutputMessage(FreeImageErrorHandler);

	mainWin = NULL;
	engineInitThread = NULL;
	engineInitDone = false;
	renderingStartTime = 0.0;
	renderConfig = NULL;
	renderRefreshTimer = NULL;
}

LuxMarkApp::~LuxMarkApp() {
	if (engineInitThread) {
		// Wait for the init rendering thread
		engineInitThread->join();
		delete engineInitThread;
	}
	delete renderConfig;
	delete mainWin;
}

void LuxMarkApp::Init(void) {
	mainWin = new MainWindow();
	mainWin->show();
	LogWindow = mainWin;

	LM_LOG("<FONT COLOR=\"#0000ff\">LuxMark V" << LUXMARK_VERSION_MAJOR << "." << LUXMARK_VERSION_MINOR << "</FONT>");
	LM_LOG("Based on <FONT COLOR=\"#0000ff\">" << SLG_LABEL << "</FONT>");

	InitRendering(BENCHMARK, SCENE_LUXBALL_HDR);
}

void LuxMarkApp::SetMode(LuxMarkAppMode m) {
	InitRendering(m, sceneName);
}

void LuxMarkApp::SetScene(const char *name) {
	InitRendering(mode, name);
}

void LuxMarkApp::InitRendering(LuxMarkAppMode m, const char *scnName) {
	mode = m;
	sceneName = scnName;

	delete renderRefreshTimer;
	renderRefreshTimer = NULL;

	if (engineInitThread) {
		// Wait for the init rendering thread
		engineInitThread->join();
		delete engineInitThread;
		engineInitThread = NULL;
	}
	engineInitDone = false;

	// Free the scene if required
	delete renderConfig;
	renderConfig = NULL;

	if (!strcmp(scnName, SCENE_LUXBALL_HDR))
		mainWin->SetSceneCheck(0);
	else if (!strcmp(scnName, SCENE_LUXBALL))
		mainWin->SetSceneCheck(1);

	// Initialize the new mode
	if (mode == BENCHMARK) {
		mainWin->SetModeCheck(0);

		// Update timer
		renderRefreshTimer = new QTimer();
		connect(renderRefreshTimer, SIGNAL(timeout()), SLOT(RenderRefreshTimeout()));

		// Refresh the screen every 5 secs in benchmark mode
		renderRefreshTimer->start(5 * 1000);
	} else
		mainWin->SetModeCheck(1);

	mainWin->ShowLogo();

	// Start the engine init thread
	engineInitThread = new boost::thread(boost::bind(LuxMarkApp::EngineInitThreadImpl, this));
}

void LuxMarkApp::EngineInitThreadImpl(LuxMarkApp *app) {
	// Initialize the new mode
	if (app->mode == BENCHMARK) {
		app->renderConfig = new RenderingConfig(app->sceneName);
		app->renderConfig->Init();
	} else {
		// TO DO
	}

	app->renderingStartTime = luxrays::WallClockTime();
	app->engineInitDone = true;
}

void LuxMarkApp::RenderRefreshTimeout() {
	if (!engineInitDone)
		return;

#if !defined(LUXRAYS_DISABLE_OPENCL)
	if (renderConfig->GetRenderEngine()->GetEngineType() == PATHGPU) {
		PathGPURenderEngine *pre = (PathGPURenderEngine *)renderConfig->GetRenderEngine();

		// Need to update the Film
		pre->UpdateFilm();
	}
#endif

	// Get the rendered image
	renderConfig->film->UpdateScreenBuffer();
	const float *pixels = renderConfig->film->GetScreenBuffer();

	// Update the window
	mainWin->ShowFrameBuffer(pixels, renderConfig->film->GetWidth(), renderConfig->film->GetHeight());

	// Update the statistics

	double raysSec = 0.0;
	const vector<IntersectionDevice *> &intersectionDevices = renderConfig->GetIntersectionDevices();
	for (size_t i = 0; i < intersectionDevices.size(); ++i)
		raysSec += intersectionDevices[i]->GetPerformance();

	double sampleSec = 0.0;
	int renderingTime = 0;
	char buf[512];
	
	switch (renderConfig->GetRenderEngine()->GetEngineType()) {
		case DIRECTLIGHT:
		case PATH: {
			renderingTime = int(renderConfig->film->GetTotalTime());
			sampleSec = renderConfig->film->GetAvgSampleSec() * 1000.0;
			break;
		}
		case SPPM: {
			SPPMRenderEngine *sre = (SPPMRenderEngine *)renderConfig->GetRenderEngine();
			renderingTime = int(sre->GetRenderingTime());
			break;
		}
#if !defined(LUXRAYS_DISABLE_OPENCL)
		case PATHGPU: {
			PathGPURenderEngine *pre = (PathGPURenderEngine *)renderConfig->GetRenderEngine();
			renderingTime = int(pre->GetRenderingTime());
			sampleSec = pre->GetTotalSamplesSec();
		}
#endif
		default:
			assert (false);
	}

	const bool valid = (renderingTime > 120);
	sprintf(buf, "[Time: %dsecs (%s)][Samples/sec % 5dK][Rays/sec % 5dK on %.1fK tris]",
			renderingTime, valid ? "OK" : "Wait", int(sampleSec / 1000.0),
			int(raysSec / 1000.0), renderConfig->scene->dataSet->GetTotalTriangleCount() / 1000.0);
	mainWin->UpdateScreenLabel(buf, valid);
}
