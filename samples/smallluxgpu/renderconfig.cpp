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
#include "path/path.h"
#include "sppm/sppm.h"
#include "pathgpu/pathgpu.h"
#include "pathgpu2/pathgpu2.h"

#include "luxrays/utils/film/film.h"

string SLG_LABEL = "SmallLuxGPU v" SLG_VERSION_MAJOR "." SLG_VERSION_MINOR " (LuxRays demo: http://www.luxrender.net)";

RenderingConfig::RenderingConfig(const string &fileName) {
	cerr << "Reading configuration file: " << fileName << endl;
	cfg.LoadFile(fileName);

	cerr << "Configuration: " << endl;
	vector<string> keys = cfg.GetAllKeys();
	for (vector<string>::iterator i = keys.begin(); i != keys.end(); ++i)
		cerr << "  " << *i << " = " << cfg.GetString(*i, "") << endl;

	renderEngine = NULL;
	renderThreadsStarted = false;
	periodicSaveEnabled = false;
}

RenderingConfig::~RenderingConfig() {
	StopAllRenderThreadsLockless();

	delete renderEngine;
	delete scene;
	delete film;
	delete ctx;
}

void RenderingConfig::Init() {
	boost::unique_lock<boost::mutex> lock(cfgMutex);

	const bool lowLatency = (cfg.GetInt("opencl.latency.mode", 1) != 0);
	const string sceneFileName = cfg.GetString("scene.file", "scenes/luxball/luxball.scn");
	const unsigned int w = cfg.GetInt("image.width", 640);
	const unsigned int h = cfg.GetInt("image.height", 480);
	const unsigned int nativeThreadCount = cfg.GetInt("opencl.nativethread.count", 2);
	const bool useCPUs = (cfg.GetInt("opencl.cpu.use", 0) != 0);
	const bool useGPUs = (cfg.GetInt("opencl.gpu.use", 1) != 0);
	const unsigned int forceGPUWorkSize = cfg.GetInt("opencl.gpu.workgroup.size", 64);
	const unsigned int oclPlatformIndex = cfg.GetInt("opencl.platform.index", 0);
	const string oclIntersectionDeviceConfig = cfg.GetString("opencl.devices.select", "");
	const int oclPixelDeviceConfig = cfg.GetInt("opencl.pixeldevice.select", -1);
	const unsigned int oclDeviceThreads = cfg.GetInt("opencl.renderthread.count", 0);
	luxrays::RAY_EPSILON = cfg.GetFloat("scene.epsilon", luxrays::RAY_EPSILON);
	periodiceSaveTime = cfg.GetFloat("batch.periodicsave", 0.f);
	lastPeriodicSave = WallClockTime();
	periodicSaveEnabled = (periodiceSaveTime > 0.f);

	const unsigned int screenRefreshInterval = cfg.GetInt("screen.refresh.interval", lowLatency ? 100 : 2000);

	captionBuffer[0] = '\0';

	// Create LuxRays context
	ctx = new Context(DebugHandler, oclPlatformIndex);

	// Create the Film
	std::vector<DeviceDescription *> descs = ctx->GetAvailableDeviceDescriptions();
	if (oclPixelDeviceConfig == -1) {
		DeviceDescription::Filter(DEVICE_TYPE_NATIVE_THREAD, descs);
		film = new LuxRaysFilm(ctx, w, h, descs[0]);
	} else {
		DeviceDescription::Filter(DEVICE_TYPE_OPENCL, descs);
		film = new LuxRaysFilm(ctx, w, h, descs[oclPixelDeviceConfig]);
	}

	const int filterType = cfg.GetInt("film.filter.type", 1);
	if (filterType == 0)
		film->SetFilterType(FILTER_NONE);
	else
		film->SetFilterType(FILTER_GAUSSIAN);

	const int toneMapType = cfg.GetInt("film.tonemap.type", 0);
	if (toneMapType == 0) {
		LinearToneMapParams params;
		params.scale = cfg.GetFloat("film.tonemap.linear.scale", params.scale);
		film->SetToneMapParams(params);
	} else {
		Reinhard02ToneMapParams params;
		params.preScale = cfg.GetFloat("film.tonemap.reinhard02.prescale", params.preScale);
		params.postScale = cfg.GetFloat("film.tonemap.reinhard02.postscale", params.postScale);
		params.burn = cfg.GetFloat("film.tonemap.reinhard02.burn", params.burn);
		film->SetToneMapParams(params);
	}

	const float gamma = cfg.GetFloat("film.gamma", 2.2f);
	if (gamma != 2.2f)
		film->InitGammaTable(gamma);

	// Load film files
	const vector<string> filmNames = cfg.GetStringVector("film.file", "");
	if (filmNames.size() > 0) {
		for (size_t i = 0; i < filmNames.size(); ++i)
			film->AddFilm(filmNames[i]);
	}

	// Create the Scene
	const int accelType = cfg.GetInt("accelerator.type", -1);
	scene = new SLGScene(ctx, sceneFileName, film, accelType);
	// For compatibility with old scenes
	scene->camera->fieldOfView = cfg.GetFloat("scene.fieldofview", scene->camera->fieldOfView);
	scene->camera->Update(film->GetWidth(), film->GetHeight());

	// Start OpenCL devices
	SetUpOpenCLDevices(useCPUs, useGPUs, forceGPUWorkSize, oclDeviceThreads, oclIntersectionDeviceConfig);

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

#if !defined(LUXRAYS_DISABLE_OPENCL)
	// Check if I have to disable image storage
	const bool frocedDisableImageStorage = (accelType == 2);
	for (size_t i = 0; i < intersectionGPUDevices.size(); ++i)
		((OpenCLIntersectionDevice *)intersectionGPUDevices[i])->SetQBVHDisableImageStorage(frocedDisableImageStorage);
#endif

	// Set the Luxrays SataSet
	ctx->SetDataSet(scene->dataSet);

	intersectionAllDevices = ctx->GetIntersectionDevices();

	// Check the kind of render engine to start
	const int renderEngineType = cfg.GetInt("renderengine.type", 0);
	// Create and start the render engine
	switch (renderEngineType) {
		case 0:
			renderEngine = new PathRenderEngine(scene, film, &filmMutex, intersectionAllDevices, false, cfg);
			break;
		case 1:
			renderEngine = new SPPMRenderEngine(scene, film, &filmMutex, intersectionAllDevices, cfg);
			break;
		case 2:
			renderEngine = new PathRenderEngine(scene, film, &filmMutex, intersectionAllDevices, true, cfg);
			break;
#if !defined(LUXRAYS_DISABLE_OPENCL)
		case 3:
			renderEngine = new PathGPURenderEngine(scene, film, &filmMutex, intersectionGPUDevices, cfg);
			break;
		case 4:
			renderEngine = new PathGPU2RenderEngine(scene, film, &filmMutex, intersectionGPUDevices, cfg);
			break;
#endif
		default:
			assert (false);
	}
	renderEngine->SetScreenRefreshInterval(screenRefreshInterval);

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
	scene->camera->Update(film->GetWidth(), film->GetHeight());

	// Restart all devices
	if (wasRunning)
		StartAllRenderThreadsLockless();
}

