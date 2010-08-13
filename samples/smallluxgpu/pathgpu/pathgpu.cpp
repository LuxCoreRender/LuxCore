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

#if !defined(LUXRAYS_DISABLE_OPENCL)

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <sstream>
#include <stdexcept>

#include "luxrays/core/pixel/samplebuffer.h"

#include "smalllux.h"
#include "pathgpu/pathgpu.h"
#include "pathgpu/kernels/kernels.h"
#include "renderconfig.h"
#include "displayfunc.h"

//------------------------------------------------------------------------------
// PathGPURenderThread
//------------------------------------------------------------------------------

PathGPURenderThread::PathGPURenderThread(unsigned int index, PathGPURenderEngine *re) {
	threadIndex = index;
	renderEngine = re;
	started = false;
	frameBuffer = NULL;
}

PathGPURenderThread::~PathGPURenderThread() {
}

void PathGPURenderThread::Start() {
	started = true;
	frameBuffer = new PathGPUPixel[renderEngine->film->GetWidth() * renderEngine->film->GetHeight()];
}

void PathGPURenderThread::Stop() {
	started = false;
	delete[] frameBuffer;
}

//------------------------------------------------------------------------------
// PathGPUDeviceRenderThread
//------------------------------------------------------------------------------

PathGPUDeviceRenderThread::PathGPUDeviceRenderThread(const unsigned int index, const unsigned long seedBase,
		const float samplStart,  const unsigned int samplePerPixel, OpenCLIntersectionDevice *device,
		PathGPURenderEngine *re) : PathGPURenderThread(index, re) {
	intersectionDevice = device;
	samplingStart = samplStart;
	reportedPermissionError = false;

	renderThread = NULL;

	initKernel = NULL;
	advancePathKernel = NULL;
}

PathGPUDeviceRenderThread::~PathGPUDeviceRenderThread() {
	if (started)
		Stop();
}

static void AppendMatrixDefinition(stringstream &ss, const char *paramName, const Matrix4x4 &m) {
	for (unsigned int i = 0; i < 4; ++i) {
		for (unsigned int j = 0; j < 4; ++j)
			ss << " -D " << paramName << "_" << i << j << "=" << m.m[i][j];
	}
}

