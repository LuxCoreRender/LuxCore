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

#include <boost/format.hpp>

#include "slg/renderengine.h"
#include "slg/renderconfig.h"
#include "slg/sdl/bsdf.h"

#include "luxrays/core/intersectiondevice.h"
#if !defined(LUXRAYS_DISABLE_OPENCL)
#include "luxrays/core/ocldevice.h"
#endif

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// RenderEngine
//------------------------------------------------------------------------------

RenderEngine::RenderEngine(const RenderConfig *cfg, Film *flm, boost::mutex *flmMutex) :
	seedBaseGenerator(131) {
	renderConfig = cfg;
	film = flm;
	filmMutex = flmMutex;
	started = false;
	editMode = false;
	GenerateNewSeed();

	film->AddChannel(Film::TONEMAPPED_FRAMEBUFFER);

	// Create LuxRays context
	const int oclPlatformIndex = renderConfig->cfg.GetInt("opencl.platform.index", -1);
	ctx = new Context(LuxRays_DebugHandler ? LuxRays_DebugHandler : NullDebugHandler, oclPlatformIndex);

	renderConfig->scene->Preprocess(ctx, film->GetWidth(), film->GetHeight());

	samplesCount = 0;
	elapsedTime = 0.0f;
}

RenderEngine::~RenderEngine() {
	if (editMode)
		EndSceneEdit(EditActionList());
	if (started)
		Stop();

	delete ctx;
}

