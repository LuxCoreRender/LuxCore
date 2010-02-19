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

#ifndef _RENDERCONFIG_H
#define	_RENDERCONFIG_H

#include "scene.h"
#include "renderthread.h"
#include "displayfunc.h"
#include "luxrays/core/context.h"
#include "luxrays/core/device.h"
#include "luxrays/core/virtualdevice.h"

// Must be a multiple of 1024 (for AMD CPU device) and 64 (for most GPUs)
#define RAY_BUFFER_SIZE (65536)

class RenderingConfig {
public:
	RenderingConfig(const bool lowLatency, const string &sceneFileName, const unsigned int w,
		const unsigned int h, const unsigned int nativeThreadCount,
		const bool useCPUs, const bool useGPUs,
		const unsigned int forceGPUWorkSize) {
		screenRefreshInterval = 100;

		Init(lowLatency, sceneFileName, w, h, nativeThreadCount, useCPUs, useGPUs,
			forceGPUWorkSize, 3);
	}

	RenderingConfig(const string &fileName) {
		cfg.insert(make_pair("image.width", "640"));
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
		cfg.insert(make_pair("path.shadowrays", "1"));

		cerr << "Reading configuration file: " << fileName << endl;

		ifstream file(fileName.c_str(), ios::in);
		char buf[512], key[512], value[512];
		for (;;) {
			file.getline(buf, 512);
			if (file.eof())
				break;
			// Ignore comments
			if (buf[0] == '#')
				continue;

			const int count = sscanf(buf, "%s = %s", key, value);
			if (count != 2) {
				cerr << "Ingoring line in configuration file: [" << buf << "]" << endl;
				continue;
			}

			// Check if it is a valid key
			string skey(key);
			if (cfg.count(skey) != 1)
				cerr << "Ingoring unknown key in configuration file:  [" << skey << "]" << endl;
			else {
				cfg.erase(skey);
				cfg.insert(make_pair(skey, string(value)));
			}
		}

		cerr << "Configuration: " << endl;
		for (map<string, string>::iterator i = cfg.begin(); i != cfg.end(); ++i)
			cerr << "  " << i->first << " = " << i->second << endl;
	}

	~RenderingConfig() {
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

	void Init() {
		const bool lowLatency = (atoi(cfg.find("opencl.latency.mode")->second.c_str()) == 1);
		const string sceneFileName = cfg.find("scene.file")->second.c_str();
		const unsigned int w = atoi(cfg.find("image.width")->second.c_str());
		const unsigned int h = atoi(cfg.find("image.height")->second.c_str());
		const unsigned int nativeThreadCount = atoi(cfg.find("opencl.nativethread.count")->second.c_str());
		const bool useCPUs = (atoi(cfg.find("opencl.cpu.use")->second.c_str()) == 1);
		const bool useGPUs = (atoi(cfg.find("opencl.gpu.use")->second.c_str()) == 1);
		const unsigned int forceGPUWorkSize = atoi(cfg.find("opencl.gpu.workgroup.size")->second.c_str());
		const unsigned int filmType = atoi(cfg.find("screen.type")->second.c_str());
		const unsigned int oclPlatformIndex = atoi(cfg.find("opencl.platform.index")->second.c_str());
		const string oclDeviceConfig = cfg.find("opencl.devices.select")->second;
		const string oclDeviceThreads = cfg.find("opencl.devices.threads")->second;

		screenRefreshInterval = atoi(cfg.find("screen.refresh.interval")->second.c_str());

		Init(lowLatency, sceneFileName, w, h, nativeThreadCount,
			useCPUs, useGPUs, forceGPUWorkSize, filmType,
			oclPlatformIndex, oclDeviceThreads, oclDeviceConfig);

		StopAllRenderThreads();
		for (size_t i = 0; i < renderThreads.size(); ++i)
			renderThreads[i]->ClearPaths();

		scene->camera->fieldOfView = atof(cfg.find("scene.fieldofview")->second.c_str());
		scene->maxPathDepth = atoi(cfg.find("path.maxdepth")->second.c_str());
		scene->shadowRayCount = atoi(cfg.find("path.shadowrays")->second.c_str());
		StartAllRenderThreads();
	}

	void Init(const bool lowLatency, const string &sceneFileName, const unsigned int w,
		const unsigned int h, const unsigned int nativeThreadCount,
		const bool useCPUs, const bool useGPUs,
		const unsigned int forceGPUWorkSize, const unsigned int filmType,
		const unsigned int oclPlatformIndex = 0,
		const string &oclDeviceThreads = "", const string &oclDeviceConfig = "") {

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
		const size_t gpuRenderThreadCount = ((oclDeviceThreads.length() == 0) || (intersectionGPUDevices.size() == 0)) ?
			(2 * intersectionGPUDevices.size()) : atoi(oclDeviceThreads.c_str());
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

	void ReInit(const bool reallocBuffers, const unsigned int w = 0, unsigned int h = 0) {
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

	void SetMaxPathDepth(const int delta) {
		// First stop all devices
		StopAllRenderThreads();

		film->Reset();
		scene->maxPathDepth = max<unsigned int>(2, scene->maxPathDepth + delta);

		// Restart all devices
		StartAllRenderThreads();
	}

	void SetShadowRays(const int delta) {
		// First stop all devices
		StopAllRenderThreads();

		scene->shadowRayCount = max<unsigned int>(1, scene->shadowRayCount + delta);
		for (size_t i = 0; i < renderThreads.size(); ++i)
			renderThreads[i]->ClearPaths();

		// Restart all devices
		StartAllRenderThreads();
	}

	map<string, string> cfg;

	const vector<IntersectionDevice *> &GetIntersectionDevices() { return intersectionAllDevices; }
	const vector<RenderThread *> &GetRenderThreads() { return renderThreads; }

	char captionBuffer[512];
	unsigned int screenRefreshInterval;

	Scene *scene;
	Film *film;

private:
	void SetUpOpenCLDevices(const bool lowLatency, const bool useCPUs, const bool useGPUs,
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

	void SetUpNativeDevices(const unsigned int nativeThreadCount) {
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

			cerr << "NAtive Devices used: ";
			for (size_t i = 0; i < intersectionCPUDevices.size(); ++i)
				cerr << "[" << intersectionCPUDevices[i]->GetName() << "]";
			cerr << endl;
		}
	}

	void StartAllRenderThreads() {
		for (size_t i = 0; i < renderThreads.size(); ++i)
			renderThreads[i]->Start();
	}

	void StopAllRenderThreads() {
		for (size_t i = 0; i < renderThreads.size(); ++i)
			renderThreads[i]->Interrupt();

		for (size_t i = 0; i < renderThreads.size(); ++i)
			renderThreads[i]->Stop();
	}

	Context *ctx;

	vector<RenderThread *> renderThreads;

	vector<IntersectionDevice *> intersectionGPUDevices;
	VirtualM2OIntersectionDevice *m2oDevice;
	VirtualO2MIntersectionDevice *o2mDevice;

	vector<IntersectionDevice *> intersectionCPUDevices;

	vector<IntersectionDevice *> intersectionAllDevices;
};

#endif	/* _RENDERCONFIG_H */
