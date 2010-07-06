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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(WIN32)
#define _USE_MATH_DEFINES
#endif
#include <math.h>

#include "displayfunc.h"
#include "renderconfig.h"

#include "luxrays/utils/film/film.h"
#include "path.h"
#include "sppm.h"

RenderingConfig *config;

static int printHelp = 1;

void DebugHandler(const char *msg) {
	cerr << "[LuxRays] " << msg << endl;
}

static void PrintString(void *font, const char *string) {
	int len, i;

	len = (int)strlen(string);
	for (i = 0; i < len; i++)
		glutBitmapCharacter(font, string[i]);
}

static void PrintHelpString(const unsigned int x, const unsigned int y, const char *key, const char *msg) {
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
	glRecti(10, 40, config->film->GetWidth() - 10, config->film->GetHeight() - 40);
	glDisable(GL_BLEND);

	glColor3f(1.f, 1.f, 1.f);
	int fontOffset = config->film->GetHeight() - 40 - 20;
	glRasterPos2i((config->film->GetWidth() - glutBitmapLength(GLUT_BITMAP_9_BY_15, (unsigned char *)"Help & Settings & Devices")) / 2, fontOffset);
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
	PrintHelpString(15, fontOffset, "y", "toggle camera motion blur");
	PrintHelpString(320, fontOffset, "t", "toggle tonemapping");
	fontOffset -= 15;
	PrintHelpString(15, fontOffset, "n, m", "dec./inc. the screen refresh");
	PrintHelpString(320, fontOffset, "0", "direct lighting rendering");
	fontOffset -= 15;
	PrintHelpString(15, fontOffset, "1", "path tracing rendering");
	PrintHelpString(320, fontOffset, "2", "SPPM rendering");
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
	int renderingTime = 0;
	if (config->GetRenderEngine()->GetEngineType() == SPPM) {
		SPPMRenderEngine *sre = (SPPMRenderEngine *)config->GetRenderEngine();

		renderingTime = int(sre->GetRenderingTime());
	} else
		renderingTime = int(config->film->GetTotalTime());
	sprintf(buf, "[Rendering time %dsecs][FOV %.1f][Screen refresh %dms][Render threads %d]",
			renderingTime,
			config->scene->camera->fieldOfView,
			config->GetScreenRefreshInterval(), int(config->GetRenderEngine()->GetThreadCount()));
	PrintString(GLUT_BITMAP_8_BY_13, buf);
	fontOffset -= 15;
	glRasterPos2i(20, fontOffset);
	sprintf(buf, "[Camera motion blur %s][Tonemapping %s]",
			config->scene->camera->motionBlur ? "YES" : "NO",
			(config->film->GetToneMapParams()->GetType() == TONEMAP_LINEAR) ? "LINEAR" : "REINHARD02");
	PrintString(GLUT_BITMAP_8_BY_13, buf);
	fontOffset -= 15;
	glRasterPos2i(20, fontOffset);

	// Renering engine settings
	switch (config->GetRenderEngine()->GetEngineType()) {
		case DIRECTLIGHT: {
			PathRenderEngine *pre = (PathRenderEngine *)config->GetRenderEngine();

			sprintf(buf, "[DirectLight RE][Shadow rays %d][Max path depth %d][RR Depth %d]",
					(pre->lightStrategy == ONE_UNIFORM) ? pre->shadowRayCount :
						(pre->shadowRayCount * (int)config->scene->lights.size()),
					pre->maxPathDepth,
					pre->rrDepth);
			PrintString(GLUT_BITMAP_8_BY_13, buf);
			fontOffset -= 15;
			glRasterPos2i(20, fontOffset);
			break;
		}
		case PATH: {
			PathRenderEngine *pre = (PathRenderEngine *)config->GetRenderEngine();

			sprintf(buf, "[Path RE][Shadow rays %d][Max path depth %d][RR Depth %d]",
					(pre->lightStrategy == ONE_UNIFORM) ? pre->shadowRayCount :
						(pre->shadowRayCount * (int)config->scene->lights.size()),
					pre->maxPathDepth,
					pre->rrDepth);
			PrintString(GLUT_BITMAP_8_BY_13, buf);
			fontOffset -= 15;
			glRasterPos2i(20, fontOffset);
			break;
		}
		case SPPM: {
			SPPMRenderEngine *sre = (SPPMRenderEngine *)config->GetRenderEngine();

			sprintf(buf, "[SPPM RE][Stochastic count %.1fM][Eye depth %d][Photon depth %d]",
					sre->GetStocasticInterval() / 1000000.f, sre->GetMaxEyePathDepth(),
					sre->GetMaxPhotonPathDepth());
			PrintString(GLUT_BITMAP_8_BY_13, buf);
			fontOffset -= 15;
			glRasterPos2i(20, fontOffset);
			break;
		}
		default:
			assert (false);
	}

	// Pixel Device
	char buff[512];
	glColor3f(1.0f, 0.25f, 0.f);
	const vector<IntersectionDevice *> idevices = config->GetIntersectionDevices();
	fontOffset -= 15;
	glRasterPos2i(15, fontOffset);
	PrintString(GLUT_BITMAP_8_BY_13, "Pixel device: ");
	const vector<PixelDevice *> pdevices = config->GetPixelDevices();
	assert (pdevices.size() > 0);
	sprintf(buff, "[%s][Samples/sec % 3.2fM]", pdevices[0]->GetName().c_str(),
			pdevices[0]->GetPerformance() / 1000000.0);
	PrintString(GLUT_BITMAP_8_BY_13, buff);

#if !defined(LUXRAYS_DISABLE_OPENCL)
	if (pdevices[0]->GetType() == DEVICE_TYPE_OPENCL) {
		OpenCLPixelDevice *dev = (OpenCLPixelDevice *)pdevices[0];
		const OpenCLDeviceDescription *desc = dev->GetDeviceDesc();
		sprintf(buff, "[Mem %dM/%dM][Free buffers % 2d/%d]",
				int(desc->GetUsedMemory() / (1024 * 1024)),
				int(desc->GetMaxMemory() / (1024 * 1024)),
				int(dev->GetFreeDevBufferCount()), int(dev->GetTotalDevBufferCount()));
		PrintString(GLUT_BITMAP_8_BY_13, buff);
	} else
#endif
	if (pdevices[0]->GetType() == DEVICE_TYPE_NATIVE_THREAD) {
		NativePixelDevice *dev = (NativePixelDevice *)pdevices[0];
		sprintf(buff, "[Free buffers % 2d/%d]",
				int(dev->GetFreeDevBufferCount()), int(dev->GetTotalDevBufferCount()));
		PrintString(GLUT_BITMAP_8_BY_13, buff);
	}

	// Intersection devices
	double minPerf = idevices[0]->GetPerformance();
	double totalPerf = idevices[0]->GetPerformance();
	for (size_t i = 1; i < idevices.size(); ++i) {
		minPerf = min(minPerf, idevices[i]->GetPerformance());
		totalPerf += idevices[i]->GetPerformance();
	}

	glColor3f(1.0f, 0.5f, 0.f);
	int offset = 45;
	for (size_t i = 0; i < idevices.size(); ++i) {
		sprintf(buff, "[%s][Rays/sec % 3dK][Load %.1f%%][Prf Idx %.2f][Wrkld %.1f%%]",
				idevices[i]->GetName().c_str(),
				int(idevices[i]->GetPerformance() / 1000.0),
				100.0 * idevices[i]->GetLoad(),
				idevices[i]->GetPerformance() / minPerf,
				100.0 * idevices[i]->GetPerformance() / totalPerf);
		glRasterPos2i(20, offset);
		PrintString(GLUT_BITMAP_8_BY_13, buff);

#if !defined(LUXRAYS_DISABLE_OPENCL)
		// Check if it is an OpenCL device
		if (idevices[i]->GetType() == DEVICE_TYPE_OPENCL) {
			const OpenCLDeviceDescription *desc = ((OpenCLIntersectionDevice *)idevices[i])->GetDeviceDesc();
			sprintf(buff, "[Mem %dM/%dM]", int(desc->GetUsedMemory() / (1024 * 1024)),
					int(desc->GetMaxMemory() / (1024 * 1024)));
			PrintString(GLUT_BITMAP_8_BY_13, buff);
		}
#endif

		offset += 15;
	}

	glRasterPos2i(15, offset);
	PrintString(GLUT_BITMAP_9_BY_15, "Rendering devices:");
}

