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
#include <iomanip>
#include <string.h>
#include <string>
#include <sstream>
#include <stdexcept>

#include <boost/thread/mutex.hpp>

#include "smalllux.h"

#include "lightcpu/lightcpu.h"
#include "renderconfig.h"
#include "luxrays/core/geometry/transform.h"
#include "luxrays/core/pixel/samplebuffer.h"

//------------------------------------------------------------------------------
// PathOCLRenderEngine
//------------------------------------------------------------------------------

LightCPURenderEngine::LightCPURenderEngine(RenderConfig *rcfg, NativeFilm *flm, boost::mutex *flmMutex) :
		RenderEngine(rcfg, flm, flmMutex) {
	const Properties &cfg = renderConfig->cfg;

	//--------------------------------------------------------------------------
	// Rendering parameters
	//--------------------------------------------------------------------------

	maxPathDepth = cfg.GetInt("path.maxdepth", 5);
	rrDepth = cfg.GetInt("path.russianroulette.depth", 3);
	rrImportanceCap = cfg.GetFloat("path.russianroulette.cap", 0.125f);
	const float epsilon = cfg.GetFloat("scene.epsilon", .0001f);
	MachineEpsilon::SetMin(epsilon);
	MachineEpsilon::SetMax(epsilon);

	startTime = 0.0;
	samplesCount = 0;
	convergence = 0.f;

	const unsigned int seedBase = (unsigned int)(WallClockTime() / 1000.0);

	film->EnablePerScreenNormalization(true);

	// Create LuxRays context
	const int oclPlatformIndex = cfg.GetInt("opencl.platform.index", -1);
	ctx = new Context(DebugHandler, oclPlatformIndex);
	renderConfig->scene->UpdateDataSet(ctx);

	// Create and start render threads
	const size_t renderThreadCount = boost::thread::hardware_concurrency();
	SLG_LOG("Starting "<< renderThreadCount << " LightCPU render threads");
	for (size_t i = 0; i < renderThreadCount; ++i) {
		LightCPURenderThread *t = new LightCPURenderThread(i, seedBase + i, this);
		renderThreads.push_back(t);
	}
}

LightCPURenderEngine::~LightCPURenderEngine() {
	if (editMode)
		EndEditLockLess(EditActionList());
	if (started)
		StopLockLess();

	for (size_t i = 0; i < renderThreads.size(); ++i)
		delete renderThreads[i];
}

void LightCPURenderEngine::StartLockLess() {
	RenderEngine::StartLockLess();

	ctx->Start();
	
	samplesCount = 0;
	elapsedTime = 0.0f;

	startTime = WallClockTime();
	film->ResetConvergenceTest();
	lastConvergenceTestTime = startTime;
	lastConvergenceTestSamplesCount = 0;

	for (size_t i = 0; i < renderThreads.size(); ++i)
		renderThreads[i]->Start();
}

void LightCPURenderEngine::StopLockLess() {
	for (size_t i = 0; i < renderThreads.size(); ++i)
		renderThreads[i]->Interrupt();
	for (size_t i = 0; i < renderThreads.size(); ++i)
		renderThreads[i]->Stop();

	UpdateFilmLockLess();

	RenderEngine::StopLockLess();

	ctx->Stop();
}

void LightCPURenderEngine::BeginEditLockLess() {
	for (size_t i = 0; i < renderThreads.size(); ++i)
		renderThreads[i]->Interrupt();
	for (size_t i = 0; i < renderThreads.size(); ++i)
		renderThreads[i]->BeginEdit();

	RenderEngine::BeginEditLockLess();

	// Stop all intersection devices
	ctx->Stop();
}

void LightCPURenderEngine::EndEditLockLess(const EditActionList &editActions) {
	RenderEngine::EndEditLockLess(editActions);

	bool dataSetUpdated;
	if (editActions.Has(GEOMETRY_EDIT) ||
			((renderConfig->scene->dataSet->GetAcceleratorType() != ACCEL_MQBVH) &&
			editActions.Has(INSTANCE_TRANS_EDIT))) {
		// To avoid reference to the DataSet de-allocated inside UpdateDataSet()
		ctx->SetDataSet(NULL);

		// For all other accelerator, I have to rebuild the DataSet
		renderConfig->scene->UpdateDataSet(ctx);

		// Set the Luxrays SataSet
		ctx->SetDataSet(renderConfig->scene->dataSet);

		dataSetUpdated = true;
	} else
		dataSetUpdated = false;

	// Restart all intersection devices
	ctx->Start();

	if (!dataSetUpdated && (renderConfig->scene->dataSet->GetAcceleratorType() == ACCEL_MQBVH) &&
			editActions.Has(INSTANCE_TRANS_EDIT)) {
		// Update the DataSet
		ctx->UpdateDataSet();
	}


	film->Reset();

	elapsedTime = 0.0f;
	startTime = WallClockTime();
	film->ResetConvergenceTest();
	lastConvergenceTestTime = startTime;
	lastConvergenceTestSamplesCount = 0;

	for (size_t i = 0; i < renderThreads.size(); ++i)
		renderThreads[i]->EndEdit(editActions);
}

void LightCPURenderEngine::UpdateFilm() {
	boost::unique_lock<boost::mutex> lock(engineMutex);

	if (started)
		UpdateFilmLockLess();
}

void LightCPURenderEngine::UpdateFilmLockLess() {
	boost::unique_lock<boost::mutex> lock(*filmMutex);

	const float haltthreshold = renderConfig->cfg.GetFloat("batch.haltthreshold", -1.f);
	if (haltthreshold >= 0.f) {
		// Check if it is time to run the convergence test again
		const unsigned int imgWidth = film->GetWidth();
		const unsigned int imgHeight = film->GetHeight();
		const unsigned int pixelCount = imgWidth * imgHeight;
		const double now = WallClockTime();

		// Do not run the test if we don't have at least 16 new samples per pixel
		if ((samplesCount  - lastConvergenceTestSamplesCount > pixelCount * 16) &&
				((now - lastConvergenceTestTime) * 1000.0 >= renderConfig->GetScreenRefreshInterval())) {
			film->UpdateScreenBuffer(); // Required in order to have a valid convergence test
			convergence = 1.f - film->RunConvergenceTest() / (float)pixelCount;
			lastConvergenceTestTime = now;
			lastConvergenceTestSamplesCount = samplesCount;
		}
	}
}

unsigned int LightCPURenderEngine::GetPass() const {
	return samplesCount / (film->GetWidth() * film->GetHeight());
}

float LightCPURenderEngine::GetConvergence() const {
	return convergence;
}
