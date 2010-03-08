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
#include "film.h"

RenderingConfig *config;

static int printHelp = 1;

void DebugHandler(const char *msg) {
	std::cerr << "[LuxRays] " << msg << std::endl;
}

static void PrintString(void *font, const char *string) {
	int len, i;

	len = (int)strlen(string);
	for (i = 0; i < len; i++)
		glutBitmapCharacter(font, string[i]);
}

static void PrintHelpAndSettings() {
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glColor4f(0.f, 0.f, 0.f, 0.5f);
	glRecti(20, 40, 620, 440);
	glDisable(GL_BLEND);

	glColor3f(1.f, 1.f, 1.f);
	glRasterPos2i(320 - glutBitmapLength(GLUT_BITMAP_9_BY_15, (unsigned char *)"Help & Settings & Devices") / 2, 420);
	PrintString(GLUT_BITMAP_9_BY_15, "Help & Settings & Devices");

	// Help
	glRasterPos2i(60, 390);
	PrintString(GLUT_BITMAP_8_BY_13, "h - toggle Help");
	glRasterPos2i(60, 370);
	PrintString(GLUT_BITMAP_8_BY_13, "arrow Keys or mouse X/Y + mouse button 0 - rotate camera");
	glRasterPos2i(60, 350);
	PrintString(GLUT_BITMAP_8_BY_13, "a, s, d, w or mouse X/Y + mouse button 2 - move camera");
	glRasterPos2i(60, 330);
	PrintString(GLUT_BITMAP_8_BY_13, "p/shift+p - save image.ppm/image.png");
	glRasterPos2i(60, 310);
	PrintString(GLUT_BITMAP_8_BY_13, "n, m - decrease/increase the minimum screen refresh time");
	glRasterPos2i(60, 290);
	PrintString(GLUT_BITMAP_8_BY_13, "v, b - decrease/increase the max. path depth");
	glRasterPos2i(60, 270);
	PrintString(GLUT_BITMAP_8_BY_13, "x, c - decrease/increase the field of view");
	glRasterPos2i(60, 250);
	PrintString(GLUT_BITMAP_8_BY_13, "i, o - decrease/increase the shadow ray count");

	// Settings
	char buf[512];
	glColor3f(0.5f, 1.0f, 0.f);
	glRasterPos2i(25, 230);
	PrintString(GLUT_BITMAP_8_BY_13, "Settings:");
	glRasterPos2i(30, 210);
	sprintf(buf, "[Rendering time: %dsecs][FOV: %.1f][Max path depth: %d][RR Depth: %d]",
			int(config->scene->camera->film->GetTotalTime()),
			config->scene->camera->fieldOfView,
			config->scene->maxPathDepth,
			config->scene->rrDepth);
	PrintString(GLUT_BITMAP_8_BY_13, buf);
	glRasterPos2i(30, 190);
	sprintf(buf, "[Screen refresh: %dms][Render threads: %d][Shadow rays: %d]",
			config->screenRefreshInterval, int(config->GetRenderThreads().size()),
			config->scene->shadowRayCount);
	PrintString(GLUT_BITMAP_8_BY_13, buf);

	// Devices
	const vector<IntersectionDevice *> devices = config->GetIntersectionDevices();
	double minPerf = devices[0]->GetPerformance();
	double totalPerf = devices[0]->GetPerformance();
	for (size_t i = 1; i < devices.size(); ++i) {
		minPerf = min(minPerf, devices[i]->GetPerformance());
		totalPerf += devices[i]->GetPerformance();
	}

	glColor3f(1.0f, 0.5f, 0.f);
	int offset = 45;
	char buff[512];
	for (size_t i = 0; i < devices.size(); ++i) {
		sprintf(buff, "[%s][Rays/sec % 3dK][Load %.1f%%][Prf Idx %.2f][Wrkld %.1f%%]",
				devices[i]->GetName().c_str(),
				int(devices[i]->GetPerformance() / 1000.0),
				100.0 * devices[i]->GetLoad(),
				devices[i]->GetPerformance() / minPerf,
				100.0 * devices[i]->GetPerformance() / totalPerf);
		glRasterPos2i(30, offset);
		PrintString(GLUT_BITMAP_8_BY_13, buff);

		offset += 15;
	}

	glRasterPos2i(25, offset);
	PrintString(GLUT_BITMAP_9_BY_15, "Rendering devices:");
}

