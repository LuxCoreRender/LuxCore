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
#include "cbidirhybrid/cbidirhybrid.h"
#include "bidirvmcpu/bidirvmcpu.h"

#include "luxrays/core/intersectiondevice.h"
#include "luxrays/utils/sdl/bsdf.h"
#include "luxrays/opencl/device.h"

using namespace std;
using namespace luxrays;
using namespace luxrays::sdl;
using namespace luxrays::utils;

namespace slg {

//------------------------------------------------------------------------------
// RenderEngine
//------------------------------------------------------------------------------

RenderEngine::RenderEngine(RenderConfig *cfg, Film *flm, boost::mutex *flmMutex) :
	seedBaseGenerator(131) {
	renderConfig = cfg;
	film = flm;
	filmMutex = flmMutex;
	started = false;
	editMode = false;
	GenerateNewSeed();

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

	const float epsilonMin = renderConfig->cfg.GetFloat("scene.epsilon.min", DEFAULT_EPSILON_MIN);
	MachineEpsilon::SetMin(epsilonMin);
	const float epsilonMax = renderConfig->cfg.GetFloat("scene.epsilon.max", DEFAULT_EPSILON_MAX);
	MachineEpsilon::SetMax(epsilonMax);
	
	ctx->Start();

	StartLockLess();

	samplesCount = 0;
	elapsedTime = 0.0f;

	startTime = WallClockTime();
	film->ResetConvergenceTest();
	convergence = 0.f;
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
	boost::unique_lock<boost::mutex> lock(engineMutex);

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

		// Set the LuxRays SataSet
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

	samplesCount = 0;
	elapsedTime = 0.0f;

	startTime = WallClockTime();
	film->ResetConvergenceTest();
	convergence = 0.f;
	lastConvergenceTestTime = startTime;
	lastConvergenceTestSamplesCount = 0;

	editMode = false;

	EndEditLockLess(editActions);
}

void RenderEngine::SetSeed(const unsigned long seed) {
	seedBaseGenerator.init(seed);

	GenerateNewSeed();
}

void RenderEngine::GenerateNewSeed() {
	seedBase = seedBaseGenerator.uintValue();
}

void RenderEngine::UpdateFilm() {
	boost::unique_lock<boost::mutex> lock(engineMutex);

	if (started) {
		elapsedTime = WallClockTime() - startTime;
		UpdateFilmLockLess();
		UpdateCounters();

		const float haltthreshold = renderConfig->cfg.GetFloat("batch.haltthreshold", -1.f);
		if (haltthreshold >= 0.f) {
			// Check if it is time to run the convergence test again
			const u_int imgWidth = film->GetWidth();
			const u_int imgHeight = film->GetHeight();
			const u_int pixelCount = imgWidth * imgHeight;
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
	if ((type.compare("4") == 0) || (type.compare("PATHOCL") == 0))
		return PATHOCL;
	if ((type.compare("5") == 0) || (type.compare("LIGHTCPU") == 0))
		return LIGHTCPU;
	if ((type.compare("6") == 0) || (type.compare("PATHCPU") == 0))
		return PATHCPU;
	if ((type.compare("7") == 0) || (type.compare("BIDIRCPU") == 0))
		return BIDIRCPU;
	if ((type.compare("8") == 0) || (type.compare("BIDIRHYBRID") == 0))
		return BIDIRHYBRID;
	if ((type.compare("9") == 0) || (type.compare("CBIDIRHYBRID") == 0))
		return CBIDIRHYBRID;
	if ((type.compare("10") == 0) || (type.compare("BIDIRVMCPU") == 0))
		return BIDIRVMCPU;
	throw runtime_error("Unknown render engine type: " + type);
}

const string RenderEngine::RenderEngineType2String(const RenderEngineType type) {
	switch (type) {
		case PATHOCL:
			return "PATHOCL";
		case LIGHTCPU:
			return "LIGHTCPU";
		case PATHCPU:
			return "PATHCPU";
		case BIDIRCPU:
			return "BIDIRCPU";
		case BIDIRHYBRID:
			return "BIDIRHYBRID";
		case CBIDIRHYBRID:
			return "CBIDIRHYBRID";
		case BIDIRVMCPU:
			return "BIDIRVMCPU";
		default:
			throw runtime_error("Unknown render engine type: " + type);
	}
}

RenderEngine *RenderEngine::AllocRenderEngine(const RenderEngineType engineType,
		RenderConfig *renderConfig, Film *film, boost::mutex *filmMutex) {
	switch (engineType) {
		case LIGHTCPU:
			return new LightCPURenderEngine(renderConfig, film, filmMutex);
		case PATHOCL:
#ifndef LUXRAYS_DISABLE_OPENCL
			return new PathOCLRenderEngine(renderConfig, film, filmMutex);
#else
			SLG_LOG("OpenCL unavailable, falling back to CPU rendering");
#endif
		case PATHCPU:
			return new PathCPURenderEngine(renderConfig, film, filmMutex);
		case BIDIRCPU:
			return new BiDirCPURenderEngine(renderConfig, film, filmMutex);
		case BIDIRHYBRID:
			return new BiDirHybridRenderEngine(renderConfig, film, filmMutex);
		case CBIDIRHYBRID:
			return new CBiDirHybridRenderEngine(renderConfig, film, filmMutex);
		case BIDIRVMCPU:
			return new BiDirVMCPURenderEngine(renderConfig, film, filmMutex);
		default:
			throw runtime_error("Unknown render engine type");
	}
}

//------------------------------------------------------------------------------
// CPURenderThread
//------------------------------------------------------------------------------

CPURenderThread::CPURenderThread(CPURenderEngine *engine,
		const u_int index, IntersectionDevice *dev,
		const bool enablePerPixelNormBuf, const bool enablePerScreenNormBuf) {
	threadIndex = index;
	renderEngine = engine;
	device = dev;

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
	const u_int filmWidth = renderEngine->film->GetWidth();
	const u_int filmHeight = renderEngine->film->GetHeight();

	delete threadFilm;
	threadFilm = new Film(filmWidth, filmHeight,
			enablePerPixelNormBuffer, enablePerScreenNormBuffer, false);
	threadFilm->CopyDynamicSettings(*(renderEngine->film));
	threadFilm->Init(filmWidth, filmHeight);

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

void CPURenderThread::BeginEdit() {
	StopRenderThread();
}

void CPURenderThread::EndEdit(const EditActionList &editActions) {
	StartRenderThread();
}

//------------------------------------------------------------------------------
// CPURenderEngine
//------------------------------------------------------------------------------

CPURenderEngine::CPURenderEngine(RenderConfig *cfg, Film *flm, boost::mutex *flmMutex) :
	RenderEngine(cfg, flm, flmMutex) {

	const size_t renderThreadCount =  cfg->cfg.GetInt("native.threads.count",
			boost::thread::hardware_concurrency());

	//--------------------------------------------------------------------------
	// Allocate devices
	//--------------------------------------------------------------------------

	vector<DeviceDescription *>  devDescs = ctx->GetAvailableDeviceDescriptions();
	DeviceDescription::Filter(DEVICE_TYPE_NATIVE_THREAD, devDescs);

	selectedDeviceDescs.resize(renderThreadCount, NULL);
	for (size_t i = 0; i < selectedDeviceDescs.size(); ++i)
		selectedDeviceDescs[i] = devDescs[i % devDescs.size()];

	intersectionDevices = ctx->AddIntersectionDevices(selectedDeviceDescs);

	for (size_t i = 0; i < intersectionDevices.size(); ++i) {
		// Disable the support for hybrid rendering in order to not waste resource
		intersectionDevices[i]->SetDataParallelSupport(false);
	}

	// Set the LuxRays SataSet
	ctx->SetDataSet(renderConfig->scene->dataSet);

	//--------------------------------------------------------------------------
	// Setup render threads array
	//--------------------------------------------------------------------------

	SLG_LOG("Configuring "<< renderThreadCount << " CPU render threads");
	renderThreads.resize(renderThreadCount, NULL);
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
	for (size_t i = 0; i < renderThreads.size(); ++i) {
		if (!renderThreads[i])
			renderThreads[i] = NewRenderThread(i, intersectionDevices[i]);
		renderThreads[i]->Start();
	}
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
		if (renderThreads[i] && renderThreads[i]->threadFilm)
			film->AddFilm(*(renderThreads[i]->threadFilm));
	}
}

void CPURenderEngine::UpdateCounters() {
	// Update the sample count statistic
	samplesCount = film->GetTotalSampleCount();

	// Update the ray count statistic
	double totalCount = 0.0;
	for (size_t i = 0; i < renderThreads.size(); ++i)
		totalCount += renderThreads[i]->device->GetTotalRaysCount();
	raysCount = totalCount;
}

//------------------------------------------------------------------------------
// OCLRenderEngine
//------------------------------------------------------------------------------

OCLRenderEngine::OCLRenderEngine(RenderConfig *rcfg, Film *flm,
	boost::mutex *flmMutex, bool fatal) : RenderEngine(rcfg, flm, flmMutex) {
	const Properties &cfg = renderConfig->cfg;

	const bool useCPUs = (cfg.GetInt("opencl.cpu.use", 1) != 0);
	const bool useGPUs = (cfg.GetInt("opencl.gpu.use", 1) != 0);
	forceGPUWorkSize = cfg.GetInt("opencl.gpu.workgroup.size", 64);
	forceCPUWorkSize = cfg.GetInt("opencl.cpu.workgroup.size", 1);
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
		OpenCLDeviceDescription *desc = static_cast<OpenCLDeviceDescription *>(descs[i]);

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

	if (fatal && selectedDeviceDescs.size() == 0)
		throw runtime_error("No OpenCL device selected or available");
}

//------------------------------------------------------------------------------
// HybridRenderState
//------------------------------------------------------------------------------

HybridRenderState::HybridRenderState(HybridRenderThread *rendeThread,
		Film *film, RandomGenerator *rndGen) {
	// Setup the sampler
	sampler = rendeThread->renderEngine->renderConfig->AllocSampler(rndGen, film,
			&rendeThread->metropolisSharedTotalLuminance,
			&rendeThread->metropolisSharedSampleCount);
}

HybridRenderState::~HybridRenderState() {
	delete sampler;
}

//------------------------------------------------------------------------------
// HybridRenderThread
//------------------------------------------------------------------------------

HybridRenderThread::HybridRenderThread(HybridRenderEngine *re,
		const u_int index, IntersectionDevice *dev) {
	device = dev;

	renderThread = NULL;
	threadFilm = NULL;

	threadIndex = index;
	renderEngine = re;

	samplesCount = 0.0;

	pendingRayBuffers = 0;
	currentRayBufferToSend = NULL;
	currentReiceivedRayBuffer = NULL;

	started = false;
	editMode = false;
}

HybridRenderThread::~HybridRenderThread() {
	if (editMode)
		EndEdit(EditActionList());
	if (started)
		Stop();

	delete threadFilm;
}

void HybridRenderThread::Start() {
	started = true;
	StartRenderThread();
}

void HybridRenderThread::Interrupt() {
	if (renderThread)
		renderThread->interrupt();
}

void HybridRenderThread::Stop() {
	StopRenderThread();
	started = false;
}

void HybridRenderThread::StartRenderThread() {
	const u_int filmWidth = renderEngine->film->GetWidth();
	const u_int filmHeight = renderEngine->film->GetHeight();

	delete threadFilm;
	threadFilm = new Film(filmWidth, filmHeight, true, true, false);
	threadFilm->CopyDynamicSettings(*(renderEngine->film));
	threadFilm->Init(filmWidth, filmHeight);

	samplesCount = 0.0;

	// Create the thread for the rendering
	renderThread = AllocRenderThread();
}

void HybridRenderThread::StopRenderThread() {
	if (renderThread) {
		renderThread->interrupt();
		renderThread->join();
		delete renderThread;
		renderThread = NULL;
	}
}

void HybridRenderThread::BeginEdit() {
	StopRenderThread();
}

void HybridRenderThread::EndEdit(const EditActionList &editActions) {
	// Reset statistics in order to be more accurate
	device->ResetPerformaceStats();

	StartRenderThread();
}

size_t HybridRenderThread::PushRay(const Ray &ray) {
	// Check if I have a valid currentRayBuffer
	if (!currentRayBufferToSend) {
		if (freeRayBuffers.size() == 0) {
			// I have to allocate a new RayBuffer
			currentRayBufferToSend = device->NewRayBuffer();
		} else {
			// I can reuse one in queue
			currentRayBufferToSend = freeRayBuffers.front();
			freeRayBuffers.pop_front();
		}
	}

	const size_t index = currentRayBufferToSend->AddRay(ray);

	// Check if the buffer is now full
	if (currentRayBufferToSend->IsFull()) {
		// Send the work to the device
		device->PushRayBuffer(currentRayBufferToSend);
		currentRayBufferToSend = NULL;
		++pendingRayBuffers;
	}

	return index;
}

void HybridRenderThread::PopRay(const Ray **ray, const RayHit **rayHit) {
	// Check if I have to get  the results out of intersection device
	if (!currentReiceivedRayBuffer) {
		currentReiceivedRayBuffer = device->PopRayBuffer();
		--pendingRayBuffers;
		currentReiceivedRayBufferIndex = 0;
	} else if (currentReiceivedRayBufferIndex >= currentReiceivedRayBuffer->GetSize()) {
		// All the results in the RayBuffer has been elaborated
		currentReiceivedRayBuffer->Reset();
		freeRayBuffers.push_back(currentReiceivedRayBuffer);

		// Get a new buffer
		currentReiceivedRayBuffer = device->PopRayBuffer();
		--pendingRayBuffers;
		currentReiceivedRayBufferIndex = 0;
	}

	*ray = currentReiceivedRayBuffer->GetRay(currentReiceivedRayBufferIndex);
	*rayHit = currentReiceivedRayBuffer->GetRayHit(currentReiceivedRayBufferIndex++);
}

void HybridRenderThread::RenderFunc() {
	//SLG_LOG("[HybridRenderThread::" << threadIndex << "] Rendering thread started");
	boost::this_thread::disable_interruption di;

	Film *film = threadFilm;
	const u_int filmWidth = film->GetWidth();
	const u_int filmHeight = film->GetHeight();
	pixelCount = filmWidth * filmHeight;

	RandomGenerator *rndGen = new RandomGenerator(threadIndex + renderEngine->seedBase);

	const u_int incrementStep = 4096;
	vector<HybridRenderState *> states(incrementStep);

	try {
		// Initialize the first states
		for (u_int i = 0; i < states.size(); ++i)
			states[i] = AllocRenderState(rndGen);

		u_int generateIndex = 0;
		u_int collectIndex = 0;
		while (!boost::this_thread::interruption_requested()) {
			// Generate new rays up to the point to have 3 pending buffers
			while (pendingRayBuffers < 3) {
				states[generateIndex]->GenerateRays(this);

				generateIndex = (generateIndex + 1) % states.size();
				if (generateIndex == collectIndex) {
					//SLG_LOG("[HybridRenderThread::" << threadIndex << "] Increasing states size by " << incrementStep);
					//SLG_LOG("[HybridRenderThread::" << threadIndex << "] State size: " << states.size());

					// Insert a set of new states and continue
					states.insert(states.begin() + generateIndex, incrementStep, NULL);
					for (u_int i = generateIndex; i < generateIndex + incrementStep; ++i)
						states[i] = AllocRenderState(rndGen);
					collectIndex += incrementStep;
				}
			}

			//SLG_LOG("[HybridRenderThread::" << threadIndex << "] State size: " << states.size());
			//SLG_LOG("[HybridRenderThread::" << threadIndex << "] generateIndex: " << generateIndex);
			//SLG_LOG("[HybridRenderThread::" << threadIndex << "] collectIndex: " << collectIndex);
			//SLG_LOG("[HybridRenderThread::" << threadIndex << "] pendingRayBuffers: " << pendingRayBuffers);

			// Collect rays up to the point to have only 1 pending buffer
			while (pendingRayBuffers > 1) {
				samplesCount += states[collectIndex]->CollectResults(this);

				const u_int newCollectIndex = (collectIndex + 1) % states.size();
				// A safety-check, it should never happen
				if (newCollectIndex == generateIndex)
					break;
				collectIndex = newCollectIndex;
			}
		}

		//SLG_LOG("[HybridRenderThread::" << threadIndex << "] Rendering thread halted");
	} catch (boost::thread_interrupted) {
		SLG_LOG("[HybridRenderThread::" << threadIndex << "] Rendering thread halted");
	}
#ifndef LUXRAYS_DISABLE_OPENCL
	catch (cl::Error err) {
		SLG_LOG("[HybridRenderThread::" << threadIndex << "] Rendering thread ERROR: " << err.what() <<
				"(" << luxrays::utils::oclErrorString(err.err()) << ")");
	}
#endif

	// Clean current ray buffers
	if (currentRayBufferToSend) {
		currentRayBufferToSend->Reset();
		freeRayBuffers.push_back(currentRayBufferToSend);
		currentRayBufferToSend = NULL;
	}
	if (currentReiceivedRayBuffer) {
		currentReiceivedRayBuffer->Reset();
		freeRayBuffers.push_back(currentReiceivedRayBuffer);
		currentReiceivedRayBuffer = NULL;
	}

	// Free all states
	for (u_int i = 0; i < states.size(); ++i)
		delete states[i];
	delete rndGen;

	// Remove all pending ray buffers
	while (pendingRayBuffers > 0) {
		RayBuffer *rayBuffer = device->PopRayBuffer();
		--(pendingRayBuffers);
		rayBuffer->Reset();
		freeRayBuffers.push_back(rayBuffer);
	}
}

//------------------------------------------------------------------------------
// HybridRenderEngine
//------------------------------------------------------------------------------

HybridRenderEngine::HybridRenderEngine(RenderConfig *rcfg, Film *flm,
	boost::mutex *flmMutex) : OCLRenderEngine(rcfg, flm, flmMutex, false) {
	//--------------------------------------------------------------------------
	// Create the intersection devices and render threads
	//--------------------------------------------------------------------------

	if (selectedDeviceDescs.empty()) {
		SLG_LOG("No OpenCL device found, falling back to CPU rendering");
		selectedDeviceDescs = ctx->GetAvailableDeviceDescriptions();
		DeviceDescription::Filter(DEVICE_TYPE_NATIVE_THREAD,
			selectedDeviceDescs);
		if (selectedDeviceDescs.empty())
			throw runtime_error("No native CPU device found");
	}
	const size_t renderThreadCount = boost::thread::hardware_concurrency();
	if (selectedDeviceDescs.size() == 1) {
		// Only one intersection device, use a M2O device
		intersectionDevices = ctx->AddVirtualM2OIntersectionDevices(renderThreadCount, selectedDeviceDescs);
	} else {
		// Multiple intersection devices, use a M2M device
		intersectionDevices = ctx->AddVirtualM2MIntersectionDevices(renderThreadCount, selectedDeviceDescs);
	}
	devices = ctx->GetIntersectionDevices();

	// Set the LuxRays DataSet
	ctx->SetDataSet(renderConfig->scene->dataSet);

	SLG_LOG("Starting "<< renderThreadCount << " BiDir hybrid render threads");
	renderThreads.resize(renderThreadCount, NULL);
}

void HybridRenderEngine::StartLockLess() {
	for (size_t i = 0; i < renderThreads.size(); ++i) {
		if (!renderThreads[i])
			renderThreads[i] = NewRenderThread(i, devices[i]);
		renderThreads[i]->Start();
	}
}

void HybridRenderEngine::StopLockLess() {
	for (size_t i = 0; i < renderThreads.size(); ++i)
		renderThreads[i]->Interrupt();
	for (size_t i = 0; i < renderThreads.size(); ++i)
		renderThreads[i]->Stop();
}

void HybridRenderEngine::BeginEditLockLess() {
	for (size_t i = 0; i < renderThreads.size(); ++i)
		renderThreads[i]->Interrupt();
	for (size_t i = 0; i < renderThreads.size(); ++i)
		renderThreads[i]->BeginEdit();
}

void HybridRenderEngine::EndEditLockLess(const EditActionList &editActions) {
	for (size_t i = 0; i < renderThreads.size(); ++i)
		renderThreads[i]->EndEdit(editActions);
}

void HybridRenderEngine::UpdateFilmLockLess() {
	boost::unique_lock<boost::mutex> lock(*filmMutex);

	film->Reset();

	// Merge the all thread films
	for (size_t i = 0; i < renderThreads.size(); ++i) {
		if (renderThreads[i]->threadFilm)
			film->AddFilm(*(renderThreads[i]->threadFilm));
	}
}

void HybridRenderEngine::UpdateCounters() {
	// Update the sample count statistic
	double totalCount = 0.0;
	for (size_t i = 0; i < renderThreads.size(); ++i)
		totalCount += renderThreads[i]->samplesCount;
	samplesCount = totalCount;

	// Update the ray count statistic
	totalCount = 0.0;
	for (size_t i = 0; i < renderThreads.size(); ++i)
		totalCount += renderThreads[i]->device->GetTotalRaysCount();
	raysCount = totalCount;
}

}