void RenderEngine::Start() {
	boost::unique_lock<boost::mutex> lock(engineMutex);

	// A safety check for stereo support
	if (renderConfig->scene->camera->IsHorizontalStereoEnabled() && !IsHorizontalStereoSupported())
		throw runtime_error("Horizontal stereo is not supported by " + RenderEngineType2String(GetEngineType()) + " render engine");

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

void RenderEngine::BeginSceneEdit() {
	boost::unique_lock<boost::mutex> lock(engineMutex);

	assert (started);
	assert (!editMode);
	editMode = true;

	BeginSceneEditLockLess();
}

void RenderEngine::EndSceneEdit(const EditActionList &editActions) {
	boost::unique_lock<boost::mutex> lock(engineMutex);

	assert (started);
	assert (editMode);

	// Check if I have to stop the LuxRays Context
	bool contextStopped;
	if (editActions.Has(GEOMETRY_EDIT) ||
			(editActions.Has(INSTANCE_TRANS_EDIT) &&
			!renderConfig->scene->dataSet->DoesAllAcceleratorsSupportUpdate())) {
		// Stop all intersection devices
		ctx->Stop();

		// To avoid reference to the DataSet de-allocated inside UpdateDataSet()
		ctx->SetDataSet(NULL);
		
		contextStopped = true;
	} else
		contextStopped = false;

	// Pre-process scene data
	renderConfig->scene->Preprocess(ctx, film->GetWidth(), film->GetHeight());

	if (contextStopped) {
		// Set the LuxRays SataSet
		ctx->SetDataSet(renderConfig->scene->dataSet);

		// Restart all intersection devices
		ctx->Start();
	} else if (renderConfig->scene->dataSet->DoesAllAcceleratorsSupportUpdate() &&
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

	EndSceneEditLockLess(editActions);
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
	if ((type.compare("11") == 0) || (type.compare("FILESAVER") == 0))
		return FILESAVER;
	if ((type.compare("12") == 0) || (type.compare("RTPATHOCL") == 0))
		return RTPATHOCL;
	if ((type.compare("13") == 0) || (type.compare("PATHHYBRID") == 0))
		return PATHHYBRID;
	if ((type.compare("14") == 0) || (type.compare("BIASPATHCPU") == 0))
		return BIASPATHCPU;
	if ((type.compare("15") == 0) || (type.compare("BIASPATHOCL") == 0))
		return BIASPATHOCL;
	if ((type.compare("16") == 0) || (type.compare("RTBIASPATHOCL") == 0))
		return RTBIASPATHOCL;
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
		case FILESAVER:
			return "FILESAVER";
		case RTPATHOCL:
			return "RTPATHOCL";
		case PATHHYBRID:
			return "PATHHYBRID";
		case BIASPATHCPU:
			return "BIASPATHCPU";
		case BIASPATHOCL:
			return "BIASPATHOCL";
		case RTBIASPATHOCL:
			return "RTBIASPATHOCL";
		default:
			throw runtime_error("Unknown render engine type: " + boost::lexical_cast<std::string>(type));
	}
}

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

//------------------------------------------------------------------------------
// CPURenderEngine
//------------------------------------------------------------------------------

CPURenderEngine::CPURenderEngine(const RenderConfig *cfg, Film *flm, boost::mutex *flmMutex) :
	RenderEngine(cfg, flm, flmMutex) {
	const size_t renderThreadCount =  cfg->cfg.GetInt("native.threads.count",
			boost::thread::hardware_concurrency());

	//--------------------------------------------------------------------------
	// Allocate devices
	//--------------------------------------------------------------------------

	vector<DeviceDescription *>  devDescs = ctx->GetAvailableDeviceDescriptions();
	DeviceDescription::Filter(DEVICE_TYPE_NATIVE_THREAD, devDescs);
	devDescs.resize(1);

	selectedDeviceDescs.resize(renderThreadCount, devDescs[0]);
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
	for (size_t i = 0; i < renderThreads.size(); ++i)
		renderThreads[i]->Interrupt();
	for (size_t i = 0; i < renderThreads.size(); ++i)
		renderThreads[i]->Stop();
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

//------------------------------------------------------------------------------
// CPUNoTileRenderThread
//------------------------------------------------------------------------------

CPUNoTileRenderThread::CPUNoTileRenderThread(CPUNoTileRenderEngine *engine,
		const u_int index, IntersectionDevice *dev) : CPURenderThread(engine, index, dev) {
	threadFilm = NULL;
}

CPUNoTileRenderThread::~CPUNoTileRenderThread() {
	delete threadFilm;
}

void CPUNoTileRenderThread::StartRenderThread() {
	CPUNoTileRenderEngine *cpuNoTileEngine = (CPUNoTileRenderEngine *)renderEngine;

	const u_int filmWidth = cpuNoTileEngine->film->GetWidth();
	const u_int filmHeight = cpuNoTileEngine->film->GetHeight();

	delete threadFilm;

	threadFilm = new Film(filmWidth, filmHeight);
	threadFilm->CopyDynamicSettings(*(cpuNoTileEngine->film));
	threadFilm->RemoveChannel(Film::TONEMAPPED_FRAMEBUFFER);
	threadFilm->Init();

	CPURenderThread::StartRenderThread();
}

//------------------------------------------------------------------------------
// CPUNoTileRenderEngine
//------------------------------------------------------------------------------

CPUNoTileRenderEngine::CPUNoTileRenderEngine(const RenderConfig *cfg, Film *flm, boost::mutex *flmMutex) :
	CPURenderEngine(cfg, flm, flmMutex) {
}

CPUNoTileRenderEngine::~CPUNoTileRenderEngine() {
}

void CPUNoTileRenderEngine::UpdateFilmLockLess() {
	boost::unique_lock<boost::mutex> lock(*filmMutex);

	film->Reset();

	// Merge the all thread films
	for (size_t i = 0; i < renderThreads.size(); ++i) {
		if (!renderThreads[i])
			continue;

		const Film *threadFilm = ((CPUNoTileRenderThread *)renderThreads[i])->threadFilm;
		if (threadFilm)
			film->AddFilm(*threadFilm);
	}
}

void CPUNoTileRenderEngine::UpdateCounters() {
	elapsedTime = WallClockTime() - startTime;

	// Update the sample count statistic
	samplesCount = film->GetTotalSampleCount();

	// Update the ray count statistic
	double totalCount = 0.0;
	for (size_t i = 0; i < renderThreads.size(); ++i) {
		const CPUNoTileRenderThread *thread = (CPUNoTileRenderThread *)renderThreads[i];
		totalCount += thread->device->GetTotalRaysCount();
	}
	raysCount = totalCount;
}

//------------------------------------------------------------------------------
// TileRepository
//------------------------------------------------------------------------------

TileRepository::TileRepository(const u_int size) {
	tileSize = size;

	enableProgressiveRefinement = false;
	enableMultipassRendering = false;
	done = false;
}

TileRepository::~TileRepository() {
	Clear();
}

void TileRepository::Clear() {
	// Free all left tiles
	BOOST_FOREACH(Tile *tile, todoTiles) {
		delete tile;
	}
	todoTiles.clear();

	BOOST_FOREACH(Tile *tile, pendingTiles) {
		delete tile;
	}
	pendingTiles.clear();
}

void TileRepository::GetPendingTiles(vector<Tile> &tiles) {
	boost::unique_lock<boost::mutex> lock(tileMutex);

	tiles.resize(pendingTiles.size());
	for (u_int i = 0; i < pendingTiles.size(); ++i)
		tiles[i] = *(pendingTiles[i]);
}

void TileRepository::HilberCurveTiles(const int sampleIndex,
		const u_int n, const int xo, const int yo,
		const int xd, const int yd, const int xp, const int yp,
		const int xEnd, const int yEnd) {
	if (n <= 1) {
		if((xo < xEnd) && (yo < yEnd)) {
			Tile *tile = new Tile;
			tile->xStart = xo;
			tile->yStart = yo;
			tile->sampleIndex = sampleIndex;
			todoTiles.push_back(tile);
		}
	} else {
		const u_int n2 = n >> 1;

		HilberCurveTiles(sampleIndex, n2,
			xo,
			yo,
			xp, yp, xd, yd, xEnd, yEnd);
		HilberCurveTiles(sampleIndex, n2,
			xo + xd * static_cast<int>(n2),
			yo + yd * static_cast<int>(n2),
			xd, yd, xp, yp, xEnd, yEnd);
		HilberCurveTiles(sampleIndex, n2,
			xo + (xp + xd) * static_cast<int>(n2),
			yo + (yp + yd) * static_cast<int>(n2),
			xd, yd, xp, yp, xEnd, yEnd);
		HilberCurveTiles(sampleIndex, n2,
			xo + xd * static_cast<int>(n2 - 1) + xp * static_cast<int>(n - 1),
			yo + yd * static_cast<int>(n2 - 1) + yp * static_cast<int>(n - 1),
			-xp, -yp, -xd, -yd, xEnd, yEnd);
	}	
}

void TileRepository::InitTiles(const u_int width, const u_int height) {
	u_int n = RoundUp(Max(width, height), tileSize) / tileSize;
	if (!IsPowerOf2(n))
		n = RoundUpPow2(n);
	if (enableProgressiveRefinement) {
		for (u_int i = 0; i < totalSamplesPerPixel; ++i)
			HilberCurveTiles(i, n, 0, 0, 0, tileSize, tileSize, 0, width, height);
	} else
		HilberCurveTiles(-1, n, 0, 0, 0, tileSize, tileSize, 0, width, height);

	done = false;
}

const bool TileRepository::NextTile(Tile **tile, const u_int width, const u_int height) {
	boost::unique_lock<boost::mutex> lock(tileMutex);

	if (*tile) {
		// Remove the tile from pending list
		pendingTiles.erase(std::remove(pendingTiles.begin(), pendingTiles.end(), *tile), pendingTiles.end());

		// Now I can free the memory
		delete *tile;
	}

	if (todoTiles.size() == 0) {
		// Check if multi-pass is enabled
		if (enableMultipassRendering)
			InitTiles(width, height);
		else {
			if (pendingTiles.size() == 0) {
				// Rendering done
				done = true;
			}

			return false;
		}
	}

	*tile = todoTiles.front();
	todoTiles.pop_front();
	pendingTiles.push_back(*tile);

	return true;
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
	tileFilm = new Film(cpuTileEngine->tileRepository->tileSize, cpuTileEngine->tileRepository->tileSize);
	tileFilm->CopyDynamicSettings(*(cpuTileEngine->film));
	tileFilm->Init();

	CPURenderThread::StartRenderThread();
}

//------------------------------------------------------------------------------
// CPUTileRenderEngine
//------------------------------------------------------------------------------

CPUTileRenderEngine::CPUTileRenderEngine(const RenderConfig *cfg, Film *flm, boost::mutex *flmMutex) :
	CPURenderEngine(cfg, flm, flmMutex) {
	tileRepository = NULL;
}

CPUTileRenderEngine::~CPUTileRenderEngine() {
	delete tileRepository;
}

const bool CPUTileRenderEngine::NextTile(TileRepository::Tile **tile, const Film *tileFilm) {
	// Check if I have to add the tile to the film
	if (*tile) {
		boost::unique_lock<boost::mutex> lock(*filmMutex);

		film->AddFilm(*tileFilm,
				0, 0,
				Min(tileRepository->tileSize, film->GetWidth() - (*tile)->xStart),
				Min(tileRepository->tileSize, film->GetHeight() - (*tile)->yStart),
				(*tile)->xStart, (*tile)->yStart);
	}

	if (!tileRepository->NextTile(tile, film->GetWidth(), film->GetHeight())) {
		boost::unique_lock<boost::mutex> lock(engineMutex);

		if (!printedRenderingTime && tileRepository->done) {
			elapsedTime = WallClockTime() - startTime;
			SLG_LOG(boost::format("Rendering time: %.2f secs") % elapsedTime);
			printedRenderingTime = true;
		}

		return false;
	} else
		return true;
}

void CPUTileRenderEngine::StopLockLess() {
	CPURenderEngine::StopLockLess();

	delete tileRepository;
	tileRepository = NULL;
}

void CPUTileRenderEngine::EndSceneEditLockLess(const EditActionList &editActions) {
	tileRepository->Clear();
	tileRepository->InitTiles(film->GetWidth(), film->GetHeight());
	printedRenderingTime = false;

	CPURenderEngine::EndSceneEditLockLess(editActions);
}

void CPUTileRenderEngine::UpdateCounters() {
	// Update the sample count statistic
	samplesCount = film->GetTotalSampleCount();

	// Update the ray count statistic
	double totalCount = 0.0;
	for (size_t i = 0; i < renderThreads.size(); ++i) {
		const CPUTileRenderThread *thread = (CPUTileRenderThread *)renderThreads[i];
		totalCount += thread->device->GetTotalRaysCount();
	}
	raysCount = totalCount;

	// Update the time only until when the rendering is not finished
	if (!tileRepository->done)
		elapsedTime = WallClockTime() - startTime;
}

//------------------------------------------------------------------------------
// OCLRenderEngine
//------------------------------------------------------------------------------

OCLRenderEngine::OCLRenderEngine(const RenderConfig *rcfg, Film *flm,
	boost::mutex *flmMutex, bool fatal) : RenderEngine(rcfg, flm, flmMutex) {
#if !defined(LUXRAYS_DISABLE_OPENCL)
	const Properties &cfg = renderConfig->cfg;

	const bool useCPUs = (cfg.GetInt("opencl.cpu.use", 1) != 0);
	const bool useGPUs = (cfg.GetInt("opencl.gpu.use", 1) != 0);
	// 0 means use the value suggested by the OpenCL driver
	const u_int forceGPUWorkSize = cfg.GetInt("opencl.gpu.workgroup.size", 0);
	// 0 means use the value suggested by the OpenCL driver
#if defined(__APPLE__)	
	const u_int forceCPUWorkSize = cfg.GetInt("opencl.cpu.workgroup.size", 1);
#else
	const u_int forceCPUWorkSize = cfg.GetInt("opencl.cpu.workgroup.size", 0);
#endif
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
				if (desc->GetType() & DEVICE_TYPE_OPENCL_GPU)
					desc->SetForceWorkGroupSize(forceGPUWorkSize);
				else if (desc->GetType() & DEVICE_TYPE_OPENCL_CPU)
					desc->SetForceWorkGroupSize(forceCPUWorkSize);
				selectedDeviceDescs.push_back(desc);
			}
		} else {
			if ((useCPUs && desc->GetType() & DEVICE_TYPE_OPENCL_CPU) ||
					(useGPUs && desc->GetType() & DEVICE_TYPE_OPENCL_GPU)) {
				if (desc->GetType() & DEVICE_TYPE_OPENCL_GPU)
					desc->SetForceWorkGroupSize(forceGPUWorkSize);
				else if (desc->GetType() & DEVICE_TYPE_OPENCL_CPU)
					desc->SetForceWorkGroupSize(forceCPUWorkSize);
				selectedDeviceDescs.push_back(descs[i]);
			}
		}
	}
