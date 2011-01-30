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
	engineThread = NULL;
	renderConfig = NULL;
	renderRefreshTimer = NULL;
}

LuxMarkApp::~LuxMarkApp() {
	if (engineThread) {
		// Stop the rendering engine
		engineThread->interrupt();
		engineThread->join();
		delete engineThread;
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

	SetMode(BENCHMARK);
}

void LuxMarkApp::SetMode(LuxMarkAppMode m) {
	delete renderRefreshTimer;
	renderRefreshTimer = NULL;

	// Stop the engine thread if required
	if (engineThread) {
		// Stop the rendering engine
		engineThread->interrupt();
		engineThread->join();
	}
	delete engineThread;
	engineThread = NULL;

	// Free the scene if required

	delete renderConfig;
	renderConfig = NULL;

	// Initialize the new mode

	mode = m;
	if (mode == BENCHMARK) {
		renderConfig = new RenderingConfig("scenes/luxball/render.cfg");
		renderConfig->Init();

		// Update timer
		renderRefreshTimer = new QTimer();
		connect(renderRefreshTimer, SIGNAL(timeout()), SLOT(RenderRefreshTimeout()));

		// Refresh the screen every 5 secs in benchmark mode
		renderRefreshTimer->start(5 * 1000);
	}
}

void LuxMarkApp::RenderRefreshTimeout() {
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
}
