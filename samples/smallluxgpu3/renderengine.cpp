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

#include "renderengine.h"
#include "renderconfig.h"
#include "pathocl/pathocl.h"
#include "lightcpu/lightcpu.h"
#include "pathcpu/pathcpu.h"
#include "bidircpu/bidircpu.h"
#include "bidirhybrid/bidirhybrid.h"

#include "luxrays/core/intersectiondevice.h"
#include "luxrays/opencl/intersectiondevice.h"
#include "luxrays/utils/sdl/bsdf.h"

//------------------------------------------------------------------------------
// RenderEngine
//------------------------------------------------------------------------------

RenderEngine::RenderEngine(RenderConfig *cfg, Film *flm, boost::mutex *flmMutex) {
	renderConfig = cfg;
	film = flm;
	filmMutex = flmMutex;
	started = false;
	editMode = false;

	// Create LuxRays context
	const int oclPlatformIndex = renderConfig->cfg.GetInt("opencl.platform.index", -1);
	ctx = new Context(DebugHandler, oclPlatformIndex);

	renderConfig->scene->UpdateDataSet(ctx);
}

RenderEngine::~RenderEngine() {
	if (editMode)
		EndEdit(EditActionList());
	if (started)
		Stop();

	delete ctx;
}

void RenderEngine::Start() {
	boost::unique_lock<boost::mutex> lock(engineMutex);

	assert (!started);
	started = true;

	ctx->Start();

	StartLockLess();

	samplesCount = 0;
	elapsedTime = 0.0f;

	startTime = WallClockTime();
	film->ResetConvergenceTest();
	lastConvergenceTestTime = startTime;
	lastConvergenceTestSamplesCount = 0;	
}

void RenderEngine::Stop() {
	boost::unique_lock<boost::mutex> lock(engineMutex);

	StopLockLess();

	assert (started);
	started = false;

	ctx->Stop();

	UpdateFilmLockLess();
}

void RenderEngine::BeginEdit() {
	boost::unique_lock<boost::mutex> lock(engineMutex);

	assert (started);
	assert (!editMode);
	editMode = true;

	BeginEditLockLess();

	// Stop all intersection devices
	ctx->Stop();
}

void RenderEngine::EndEdit(const EditActionList &editActions) {
	assert (started);
	assert (editMode);

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

	if (!dataSetUpdated &&
			(renderConfig->scene->dataSet->GetAcceleratorType() == ACCEL_MQBVH) &&
			editActions.Has(INSTANCE_TRANS_EDIT)) {
		// Update the DataSet
		ctx->UpdateDataSet();
	}

	elapsedTime = 0.0f;
	startTime = WallClockTime();
	film->ResetConvergenceTest();
	lastConvergenceTestTime = startTime;
	lastConvergenceTestSamplesCount = 0;

	editMode = false;

	EndEditLockLess(editActions);
}

