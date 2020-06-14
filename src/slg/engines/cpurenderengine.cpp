/***************************************************************************
 * Copyright 1998-2020 by authors (see AUTHORS.txt)                        *
 *                                                                         *
 *   This file is part of LuxCoreRender.                                   *
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

#include <boost/format.hpp>

#include "luxrays/utils/thread.h"

#include "slg/engines/cpurenderengine.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// CPURenderThread
//------------------------------------------------------------------------------

CPURenderThread::CPURenderThread(CPURenderEngine *engine,
		const u_int index, IntersectionDevice *dev) {
	threadIndex = index;
	renderEngine = engine;
	device = dev;

	started = false;
	editMode = false;
	threadDone = false;
}

CPURenderThread::~CPURenderThread() {
	if (editMode)
		EndSceneEdit(EditActionList());
	if (started)
		Stop();
}

void CPURenderThread::Start() {
	started = true;

	StartRenderThread();
}

void CPURenderThread::Interrupt() {
	if (renderThread)
		renderThread->interrupt();
}

void CPURenderThread::Stop() {
	StopRenderThread();

	started = false;
}

void CPURenderThread::StartRenderThread() {
	threadDone = false;

	// Create the thread for the rendering
	renderThread = AllocRenderThread();
}

void CPURenderThread::StopRenderThread() {
	if (renderThread) {
		renderThread->interrupt();
		renderThread->join();
		delete renderThread;
		renderThread = NULL;
	}
}

void CPURenderThread::BeginSceneEdit() {
	StopRenderThread();
}

void CPURenderThread::EndSceneEdit(const EditActionList &editActions) {
	StartRenderThread();
}

bool CPURenderThread::HasDone() const {
	return (renderThread == NULL) || threadDone;
}

void CPURenderThread::WaitForDone() const {
	if (renderThread)
		renderThread->join();
}

//------------------------------------------------------------------------------
// CPURenderEngine
//------------------------------------------------------------------------------

CPURenderEngine::CPURenderEngine(const RenderConfig *cfg) : RenderEngine(cfg) {
	// I have to use u_int because Property::Get<size_t>() is not defined
	const size_t renderThreadCount =  Max<u_int>(1u, cfg->cfg.Get(GetDefaultProps().Get("native.threads.count")).Get<u_int>());

	//--------------------------------------------------------------------------
	// Allocate devices
	//--------------------------------------------------------------------------

	vector<DeviceDescription *>  devDescs = ctx->GetAvailableDeviceDescriptions();
	DeviceDescription::Filter(DEVICE_TYPE_NATIVE, devDescs);
	devDescs.resize(1);

	selectedDeviceDescs.resize(renderThreadCount, devDescs[0]);
	intersectionDevices = ctx->AddIntersectionDevices(selectedDeviceDescs);

	//--------------------------------------------------------------------------
	// Setup render threads array
	//--------------------------------------------------------------------------

	SLG_LOG("Configuring "<< renderThreadCount << " CPU render threads");
	renderThreads.resize(renderThreadCount, NULL);
}

CPURenderEngine::~CPURenderEngine() {
	if (editMode)
		EndSceneEdit(EditActionList());
	if (started)
		Stop();

	for (size_t i = 0; i < renderThreads.size(); ++i)
		delete renderThreads[i];
}

void CPURenderEngine::StartLockLess() {
	for (size_t i = 0; i < renderThreads.size(); ++i) {
		if (!renderThreads[i])
			renderThreads[i] = NewRenderThread(i, intersectionDevices[i]);
		renderThreads[i]->Start();
	}
}

void CPURenderEngine::StopLockLess() {
	for (size_t i = 0; i < renderThreads.size(); ++i) {
		if (renderThreads[i])
			renderThreads[i]->Interrupt();
	}
	for (size_t i = 0; i < renderThreads.size(); ++i) {
		if (renderThreads[i])
			renderThreads[i]->Stop();
	}
}

void CPURenderEngine::BeginSceneEditLockLess() {
	for (size_t i = 0; i < renderThreads.size(); ++i)
		renderThreads[i]->Interrupt();
	for (size_t i = 0; i < renderThreads.size(); ++i)
		renderThreads[i]->BeginSceneEdit();
}

void CPURenderEngine::EndSceneEditLockLess(const EditActionList &editActions) {
	for (size_t i = 0; i < renderThreads.size(); ++i)
		renderThreads[i]->EndSceneEdit(editActions);
}

bool CPURenderEngine::HasDone() const {
	for (size_t i = 0; i < renderThreads.size(); ++i) {
		if (!renderThreads[i]->HasDone())
			return false;
	}

	return true;
}

void CPURenderEngine::WaitForDone() const {
	for (size_t i = 0; i < renderThreads.size(); ++i)
		renderThreads[i]->WaitForDone();
}

Properties CPURenderEngine::ToProperties(const Properties &cfg) {
	return Properties() <<
			cfg.Get(GetDefaultProps().Get("native.threads.count"));
}

const Properties &CPURenderEngine::GetDefaultProps() {
	static Properties props = Properties() <<
			RenderEngine::GetDefaultProps() <<
			Property("native.threads.count")((u_int)GetHardwareThreadCount());

	return props;
}

//------------------------------------------------------------------------------
// CPUNoTileRenderThread
//------------------------------------------------------------------------------

CPUNoTileRenderThread::CPUNoTileRenderThread(CPUNoTileRenderEngine *engine,
		const u_int index, IntersectionDevice *dev) : CPURenderThread(engine, index, dev) {
}

CPUNoTileRenderThread::~CPUNoTileRenderThread() {
}

//------------------------------------------------------------------------------
// CPUNoTileRenderEngine
//------------------------------------------------------------------------------

CPUNoTileRenderEngine::CPUNoTileRenderEngine(const RenderConfig *cfg) : CPURenderEngine(cfg) {
	samplerSharedData = NULL;
}

CPUNoTileRenderEngine::~CPUNoTileRenderEngine() {
	delete samplerSharedData;
}

void CPUNoTileRenderEngine::StartLockLess() {
	samplerSharedData = renderConfig->AllocSamplerSharedData(&seedBaseGenerator, film);
	
	CPURenderEngine::StartLockLess();
}

void CPUNoTileRenderEngine::StopLockLess() {
	CPURenderEngine::StopLockLess();

	delete samplerSharedData;
	samplerSharedData = NULL;
}

void CPUNoTileRenderEngine::EndSceneEditLockLess(const EditActionList &editActions) {
	samplerSharedData->Reset();

	CPURenderEngine::EndSceneEditLockLess(editActions);
}

void CPUNoTileRenderEngine::UpdateFilmLockLess() {
}

void CPUNoTileRenderEngine::UpdateCounters() {
	// Update the ray count statistic
	double totalCount = 0.0;
	for (size_t i = 0; i < renderThreads.size(); ++i) {
		const CPUNoTileRenderThread *thread = (CPUNoTileRenderThread *)renderThreads[i];
		totalCount += thread->device->GetTotalRaysCount();
	}
	raysCount = totalCount;
}

Properties CPUNoTileRenderEngine::ToProperties(const Properties &cfg) {
	return CPURenderEngine::ToProperties(cfg);
}

const Properties &CPUNoTileRenderEngine::GetDefaultProps() {
	static Properties props;
	return props;
}

//------------------------------------------------------------------------------
// CPUTileRenderThread
//------------------------------------------------------------------------------

CPUTileRenderThread::CPUTileRenderThread(CPUTileRenderEngine *engine,
		const u_int index, IntersectionDevice *dev) :
		CPURenderThread(engine, index, dev) {
	tileFilm = NULL;
}

CPUTileRenderThread::~CPUTileRenderThread() {
	delete tileFilm;
}

void CPUTileRenderThread::StartRenderThread() {
	delete tileFilm;

	CPUTileRenderEngine *cpuTileEngine = (CPUTileRenderEngine *)renderEngine;
	tileFilm = new Film(cpuTileEngine->tileRepository->tileWidth, cpuTileEngine->tileRepository->tileHeight, NULL);
	tileFilm->CopyDynamicSettings(*(cpuTileEngine->film));
	tileFilm->Init();

	CPURenderThread::StartRenderThread();
}

//------------------------------------------------------------------------------
// CPUTileRenderEngine
//------------------------------------------------------------------------------

CPUTileRenderEngine::CPUTileRenderEngine(const RenderConfig *cfg) : CPURenderEngine(cfg) {
	tileRepository = NULL;
}

CPUTileRenderEngine::~CPUTileRenderEngine() {
	delete tileRepository;
}

void CPUTileRenderEngine::StopLockLess() {
	CPURenderEngine::StopLockLess();

	delete tileRepository;
	tileRepository = NULL;
}

void CPUTileRenderEngine::EndSceneEditLockLess(const EditActionList &editActions) {
	tileRepository->Clear();
	tileRepository->InitTiles(*film);

	CPURenderEngine::EndSceneEditLockLess(editActions);
}

void CPUTileRenderEngine::UpdateCounters() {
	// Update the ray count statistic
	double totalCount = 0.0;
	for (size_t i = 0; i < renderThreads.size(); ++i) {
		const CPUTileRenderThread *thread = (CPUTileRenderThread *)renderThreads[i];
		totalCount += thread->device->GetTotalRaysCount();
	}
	raysCount = totalCount;
}

Properties CPUTileRenderEngine::ToProperties(const Properties &cfg) {
	return CPURenderEngine::ToProperties(cfg) <<
			TileRepository::ToProperties(cfg);
}

const Properties &CPUTileRenderEngine::GetDefaultProps() {
	return TileRepository::GetDefaultProps();
}