#endif
	if (fatal && selectedDeviceDescs.size() == 0)
		throw runtime_error("No OpenCL device selected or available");
}

size_t OCLRenderEngine::GetQBVHEstimatedStackSize(const luxrays::DataSet &dataSet) {
	if (dataSet.GetTotalTriangleCount() < 250000)
		return 24;
	else if (dataSet.GetTotalTriangleCount() < 500000)
		return 32;
	else if (dataSet.GetTotalTriangleCount() < 1000000)
		return 40;
	else if (dataSet.GetTotalTriangleCount() < 2000000)
		return 48;
	else 
		return 64;
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
		EndSceneEdit(EditActionList());
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
	threadFilm = new Film(filmWidth, filmHeight);
	threadFilm->CopyDynamicSettings(*(renderEngine->film));
	threadFilm->Init();

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

void HybridRenderThread::BeginSceneEdit() {
	StopRenderThread();
}

void HybridRenderThread::EndSceneEdit(const EditActionList &editActions) {
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
		device->PushRayBuffer(currentRayBufferToSend, threadIndex);
		currentRayBufferToSend = NULL;
		++pendingRayBuffers;
	}

	return index;
}

void HybridRenderThread::PopRay(const Ray **ray, const RayHit **rayHit) {
	// Check if I have to get  the results out of intersection device
	if (!currentReiceivedRayBuffer) {
		currentReiceivedRayBuffer = device->PopRayBuffer(threadIndex);
		--pendingRayBuffers;
		currentReiceivedRayBufferIndex = 0;
	} else if (currentReiceivedRayBufferIndex >= currentReiceivedRayBuffer->GetSize()) {
		// All the results in the RayBuffer has been elaborated
		currentReiceivedRayBuffer->Reset();
		freeRayBuffers.push_back(currentReiceivedRayBuffer);

		// Get a new buffer
		currentReiceivedRayBuffer = device->PopRayBuffer(threadIndex);
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
				"(" << oclErrorString(err.err()) << ")");
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
		RayBuffer *rayBuffer = device->PopRayBuffer(threadIndex);
		--(pendingRayBuffers);
		rayBuffer->Reset();
		freeRayBuffers.push_back(rayBuffer);
	}
}