void PathGPUDeviceRenderThread::Start() {
	PathGPURenderThread::Start();

	const unsigned int startLine = Clamp<unsigned int>(
			renderEngine->film->GetHeight() * samplingStart,
			0, renderEngine->film->GetHeight() - 1);

	// Compile kernels
	cl::Context &oclContext = intersectionDevice->GetOpenCLContext();
	cl::Device &oclDevice = intersectionDevice->GetOpenCLDevice();

	// Compile sources
	cl::Program::Sources source(1, std::make_pair(KernelSource_PathGPU.c_str(), KernelSource_PathGPU.length()));
	cl::Program program = cl::Program(oclContext, source);

	// Set #define symbols
	stringstream ss;
	ss << "-I." <<
			" -D PARAM_STARTLINE=" << startLine <<
			" -D PARAM_PATH_COUNT=" << PATHGPU_PATH_COUNT <<
			" -D PARAM_IMAGE_WIDTH=" << renderEngine->film->GetWidth() <<
			" -D PARAM_IMAGE_HEIGHT=" << renderEngine->film->GetHeight() <<
			" -D PARAM_RAY_EPSILON=" << RAY_EPSILON <<
			" -D PARAM_CLIP_YON=" << renderEngine->scene->camera->GetClipYon() <<
			" -D PARAM_CLIP_HITHER=" << renderEngine->scene->camera->GetClipHither()
			;
	AppendMatrixDefinition(ss, "PARAM_RASTER2CAMERA", renderEngine->scene->camera->GetRasterToCameraMatrix());
	AppendMatrixDefinition(ss, "PARAM_CAMERA2WORLD", renderEngine->scene->camera->GetCameraToWorldMatrix());
	cerr << "[PathGPUDeviceRenderThread::" << threadIndex << "] Defined symbols: " << ss.str() << endl;

	try {
		VECTOR_CLASS<cl::Device> buildDevice;
		buildDevice.push_back(oclDevice);
		program.build(buildDevice, ss.str().c_str());
	} catch (cl::Error err) {
		cl::STRING_CLASS strError = program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(oclDevice);
		cerr << "[PathGPUDeviceRenderThread::" << threadIndex << "] PathGPU compilation error:\n" << strError.c_str() << endl;

		throw err;
	}

	//--------------------------------------------------------------------------
	// Init kernel
	//--------------------------------------------------------------------------

	initKernel = new cl::Kernel(program, "Init");
	initKernel->getWorkGroupInfo<size_t>(oclDevice, CL_KERNEL_WORK_GROUP_SIZE, &initWorkGroupSize);
	cerr << "[PathGPUDeviceRenderThread::" << threadIndex << "] PathGPU Init kernel work group size: " << initWorkGroupSize << endl;
	cl_ulong memSize;
	initKernel->getWorkGroupInfo<cl_ulong>(oclDevice, CL_KERNEL_LOCAL_MEM_SIZE, &memSize);
	cerr << "[PathGPUDeviceRenderThread::" << threadIndex << "] PathGPU Init kernel memory footprint: " << memSize << endl;

	initKernel->getWorkGroupInfo<size_t>(oclDevice, CL_KERNEL_WORK_GROUP_SIZE, &initWorkGroupSize);
	cerr << "[PathGPUDeviceRenderThread::" << threadIndex << "] Suggested work group size: " << initWorkGroupSize << endl;

	if (intersectionDevice->GetForceWorkGroupSize() > 0) {
		initWorkGroupSize = intersectionDevice->GetForceWorkGroupSize();
		cerr << "[PathGPUDeviceRenderThread::" << threadIndex << "] Forced work group size: " << initWorkGroupSize << endl;
	}

	//--------------------------------------------------------------------------
	// InitFB kernel
	//--------------------------------------------------------------------------

	initFBKernel = new cl::Kernel(program, "InitFrameBuffer");
	initFBKernel->getWorkGroupInfo<size_t>(oclDevice, CL_KERNEL_WORK_GROUP_SIZE, &initFBWorkGroupSize);
	cerr << "[PathGPUDeviceRenderThread::" << threadIndex << "] PathGPU InitFrameBuffer kernel work group size: " << initFBWorkGroupSize << endl;
	initFBKernel->getWorkGroupInfo<cl_ulong>(oclDevice, CL_KERNEL_LOCAL_MEM_SIZE, &memSize);
	cerr << "[PathGPUDeviceRenderThread::" << threadIndex << "] PathGPU InitFrameBuffer kernel memory footprint: " << memSize << endl;

	initFBKernel->getWorkGroupInfo<size_t>(oclDevice, CL_KERNEL_WORK_GROUP_SIZE, &initFBWorkGroupSize);
	cerr << "[PathGPUDeviceRenderThread::" << threadIndex << "] Suggested work group size: " << initFBWorkGroupSize << endl;

	if (intersectionDevice->GetForceWorkGroupSize() > 0) {
		initFBWorkGroupSize = intersectionDevice->GetForceWorkGroupSize();
		cerr << "[PathGPUDeviceRenderThread::" << threadIndex << "] Forced work group size: " << initFBWorkGroupSize << endl;
	}

	//--------------------------------------------------------------------------
	// AdvancePaths kernel
	//--------------------------------------------------------------------------

	advancePathKernel = new cl::Kernel(program, "AdvancePaths");
	advancePathKernel->getWorkGroupInfo<size_t>(oclDevice, CL_KERNEL_WORK_GROUP_SIZE, &advancePathWorkGroupSize);
	cerr << "[PathGPUDeviceRenderThread::" << threadIndex << "] PathGPU AdvancePaths kernel work group size: " << advancePathWorkGroupSize << endl;
	advancePathKernel->getWorkGroupInfo<cl_ulong>(oclDevice, CL_KERNEL_LOCAL_MEM_SIZE, &memSize);
	cerr << "[PathGPUDeviceRenderThread::" << threadIndex << "] PathGPU AdvancePaths kernel memory footprint: " << memSize << endl;

	advancePathKernel->getWorkGroupInfo<size_t>(oclDevice, CL_KERNEL_WORK_GROUP_SIZE, &advancePathWorkGroupSize);
	cerr << "[PathGPUDeviceRenderThread::" << threadIndex << "] Suggested work group size: " << advancePathWorkGroupSize << endl;

	if (intersectionDevice->GetForceWorkGroupSize() > 0) {
		advancePathWorkGroupSize = intersectionDevice->GetForceWorkGroupSize();
		cerr << "[PathGPUDeviceRenderThread::" << threadIndex << "] Forced work group size: " << advancePathWorkGroupSize << endl;
	}

	//--------------------------------------------------------------------------
	// Allocate buffers
	//--------------------------------------------------------------------------

	const OpenCLDeviceDescription *deviceDesc = intersectionDevice->GetDeviceDesc();

	cerr << "[PathGPUDeviceRenderThread::" << threadIndex << "] Ray buffer size: " << (sizeof(Ray) * PATHGPU_PATH_COUNT / 1024) << "Kbytes" << endl;
	raysBuff = new cl::Buffer(oclContext,
			CL_MEM_READ_WRITE,
			sizeof(Ray) * PATHGPU_PATH_COUNT);
	deviceDesc->AllocMemory(raysBuff->getInfo<CL_MEM_SIZE>());

	cerr << "[PathGPUDeviceRenderThread::" << threadIndex << "] RayHit buffer size: " << (sizeof(RayHit) * PATHGPU_PATH_COUNT / 1024) << "Kbytes" << endl;
	hitsBuff = new cl::Buffer(oclContext,
			CL_MEM_READ_WRITE,
			sizeof(RayHit) * PATHGPU_PATH_COUNT);
	deviceDesc->AllocMemory(hitsBuff->getInfo<CL_MEM_SIZE>());

	cerr << "[PathGPUDeviceRenderThread::" << threadIndex << "] Paths buffer size: " << (sizeof(PathGPU) * PATHGPU_PATH_COUNT / 1024) << "Kbytes" << endl;
	pathsBuff = new cl::Buffer(oclContext,
			CL_MEM_READ_WRITE,
			sizeof(PathGPU) * PATHGPU_PATH_COUNT);
	deviceDesc->AllocMemory(pathsBuff->getInfo<CL_MEM_SIZE>());

	cerr << "[PathGPUDeviceRenderThread::" << threadIndex << "] Paths buffer size: " << (sizeof(PathGPUPixel) * renderEngine->film->GetWidth() * renderEngine->film->GetHeight() / 1024) << "Kbytes" << endl;
	framebufferBuff = new cl::Buffer(oclContext,
			CL_MEM_READ_WRITE,
			sizeof(PathGPUPixel) * renderEngine->film->GetWidth() * renderEngine->film->GetHeight());
	deviceDesc->AllocMemory(framebufferBuff->getInfo<CL_MEM_SIZE>());

	// Clear the frame buffer
	cl::CommandQueue &oclQueue = intersectionDevice->GetOpenCLQueue();
	initFBKernel->setArg(0, *framebufferBuff);
	oclQueue.enqueueNDRangeKernel(*initFBKernel, cl::NullRange,
			cl::NDRange(renderEngine->film->GetWidth() * renderEngine->film->GetHeight()), cl::NDRange(initFBWorkGroupSize));

	// Initialize the path buffer
	initKernel->setArg(0, *pathsBuff);
	initKernel->setArg(1, *raysBuff);
	oclQueue.enqueueNDRangeKernel(*initKernel, cl::NullRange,
			cl::NDRange(PATHGPU_PATH_COUNT), cl::NDRange(initWorkGroupSize));

	advancePathKernel->setArg(0, *pathsBuff);
	advancePathKernel->setArg(1, *raysBuff);
	advancePathKernel->setArg(2, *hitsBuff);
	advancePathKernel->setArg(3, *framebufferBuff);

	// Create the thread for the rendering
	renderThread = new boost::thread(boost::bind(PathGPUDeviceRenderThread::RenderThreadImpl, this));

	// Set renderThread priority
	bool res = SetThreadRRPriority(renderThread);
	if (res && !reportedPermissionError) {
		cerr << "[PathGPUDeviceRenderThread::" << threadIndex << "] Failed to set ray intersection thread priority (you probably need root/administrator permission to set thread realtime priority)" << endl;
		reportedPermissionError = true;
	}
}

