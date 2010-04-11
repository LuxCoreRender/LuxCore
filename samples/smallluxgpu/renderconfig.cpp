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

#include "renderconfig.h"

string SLG_LABEL = "SmallLuxGPU v" SLG_VERSION_MAJOR "." SLG_VERSION_MINOR " (LuxRays demo: http://www.luxrender.net)";

RenderingConfig::RenderingConfig(const string &fileName) {
	cerr << "Reading configuration file: " << fileName << endl;
	cfg.LoadFile(fileName);

	cerr << "Configuration: " << endl;
	vector<string> keys = cfg.GetAllKeys();
	for (vector<string>::iterator i = keys.begin(); i != keys.end(); ++i)
		cerr << "  " << *i << " = " << cfg.GetString(*i, "") << endl;

	renderThreadsStarted = false;
}

RenderingConfig::~RenderingConfig() {
	StopAllRenderThreadsLockless();

	for (size_t i = 0; i < renderThreads.size(); ++i)
		delete renderThreads[i];

	delete ctx;
	delete scene;
	delete film;
}

void RenderingConfig::Init() {
	boost::unique_lock<boost::mutex> lock(cfgMutex);

	const bool lowLatency = cfg.GetInt("opencl.latency.mode", 1);
	const string sceneFileName = cfg.GetString("scene.file", "scenes/luxball/luxball.scn");
	const unsigned int w = cfg.GetInt("image.width", 640);
	const unsigned int h = cfg.GetInt("image.height", 480);
	const unsigned int nativeThreadCount = cfg.GetInt("opencl.nativethread.count", 2);
	const bool useCPUs = (cfg.GetInt("opencl.cpu.use", 0) == 1);
	const bool useGPUs = (cfg.GetInt("opencl.gpu.use", 1) == 1);
	const unsigned int forceGPUWorkSize = cfg.GetInt("opencl.gpu.workgroup.size", 64);
	const unsigned int filmType = cfg.GetInt("screen.type",3 );
	const vector<string> filmNames = cfg.GetStringVector("screen.file", "");
	const float gamma = cfg.GetFloat("screen.gamma", 2.2f);
	const unsigned int oclPlatformIndex = cfg.GetInt("opencl.platform.index", 0);
	const string oclDeviceConfig = cfg.GetString("opencl.devices.select", "");
	const unsigned int oclDeviceThreads = cfg.GetInt("opencl.renderthread.count", 0);
	luxrays::RAY_EPSILON = cfg.GetFloat("scene.epsilon", luxrays::RAY_EPSILON);

	screenRefreshInterval = cfg.GetInt("screen.refresh.interval", lowLatency ? 100 : 2000);

	captionBuffer[0] = '\0';

	// Create LuxRays context
	ctx = new Context(DebugHandler, oclPlatformIndex);

	// Create the scene
	switch (filmType) {
		case 0:
			cerr << "Film type: StandardFilm" << endl;
			film = new StandardFilm(lowLatency, w, h);
			break;
		case 1:
			cerr << "Film type: BluredStandardFilm" << endl;
			film = new BluredStandardFilm(lowLatency, w, h);
			break;
		case 2:
			cerr << "Film type: GaussianFilm" << endl;
			film = new GaussianFilm(lowLatency, w, h);
			break;
		case 3:
			cerr << "Film type: FastGaussianFilm" << endl;
			film = new FastGaussianFilm(lowLatency, w, h);
			break;
		default:
			throw runtime_error("Requested an unknown film type");
	}
	if (gamma != 2.2f)
		film->InitGammaTable(gamma);

	// Load film files
	if (filmNames.size() > 0) {
		for (size_t i = 0; i < filmNames.size(); ++i)
			film->AddFilm(filmNames[i]);
	}

	scene = new Scene(ctx, lowLatency, sceneFileName, film);

	scene->camera->fieldOfView = cfg.GetFloat("scene.fieldofview", 45.f);
	scene->camera->Update();
	scene->maxPathDepth = cfg.GetInt("path.maxdepth", 3);
	int strat = cfg.GetInt("path.lightstrategy", 0);
	if (strat == 0)
		scene->lightStrategy = ONE_UNIFORM;
	else
		scene->lightStrategy = ALL_UNIFORM;
	scene->shadowRayCount = cfg.GetInt("path.shadowrays", 1);

	// Russian Roulette parameters
	int rrStrat = cfg.GetInt("path.russianroulette.strategy", 1);
	if (rrStrat == 0)
		scene->rrStrategy = PROBABILITY;
	else
		scene->rrStrategy = IMPORTANCE;
	scene->rrDepth = cfg.GetInt("path.russianroulette.depth", scene->rrDepth);
	scene->rrProb = cfg.GetFloat("path.russianroulette.prob", scene->rrProb);
	scene->rrImportanceCap = cfg.GetFloat("path.russianroulette.cap", scene->rrImportanceCap);

	// Start OpenCL devices
	SetUpOpenCLDevices(lowLatency, useCPUs, useGPUs, forceGPUWorkSize, oclDeviceThreads, oclDeviceConfig);

	// Start Native threads
	SetUpNativeDevices(nativeThreadCount);

	const size_t deviceCount = intersectionCPUDevices.size()  + intersectionGPUDevices.size();
	if (deviceCount <= 0)
		throw runtime_error("Unable to find any appropiate IntersectionDevice");

	intersectionCPUGPUDevices.resize(deviceCount);
	if (intersectionGPUDevices.size() > 0)
		copy(intersectionGPUDevices.begin(), intersectionGPUDevices.end(),
				intersectionCPUGPUDevices.begin());
	if (intersectionCPUDevices.size() > 0)
		copy(intersectionCPUDevices.begin(), intersectionCPUDevices.end(),
				intersectionCPUGPUDevices.begin() + intersectionGPUDevices.size());

	// Set the Luxrays SataSet
	ctx->SetDataSet(scene->dataSet);

	intersectionAllDevices = ctx->GetIntersectionDevices();

	const unsigned long seedBase = (unsigned long)(WallClockTime() / 1000.0);

	// Create and start render threads
	size_t renderThreadCount = intersectionAllDevices.size();
	cerr << "Starting "<< renderThreadCount << " render threads" << endl;
	for (size_t i = 0; i < renderThreadCount; ++i) {
		if (intersectionAllDevices[i]->GetType() == DEVICE_TYPE_NATIVE_THREAD) {
			NativeRenderThread *t = new NativeRenderThread(i, seedBase, i / (float)renderThreadCount,
					(NativeThreadIntersectionDevice *)intersectionAllDevices[i], scene, lowLatency);
			renderThreads.push_back(t);
		} else {
			DeviceRenderThread *t = new DeviceRenderThread(i, seedBase, i / (float)renderThreadCount,
					intersectionAllDevices[i], scene, lowLatency);
			renderThreads.push_back(t);
		}
	}

	film->StartSampleTime();
	StartAllRenderThreadsLockless();
}