static void PrintCaptions() {
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glColor4f(0.f, 0.f, 0.f, 0.8f);
	glRecti(0, config->film->GetHeight() - 15,
			config->film->GetWidth() - 1, config->film->GetHeight() - 1);
	glRecti(0, 0, config->film->GetWidth() - 1, 18);
	glDisable(GL_BLEND);

	// Caption line 0
	glColor3f(1.f, 1.f, 1.f);
	glRasterPos2i(4, 5);
	PrintString(GLUT_BITMAP_8_BY_13, config->captionBuffer);

	// Title
	glRasterPos2i(4, config->film->GetHeight() - 10);
	PrintString(GLUT_BITMAP_8_BY_13, SLG_LABEL.c_str());
}

void displayFunc(void) {
	config->film->UpdateScreenBuffer();
	const float *pixels = config->film->GetScreenBuffer();

	glRasterPos2i(0, 0);
	glDrawPixels(config->film->GetWidth(), config->film->GetHeight(), GL_RGB, GL_FLOAT, pixels);

	PrintCaptions();

	if (printHelp) {
		glPushMatrix();
		glLoadIdentity();
		glOrtho(-0.5f, config->film->GetWidth() - 0.5f,
				-0.5f, config->film->GetHeight() - 0.5f, -1.f, 1.f);

		PrintHelpAndSettings();

		glPopMatrix();
	}

	glutSwapBuffers();
}