void PathGPUDeviceRenderThread::Interrupt() {
	if (renderThread)
		renderThread->interrupt();
}

void PathGPUDeviceRenderThread::Stop() {
	if (renderThread) {
		renderThread->interrupt();
		renderThread->join();
		delete renderThread;
		renderThread = NULL;
	}

	const OpenCLDeviceDescription *deviceDesc = intersectionDevice->GetDeviceDesc();
	deviceDesc->FreeMemory(raysBuff->getInfo<CL_MEM_SIZE>());
	delete raysBuff;
	deviceDesc->FreeMemory(hitsBuff->getInfo<CL_MEM_SIZE>());
	delete hitsBuff;
	deviceDesc->FreeMemory(pathsBuff->getInfo<CL_MEM_SIZE>());
	delete pathsBuff;
	deviceDesc->FreeMemory(framebufferBuff->getInfo<CL_MEM_SIZE>());
	delete framebufferBuff;

	delete initKernel;
	delete initFBKernel;
	delete advancePathKernel;

	PathGPURenderThread::Stop();
}

void PathGPUDeviceRenderThread::ClearPaths() {
	assert (!started);
}

void PathGPUDeviceRenderThread::RenderThreadImpl(PathGPUDeviceRenderThread *renderThread) {
	cerr << "[PathGPUDeviceRenderThread::" << renderThread->threadIndex << "] Rendering thread started" << endl;

	try {
		cl::CommandQueue &oclQueue = renderThread->intersectionDevice->GetOpenCLQueue();
		const unsigned int pixelCount = renderThread->renderEngine->film->GetWidth() * renderThread->renderEngine->film->GetHeight();

		while (!boost::this_thread::interruption_requested()) {
			const double startTime = WallClockTime();
			for(;;) {
				for (unsigned int i = 0; i < 4; ++i) {
					// Trace rays

					// Advance paths
					oclQueue.enqueueNDRangeKernel(*(renderThread->advancePathKernel), cl::NullRange,
						cl::NDRange(PATHGPU_PATH_COUNT), cl::NDRange(renderThread->advancePathWorkGroupSize));
				}

				oclQueue.finish();
				const double elapsedTime = WallClockTime() - startTime;

				if ((elapsedTime > 0.5) || boost::this_thread::interruption_requested())
					break;
			}

			// Transfer the frame buffer
			oclQueue.enqueueReadBuffer(
				*(renderThread->framebufferBuff),
				CL_TRUE,
				0,
				sizeof(PathGPUPixel) * pixelCount,
				renderThread->frameBuffer);
		}

		cerr << "[PathGPUDeviceRenderThread::" << renderThread->threadIndex << "] Rendering thread halted" << endl;
	} catch (boost::thread_interrupted) {
		cerr << "[PathGPUDeviceRenderThread::" << renderThread->threadIndex << "] Rendering thread halted" << endl;
	}
#if !defined(LUXRAYS_DISABLE_OPENCL)
	catch (cl::Error err) {
		cerr << "[PathGPUDeviceRenderThread::" << renderThread->threadIndex << "] Rendering thread ERROR: " << err.what() << "(" << err.err() << ")" << endl;
	}
#endif
}