void RenderingConfig::SetRenderingEngineType(const RenderEngineType type) {
	boost::unique_lock<boost::mutex> lock(cfgMutex);

	// Check the current rendering engine type is different
	if (renderEngine->GetEngineType() != type) {
		bool wasRunning = renderThreadsStarted;
		// First stop all devices
		if (wasRunning)
			StopAllRenderThreadsLockless();

		delete renderEngine;
		switch (type) {
			case PATH:
				renderEngine = new PathRenderEngine(scene, film, &filmMutex, intersectionAllDevices, false, cfg);
				break;
			case SPPM:
				renderEngine = new SPPMRenderEngine(scene, film, &filmMutex, intersectionAllDevices, cfg);
				break;
			case DIRECTLIGHT:
				renderEngine = new PathRenderEngine(scene, film, &filmMutex, intersectionAllDevices, true, cfg);
				break;
#if !defined(LUXRAYS_DISABLE_OPENCL)
			case PATHGPU:
				renderEngine = new PathGPURenderEngine(scene, film, &filmMutex, intersectionGPUDevices, cfg);
				break;
			case PATHGPU2:
				renderEngine = new PathGPU2RenderEngine(scene, film, &filmMutex, intersectionGPUDevices, cfg);
				break;
#endif
			default:
				assert (false);
		}
		film->Reset();

		// Restart all devices
		if (wasRunning)
			StartAllRenderThreadsLockless();
	}
}