static void PrintCaptions() {
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glColor4f(0.f, 0.f, 0.f, 0.8f);
	glRecti(0, config->scene->camera->film->GetHeight() - 15,
			config->scene->camera->film->GetWidth() - 1, config->scene->camera->film->GetHeight() - 1);
	glRecti(0, 0, config->scene->camera->film->GetWidth() - 1, 18);
	glDisable(GL_BLEND);

	// Caption line 0
	glColor3f(1.f, 1.f, 1.f);
	glRasterPos2i(4, 5);
	PrintString(GLUT_BITMAP_8_BY_13, config->captionBuffer);
	// Title
	glRasterPos2i(4, config->scene->camera->film->GetHeight() - 10);
	PrintString(GLUT_BITMAP_8_BY_13, SLG_LABEL.c_str());
}

void displayFunc(void) {
	config->scene->camera->film->UpdateScreenBuffer();
	const float *pixels = config->scene->camera->film->GetScreenBuffer();

	glRasterPos2i(0, 0);
	glDrawPixels(config->scene->camera->film->GetWidth(), config->scene->camera->film->GetHeight(), GL_RGB, GL_FLOAT, pixels);

	PrintCaptions();

	if (printHelp) {
		glPushMatrix();
		glLoadIdentity();
		glOrtho(-0.5, 639.5, -0.5, 479.5, -1.0, 1.0);

		PrintHelpAndSettings();

		glPopMatrix();
	}

	glutSwapBuffers();
}

void reshapeFunc(int newWidth, int newHeight) {
	glViewport(0, 0, newWidth, newHeight);
	glLoadIdentity();
	glOrtho(0.f, newWidth - 1.0f, 0.f, newHeight - 1.0f, -1.f, 1.f);

	config->ReInit(true, newWidth, newHeight);

	glutPostRedisplay();
}

#define MOVE_STEP 0.5f
#define ROTATE_STEP 4.f
void keyFunc(unsigned char key, int x, int y) {
	switch (key) {
		case 'p': {
			config->scene->camera->film->SavePPM("image.ppm");
			break;
		}
		case 'P': {
			config->scene->camera->film->SavePNG("image.png");
			break;
		}
		case 27: // Escape key
			delete config;
			cerr << "Done." << endl;
			exit(EXIT_SUCCESS);
			break;
		case ' ': // Restart rendering
			config->ReInit(true, config->scene->camera->film->GetWidth(), config->scene->camera->film->GetHeight());
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
		case 'v':
			config->SetMaxPathDepth(-1);
			config->ReInit(false);
			break;
		case 'b':
			config->SetMaxPathDepth(+1);
			config->ReInit(false);
			break;
		case 'n':
			config->screenRefreshInterval = max(50u, config->screenRefreshInterval - 50);
			break;
		case 'm':
			config->screenRefreshInterval += 50;
			break;
		case 'i':
			config->SetShadowRays(-1);
			config->ReInit(false);
			break;
		case 'o':
			config->SetShadowRays(+1);
			config->ReInit(false);
			break;
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
	unsigned int pass = 0;
	const vector<RenderThread *> &renderThreads = config->GetRenderThreads();
	for (size_t i = 0; i < renderThreads.size(); ++i)
		pass += renderThreads[i]->GetPass();

	double raysSec = 0.0;
	const vector<IntersectionDevice *> &intersectionDevices = config->GetIntersectionDevices();
	for (size_t i = 0; i < intersectionDevices.size(); ++i)
		raysSec += intersectionDevices[i]->GetPerformance();

	const double sampleSec = config->scene->camera->film->GetAvgSampleSec();

	sprintf(config->captionBuffer, "[Samples %4d][Avg. samples/sec % 4dK][Avg. rays/sec % 4dK on %.1fK tris]",
			pass, int(sampleSec/ 1000.0), int(raysSec / 1000.0), config->scene->dataSet->GetTotalTriangleCount() / 1000.0);

	glutPostRedisplay();

	glutTimerFunc(config->screenRefreshInterval, timerFunc, 0);
}

void InitGlut(int argc, char *argv[], unsigned int width, unsigned int height) {
	glutInitWindowSize(width, height);
	glutInitWindowPosition(0, 0);
	glutInitDisplayMode(GLUT_RGB | GLUT_DOUBLE);
	glutInit(&argc, argv);

	glutCreateWindow(SLG_LABEL.c_str());
}

void RunGlut() {
	glutReshapeFunc(reshapeFunc);
	glutKeyboardFunc(keyFunc);
	glutSpecialFunc(specialFunc);
	glutDisplayFunc(displayFunc);
	glutMouseFunc(mouseFunc);
	glutMotionFunc(motionFunc);

	glutTimerFunc(config->screenRefreshInterval, timerFunc, 0);

	glMatrixMode(GL_PROJECTION);
	glViewport(0, 0, config->scene->camera->film->GetWidth(), config->scene->camera->film->GetHeight());
	glLoadIdentity();
	glOrtho(0.f, config->scene->camera->film->GetWidth() - 1.f,
			0.f, config->scene->camera->film->GetHeight() - 1.f, -1.f, 1.f);

	glutMainLoop();
}