//------------------------------------------------------------------------------
// PathGPURenderEngine
//------------------------------------------------------------------------------

PathGPURenderEngine::PathGPURenderEngine(SLGScene *scn, Film *flm, boost::mutex *filmMutex,
		vector<IntersectionDevice *> intersectionDevices, const Properties &cfg) :
		RenderEngine(scn, flm, filmMutex) {
	samplePerPixel = max(1, cfg.GetInt("path.sampler.spp", cfg.GetInt("sampler.spp", 4)));
	maxPathDepth = cfg.GetInt("path.maxdepth", 3);
	rrDepth = cfg.GetInt("path.russianroulette.depth", 2);
	rrProb = cfg.GetFloat("path.russianroulette.prob", 0.5f);

	sampleBuffer = film->GetFreeSampleBuffer();

	// Look for OpenCL devices
	for (size_t i = 0; i < intersectionDevices.size(); ++i) {
		if (intersectionDevices[i]->GetType() == DEVICE_TYPE_OPENCL)
			oclIntersectionDevices.push_back((OpenCLIntersectionDevice *)intersectionDevices[i]);
	}

	cerr << "Found "<< oclIntersectionDevices.size() << " OpenCL intersection devices for PathGPU render engine" << endl;

	if (oclIntersectionDevices.size() < 1)
		throw runtime_error("Unable to find an OpenCL intersection device for PathGPU render engine");

	const unsigned long seedBase = (unsigned long)(WallClockTime() / 1000.0);

	// Create and start render threads
	const size_t renderThreadCount = oclIntersectionDevices.size();
	cerr << "Starting "<< renderThreadCount << " PathGPU render threads" << endl;
	for (size_t i = 0; i < renderThreadCount; ++i) {
		PathGPUDeviceRenderThread *t = new PathGPUDeviceRenderThread(
				i, seedBase, i / (float)renderThreadCount,
				samplePerPixel, oclIntersectionDevices[i], this);
		renderThreads.push_back(t);
	}
}

