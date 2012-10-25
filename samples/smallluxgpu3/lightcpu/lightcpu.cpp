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

//------------------------------------------------------------------------------
// PathOCLRenderEngine
//------------------------------------------------------------------------------

LightCPURenderEngine::LightCPURenderEngine(RenderConfig *rcfg, Film *flm, boost::mutex *flmMutex) :
		CPURenderEngine(rcfg, flm, flmMutex) {
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

	const unsigned int seedBase = (unsigned int)(WallClockTime() / 1000.0);

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
		EndEdit(EditActionList());
	if (started)
		Stop();

	for (size_t i = 0; i < renderThreads.size(); ++i)
		delete renderThreads[i];
}

void LightCPURenderEngine::StartLockLess() {
	for (size_t i = 0; i < renderThreads.size(); ++i)
		renderThreads[i]->Start();
}

void LightCPURenderEngine::StopLockLess() {
	for (size_t i = 0; i < renderThreads.size(); ++i)
		renderThreads[i]->Interrupt();
	for (size_t i = 0; i < renderThreads.size(); ++i)
		renderThreads[i]->Stop();
}

void LightCPURenderEngine::BeginEditLockLess() {
	for (size_t i = 0; i < renderThreads.size(); ++i)
		renderThreads[i]->Interrupt();
	for (size_t i = 0; i < renderThreads.size(); ++i)
		renderThreads[i]->BeginEdit();
}

void LightCPURenderEngine::EndEditLockLess(const EditActionList &editActions) {
	for (size_t i = 0; i < renderThreads.size(); ++i)
		renderThreads[i]->EndEdit(editActions);
}

void LightCPURenderEngine::UpdateFilmLockLess() {
	boost::unique_lock<boost::mutex> lock(*filmMutex);

	film->Reset();

	// Merge the all thread films
	for (size_t i = 0; i < renderThreads.size(); ++i)
		film->AddFilm(*(renderThreads[i]->threadFilm));
	samplesCount = film->GetTotalSampleCount();
}
