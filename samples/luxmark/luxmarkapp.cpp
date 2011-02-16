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

#include <QGraphicsSceneMouseEvent>

#include "luxmarkcfg.h"
#include "luxmarkapp.h"
#include "renderconfig.h"
#include "luxrays/utils/film/film.h"
#include "path/path.h"
#include "sppm/sppm.h"
#include "pathgpu/pathgpu.h"
#include "resultdialog.h"

void FreeImageErrorHandler(FREE_IMAGE_FORMAT fif, const char *message) {
	printf("\n*** ");
	if(fif != FIF_UNKNOWN)
		printf("%s Format\n", FreeImage_GetFormatFromFIF(fif));

	printf("%s", message);
	printf(" ***\n");
}

//------------------------------------------------------------------------------
// LuxMark Qt application
//------------------------------------------------------------------------------

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
	hardwareTreeModel = NULL;
}

LuxMarkApp::~LuxMarkApp() {
	if (engineInitThread) {
		// Wait for the init rendering thread
		engineInitThread->join();
		delete engineInitThread;
	}
	delete renderConfig;
	delete mainWin;
	delete hardwareTreeModel;
}

void LuxMarkApp::Init(void) {
	// Set numeric format to standard to avoid errors when parsing files
	setlocale(LC_ALL, "C");

	mainWin = new MainWindow();
	mainWin->setWindowTitle("LuxMark v" LUXMARK_VERSION_MAJOR "." LUXMARK_VERSION_MINOR);
	mainWin->show();
	mainWin->SetLuxApp(this);
	LogWindow = mainWin;

	LM_LOG("<FONT COLOR=\"#0000ff\">LuxMark v" << LUXMARK_VERSION_MAJOR << "." << LUXMARK_VERSION_MINOR << "</FONT>");
	LM_LOG("Based on <FONT COLOR=\"#0000ff\">" << SLG_LABEL << "</FONT>");

	InitRendering(BENCHMARK_OCL_GPU, SCENE_LUXBALL_HDR);
}

void LuxMarkApp::SetMode(LuxMarkAppMode m) {
	InitRendering(m, sceneName);
}

void LuxMarkApp::SetScene(const char *name) {
	InitRendering(mode, name);
}

void LuxMarkApp::Stop() {
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
}

void LuxMarkApp::InitRendering(LuxMarkAppMode m, const char *scnName) {
	mode = m;
	sceneName = scnName;

	Stop();

	if (!strcmp(scnName, SCENE_LUXBALL_HDR))
		mainWin->SetSceneCheck(0);
	else if (!strcmp(scnName, SCENE_LUXBALL))
		mainWin->SetSceneCheck(1);

	// Initialize the new mode
	if ((mode == BENCHMARK_OCL_GPU) || (mode == BENCHMARK_OCL_CPUGPU) ||
			(mode == BENCHMARK_OCL_CPU) || (mode == BENCHMARK_NATIVE)) {
		if (mode == BENCHMARK_OCL_GPU)
			mainWin->SetModeCheck(0);
		else if (mode == BENCHMARK_OCL_CPUGPU)
			mainWin->SetModeCheck(1);
		else if (mode == BENCHMARK_OCL_CPU)
			mainWin->SetModeCheck(2);
		else
			mainWin->SetModeCheck(3);

		// Update timer
		renderRefreshTimer = new QTimer();
		connect(renderRefreshTimer, SIGNAL(timeout()), SLOT(RenderRefreshTimeout()));

		// Refresh the screen every 5 secs in benchmark mode
		renderRefreshTimer->start(5 * 1000);
	} else if (mode == INTERACTIVE) {
		mainWin->SetModeCheck(4);

		// Update timer
		renderRefreshTimer = new QTimer();
		connect(renderRefreshTimer, SIGNAL(timeout()), SLOT(RenderRefreshTimeout()));

		// Refresh the screen every 100ms in benchmark mode
		renderRefreshTimer->start(100);
	} else if (mode == PAUSE) {
		mainWin->SetModeCheck(5);
	} else
		assert (false);

	mainWin->ShowLogo();

	if (mode != PAUSE) {
		// Start the engine init thread
		engineInitThread = new boost::thread(boost::bind(LuxMarkApp::EngineInitThreadImpl, this));
	}
}