PathGPURenderEngine::~PathGPURenderEngine() {
	if (started) {
		Interrupt();
		Stop();
	}

	for (size_t i = 0; i < renderThreads.size(); ++i)
		delete renderThreads[i];

	film->FreeSampleBuffer(sampleBuffer);
}

void PathGPURenderEngine::Start() {
	RenderEngine::Start();

	for (size_t i = 0; i < renderThreads.size(); ++i)
		renderThreads[i]->Start();
}

void PathGPURenderEngine::Interrupt() {
	for (size_t i = 0; i < renderThreads.size(); ++i)
		renderThreads[i]->Interrupt();
}

void PathGPURenderEngine::Stop() {
	RenderEngine::Stop();

	for (size_t i = 0; i < renderThreads.size(); ++i)
		renderThreads[i]->Stop();
}

void PathGPURenderEngine::Reset() {
	assert (!started);
}

unsigned int PathGPURenderEngine::GetPass() const {
	unsigned int pass = 0;

	for (size_t i = 0; i < renderThreads.size(); ++i)
		pass += renderThreads[i]->GetPass();

	return pass;
}

unsigned int PathGPURenderEngine::GetThreadCount() const {
	return renderThreads.size();
}

void PathGPURenderEngine::UpdateFilm() {
	boost::unique_lock<boost::mutex> lock(*filmMutex);

	film->Reset();

	const unsigned int imgWidth = film->GetWidth();
	const unsigned int pixelCount = imgWidth * film->GetHeight();
	for (unsigned int p = 0; p < pixelCount; ++p) {
		Spectrum c;
		unsigned int count = 0;
		for (size_t i = 0; i < renderThreads.size(); ++i) {
			c += renderThreads[i]->frameBuffer[p].c;
			count += renderThreads[i]->frameBuffer[p].count;
		}

		if (count > 0) {
			const float scrX = p % imgWidth;
			const float scrY = p / imgWidth;
			c /= count;
			sampleBuffer->SplatSample(scrX, scrY, c);

			if (sampleBuffer->IsFull()) {
				// Splat all samples on the film
				film->SplatSampleBuffer(true, sampleBuffer);
				sampleBuffer = film->GetFreeSampleBuffer();
			}
		}

		if (sampleBuffer->GetSampleCount() > 0) {
			// Splat all samples on the film
			film->SplatSampleBuffer(true, sampleBuffer);
			sampleBuffer = film->GetFreeSampleBuffer();
		}
	}
}

#endif
