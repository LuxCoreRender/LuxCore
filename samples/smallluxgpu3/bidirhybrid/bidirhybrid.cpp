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

#include "smalllux.h"
#include "renderconfig.h"

#if !defined(LUXRAYS_DISABLE_OPENCL)

#include "bidirhybrid/bidirhybrid.h"

//------------------------------------------------------------------------------
// BiDirCPURenderEngine
//------------------------------------------------------------------------------

BiDirHybridRenderEngine::BiDirHybridRenderEngine(RenderConfig *rcfg, Film *flm, boost::mutex *flmMutex) :
		OCLRenderEngine(rcfg, flm, flmMutex) {
	const Properties &cfg = renderConfig->cfg;

	//--------------------------------------------------------------------------
	// Rendering parameters
	//--------------------------------------------------------------------------

	maxEyePathDepth = cfg.GetInt("path.maxdepth", 5);
	maxLightPathDepth = cfg.GetInt("light.maxdepth", 5);
	rrDepth = cfg.GetInt("light.russianroulette.depth", cfg.GetInt("path.russianroulette.depth", 3));
	rrImportanceCap = cfg.GetFloat("light.russianroulette.cap", cfg.GetFloat("path.russianroulette.cap", 0.125f));
	const float epsilon = cfg.GetFloat("scene.epsilon", .0001f);
	MachineEpsilon::SetMin(epsilon);
	MachineEpsilon::SetMax(epsilon);

	film->EnableOverlappedScreenBufferUpdate(true);

	//--------------------------------------------------------------------------
	// Create render threads
	//--------------------------------------------------------------------------

	const size_t renderThreadCount = boost::thread::hardware_concurrency();
	if (selectedDeviceDescs.size() == 1) {
		// Only one intersection device, use a M2O device
		intersectionDevices = ctx->AddVirtualM2OIntersectionDevices(renderThreadCount, selectedDeviceDescs);
	} else {
		// Multiple intersection devices, use a M2M device
		intersectionDevices = ctx->AddVirtualM2MIntersectionDevices(renderThreadCount, selectedDeviceDescs);
	}
	devices = ctx->GetIntersectionDevices();

	// Set the Luxrays DataSet
	ctx->SetDataSet(renderConfig->scene->dataSet);

	const unsigned int seedBase = (unsigned int)(WallClockTime() / 1000.0);
	SLG_LOG("Starting "<< renderThreadCount << " BiDir hybrid render threads");
	for (size_t i = 0; i < renderThreadCount; ++i) {
		BiDirHybridRenderThread *t = new BiDirHybridRenderThread(i, seedBase + i, devices[i], this);
		renderThreads.push_back(t);
	}
}

BiDirHybridRenderEngine::~BiDirHybridRenderEngine() {
	if (editMode)
		EndEdit(EditActionList());
	if (started)
		Stop();
}

void BiDirHybridRenderEngine::StartLockLess() {
	for (size_t i = 0; i < renderThreads.size(); ++i)
		renderThreads[i]->Start();
}

void BiDirHybridRenderEngine::StopLockLess() {
	for (size_t i = 0; i < renderThreads.size(); ++i)
		renderThreads[i]->Interrupt();
	for (size_t i = 0; i < renderThreads.size(); ++i)
		renderThreads[i]->Stop();
}

void BiDirHybridRenderEngine::BeginEditLockLess() {
	for (size_t i = 0; i < renderThreads.size(); ++i)
		renderThreads[i]->Interrupt();
	for (size_t i = 0; i < renderThreads.size(); ++i)
		renderThreads[i]->BeginEdit();
}

void BiDirHybridRenderEngine::EndEditLockLess(const EditActionList &editActions) {
	for (size_t i = 0; i < renderThreads.size(); ++i)
		renderThreads[i]->EndEdit(editActions);
}

void BiDirHybridRenderEngine::UpdateFilmLockLess() {
	boost::unique_lock<boost::mutex> lock(*filmMutex);

	film->Reset();

	// Merge the all thread films
	for (size_t i = 0; i < renderThreads.size(); ++i) {
		if (renderThreads[i]->threadFilm)
			film->AddFilm(*(renderThreads[i]->threadFilm));
	}
}

#endif