void RenderingConfig::ReInit(const bool reallocBuffers, const unsigned int w, unsigned int h) {
	boost::unique_lock<boost::mutex> lock(cfgMutex);

	bool wasRunning = renderThreadsStarted;
	// First stop all devices
	if (wasRunning)
		StopAllRenderThreadsLockless();

	// Check if I have to reallocate buffers
	if (reallocBuffers)
		film->Init(w, h);
	else
		film->Reset();
	scene->camera->Update();

	// Restart all devices
	if (wasRunning)
		StartAllRenderThreadsLockless();
}

void RenderingConfig::SetMaxPathDepth(const int delta) {
	boost::unique_lock<boost::mutex> lock(cfgMutex);

	bool wasRunning = renderThreadsStarted;
	// First stop all devices
	if (wasRunning)
		StopAllRenderThreadsLockless();

	film->Reset();
	scene->maxPathDepth = max<unsigned int>(2, scene->maxPathDepth + delta);

	// Restart all devices
	if (wasRunning)
		StartAllRenderThreadsLockless();
}

void RenderingConfig::SetShadowRays(const int delta) {
	boost::unique_lock<boost::mutex> lock(cfgMutex);

	bool wasRunning = renderThreadsStarted;
	// First stop all devices
	if (wasRunning)
		StopAllRenderThreadsLockless();

	scene->shadowRayCount = max<unsigned int>(1, scene->shadowRayCount + delta);
	for (size_t i = 0; i < renderThreads.size(); ++i)
		renderThreads[i]->ClearPaths();

	// Restart all devices
	if (wasRunning)
		StartAllRenderThreadsLockless();
}