//------------------------------------------------------------------------------
// HybridRenderEngine
//------------------------------------------------------------------------------

HybridRenderEngine::HybridRenderEngine(const RenderConfig *rcfg, Film *flm,
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
	const unsigned int renderThreadCount = boost::thread::hardware_concurrency();
	if (selectedDeviceDescs.size() == 1) {
		// Only one intersection device, use directly the device
		ctx->AddIntersectionDevices(selectedDeviceDescs);
	} else {
		// Multiple intersection devices, use a virtual device
		ctx->AddVirtualIntersectionDevice(selectedDeviceDescs);
	}
	intersectionDevices.push_back(ctx->GetIntersectionDevices()[0]);
	intersectionDevices[0]->SetQueueCount(renderThreadCount);

	// Check if I have to set max. QBVH stack size
	const bool enableImageStorage = renderConfig->cfg.GetBoolean("accelerator.imagestorage.enable", true);
	const size_t qbvhStackSize = renderConfig->cfg.GetInt("accelerator.qbvh.stacksize.max",
			OCLRenderEngine::GetQBVHEstimatedStackSize(*(renderConfig->scene->dataSet)));
	for (size_t i = 0; i < intersectionDevices.size(); ++i) {
		intersectionDevices[i]->SetEnableImageStorage(enableImageStorage);
		intersectionDevices[i]->SetMaxStackSize(qbvhStackSize);
	}

	// Set the LuxRays DataSet
	ctx->SetDataSet(renderConfig->scene->dataSet);

	SLG_LOG("Starting "<< renderThreadCount << " Hybrid render threads");
	renderThreads.resize(renderThreadCount, NULL);
}