void RenderEngine::UpdateFilm() {
	boost::unique_lock<boost::mutex> lock(engineMutex);

	if (started) {
		elapsedTime = WallClockTime() - startTime;
		UpdateFilmLockLess();
		UpdateSamplesCount();

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
}

RenderEngineType RenderEngine::String2RenderEngineType(const string &type) {
#if !defined(LUXRAYS_DISABLE_OPENCL)
	if ((type.compare("4") == 0) || (type.compare("PATHOCL") == 0))
		return PATHOCL;
#endif
	if ((type.compare("5") == 0) || (type.compare("LIGHTCPU") == 0))
		return LIGHTCPU;
	if ((type.compare("6") == 0) || (type.compare("PATHCPU") == 0))
		return PATHCPU;
	if ((type.compare("7") == 0) || (type.compare("BIDIRCPU") == 0))
		return BIDIRCPU;
#if !defined(LUXRAYS_DISABLE_OPENCL)
	if ((type.compare("8") == 0) || (type.compare("BIDIRHYBRID") == 0))
		return BIDIRHYBRID;
#endif
	throw runtime_error("Unknown render engine type: " + type);
}

const string RenderEngine::RenderEngineType2String(const RenderEngineType type) {
	switch (type) {
#if !defined(LUXRAYS_DISABLE_OPENCL)
		case PATHOCL:
			return "PATHOCL";
#endif
		case LIGHTCPU:
			return "LIGHTCPU";
		case PATHCPU:
			return "PATHCPU";
		case BIDIRCPU:
			return "BIDIRCPU";
#if !defined(LUXRAYS_DISABLE_OPENCL)
		case BIDIRHYBRID:
			return "BIDIRHYBRID";
#endif
		default:
			throw runtime_error("Unknown render engine type: " + type);
	}
}

RenderEngine *RenderEngine::AllocRenderEngine(const RenderEngineType engineType,
		RenderConfig *renderConfig, Film *film, boost::mutex *filmMutex) {
	switch (engineType) {
#if !defined(LUXRAYS_DISABLE_OPENCL)
		case PATHOCL:
			return new PathOCLRenderEngine(renderConfig, film, filmMutex);
#endif
		case LIGHTCPU:
			return new LightCPURenderEngine(renderConfig, film, filmMutex);
		case PATHCPU:
			return new PathCPURenderEngine(renderConfig, film, filmMutex);
		case BIDIRCPU:
			return new BiDirCPURenderEngine(renderConfig, film, filmMutex);
#if !defined(LUXRAYS_DISABLE_OPENCL)
		case BIDIRHYBRID:
			return new BiDirHybridRenderEngine(renderConfig, film, filmMutex);
#endif
		default:
			throw runtime_error("Unknown render engine type");
	}
}

double RenderEngine::GetTotalRaysSec() const
{
	double raysSec = 0.;
	for (size_t i = 0; i < intersectionDevices.size(); ++i)
		raysSec += intersectionDevices[i]->GetPerformance();
	return raysSec;
}

//------------------------------------------------------------------------------
// OCLRenderEngine
//------------------------------------------------------------------------------

OCLRenderEngine::OCLRenderEngine(RenderConfig *rcfg, Film *flm, boost::mutex *flmMutex) :
	RenderEngine(rcfg, flm, flmMutex) {
	const Properties &cfg = renderConfig->cfg;

	const bool useCPUs = (cfg.GetInt("opencl.cpu.use", 1) != 0);
	const bool useGPUs = (cfg.GetInt("opencl.gpu.use", 1) != 0);
	const unsigned int forceGPUWorkSize = cfg.GetInt("opencl.gpu.workgroup.size", 64);
	const unsigned int forceCPUWorkSize = cfg.GetInt("opencl.cpu.workgroup.size", 1);
	const string oclDeviceConfig = cfg.GetString("opencl.devices.select", "");

	// Start OpenCL devices
	std::vector<DeviceDescription *> descs = ctx->GetAvailableDeviceDescriptions();
	DeviceDescription::Filter(DEVICE_TYPE_OPENCL_ALL, descs);

	// Device info
	bool haveSelectionString = (oclDeviceConfig.length() > 0);
	if (haveSelectionString && (oclDeviceConfig.length() != descs.size())) {
		stringstream ss;
		ss << "OpenCL device selection string has the wrong length, must be " <<
				descs.size() << " instead of " << oclDeviceConfig.length();
		throw runtime_error(ss.str().c_str());
	}

	for (size_t i = 0; i < descs.size(); ++i) {
		DeviceDescription *desc = descs[i];

		if (haveSelectionString) {
			if (oclDeviceConfig.at(i) == '1') {
				if (desc->GetType() == DEVICE_TYPE_OPENCL_GPU)
					desc->SetForceWorkGroupSize(forceGPUWorkSize);
				else if (desc->GetType() == DEVICE_TYPE_OPENCL_CPU)
					desc->SetForceWorkGroupSize(forceCPUWorkSize);
				selectedDeviceDescs.push_back(desc);
			}
		} else {
			if ((useCPUs && desc->GetType() == DEVICE_TYPE_OPENCL_CPU) ||
					(useGPUs && desc->GetType() == DEVICE_TYPE_OPENCL_GPU)) {
				if (desc->GetType() == DEVICE_TYPE_OPENCL_GPU)
					desc->SetForceWorkGroupSize(forceGPUWorkSize);
				else if (desc->GetType() == DEVICE_TYPE_OPENCL_CPU)
					desc->SetForceWorkGroupSize(forceCPUWorkSize);
				selectedDeviceDescs.push_back(descs[i]);
			}
		}
	}

	if (selectedDeviceDescs.size() == 0)
		throw runtime_error("No OpenCL device selected or available");
}

//------------------------------------------------------------------------------
// CPURenderEngine
//------------------------------------------------------------------------------

CPURenderEngine::CPURenderEngine(RenderConfig *cfg, Film *flm,
	boost::mutex *flmMutex, void (* threadFunc)(CPURenderThread *),
	const bool enablePerPixelNormBuffer,
	const bool enablePerScreenNormBuffer) :
	RenderEngine(cfg, flm, flmMutex)
{
	renderThreadFunc = threadFunc;
	samplesCount = 0.;

	const unsigned int seedBase = (unsigned int)(WallClockTime() / 1000.0);

	// Start native devices
	std::vector<DeviceDescription *> descs = ctx->GetAvailableDeviceDescriptions();
	DeviceDescription::Filter(DEVICE_TYPE_NATIVE_THREAD, descs);

	if (descs.size() == 0)
		throw runtime_error("No CPU device selected or available");

	// Allocate devices
	std::vector<IntersectionDevice *> devs = ctx->AddIntersectionDevices(descs);

	// Check if I have to set max. QBVH stack size
	const size_t qbvhStackSize = cfg->cfg.GetInt("accelerator.qbvh.stacksize.max", 24);
	SLG_LOG("CPU Devices used:");
	for (size_t i = 0; i < devs.size(); ++i) {
		SLG_LOG("[" << devs[i]->GetName() << "]");
		devs[i]->SetMaxStackSize(qbvhStackSize);
		intersectionDevices.push_back(devs[i]);
	}

	// Create and start render threads
	const size_t renderThreadCount = boost::thread::hardware_concurrency();
	SLG_LOG("Starting "<< renderThreadCount << " CPU render threads");
	for (unsigned int i = 0; i < renderThreadCount; ++i) {
		CPURenderThread *t = new CPURenderThread(this, devs[i],
			seedBase + i, threadFunc,
			enablePerPixelNormBuffer, enablePerScreenNormBuffer);
		renderThreads.push_back(t);
	}
}

CPURenderEngine::~CPURenderEngine() {
	if (editMode)
		EndEdit(EditActionList());
	if (started)
		Stop();

	for (size_t i = 0; i < renderThreads.size(); ++i)
		delete renderThreads[i];
}

void CPURenderEngine::StartLockLess() {
	for (size_t i = 0; i < renderThreads.size(); ++i)
		renderThreads[i]->Start();
}

void CPURenderEngine::StopLockLess() {
	for (size_t i = 0; i < renderThreads.size(); ++i)
		renderThreads[i]->Interrupt();
	for (size_t i = 0; i < renderThreads.size(); ++i)
		renderThreads[i]->Stop();
}

void CPURenderEngine::BeginEditLockLess() {
	for (size_t i = 0; i < renderThreads.size(); ++i)
		renderThreads[i]->Interrupt();
	for (size_t i = 0; i < renderThreads.size(); ++i)
		renderThreads[i]->BeginEdit();
}

void CPURenderEngine::EndEditLockLess(const EditActionList &editActions) {
	for (size_t i = 0; i < renderThreads.size(); ++i)
		renderThreads[i]->EndEdit(editActions);
}

void CPURenderEngine::UpdateFilmLockLess() {
	boost::unique_lock<boost::mutex> lock(*filmMutex);

	film->Reset();

	// Merge the all thread films
	for (size_t i = 0; i < renderThreads.size(); ++i) {
		if (renderThreads[i]->threadFilm)
			film->AddFilm(*(renderThreads[i]->threadFilm));
	}
}

//------------------------------------------------------------------------------
// CPURenderThread
//------------------------------------------------------------------------------

CPURenderThread::CPURenderThread(CPURenderEngine *engine,
	IntersectionDevice *dev, const unsigned int seedVal,
	void (* threadFunc)(CPURenderThread *),
	const bool enablePerPixelNormBuf, const bool enablePerScreenNormBuf)
{
	device = dev;
	seed = seedVal;
	renderThreadFunc = threadFunc;
	renderEngine = engine;
	started = false;
	editMode = false;

	threadFilm = NULL;
	enablePerPixelNormBuffer = enablePerPixelNormBuf;
	enablePerScreenNormBuffer = enablePerScreenNormBuf;
}

CPURenderThread::~CPURenderThread() {
	if (editMode)
		EndEdit(EditActionList());
	if (started)
		Stop();

	delete threadFilm;
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
	const unsigned int filmWidth = renderEngine->film->GetWidth();
	const unsigned int filmHeight = renderEngine->film->GetHeight();

	delete threadFilm;
	threadFilm = new Film(filmWidth, filmHeight,
			enablePerPixelNormBuffer, enablePerScreenNormBuffer, false);
	threadFilm->CopyDynamicSettings(*(renderEngine->film));
	threadFilm->Init(filmWidth, filmHeight);

	// Create the thread for the rendering
	renderThread = new boost::thread(boost::bind(renderThreadFunc, this));
}

void CPURenderThread::StopRenderThread() {
	if (renderThread) {
		renderThread->interrupt();
		renderThread->join();
		delete renderThread;
		renderThread = NULL;
	}
}

void CPURenderThread::BeginEdit() {
	StopRenderThread();
}

void CPURenderThread::EndEdit(const EditActionList &editActions) {
	StartRenderThread();
}