void RenderingConfig::SetUpOpenCLDevices(const bool lowLatency, const bool useCPUs, const bool useGPUs,
	const unsigned int forceGPUWorkSize, const unsigned int oclDeviceThreads, const string &oclDeviceConfig) {

	std::vector<DeviceDescription *> descs = ctx->GetAvailableDeviceDescriptions();
	DeviceDescription::Filter(DEVICE_TYPE_OPENCL, descs);

	// Device info
	bool haveSelectionString = (oclDeviceConfig.length() > 0);
	if (haveSelectionString && (oclDeviceConfig.length() != descs.size())) {
		stringstream ss;
		ss << "OpenCL device selection string has the wrong length, must be " <<
				descs.size() << " instead of " << oclDeviceConfig.length();
		throw runtime_error(ss.str().c_str());
	}

	std::vector<DeviceDescription *> selectedDescs;
#if !defined(LUXRAYS_DISABLE_OPENCL)
	for (size_t i = 0; i < descs.size(); ++i) {
		OpenCLDeviceDescription *desc = (OpenCLDeviceDescription *)descs[i];

		if (haveSelectionString) {
			if (oclDeviceConfig.at(i) == '1') {
				if (desc->GetOpenCLType() == OCL_DEVICE_TYPE_GPU)
					desc->SetForceWorkGroupSize(forceGPUWorkSize);
				selectedDescs.push_back(desc);
			}
		} else {
			if ((useCPUs && desc->GetOpenCLType() == OCL_DEVICE_TYPE_CPU) ||
					(useGPUs && desc->GetOpenCLType() == OCL_DEVICE_TYPE_GPU)) {
				if (desc->GetOpenCLType() == OCL_DEVICE_TYPE_GPU)
					desc->SetForceWorkGroupSize(forceGPUWorkSize);
				selectedDescs.push_back(descs[i]);
			}
		}
	}
#endif

	if (selectedDescs.size() == 0)
		cerr << "No OpenCL device selected" << endl;
	else {
		// Allocate devices
		const size_t gpuRenderThreadCount = (oclDeviceThreads < 1) ?
			(2 * selectedDescs.size()) : oclDeviceThreads;
		if ((gpuRenderThreadCount == 1) && (selectedDescs.size() == 1)) {
			// Optimize the special case of one render thread and one GPU
			intersectionGPUDevices =  ctx->AddIntersectionDevices(selectedDescs);
		} else if ((gpuRenderThreadCount > 1) && (selectedDescs.size() == 1)) {
			// Optimize the special case of many render thread and one GPU
			intersectionGPUDevices =  ctx->AddVirtualM2OIntersectionDevices(gpuRenderThreadCount, selectedDescs);
		} else {
			// Create and start the virtual devices (only if there is more than one GPUs)
			intersectionGPUDevices =  ctx->AddVirtualM2MIntersectionDevices(gpuRenderThreadCount, selectedDescs);
		}

		cerr << "OpenCL Devices used: ";
		for (size_t i = 0; i < intersectionGPUDevices.size(); ++i)
			cerr << "[" << intersectionGPUDevices[i]->GetName() << "]";
		cerr << endl;
	}
}

void RenderingConfig::SetUpNativeDevices(const unsigned int nativeThreadCount) {
	std::vector<DeviceDescription *> descs = ctx->GetAvailableDeviceDescriptions();
	DeviceDescription::Filter(DEVICE_TYPE_NATIVE_THREAD, descs);

	std::vector<DeviceDescription *> selectedDescs;
	for (size_t i = 0; i < nativeThreadCount; ++i) {
		if (i < descs.size())
			selectedDescs.push_back(descs[i]);
		else
			selectedDescs.push_back(descs[descs.size() - 1]);
	}


	if (selectedDescs.size() == 0)
		cerr << "No Native thread device selected" << endl;
	else {
		// Allocate devices
		intersectionCPUDevices =  ctx->AddIntersectionDevices(selectedDescs);

		cerr << "Native Devices used: ";
		for (size_t i = 0; i < intersectionCPUDevices.size(); ++i)
			cerr << "[" << intersectionCPUDevices[i]->GetName() << "]";
		cerr << endl;
	}
}

void RenderingConfig::StartAllRenderThreads() {
	boost::unique_lock<boost::mutex> lock(cfgMutex);

	StartAllRenderThreadsLockless();
}

void RenderingConfig::StopAllRenderThreads() {
	boost::unique_lock<boost::mutex> lock(cfgMutex);

	StopAllRenderThreadsLockless();
}

void RenderingConfig::StartAllRenderThreadsLockless() {
	if (!renderThreadsStarted) {
		ctx->Start();

		for (size_t i = 0; i < renderThreads.size(); ++i)
			renderThreads[i]->Start();

		renderThreadsStarted = true;
	}
}

void RenderingConfig::StopAllRenderThreadsLockless() {
	if (renderThreadsStarted) {
		for (size_t i = 0; i < renderThreads.size(); ++i)
			renderThreads[i]->Interrupt();
		ctx->Interrupt();

		for (size_t i = 0; i < renderThreads.size(); ++i)
			renderThreads[i]->Stop();
		ctx->Stop();

		renderThreadsStarted = false;
	}
}
