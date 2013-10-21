/***************************************************************************
 * Copyright 1998-2013 by authors (see AUTHORS.txt)                        *
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(WIN32)
#define _USE_MATH_DEFINES
#endif
#include <math.h>

// Jens's patch for MacOS
#if defined(__APPLE__)
#include <GLUT/glut.h>
#else
#include <GL/glut.h>
#endif

#include "smallluxgpu.h"
#include "displayfunc.h"

#include "luxrays/core/intersectiondevice.h"
#include "luxrays/core/virtualdevice.h"

#include "slg/renderconfig.h"
#include "slg/rendersession.h"
#include "slg/film/film.h"
#include "slg/engines/rtpathocl/rtpathocl.h"
#include "slg/engines/rtbiaspathocl/rtbiaspathocl.h"
#include "slg/engines/biaspathcpu/biaspathcpu.h"
#include "slg/engines/biaspathocl/biaspathocl.h"

using namespace std;
using namespace luxrays;
using namespace slg;

static char captionBuffer[512];

static void PrintString(void *font, const char *string) {
	int len, i;

	len = (int)strlen(string);
	for (i = 0; i < len; i++)
		glutBitmapCharacter(font, string[i]);
}

static void PrintHelpString(const u_int x, const u_int y, const char *key, const char *msg) {
	glColor3f(0.9f, 0.9f, 0.5f);
	glRasterPos2i(x, y);
	PrintString(GLUT_BITMAP_8_BY_13, key);

	glColor3f(1.f, 1.f, 1.f);
	// To update raster color
	glRasterPos2i(x + glutBitmapLength(GLUT_BITMAP_8_BY_13, (unsigned char *)key), y);
	PrintString(GLUT_BITMAP_8_BY_13, ": ");
	PrintString(GLUT_BITMAP_8_BY_13, msg);
}

static void PrintHelpAndSettings() {
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glColor4f(0.f, 0.f, 0.f, 0.5f);
	glRecti(10, 20, session->film->GetWidth() - 10, session->film->GetHeight() - 20);
	glDisable(GL_BLEND);

	glColor3f(1.f, 1.f, 1.f);
	int fontOffset = session->film->GetHeight() - 20 - 20;
	glRasterPos2i((session->film->GetWidth() - glutBitmapLength(GLUT_BITMAP_9_BY_15, (unsigned char *)"Help & Settings & Devices")) / 2, fontOffset);
	PrintString(GLUT_BITMAP_9_BY_15, "Help & Settings & Devices");

	// Help
	fontOffset -= 25;
	PrintHelpString(15, fontOffset, "h", "toggle Help");
	fontOffset -= 15;
	PrintHelpString(15, fontOffset, "arrow Keys or mouse X/Y + mouse button 0", "rotate camera");
	fontOffset -= 15;
	PrintHelpString(15, fontOffset, "a, s, d, w or mouse X/Y + mouse button 1", "move camera");
	fontOffset -= 15;
	PrintHelpString(15, fontOffset, "p", "save image.png (or to image.filename property value)");
	fontOffset -= 15;
	PrintHelpString(15, fontOffset, "t", "toggle tonemapping");
	PrintHelpString(320, fontOffset, "n, m", "dec./inc. the screen refresh");
	fontOffset -= 15;
	PrintHelpString(15, fontOffset, "i", "switch sampler");
	fontOffset -= 15;
	PrintHelpString(15, fontOffset, "1", "OpenCL path tracing");
	PrintHelpString(320, fontOffset, "2", "CPU light tracing");
	fontOffset -= 15;
	PrintHelpString(15, fontOffset, "3", "CPU path tracing");
	PrintHelpString(320, fontOffset, "4", "CPU bidir. path tracing");
	fontOffset -= 15;
	PrintHelpString(15, fontOffset, "5", "Hybrid bidir. path tracing");
	PrintHelpString(320, fontOffset, "6", "Hybrid combinatorial bidir. path tracing");
	fontOffset -= 15;
	PrintHelpString(15, fontOffset, "7", "CPU bidir. VM path tracing");
	PrintHelpString(320, fontOffset, "8", "RT OpenCL path tracing");
	fontOffset -= 15;
	PrintHelpString(15, fontOffset, "9", "Hybrid path tracing");
	fontOffset -= 15;
#if defined(WIN32)
	PrintHelpString(15, fontOffset, "o", "windows always on top");
	fontOffset -= 15;
#endif

	// Settings
	char buf[512];
	glColor3f(0.5f, 1.0f, 0.f);
	fontOffset -= 15;
	glRasterPos2i(15, fontOffset);
	PrintString(GLUT_BITMAP_8_BY_13, "Settings:");
	fontOffset -= 15;
	glRasterPos2i(20, fontOffset);
#if !defined(LUXRAYS_DISABLE_OPENCL)
	if (session->renderEngine->GetEngineType() == RTPATHOCL) {
		static float fps = 0.f;
		// This is a simple trick to smooth the fps counter
		const double frameTime = ((RTPathOCLRenderEngine *)session->renderEngine)->GetFrameTime();
		const double adjustFactor = (frameTime > 0.1) ? 0.25 : .025;
		fps = Lerp<float>(adjustFactor, fps, (frameTime > 0.0) ? (1.0 / frameTime) : 0.0);

		sprintf(buf, "[Rendering time %dsecs][Screen refresh %d/%dms %.1ffps]",
				int(session->renderEngine->GetRenderingTime()),
				int((fps > 0.f) ? (1000.0 / fps) : 0.0),
				session->renderConfig->GetScreenRefreshInterval(),
				fps);
	} else if (session->renderEngine->GetEngineType() == RTBIASPATHOCL) {
		static float fps = 0.f;
		// This is a simple trick to smooth the fps counter
		const double frameTime = ((RTBiasPathOCLRenderEngine *)session->renderEngine)->GetFrameTime();
		const double adjustFactor = (frameTime > 0.1) ? 0.25 : .025;
		fps = Lerp<float>(adjustFactor, fps, (frameTime > 0.0) ? (1.0 / frameTime) : 0.0);

		sprintf(buf, "[Rendering time %dsecs][Screen refresh %dms %.1ffps]",
				int(session->renderEngine->GetRenderingTime()),
				int((fps > 0.f) ? (1000.0 / fps) : 0.0),
				fps);
	} else
#endif
		sprintf(buf, "[Rendering time %dsecs][Screen refresh %dms]",
				int(session->renderEngine->GetRenderingTime()),
				session->renderConfig->GetScreenRefreshInterval());
	PrintString(GLUT_BITMAP_8_BY_13, buf);
	fontOffset -= 15;
	glRasterPos2i(20, fontOffset);
	const string samplerName = ((session->renderEngine->GetEngineType() == BIASPATHCPU) ||
		(session->renderEngine->GetEngineType() == RTBIASPATHOCL)) ?
			"N/A" : session->renderConfig->cfg.GetString("sampler.type", "RANDOM");
	sprintf(buf, "[Render engine %s][Sampler %s][Tone mapping %s]",
			RenderEngine::RenderEngineType2String(session->renderEngine->GetEngineType()).c_str(),
			samplerName.c_str(),
			(session->film->GetToneMapParams()->GetType() == TONEMAP_LINEAR) ? "LINEAR" : "REINHARD02");
	PrintString(GLUT_BITMAP_8_BY_13, buf);
	fontOffset -= 15;
	glRasterPos2i(20, fontOffset);

	// Intersection devices
	const vector<IntersectionDevice *> &idevices = session->renderEngine->GetIntersectionDevices();

	// Replace all virtual devices with real
	vector<IntersectionDevice *> realDevices;
	for (size_t i = 0; i < idevices.size(); ++i) {
		VirtualIntersectionDevice *vdev = dynamic_cast<VirtualIntersectionDevice *>(idevices[i]);
		if (vdev) {
			const vector<IntersectionDevice *> &realDevs = vdev->GetRealDevices();
			realDevices.insert(realDevices.end(), realDevs.begin(), realDevs.end());
		} else
			realDevices.push_back(idevices[i]);
	}

	double minPerf = realDevices[0]->GetTotalPerformance();
	double totalPerf = realDevices[0]->GetTotalPerformance();
	for (size_t i = 1; i < realDevices.size(); ++i) {
		minPerf = min(minPerf, realDevices[i]->GetTotalPerformance());
		totalPerf += realDevices[i]->GetTotalPerformance();
	}

	glColor3f(1.0f, 0.5f, 0.f);
	int offset = 45;
	size_t deviceCount = realDevices.size();

	char buff[512];
	for (size_t i = 0; i < deviceCount; ++i) {
		sprintf(buff, "[%s][Rays/sec %dK (%dK + %dK)][Prf Idx %.2f][Wrkld %.1f%%][Mem %dM/%dM]",
			realDevices[i]->GetName().c_str(),
			int(realDevices[i]->GetTotalPerformance() / 1000.0),
				int(realDevices[i]->GetSerialPerformance() / 1000.0),
				int(realDevices[i]->GetDataParallelPerformance() / 1000.0),
			realDevices[i]->GetTotalPerformance() / minPerf,
			100.0 * realDevices[i]->GetTotalPerformance() / totalPerf,
			int(realDevices[i]->GetUsedMemory() / (1024 * 1024)),
			int(realDevices[i]->GetMaxMemory() / (1024 * 1024)));
		glRasterPos2i(20, offset);
		PrintString(GLUT_BITMAP_8_BY_13, buff);
		offset += 15;
	}

	glRasterPos2i(15, offset);
	PrintString(GLUT_BITMAP_9_BY_15, "Rendering devices:");
}

static void PrintCaptions() {
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glColor4f(0.f, 0.f, 0.f, 0.8f);
	glRecti(0, session->film->GetHeight() - 15,
			session->film->GetWidth() - 1, session->film->GetHeight() - 1);
	glRecti(0, 0, session->film->GetWidth() - 1, 18);
	glDisable(GL_BLEND);

	// Draw the pending tiles for BIASPATHCPU or BIASPATHOCL
	vector<TileRepository::Tile> tiles;
	u_int tileSize = 0;
	switch (session->renderEngine->GetEngineType()) {
		case BIASPATHCPU: {
			CPUTileRenderEngine *engine = (CPUTileRenderEngine *)session->renderEngine;
			engine->GetPendingTiles(tiles);
			tileSize = engine->GetTileSize();
			break;
		}
#if !defined(LUXRAYS_DISABLE_OPENCL)
		case BIASPATHOCL: {
			BiasPathOCLRenderEngine *engine = (BiasPathOCLRenderEngine *)session->renderEngine;
			engine->GetPendingTiles(tiles);
			tileSize = engine->GetTileSize();
			break;
        }
#endif
		default:
			break;
	}

	if (tiles.size() > 0) {
		// Draw tiles borders
		glColor3f(1.f, 1.f, 0.f);
		BOOST_FOREACH(TileRepository::Tile &tile, tiles) {
			glBegin(GL_LINE_LOOP);
			glVertex2i(tile.xStart, tile.yStart);
			glVertex2i(tile.xStart + tileSize, tile.yStart);
			glVertex2i(tile.xStart + tileSize, tile.yStart + tileSize);
			glVertex2i(tile.xStart, tile.yStart + tileSize);
			glEnd();
		}
	}
	
	// Caption line 0
	glColor3f(1.f, 1.f, 1.f);
	glRasterPos2i(4, 5);
	PrintString(GLUT_BITMAP_8_BY_13, captionBuffer);

	// Title
	glRasterPos2i(4, session->film->GetHeight() - 10);
	if (optUseLuxVRName)
		PrintString(GLUT_BITMAP_8_BY_13, LUXVR_LABEL.c_str());
	else
		PrintString(GLUT_BITMAP_8_BY_13, SLG_LABEL.c_str());
}

void displayFunc(void) {
	sprintf(captionBuffer, "[Pass %3d][Avg. samples/sec % 3.2fM][Avg. rays/sec % 4dK on %.1fK tris]",
		session->renderEngine->GetPass(),
		session->renderEngine->GetTotalSamplesSec() / 1000000.0,
		int(session->renderEngine->GetTotalRaysSec() / 1000.0),
		session->renderConfig->scene->dataSet->GetTotalTriangleCount() / 1000.0);

	if (!optRealTimeMode)
		session->film->UpdateScreenBuffer();
	const float *pixels = session->film->GetScreenBuffer();

	glRasterPos2i(0, 0);
	glDrawPixels(session->film->GetWidth(), session->film->GetHeight(), GL_RGB, GL_FLOAT, pixels);

	PrintCaptions();

	if (optOSDPrintHelp) {
		glPushMatrix();
		glLoadIdentity();
		glOrtho(-0.5f, session->film->GetWidth() - 0.5f,
				-0.5f, session->film->GetHeight() - 0.5f, -1.f, 1.f);

		PrintHelpAndSettings();

		glPopMatrix();
	}

	glutSwapBuffers();
}

void idleFunc(void) {
	session->renderEngine->WaitNewFrame();
	displayFunc();
}

void reshapeFunc(int newWidth, int newHeight) {
	// Check if width or height have really changed
	if ((newWidth != (int)session->film->GetWidth()) ||
			(newHeight != (int)session->film->GetHeight())) {
		glViewport(0, 0, newWidth, newHeight);
		glLoadIdentity();
		glOrtho(0.f, newWidth - 1.f, 0.f, newHeight - 1.f, -1.f, 1.f);

		// Stop the session
		session->Stop();

		// Delete the session
		delete session;
		session = NULL;

		// Change the film size
		config->cfg.Set(Property("film.width")(newWidth));
		config->cfg.Set(Property("film.height")(newHeight));
		session = new RenderSession(config);

		// Re-start the rendering
		session->Start();

		glutPostRedisplay();
	}
}

void timerFunc(int value) {
	// Need to update the Film
	session->renderEngine->UpdateFilm();

	// Check if periodic save is enabled
	if (session->NeedPeriodicFilmSave()) {
		// Time to save the image and film
		session->SaveFilm();
	}

	glutPostRedisplay();
	if (!optRealTimeMode)
		glutTimerFunc(session->renderConfig->GetScreenRefreshInterval(), timerFunc, 0);
}

static void SetRenderingEngineType(const RenderEngineType engineType) {
	if (engineType != session->renderEngine->GetEngineType()) {
		// Stop the session
		session->Stop();

		// Delete the session
		delete session;
		session = NULL;

		// Change the render engine
		config->cfg.Set(Property("renderengine.type")(RenderEngine::RenderEngineType2String(engineType)));
		session = new RenderSession(config);

		// Re-start the rendering
		session->Start();
	}
}

void keyFunc(unsigned char key, int x, int y) {
	switch (key) {
		case 'p': {
			session->SaveFilm();
			break;
		}
		case 27: { // Escape key
			delete session;

			SLG_LOG("Done.");
			exit(EXIT_SUCCESS);
			break;
		}
		case ' ': // Restart rendering
			session->Stop();
			session->Start();
			break;
		case 'a': {
			session->BeginSceneEdit();
			session->renderConfig->scene->camera->TranslateLeft(optMoveStep);
			session->renderConfig->scene->camera->Update(
				session->film->GetWidth(), session->film->GetHeight());
			session->editActions.AddAction(CAMERA_EDIT);
			session->EndSceneEdit();
			break;
		}
		case 'd': {
			session->BeginSceneEdit();
			session->renderConfig->scene->camera->TranslateRight(optMoveStep);
			session->renderConfig->scene->camera->Update(
				session->film->GetWidth(), session->film->GetHeight());
			session->editActions.AddAction(CAMERA_EDIT);
			session->EndSceneEdit();
			break;
		}
		case 'w': {
			session->BeginSceneEdit();
			session->renderConfig->scene->camera->TranslateForward(optMoveStep);
			session->renderConfig->scene->camera->Update(
				session->film->GetWidth(), session->film->GetHeight());
			session->editActions.AddAction(CAMERA_EDIT);
			session->EndSceneEdit();
			break;
		}
		case 's': {
			session->BeginSceneEdit();
			session->renderConfig->scene->camera->TranslateBackward(optMoveStep);
			session->renderConfig->scene->camera->Update(
				session->film->GetWidth(), session->film->GetHeight());
			session->editActions.AddAction(CAMERA_EDIT);
			session->EndSceneEdit();
			break;
		}
		case 'r':
			session->BeginSceneEdit();
			session->renderConfig->scene->camera->Translate(Vector(0.f, 0.f, optMoveStep));
			session->renderConfig->scene->camera->Update(
				session->film->GetWidth(), session->film->GetHeight());
			session->editActions.AddAction(CAMERA_EDIT);
			session->EndSceneEdit();
			break;
		case 'f':
			session->BeginSceneEdit();
			session->renderConfig->scene->camera->Translate(Vector(0.f, 0.f, -optMoveStep));
			session->renderConfig->scene->camera->Update(
				session->film->GetWidth(), session->film->GetHeight());
			session->editActions.AddAction(CAMERA_EDIT);
			session->EndSceneEdit();
			break;
		case 'h':
			optOSDPrintHelp = (!optOSDPrintHelp);
			break;
		case 'i':
			// Stop the session
			session->Stop();

			// Delete the session
			delete session;
			session = NULL;

			// Change the Sampler
			if (config->cfg.GetString("sampler.type", "RANDOM") == "RANDOM") {
				config->cfg.SetString("sampler.type", "SOBOL");
				config->cfg.SetString("path.sampler.type", "SOBOL");
			} else if (config->cfg.GetString("sampler.type", "SOBOL") == "SOBOL") {
				config->cfg.SetString("sampler.type", "METROPOLIS");
				config->cfg.SetString("path.sampler.type", "METROPOLIS");
			} else {
				config->cfg.SetString("sampler.type", "RANDOM");
				config->cfg.SetString("path.sampler.type", "RANDOM");				
			}
			session = new RenderSession(config);

			// Re-start the rendering
			session->Start();
			break;
		case 'n': {
			const u_int screenRefreshInterval = session->renderConfig->GetScreenRefreshInterval();
			if (screenRefreshInterval > 1000)
				session->renderConfig->SetScreenRefreshInterval(max(1000u, screenRefreshInterval - 1000));
			else if (screenRefreshInterval > 100)
				session->renderConfig->SetScreenRefreshInterval(max(50u, screenRefreshInterval - 50));
			else
				session->renderConfig->SetScreenRefreshInterval(max(10u, screenRefreshInterval - 5));
			break;
		}
		case 'm': {
			const u_int screenRefreshInterval = session->renderConfig->GetScreenRefreshInterval();
			if (screenRefreshInterval >= 1000)
				session->renderConfig->SetScreenRefreshInterval(screenRefreshInterval + 1000);
			else if (screenRefreshInterval >= 100)
				session->renderConfig->SetScreenRefreshInterval(screenRefreshInterval + 50);
			else
				session->renderConfig->SetScreenRefreshInterval(screenRefreshInterval + 5);
			break;
		}
		case 't':
			// Toggle tonemap type
			if (session->film->GetToneMapParams()->GetType() == TONEMAP_LINEAR) {
				Reinhard02ToneMapParams params;
				session->film->SetToneMapParams(params);
			} else {
				LinearToneMapParams params;
				session->film->SetToneMapParams(params);
			}
			break;
		case 'v':
			optMoveScale = Max(.0125f, optMoveScale - ((optMoveScale>= 1.f) ? .25f : 0.0125f));
			UpdateMoveStep();
			SLG_LOG("Camera move scale: " << optMoveScale);
			break;
		case 'b':
			optMoveScale = Min(100.f, optMoveScale +  ((optMoveScale>= 1.f) ? .25f : 0.0125f));
			UpdateMoveStep();
			SLG_LOG("Camera move scale: " << optMoveScale);
			break;
		case 'y': {
			if (session->renderConfig->scene->camera->IsHorizontalStereoEnabled()) {
				session->Stop();
				session->renderConfig->scene->camera->SetOculusRiftBarrel(!session->renderConfig->scene->camera->IsOculusRiftBarrelEnabled());
				session->renderConfig->scene->camera->Update(
					session->film->GetWidth(), session->film->GetHeight());
				session->Start();
			}
			break;
		}
		case 'u': {
			session->Stop();
			session->renderConfig->scene->camera->SetHorizontalStereo(!session->renderConfig->scene->camera->IsHorizontalStereoEnabled());
			session->renderConfig->scene->camera->Update(
				session->film->GetWidth(), session->film->GetHeight());
			session->editActions.AddAction(CAMERA_EDIT);
			session->Start();
			break;
		}
		case 'k': {
			session->BeginSceneEdit();
			const float currentEyeDistance = session->renderConfig->scene->camera->GetHorizontalStereoEyesDistance();
			const float newEyeDistance = currentEyeDistance + ((currentEyeDistance == 0.f) ? .0626f : (currentEyeDistance * 0.05f));
			SLG_LOG("Camera horizontal stereo eyes distance: " << newEyeDistance);
			session->renderConfig->scene->camera->SetHorizontalStereoEyesDistance(newEyeDistance);
			session->renderConfig->scene->camera->Update(
				session->film->GetWidth(), session->film->GetHeight());
			session->editActions.AddAction(CAMERA_EDIT);
			session->EndSceneEdit();
			break;
		}
		case 'l': {
			session->BeginSceneEdit();
			const float currentEyeDistance = session->renderConfig->scene->camera->GetHorizontalStereoEyesDistance();
			const float newEyeDistance = Max(0.f, currentEyeDistance - currentEyeDistance * 0.05f);
			SLG_LOG("Camera horizontal stereo eyes distance: " << newEyeDistance);
			session->renderConfig->scene->camera->SetHorizontalStereoEyesDistance(newEyeDistance);
			session->renderConfig->scene->camera->Update(
				session->film->GetWidth(), session->film->GetHeight());
			session->editActions.AddAction(CAMERA_EDIT);
			session->EndSceneEdit();
			break;
		}
		case ',': {
			session->BeginSceneEdit();
			const float currentLenDistance = session->renderConfig->scene->camera->GetHorizontalStereoLensDistance();
			const float newLensDistance = currentLenDistance + ((currentLenDistance == 0.f) ? .1f : (currentLenDistance * 0.05f));
			SLG_LOG("Camera horizontal stereo lens distance: " << newLensDistance);
			session->renderConfig->scene->camera->SetHorizontalStereoLensDistance(newLensDistance);
			session->renderConfig->scene->camera->Update(
				session->film->GetWidth(), session->film->GetHeight());
			session->editActions.AddAction(CAMERA_EDIT);
			session->EndSceneEdit();
			break;
		}
		case '.': {
			session->BeginSceneEdit();
			const float currentLensDistance = session->renderConfig->scene->camera->GetHorizontalStereoLensDistance();
			const float newLensDistance = Max(0.f, currentLensDistance - currentLensDistance * 0.05f);
			SLG_LOG("Camera horizontal stereo lens distance: " << newLensDistance);
			session->renderConfig->scene->camera->SetHorizontalStereoLensDistance(newLensDistance);
			session->renderConfig->scene->camera->Update(
				session->film->GetWidth(), session->film->GetHeight());
			session->editActions.AddAction(CAMERA_EDIT);
			session->EndSceneEdit();
			break;
		}
		case '1':
			SetRenderingEngineType(PATHOCL);
			glutIdleFunc(NULL);
			glutTimerFunc(session->renderConfig->GetScreenRefreshInterval(), timerFunc, 0);
			optRealTimeMode = false;
			break;
		case '2':
			SetRenderingEngineType(LIGHTCPU);
			glutIdleFunc(NULL);
			glutTimerFunc(session->renderConfig->GetScreenRefreshInterval(), timerFunc, 0);
			optRealTimeMode = false;
			break;
		case '3':
			SetRenderingEngineType(PATHCPU);
			glutIdleFunc(NULL);
			glutTimerFunc(session->renderConfig->GetScreenRefreshInterval(), timerFunc, 0);
			optRealTimeMode = false;
			break;
		case '4':
			SetRenderingEngineType(BIDIRCPU);
			glutIdleFunc(NULL);
			glutTimerFunc(session->renderConfig->GetScreenRefreshInterval(), timerFunc, 0);
			optRealTimeMode = false;
			break;
		case '5':
			SetRenderingEngineType(BIDIRHYBRID);
			glutIdleFunc(NULL);
			glutTimerFunc(session->renderConfig->GetScreenRefreshInterval(), timerFunc, 0);
			optRealTimeMode = false;
			break;
		case '6':
			SetRenderingEngineType(CBIDIRHYBRID);
			glutIdleFunc(NULL);
			glutTimerFunc(session->renderConfig->GetScreenRefreshInterval(), timerFunc, 0);
			optRealTimeMode = false;
			break;
		case '7':
			SetRenderingEngineType(BIDIRVMCPU);
			glutIdleFunc(NULL);
			glutTimerFunc(session->renderConfig->GetScreenRefreshInterval(), timerFunc, 0);
			optRealTimeMode = false;
			break;
#if !defined(LUXRAYS_DISABLE_OPENCL)
		case '8':
			SetRenderingEngineType(RTPATHOCL);
			glutIdleFunc(idleFunc);
			optRealTimeMode = true;
			if (session->renderConfig->GetScreenRefreshInterval() > 33)
				session->renderConfig->SetScreenRefreshInterval(33);
			break;
#endif
		case '9':
			SetRenderingEngineType(PATHHYBRID);
			glutIdleFunc(NULL);
			glutTimerFunc(session->renderConfig->GetScreenRefreshInterval(), timerFunc, 0);
			optRealTimeMode = false;
			break;
		case '0':
			SetRenderingEngineType(BIASPATHCPU);
			glutIdleFunc(NULL);
			glutTimerFunc(session->renderConfig->GetScreenRefreshInterval(), timerFunc, 0);
			optRealTimeMode = false;
			break;
		case '-':
			SetRenderingEngineType(BIASPATHOCL);
			glutIdleFunc(NULL);
			glutTimerFunc(session->renderConfig->GetScreenRefreshInterval(), timerFunc, 0);
			optRealTimeMode = false;
			break;
#if !defined(LUXRAYS_DISABLE_OPENCL)
		case '=':
			SetRenderingEngineType(RTBIASPATHOCL);
			glutIdleFunc(idleFunc);
			optRealTimeMode = true;
			break;
#endif
		case 'o': {
#if defined(WIN32)
			std::wstring ws;
			if (optUseLuxVRName)
				ws.assign(LUXVR_LABEL.begin (), LUXVR_LABEL.end());
			else
				ws.assign(SLG_LABEL.begin (), SLG_LABEL.end());
			HWND hWnd = FindWindowW(NULL, ws.c_str());
			if (GetWindowLongPtr(hWnd, GWL_EXSTYLE) & WS_EX_TOPMOST)
				SetWindowPos(hWnd, HWND_NOTOPMOST, NULL, NULL, NULL, NULL, SWP_NOMOVE | SWP_NOSIZE);
			else
				SetWindowPos(hWnd, HWND_TOPMOST, NULL, NULL, NULL, NULL, SWP_NOMOVE | SWP_NOSIZE);
#endif
			break;
		}
		default:
			break;
	}

	if (!optRealTimeMode)
		displayFunc();
}

void specialFunc(int key, int x, int y) {
	switch (key) {
		case GLUT_KEY_UP:
			session->BeginSceneEdit();
			session->renderConfig->scene->camera->RotateUp(optRotateStep);
			session->renderConfig->scene->camera->Update(
				session->film->GetWidth(), session->film->GetHeight());
			session->editActions.AddAction(CAMERA_EDIT);
			session->EndSceneEdit();
			break;
		case GLUT_KEY_DOWN:
			session->BeginSceneEdit();
			session->renderConfig->scene->camera->RotateDown(optRotateStep);
			session->renderConfig->scene->camera->Update(
				session->film->GetWidth(), session->film->GetHeight());
			session->editActions.AddAction(CAMERA_EDIT);
			session->EndSceneEdit();
			break;
		case GLUT_KEY_LEFT:
			session->BeginSceneEdit();
			session->renderConfig->scene->camera->RotateLeft(optRotateStep);
			session->renderConfig->scene->camera->Update(
				session->film->GetWidth(), session->film->GetHeight());
			session->editActions.AddAction(CAMERA_EDIT);
			session->EndSceneEdit();
			break;
		case GLUT_KEY_RIGHT:
			session->BeginSceneEdit();
			session->renderConfig->scene->camera->RotateRight(optRotateStep);
			session->renderConfig->scene->camera->Update(
				session->film->GetWidth(), session->film->GetHeight());
			session->editActions.AddAction(CAMERA_EDIT);
			session->EndSceneEdit();
			break;
		default:
			break;
	}

	if (!optRealTimeMode)
		displayFunc();
}

static int mouseButton0 = 0;
static int mouseButton2 = 0;
static int mouseGrabLastX = 0;
static int mouseGrabLastY = 0;
static double lastMouseUpdate = 0.0;

static void mouseFunc(int button, int state, int x, int y) {
	if (button == 0) {
		if (state == GLUT_DOWN) {
			// Record start position
			mouseGrabLastX = x;
			mouseGrabLastY = y;
			mouseButton0 = 1;
		} else if (state == GLUT_UP) {
			mouseButton0 = 0;
		}
	} else if (button == 2) {
		if (state == GLUT_DOWN) {
			// Record start position
			mouseGrabLastX = x;
			mouseGrabLastY = y;
			mouseButton2 = 1;
		} else if (state == GLUT_UP) {
			mouseButton2 = 0;
		}
	}
}

static void motionFunc(int x, int y) {
	const double minInterval = 0.2;

	if (mouseButton0) {
		// Check elapsed time since last update
		if (optRealTimeMode || (WallClockTime() - lastMouseUpdate > minInterval)) {
			const int distX = x - mouseGrabLastX;
			const int distY = y - mouseGrabLastY;

			session->BeginSceneEdit();

			if (optMouseGrabMode) {
				session->renderConfig->scene->camera->RotateUp(0.04f * distY * optRotateStep);
				session->renderConfig->scene->camera->RotateLeft(0.04f * distX * optRotateStep);
			}
			else {
				session->renderConfig->scene->camera->RotateDown(0.04f * distY * optRotateStep);
				session->renderConfig->scene->camera->RotateRight(0.04f * distX * optRotateStep);
			};

			session->renderConfig->scene->camera->Update(
				session->film->GetWidth(), session->film->GetHeight());
			session->editActions.AddAction(CAMERA_EDIT);
			session->EndSceneEdit();

			mouseGrabLastX = x;
			mouseGrabLastY = y;

			if (!optRealTimeMode)
				displayFunc();
			lastMouseUpdate = WallClockTime();
		}
	} else if (mouseButton2) {
		// Check elapsed time since last update
		if (optRealTimeMode || (WallClockTime() - lastMouseUpdate > minInterval)) {
			const int distX = x - mouseGrabLastX;
			const int distY = y - mouseGrabLastY;

			session->BeginSceneEdit();

			if (optMouseGrabMode) {
				session->renderConfig->scene->camera->TranslateLeft(0.04f * distX * optMoveStep);
				session->renderConfig->scene->camera->TranslateForward(0.04f * distY * optMoveStep);
			}
			else {
				session->renderConfig->scene->camera->TranslateRight(0.04f * distX * optMoveStep);
				session->renderConfig->scene->camera->TranslateBackward(0.04f * distY * optMoveStep);				
			}

			session->renderConfig->scene->camera->Update(
				session->film->GetWidth(), session->film->GetHeight());
			session->editActions.AddAction(CAMERA_EDIT);
			session->EndSceneEdit();

			mouseGrabLastX = x;
			mouseGrabLastY = y;

			if (!optRealTimeMode)
				displayFunc();
			lastMouseUpdate = WallClockTime();
		}
	}
}

void InitGlut(int argc, char *argv[], const u_int width, const u_int height) {
	glutInit(&argc, argv);

	if (optUseGameMode) {
		const string screenDef = ToString(width) + "x" + ToString(height);
		glutGameModeString(screenDef.c_str());
		glutEnterGameMode();	
	} else {
		glutInitWindowSize(width, height);
		// Center the window
		const u_int scrWidth = glutGet(GLUT_SCREEN_WIDTH);
		const u_int scrHeight = glutGet(GLUT_SCREEN_HEIGHT);
		if ((scrWidth + 50 < width) || (scrHeight + 50 < height))
			glutInitWindowPosition(0, 0);
		else
			glutInitWindowPosition((scrWidth - width) / 2, (scrHeight - height) / 2);

		glutInitDisplayMode(GLUT_RGB | GLUT_DOUBLE);
		if (optUseLuxVRName)
			glutCreateWindow(LUXVR_LABEL.c_str());
		else
			glutCreateWindow(SLG_LABEL.c_str());
	}
}

void RunGlut() {
	glutReshapeFunc(reshapeFunc);
	glutKeyboardFunc(keyFunc);
	glutSpecialFunc(specialFunc);
	glutDisplayFunc(displayFunc);
	glutMouseFunc(mouseFunc);
	glutMotionFunc(motionFunc);
#if !defined(LUXRAYS_DISABLE_OPENCL)
	if ((session->renderEngine->GetEngineType() == RTPATHOCL) ||
		(session->renderEngine->GetEngineType() == RTBIASPATHOCL)) {
		glutIdleFunc(idleFunc);
		optRealTimeMode = true;
	} else
#endif
	{
		glutTimerFunc(session->renderConfig->GetScreenRefreshInterval(), timerFunc, 0);
		optRealTimeMode = false;
	}

	glMatrixMode(GL_PROJECTION);
	glViewport(0, 0, session->film->GetWidth(), session->film->GetHeight());
	glLoadIdentity();
	glOrtho(0.f, session->film->GetWidth() - 1.f,
			0.f, session->film->GetHeight() - 1.f, -1.f, 1.f);

	glutMainLoop();
}