void LuxMarkApp::EngineInitThreadImpl(LuxMarkApp *app) {
	try {
		// Initialize the new mode
		app->renderConfig = new RenderingConfig(app->sceneName);

		// Overwrite properties according the current mode
		Properties prop;
		if (app->mode == BENCHMARK_OCL_GPU) {
			prop.SetString("renderengine.type", "3");
			prop.SetString("opencl.nativethread.count", "0");
			prop.SetString("opencl.cpu.use", "0");
			prop.SetString("opencl.gpu.use", "1");
			prop.SetString("opencl.latency.mode", "1");
			prop.SetString("sampler.spp", "4");
		} else if (app->mode == BENCHMARK_OCL_CPUGPU) {
			prop.SetString("renderengine.type", "3");
			prop.SetString("opencl.nativethread.count", "0");
			prop.SetString("opencl.cpu.use", "1");
			prop.SetString("opencl.gpu.use", "1");
			prop.SetString("opencl.latency.mode", "1");
			prop.SetString("sampler.spp", "4");
		} else if (app->mode == BENCHMARK_OCL_CPU) {
			prop.SetString("renderengine.type", "3");
			prop.SetString("opencl.nativethread.count", "0");
			prop.SetString("opencl.cpu.use", "1");
			prop.SetString("opencl.gpu.use", "0");
			prop.SetString("opencl.latency.mode", "1");
			prop.SetString("sampler.spp", "4");
		} else if (app->mode == BENCHMARK_NATIVE) {
			prop.SetString("renderengine.type", "0");
			stringstream ss;
			ss << boost::thread::hardware_concurrency();
			prop.SetString("opencl.nativethread.count", ss.str().c_str());
			prop.SetString("opencl.cpu.use", "0");
			prop.SetString("opencl.gpu.use", "0");
			prop.SetString("opencl.latency.mode", "0");
			prop.SetString("sampler.spp", "4");
		} else if (app->mode == INTERACTIVE) {
			prop.SetString("renderengine.type", "3");
			prop.SetString("opencl.nativethread.count", "0");
			prop.SetString("opencl.cpu.use", "0");
			prop.SetString("opencl.gpu.use", "1");
			prop.SetString("opencl.latency.mode", "1");
			prop.SetString("sampler.spp", "1");
		} else
			assert (false);

		app->renderConfig->cfg.Load(prop);
		app->renderConfig->Init();

		// Initialize hardware information
		app->hardwareTreeModel = new HardwareTreeModel(app->renderConfig->GetAvailableDeviceDescriptions());

		// Done
		app->renderingStartTime = luxrays::WallClockTime();
		app->engineInitDone = true;
#if !defined(LUXRAYS_DISABLE_OPENCL)
	} catch (cl::Error err) {
		LM_ERROR("OpenCL ERROR: " << err.what() << "(" << err.err() << ")");
#endif
	} catch (runtime_error err) {
		LM_ERROR("RUNTIME ERROR: " << err.what());
	} catch (exception err) {
		LM_ERROR("ERROR: " << err.what());
	}
}