void RenderingConfig::SetMotionBlur(const bool v) {
	boost::unique_lock<boost::mutex> lock(cfgMutex);

	bool wasRunning = renderThreadsStarted;
	// First stop all devices
	if (wasRunning)
		StopAllRenderThreadsLockless();

	film->Reset();
	PerspectiveCamera *camera = scene->camera;
	camera->motionBlur = v;
	if (camera->motionBlur) {
		camera->mbOrig = camera->orig;
		camera->mbTarget = camera->target;
		camera->mbUp = camera->up;
	}
	camera->Update(film->GetWidth(), film->GetHeight());

	// Restart all devices
	if (wasRunning)
		StartAllRenderThreadsLockless();
}

void RenderingConfig::SetUpOpenCLDevices(const bool useCPUs, const bool useGPUs,
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
#if !defined(LUXRAYS_DISABLE_OPENCL)
		if (cfg.GetInt("opencl.latency.mode", 1) && (cfg.GetInt("renderengine.type", 0) == 3) &&
				(cfg.GetInt("pathgpu.openglinterop.enable", 0) != 0)) {
			// Ask for OpenGL interoperability on the first device
			((OpenCLDeviceDescription *)selectedDescs[0])->EnableOGLInterop();
		}
#endif

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
		renderEngine->Start();
		renderThreadsStarted = true;
	}
}

void RenderingConfig::StopAllRenderThreadsLockless() {
	if (renderThreadsStarted) {
		renderEngine->Interrupt();
		renderEngine->Stop();
		ctx->Stop();

		renderThreadsStarted = false;
	}
}

void RenderingConfig::SaveFilmImage() {
	boost::unique_lock<boost::mutex> lock(filmMutex);

	const string fileName = cfg.GetString("image.filename", "image.png");
	film->UpdateScreenBuffer();
	film->Save(fileName);

	const vector<string> filmNames = cfg.GetStringVector("screen.file", "");
	if (filmNames.size() == 1)
		film->SaveFilm(filmNames[0]);
	else if (filmNames.size() > 1)
		film->SaveFilm("merged.flm");
}

void RenderingConfig::SaveFilm() {
	const vector<string> filmNames = cfg.GetStringVector("screen.file", "");

	if (filmNames.size() == 0)
		return;

	boost::unique_lock<boost::mutex> lock(filmMutex);

	StopAllRenderThreadsLockless();

	if (filmNames.size() == 1)
		film->SaveFilm(filmNames[0]);
	else if (filmNames.size() > 1)
		film->SaveFilm("merged.flm");

	StartAllRenderThreadsLockless();
}

void RenderingConfig::UpdateSceneDataSet() {
	// Check if we are using a MQBVH accelerator
	if (scene->dataSet->GetAcceleratorType() == ACCEL_MQBVH) {
		// Update the DataSet
		ctx->UpdateDataSet();
	} else {
		// For all other accelerator, I have to rebuild the DataSet
		scene->UpdateDataSet(ctx, scene->dataSet->GetAcceleratorType());
		// Set the Luxrays SataSet
		ctx->SetDataSet(scene->dataSet);
	}
}