void reshapeFunc(int newWidth, int newHeight) {
	// Check if width or height are really changed
	if ((newWidth != (int)config->film->GetWidth()) ||
			(newHeight != (int)config->film->GetHeight())) {
		glViewport(0, 0, newWidth, newHeight);
		glLoadIdentity();
		glOrtho(0.f, newWidth - 1.0f, 0.f, newHeight - 1.0f, -1.f, 1.f);

		config->ReInit(true, newWidth, newHeight);

		glutPostRedisplay();
	}
}

#define MOVE_STEP 0.5f
#define ROTATE_STEP 4.f
void keyFunc(unsigned char key, int x, int y) {
	switch (key) {
		case 'p': {
			config->SaveImage();
			break;
		}
		case 27: { // Escape key
			// Check if I have to save the film
			const vector<string> filmNames = config->cfg.GetStringVector("screen.file", "");
			if (filmNames.size() == 1)
				config->film->SaveFilm(filmNames[0]);
			else if (filmNames.size() > 1)
				config->film->SaveFilm("merged.flm");
			delete config;
			cerr << "Done." << endl;
			exit(EXIT_SUCCESS);
			break;
		}
		case ' ': // Restart rendering
			config->ReInit(true, config->film->GetWidth(), config->film->GetHeight());
			break;
		case 'a': {
			config->scene->camera->TranslateLeft(MOVE_STEP);
			config->ReInit(false);
			break;
		}
		case 'd': {
			config->scene->camera->TranslateRight(MOVE_STEP);
			config->ReInit(false);
			break;
		}
		case 'w': {
			config->scene->camera->TranslateForward(MOVE_STEP);
			config->ReInit(false);
			break;
		}
		case 's': {
			config->scene->camera->TranslateBackward(MOVE_STEP);
			config->ReInit(false);
			break;
		}
		case 'r':
			config->scene->camera->Translate(Vector(0.f, 0.f, MOVE_STEP));
			config->ReInit(false);
			break;
		case 'f':
			config->scene->camera->Translate(Vector(0.f, 0.f, -MOVE_STEP));
			config->ReInit(false);
			break;
		case 'h':
			printHelp = (!printHelp);
			break;
		case 'x':
			config->scene->camera->fieldOfView = max(15.f,
					config->scene->camera->fieldOfView - 5.f);
			config->ReInit(false);
			break;
		case 'c':
			config->scene->camera->fieldOfView = min(180.f,
					config->scene->camera->fieldOfView + 5.f);
			config->ReInit(false);
			break;
		case 'n': {
			const unsigned int screenRefreshInterval = config->GetScreenRefreshInterval();
			if (screenRefreshInterval > 1000)
				config->SetScreenRefreshInterval(max(1000u, screenRefreshInterval - 1000));
			else
				config->SetScreenRefreshInterval(max(50u, screenRefreshInterval - 50));
			break;
		}
		case 'm': {
			const unsigned int screenRefreshInterval = config->GetScreenRefreshInterval();
			if (screenRefreshInterval >= 1000)
				config->SetScreenRefreshInterval(screenRefreshInterval + 1000);
			else
				config->SetScreenRefreshInterval(screenRefreshInterval + 50);
			break;
		}
		case 'y':
			config->SetMotionBlur(!config->scene->camera->motionBlur);
			break;
		case 't':
			// Toggle tonemap type
			if (config->film->GetToneMapParams()->GetType() == TONEMAP_LINEAR) {
				Reinhard02ToneMapParams params;
				config->film->SetToneMapParams(params);
			} else {
				LinearToneMapParams params;
				config->film->SetToneMapParams(params);
			}
			break;
		case '0':
			config->SetRenderingEngineType(DIRECTLIGHT);
			break;
		case '1':
			config->SetRenderingEngineType(PATH);
			break;
		case '2':
			config->SetRenderingEngineType(SPPM);
			break;
		case 'o': {
#if defined(WIN32)
			HWND hWnd = FindWindow(NULL, SLG_LABEL.c_str());
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

	displayFunc();
}

void specialFunc(int key, int x, int y) {
	switch (key) {
		case GLUT_KEY_UP:
			config->scene->camera->RotateUp(ROTATE_STEP);
			config->ReInit(false);
			break;
		case GLUT_KEY_DOWN:
			config->scene->camera->RotateDown(ROTATE_STEP);
			config->ReInit(false);
			break;
		case GLUT_KEY_LEFT:
			config->scene->camera->RotateLeft(ROTATE_STEP);
			config->ReInit(false);
			break;
		case GLUT_KEY_RIGHT:
			config->scene->camera->RotateRight(ROTATE_STEP);
			config->ReInit(false);
			break;
		default:
			break;
	}

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
	if (mouseButton0) {
		// Check elapsed time since last update
		if (WallClockTime() - lastMouseUpdate > 0.2) {
			const int distX = x - mouseGrabLastX;
			const int distY = y - mouseGrabLastY;

			config->scene->camera->RotateDown(0.04f * distY * ROTATE_STEP);
			config->scene->camera->RotateRight(0.04f * distX * ROTATE_STEP);

			mouseGrabLastX = x;
			mouseGrabLastY = y;

			config->ReInit(false);
			displayFunc();
			lastMouseUpdate = WallClockTime();
		}
	} else if (mouseButton2) {
		// Check elapsed time since last update
		if (WallClockTime() - lastMouseUpdate > 0.2) {
			const int distX = x - mouseGrabLastX;
			const int distY = y - mouseGrabLastY;

			config->scene->camera->TranslateRight(0.04f * distX * MOVE_STEP);
			config->scene->camera->TranslateBackward(0.04f * distY * MOVE_STEP);

			mouseGrabLastX = x;
			mouseGrabLastY = y;

			config->ReInit(false);
			displayFunc();
			lastMouseUpdate = WallClockTime();
		}
	}
}

void timerFunc(int value) {
	const unsigned int pass = config->GetRenderEngine()->GetPass();

	double raysSec = 0.0;
	const vector<IntersectionDevice *> &intersectionDevices = config->GetIntersectionDevices();
	for (size_t i = 0; i < intersectionDevices.size(); ++i)
		raysSec += intersectionDevices[i]->GetPerformance();

	switch (config->GetRenderEngine()->GetEngineType()) {
		case DIRECTLIGHT:
		case PATH: {
			const double sampleSec = config->film->GetAvgSampleSec();
			sprintf(config->captionBuffer, "[Pass %4d][Avg. samples/sec % 4dK][Avg. rays/sec % 4dK on %.1fK tris]",
					pass, int(sampleSec/ 1000.0), int(raysSec / 1000.0), config->scene->dataSet->GetTotalTriangleCount() / 1000.0);
			break;
		}
		case SPPM: {
			SPPMRenderEngine *sre = (SPPMRenderEngine *)config->GetRenderEngine();

			sprintf(config->captionBuffer, "[Pass %3d][Photon %.1fM][Avg. photon/sec % 4dK][Avg. rays/sec % 4dK on %.1fK tris]",
					pass, sre->GetTotalPhotonCount() / 1000000.0, int(sre->GetTotalPhotonSec() / 1000.0),
					int(raysSec / 1000.0), config->scene->dataSet->GetTotalTriangleCount() / 1000.0);
			break;
		}
		default:
			assert (false);
	}

	// Check if periodic save is enabled
	if (config->NeedPeriodicSave()) {
		// Time to save the image and film
		config->SaveImage();
	}

	glutPostRedisplay();

	glutTimerFunc(config->GetScreenRefreshInterval(), timerFunc, 0);
}

void InitGlut(int argc, char *argv[], const unsigned int width, const unsigned int height) {
	glutInit(&argc, argv);

	glutInitWindowSize(width, height);
	// Center the window
	unsigned int scrWidth = glutGet(GLUT_SCREEN_WIDTH);
	unsigned int scrHeight = glutGet(GLUT_SCREEN_HEIGHT);
	if ((scrWidth + 50 < width) || (scrHeight + 50 < height))
		glutInitWindowPosition(0, 0);
	else
		glutInitWindowPosition((scrWidth - width) / 2, (scrHeight - height) / 2);

	glutInitDisplayMode(GLUT_RGB | GLUT_DOUBLE);
	glutCreateWindow(SLG_LABEL.c_str());
}

void RunGlut() {
	glutReshapeFunc(reshapeFunc);
	glutKeyboardFunc(keyFunc);
	glutSpecialFunc(specialFunc);
	glutDisplayFunc(displayFunc);
	glutMouseFunc(mouseFunc);
	glutMotionFunc(motionFunc);

	glutTimerFunc(config->GetScreenRefreshInterval(), timerFunc, 0);

	glMatrixMode(GL_PROJECTION);
	glViewport(0, 0, config->film->GetWidth(), config->film->GetHeight());
	glLoadIdentity();
	glOrtho(0.f, config->film->GetWidth() - 1.f,
			0.f, config->film->GetHeight() - 1.f, -1.f, 1.f);

	glutMainLoop();
}
