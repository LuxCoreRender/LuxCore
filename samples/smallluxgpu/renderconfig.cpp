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

RenderingConfig::RenderingConfig(const bool lowLatency, const string &sceneFileName, const unsigned int w,
	const unsigned int h, const unsigned int nativeThreadCount,
	const bool useCPUs, const bool useGPUs,
	const unsigned int forceGPUWorkSize) {
	screenRefreshInterval = 100;

	Init(lowLatency, sceneFileName, w, h, nativeThreadCount, useCPUs, useGPUs,
		forceGPUWorkSize, 3);
}

RenderingConfig::RenderingConfig(const string &fileName) {
	/*cfg.insert(make_pair("image.width", "640"));
	cfg.insert(make_pair("image.height", "480"));
	cfg.insert(make_pair("batch.halttime", "0"));
	cfg.insert(make_pair("scene.file", "scenes/luxball.scn"));
	cfg.insert(make_pair("scene.fieldofview", "45"));
	cfg.insert(make_pair("opencl.latency.mode", "0"));
	cfg.insert(make_pair("opencl.nativethread.count", "0"));
	cfg.insert(make_pair("opencl.renderthread.count", "4"));
	cfg.insert(make_pair("opencl.cpu.use", "0"));
	cfg.insert(make_pair("opencl.gpu.use", "1"));
	cfg.insert(make_pair("opencl.gpu.workgroup.size", "64"));
	cfg.insert(make_pair("opencl.platform.index", "0"));
	cfg.insert(make_pair("opencl.devices.select", ""));
	cfg.insert(make_pair("opencl.devices.threads", "")); // Initialized when the GPU count is known
	cfg.insert(make_pair("screen.refresh.interval", "100"));
	cfg.insert(make_pair("screen.type", "3"));
	cfg.insert(make_pair("path.maxdepth", "3"));
	cfg.insert(make_pair("path.shadowrays", "1"));*/

	cerr << "Reading configuration file: " << fileName << endl;
	cfg.LoadFile(fileName);

	cerr << "Configuration: " << endl;
	vector<string> keys = cfg.GetAllKeys();
	for (vector<string>::iterator i = keys.begin(); i != keys.end(); ++i)
		cerr << "  " << *i << " = " << cfg.GetString(*i, "") << endl;
}

RenderingConfig::~RenderingConfig() {
	for (size_t i = 0; i < renderThreads.size(); ++i)
		delete renderThreads[i];

	if (m2oDevice)
		delete m2oDevice;
	if (o2mDevice)
		delete o2mDevice;

	for (size_t i = 0; i < intersectionAllDevices.size(); ++i)
		delete intersectionAllDevices[i];

	delete scene;
	delete film;
}

void RenderingConfig::Init() {
	const bool lowLatency = cfg.GetInt("opencl.latency.mode", 1);
	const string sceneFileName = cfg.GetString("scene.file", "scenes/luxball.scn");
	const unsigned int w = cfg.GetInt("image.width", 640);
	const unsigned int h = cfg.GetInt("image.height", 480);
	const unsigned int nativeThreadCount = cfg.GetInt("opencl.nativethread.count", 2);
	const bool useCPUs = (cfg.GetInt("opencl.cpu.use", 0) == 1);
	const bool useGPUs = (cfg.GetInt("opencl.gpu.use", 1) == 1);
	const unsigned int forceGPUWorkSize = cfg.GetInt("opencl.gpu.workgroup.size", 64);
	const unsigned int filmType = cfg.GetInt("screen.type",3 );
	const unsigned int oclPlatformIndex = cfg.GetInt("opencl.platform.index", 0);
	const string oclDeviceConfig = cfg.GetString("opencl.devices.select", "");
	const unsigned int oclDeviceThreads = cfg.GetInt("opencl.renderthread.count", 0);

	screenRefreshInterval = cfg.GetInt("screen.refresh.interval", 100);

	Init(lowLatency, sceneFileName, w, h, nativeThreadCount,
		useCPUs, useGPUs, forceGPUWorkSize, filmType,
		oclPlatformIndex, oclDeviceThreads, oclDeviceConfig);

	StopAllRenderThreads();
	for (size_t i = 0; i < renderThreads.size(); ++i)
		renderThreads[i]->ClearPaths();

	scene->camera->fieldOfView = cfg.GetFloat("scene.fieldofview", 45.f);
	scene->maxPathDepth = cfg.GetInt("path.maxdepth", 3);
	scene->shadowRayCount = cfg.GetInt("path.shadowrays", 1);
	StartAllRenderThreads();
}