void LuxMarkApp::RenderRefreshTimeout() {
	if (!engineInitDone)
		return;

	mainWin->SetHadwareTreeModel(hardwareTreeModel);

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
	
	switch (renderConfig->GetRenderEngine()->GetEngineType()) {
		case DIRECTLIGHT:
		case PATH: {
			renderingTime = int(renderConfig->film->GetTotalTime());
			sampleSec = renderConfig->film->GetAvgSampleSec();
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

	// After 120secs of benchmark, show the result dialog
	bool benchmarkDone = (renderingTime > 120) && (mode != INTERACTIVE);

	char buf[512];
	stringstream ss("");

	char validBuf[128];
	if (mode == INTERACTIVE)
		strcpy(validBuf, "");
	else {
		if (benchmarkDone)
			strcpy(validBuf, " (OK)");
		else
			sprintf(validBuf, " (%dsecs remaining)", Max<int>(120 - renderingTime, 0));
	}	

	sprintf(buf, "[Mode: %s][Time: %dsecs%s][Samples/sec % 6dK][Rays/sec % 6dK on %.1fK tris]",
			(mode == BENCHMARK_OCL_GPU) ? "OpenCL GPUs" :
				((mode == BENCHMARK_OCL_CPUGPU) ? "OpenCL CPUs+GPUs" : 
					((mode == BENCHMARK_OCL_CPU) ? "OpenCL CPUs" :
						((mode == BENCHMARK_NATIVE) ? "Native CPUs" : "Interactive"))),
			renderingTime, validBuf, int(sampleSec / 1000.0),
			int(raysSec / 1000.0), renderConfig->scene->dataSet->GetTotalTriangleCount() / 1000.0);
	ss << buf;
#ifndef LUXRAYS_DISABLE_OPENCL
	if ((mode == BENCHMARK_OCL_GPU) || (mode == BENCHMARK_OCL_CPUGPU) || (mode == BENCHMARK_OCL_CPU)) {
		ss << "\n\nOpenCL rendering devices:";
		double minPerf = intersectionDevices[0]->GetPerformance();
		double totalPerf = intersectionDevices[0]->GetPerformance();
		for (size_t i = 1; i < intersectionDevices.size(); ++i) {
			if (intersectionDevices[i]->GetType() == DEVICE_TYPE_OPENCL) {
				minPerf = min(minPerf, intersectionDevices[i]->GetPerformance());
				totalPerf += intersectionDevices[i]->GetPerformance();
			}
		}

		for (size_t i = 0; i < intersectionDevices.size(); ++i) {
			if (intersectionDevices[i]->GetType() == DEVICE_TYPE_OPENCL) {
				const OpenCLDeviceDescription *desc = ((OpenCLIntersectionDevice *)intersectionDevices[i])->GetDeviceDesc();
				sprintf(buf, "\n    [%s][Rays/sec % 3dK][Prf Idx %.2f][Wrkld %.1f%%][Mem %dM/%dM]",
						desc->GetName().c_str(),
						int(intersectionDevices[i]->GetPerformance() / 1000.0),
						intersectionDevices[i]->GetPerformance() / minPerf,
						100.0 * intersectionDevices[i]->GetPerformance() / totalPerf,
						int(desc->GetUsedMemory() / (1024 * 1024)),
						int(desc->GetMaxMemory() / (1024 * 1024)));
				ss << buf;
			}
		}
	}
#endif
	mainWin->UpdateScreenLabel(ss.str().c_str(), benchmarkDone);

	if (benchmarkDone) {
		Stop();

		ResultDialog *dialog = new ResultDialog(mode, sceneName, sampleSec);
		dialog->exec();

		// Go in PAUSE mode
		InitRendering(PAUSE, sceneName);
	}
}

#define MOVE_STEP 0.5f
#define ROTATE_STEP 4.f
void LuxMarkApp::HandleMouseMoveEvent(QGraphicsSceneMouseEvent *event) {
	const double minInterval = 0.5;

	if (mode == INTERACTIVE) {
		if (mouseButton0) {
			// Check elapsed time since last update
			if (WallClockTime() - lastMouseUpdate > minInterval) {
				const qreal distX = event->lastPos().x() - mouseGrabLastX;
				const qreal distY = event->lastPos().y() - mouseGrabLastY;

				renderConfig->scene->camera->RotateDown(0.04f * distY * ROTATE_STEP);
				renderConfig->scene->camera->RotateRight(0.04f * distX * ROTATE_STEP);

				mouseGrabLastX = event->lastPos().x();
				mouseGrabLastY = event->lastPos().y();

				renderConfig->ReInit(false);
				lastMouseUpdate = WallClockTime();
			}
		} else if (mouseButton2) {
			// Check elapsed time since last update
			if (WallClockTime() - lastMouseUpdate > minInterval) {
				const qreal distX = event->lastPos().x() - mouseGrabLastX;
				const qreal distY = event->lastPos().y() - mouseGrabLastY;

				renderConfig->scene->camera->TranslateRight(0.04f * distX * MOVE_STEP);
				renderConfig->scene->camera->TranslateBackward(0.04f * distY * MOVE_STEP);

				mouseGrabLastX = event->lastPos().x();
				mouseGrabLastY = event->lastPos().y();

				renderConfig->ReInit(false);
				lastMouseUpdate = WallClockTime();
			}
		}
	}
}

void LuxMarkApp::HandleMousePressEvent(QGraphicsSceneMouseEvent *event) {
	if (mode == INTERACTIVE) {
		if ((event->button() == Qt::LeftButton) || (event->button() == Qt::RightButton)) {
			if (event->button() == Qt::LeftButton) {
				mouseButton0 = true;
				mouseButton2 = false;
			} else if (event->button() == Qt::RightButton) {
				mouseButton0 = false;
				mouseButton2 = true;
			} else {
				mouseButton0 = false;
				mouseButton2 = false;
			}

			// Record start position
			mouseGrabLastX = event->lastPos().x();
			mouseGrabLastY = event->lastPos().y();
			lastMouseUpdate = WallClockTime();
		}
	}
}
