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

#include "smallsppmgpu.h"
#include "displayfunc.h"

static void PrintString(void *font, const char *string) {
	int len, i;

	len = (int)strlen(string);
	for (i = 0; i < len; i++)
		glutBitmapCharacter(font, string[i]);
}

static void PrintCaptions() {
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glColor4f(0.f, 0.f, 0.f, 0.8f);
	glRecti(0, imgHeight - 15, imgWidth - 1, imgHeight - 1);
	glRecti(0, 0, imgWidth - 1, 18);
	glDisable(GL_BLEND);

	// Title
	glColor3f(1.f, 1.f, 1.f);
	glRasterPos2i(4, imgHeight - 10);
	PrintString(GLUT_BITMAP_8_BY_13, SPPMG_LABEL.c_str());

	// Stats
	glRasterPos2i(4, 5);
	char captionBuffer[512];
	const double elapsedTime = luxrays::WallClockTime() - startTime;
	const unsigned long long photonTraced = photonTracedTotal + photonTracedPass;
	const unsigned int kPhotonsSec = photonTraced / (elapsedTime * 1000.f);
	const unsigned int pass = (hitPoints) ? hitPoints->GetPassCount() : 0;
	sprintf(captionBuffer, "[Elapsed time %dsecs][Pass %d][Photons %.2fM][Avg. photons/sec % 4dK]",
		int(elapsedTime), pass, float(photonTraced / 1000000.0), kPhotonsSec);
	PrintString(GLUT_BITMAP_8_BY_13, captionBuffer);
}

static void DisplayFunc(void) {
	if (film) {
		film->UpdateScreenBuffer();
		glRasterPos2i(0, 0);
		glDrawPixels(imgWidth, imgHeight, GL_RGB, GL_FLOAT, film->GetScreenBuffer());

		PrintCaptions();
	} else
		glClear(GL_COLOR_BUFFER_BIT);

	glutSwapBuffers();
}

static void KeyFunc(unsigned char key, int x, int y) {
	switch (key) {
		case 'p':
			film->UpdateScreenBuffer();
			film->Save(imgFileName);
			break;
		case 27: { // Escape key
			// Stop photon tracing thread

			if (renderThreads.size() > 0) {
				for (unsigned int i = 0; i < renderThreads.size(); ++i)
					renderThreads[i]->interrupt();
				for (unsigned int i = 0; i < renderThreads.size(); ++i) {
					renderThreads[i]->join();
					delete renderThreads[i];
				}

				renderThreads.clear();
			}

			film->UpdateScreenBuffer();
			film->Save(imgFileName);
			film->FreeSampleBuffer(sampleBuffer);
			delete film;

			ctx->Stop();
			delete scene;
			delete hitPoints;
			delete ctx;

			std::cerr << "Done." << std::endl;
			exit(EXIT_SUCCESS);
			break;
		}
		default:
			break;
	}

	DisplayFunc();
}

static unsigned int lastFramBufferPass = 0;

static void UpdateFrameBuffer() {
	if (!film)
		return;

	if (hitPoints && (hitPoints->GetPassCount() > lastFramBufferPass)) {
		film->Reset();

		for (unsigned int i = 0; i < hitPoints->GetSize(); ++i) {
			HitPoint *hp = hitPoints->GetHitPoint(i);
			const float scrX = i % imgWidth;
			const float scrY = i / imgWidth;
			sampleBuffer->SplatSample(scrX, scrY, hp->radiance);

			if (sampleBuffer->IsFull()) {
				// Splat all samples on the film
				film->SplatSampleBuffer(true, sampleBuffer);
				sampleBuffer = film->GetFreeSampleBuffer();
			}
		}

		if (sampleBuffer->GetSampleCount() > 0) {
			// Splat all samples on the film
			film->SplatSampleBuffer(true, sampleBuffer);
			sampleBuffer = film->GetFreeSampleBuffer();
		}
		
		lastFramBufferPass = hitPoints->GetPassCount();
	}
}

static void TimerFunc(int value) {
	UpdateFrameBuffer();

	glutPostRedisplay();

	glutTimerFunc(scrRefreshInterval, TimerFunc, 0);
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
	glutCreateWindow(SPPMG_LABEL.c_str());
}

void RunGlut(const unsigned int width, const unsigned int height) {
	glutKeyboardFunc(KeyFunc);
	glutDisplayFunc(DisplayFunc);

	glutTimerFunc(scrRefreshInterval, TimerFunc, 0);

	glMatrixMode(GL_PROJECTION);
	glViewport(0, 0, width, height);
	glLoadIdentity();
	glOrtho(0.f, width - 1.f,
			0.f, height - 1.f, -1.f, 1.f);

	glutMainLoop();
}