void RenderingConfig::Init(const bool lowLatency, const string &sceneFileName, const unsigned int w,
	const unsigned int h, const unsigned int nativeThreadCount,
	const bool useCPUs, const bool useGPUs,
	const unsigned int forceGPUWorkSize, const unsigned int filmType,
	const unsigned int oclPlatformIndex,
	const unsigned int oclDeviceThreads, const string &oclDeviceConfig) {

	captionBuffer[0] = '\0';

	// Create LuxRays context
	ctx = new Context(DebugHandler ,oclPlatformIndex);

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
	scene = new Scene(ctx, lowLatency, sceneFileName, film);

	// Start OpenCL devices
	SetUpOpenCLDevices(lowLatency, useCPUs, useGPUs, forceGPUWorkSize, oclDeviceConfig);

	// Start Native threads
	SetUpNativeDevices(nativeThreadCount);

	// Set the Luxrays SataSet
	ctx->SetCurrentDataSet(scene->dataSet);

	const size_t deviceCount = intersectionCPUDevices.size()  + intersectionGPUDevices.size();
	if (deviceCount <= 0)
		throw runtime_error("Unable to find any appropiate IntersectionDevice");

	intersectionAllDevices.resize(deviceCount);
	if (intersectionGPUDevices.size() > 0)
		copy(intersectionGPUDevices.begin(), intersectionGPUDevices.end(),
				intersectionAllDevices.begin());
	if (intersectionCPUDevices.size() > 0)
		copy(intersectionCPUDevices.begin(), intersectionCPUDevices.end(),
				intersectionAllDevices.begin() + intersectionGPUDevices.size());

	// Create and start render threads
	const size_t gpuRenderThreadCount = ((oclDeviceThreads < 1) || (intersectionGPUDevices.size() == 0)) ?
		(2 * intersectionGPUDevices.size()) : oclDeviceThreads;
	size_t renderThreadCount = intersectionCPUDevices.size() + gpuRenderThreadCount;
	cerr << "Starting "<< renderThreadCount << " render threads" << endl;
	if (gpuRenderThreadCount > 0) {
		if ((gpuRenderThreadCount == 1) && (intersectionGPUDevices.size() == 1)) {
			// Optimize the special case of one render thread and one GPU
			o2mDevice = NULL;
			m2oDevice = NULL;

			DeviceRenderThread *t = new DeviceRenderThread(1, intersectionGPUDevices[0], scene, lowLatency);
			renderThreads.push_back(t);
			t->Start();
		} else {
			// Create and start the virtual devices (only if htere is more than one GPUs)
			o2mDevice = new VirtualO2MIntersectionDevice(intersectionGPUDevices, 0);
			m2oDevice = new VirtualM2OIntersectionDevice(gpuRenderThreadCount, o2mDevice);

			for (size_t i = 0; i < gpuRenderThreadCount; ++i) {
				DeviceRenderThread *t = new DeviceRenderThread(i + 1, m2oDevice->GetVirtualDevice(i), scene, lowLatency);
				renderThreads.push_back(t);
				t->Start();
			}
		}
	} else {
		o2mDevice = NULL;
		m2oDevice = NULL;
	}

	for (size_t i = 0; i < intersectionCPUDevices.size(); ++i) {
		NativeRenderThread *t = new NativeRenderThread(gpuRenderThreadCount + i,
				(NativeThreadIntersectionDevice *)intersectionCPUDevices[i], scene, lowLatency);
		renderThreads.push_back(t);
		t->Start();
	}

	film->StartSampleTime();
}

void RenderingConfig::ReInit(const bool reallocBuffers, const unsigned int w, unsigned int h) {
	// First stop all devices
	StopAllRenderThreads();

	// Check if I have to reallocate buffers
	if (reallocBuffers)
		film->Init(w, h);
	else
		film->Reset();
	scene->camera->Update();

	// Restart all devices
	StartAllRenderThreads();
}

void RenderingConfig::SetMaxPathDepth(const int delta) {
	// First stop all devices
	StopAllRenderThreads();

	film->Reset();
	scene->maxPathDepth = max<unsigned int>(2, scene->maxPathDepth + delta);

	// Restart all devices
	StartAllRenderThreads();
}

void RenderingConfig::SetShadowRays(const int delta) {
	// First stop all devices
	StopAllRenderThreads();

	scene->shadowRayCount = max<unsigned int>(1, scene->shadowRayCount + delta);
	for (size_t i = 0; i < renderThreads.size(); ++i)
		renderThreads[i]->ClearPaths();

	// Restart all devices
	StartAllRenderThreads();
}

void RenderingConfig::SetUpOpenCLDevices(const bool lowLatency, const bool useCPUs, const bool useGPUs,
	const unsigned int forceGPUWorkSize, const string &oclDeviceConfig) {

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
	for (size_t i = 0; i < descs.size(); ++i) {
		OpenCLDeviceDescription *desc = (OpenCLDeviceDescription *)descs[i];

		if (haveSelectionString) {
			if (oclDeviceConfig.at(i) == '1')
				selectedDescs.push_back(desc);
		} else {
			if ((useCPUs && desc->GetOpenCLType() == OCL_DEVICE_TYPE_CPU) ||
					(useGPUs && desc->GetOpenCLType() == OCL_DEVICE_TYPE_GPU))
				selectedDescs.push_back(descs[i]);
		}
	}

	if (selectedDescs.size() == 0)
		cerr << "No OpenCL device selected" << endl;
	else {
		// Allocate devices
		intersectionGPUDevices =  ctx->AddIntersectionDevices(selectedDescs);

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
	for (size_t i = 0; i < renderThreads.size(); ++i)
		renderThreads[i]->Start();
}

void RenderingConfig::StopAllRenderThreads() {
	for (size_t i = 0; i < renderThreads.size(); ++i)
		renderThreads[i]->Interrupt();

	for (size_t i = 0; i < renderThreads.size(); ++i)
		renderThreads[i]->Stop();
}