void HybridRenderEngine::StartLockLess() {
	for (size_t i = 0; i < renderThreads.size(); ++i) {
		if (!renderThreads[i])
			renderThreads[i] = NewRenderThread(i, intersectionDevices[0]);
		renderThreads[i]->Start();
	}
}

void HybridRenderEngine::StopLockLess() {
	for (size_t i = 0; i < renderThreads.size(); ++i)
		renderThreads[i]->Interrupt();
	for (size_t i = 0; i < renderThreads.size(); ++i)
		renderThreads[i]->Stop();
}

void HybridRenderEngine::BeginSceneEditLockLess() {
	for (size_t i = 0; i < renderThreads.size(); ++i)
		renderThreads[i]->Interrupt();
	for (size_t i = 0; i < renderThreads.size(); ++i)
		renderThreads[i]->BeginSceneEdit();
}

void HybridRenderEngine::EndSceneEditLockLess(const EditActionList &editActions) {
	// Reset statistics in order to be more accurate
	intersectionDevices[0]->ResetPerformaceStats();

	for (size_t i = 0; i < renderThreads.size(); ++i)
		renderThreads[i]->EndSceneEdit(editActions);
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
	elapsedTime = WallClockTime() - startTime;

	// Update the sample count statistic
	double totalCount = 0.0;
	for (size_t i = 0; i < renderThreads.size(); ++i)
		totalCount += renderThreads[i]->samplesCount;
	samplesCount = totalCount;

	// Update the ray count statistic
	raysCount = intersectionDevices[0]->GetTotalRaysCount();
}
