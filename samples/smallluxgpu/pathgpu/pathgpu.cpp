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
#include "luxrays/accelerators/mqbvhaccel.h"
#include "luxrays/accelerators/bvhaccel.h"

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
	const unsigned int pixelCount = renderEngine->film->GetWidth() * renderEngine->film->GetHeight();
	frameBuffer = new PathGPU::Pixel[pixelCount];

	for (unsigned int i = 0; i < pixelCount; ++i) {
		frameBuffer[i].c.r = 0.f;
		frameBuffer[i].c.g = 0.f;
		frameBuffer[i].c.b = 0.f;
		frameBuffer[i].count = 0;
	}
}

void PathGPURenderThread::Stop() {
	started = false;
	delete[] frameBuffer;
}

//------------------------------------------------------------------------------
// PathGPUDeviceRenderThread
//------------------------------------------------------------------------------

PathGPUDeviceRenderThread::PathGPUDeviceRenderThread(const unsigned int index, const unsigned int seedBase,
		const float samplStart,  const unsigned int samplePerPixel, OpenCLIntersectionDevice *device,
		PathGPURenderEngine *re) : PathGPURenderThread(index, re) {
	intersectionDevice = device;
	samplingStart = samplStart;
	seed = seedBase + index;
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

	// Check the used accelerator type
	if (renderEngine->scene->dataSet->GetAcceleratorType() == ACCEL_MQBVH)
		throw runtime_error("ACCEL_MQBVH is not yet supported by PathGPURenderEngine");

	const unsigned int startLine = Clamp<unsigned int>(
			renderEngine->film->GetHeight() * samplingStart,
			0, renderEngine->film->GetHeight() - 1);

	cl::Context &oclContext = intersectionDevice->GetOpenCLContext();
	cl::Device &oclDevice = intersectionDevice->GetOpenCLDevice();

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

	cerr << "[PathGPUDeviceRenderThread::" << threadIndex << "] Paths buffer size: " << (sizeof(PathGPU::Path) * PATHGPU_PATH_COUNT / 1024) << "Kbytes" << endl;
	pathsBuff = new cl::Buffer(oclContext,
			CL_MEM_READ_WRITE,
			sizeof(PathGPU::Path) * PATHGPU_PATH_COUNT);
	deviceDesc->AllocMemory(pathsBuff->getInfo<CL_MEM_SIZE>());

	cerr << "[PathGPUDeviceRenderThread::" << threadIndex << "] FrameBuffer buffer size: " << (sizeof(PathGPU::Pixel) * renderEngine->film->GetWidth() * renderEngine->film->GetHeight() / 1024) << "Kbytes" << endl;
	frameBufferBuff = new cl::Buffer(oclContext,
			CL_MEM_READ_WRITE,
			sizeof(PathGPU::Pixel) * renderEngine->film->GetWidth() * renderEngine->film->GetHeight());
	deviceDesc->AllocMemory(frameBufferBuff->getInfo<CL_MEM_SIZE>());

	const unsigned int trianglesCount = renderEngine->scene->dataSet->GetTotalTriangleCount();
	cerr << "[PathGPUDeviceRenderThread::" << threadIndex << "] MeshIDs buffer size: " << (sizeof(unsigned int) * trianglesCount / 1024) << "Kbytes" << endl;
	meshIDBuff = new cl::Buffer(oclContext,
			CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
			sizeof(unsigned int) * trianglesCount,
			(void *)renderEngine->scene->dataSet->GetMeshIDTable());
	deviceDesc->AllocMemory(meshIDBuff->getInfo<CL_MEM_SIZE>());

	cerr << "[PathGPUDeviceRenderThread::" << threadIndex << "] TriangleIDs buffer size: " << (sizeof(unsigned int) * trianglesCount / 1024) << "Kbytes" << endl;
	triIDBuff = new cl::Buffer(oclContext,
			CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
			sizeof(unsigned int) * trianglesCount,
			(void *)renderEngine->scene->dataSet->GetMeshTriangleIDTable());
	deviceDesc->AllocMemory(triIDBuff->getInfo<CL_MEM_SIZE>());

	//--------------------------------------------------------------------------
	// Translate material definitions
	//--------------------------------------------------------------------------

	const unsigned int materialsCount = renderEngine->scene->materials.size();
	PathGPU::Material *mats = new PathGPU::Material[materialsCount];
	for (unsigned int i = 0; i < materialsCount; ++i) {
		Material *m = renderEngine->scene->materials[i];
		PathGPU::Material *gpum = &mats[i];

		switch (m->GetType()) {
			case MATTE: {
				MatteMaterial *mm = (MatteMaterial *)m;

				gpum->type = MAT_MATTE;
				gpum->mat.matte.r = mm->GetKd().r;
				gpum->mat.matte.g = mm->GetKd().g;
				gpum->mat.matte.b = mm->GetKd().b;
				break;
			}
			case AREALIGHT: {
				AreaLightMaterial *alm = (AreaLightMaterial *)m;

				gpum->type = MAT_AREALIGHT;
				gpum->mat.areaLight.gain_r = alm->GetGain().r;
				gpum->mat.areaLight.gain_g = alm->GetGain().g;
				gpum->mat.areaLight.gain_b = alm->GetGain().b;
				break;
			}
			default: {
				gpum->type = MAT_MATTE;
				gpum->mat.matte.r = 0.75f;
				gpum->mat.matte.g = 0.75f;
				gpum->mat.matte.b = 0.75f;
				break;
			}
		}
	}

	cerr << "[PathGPUDeviceRenderThread::" << threadIndex << "] Materials buffer size: " << (sizeof(PathGPU::Material) * materialsCount / 1024) << "Kbytes" << endl;
	materialsBuff = new cl::Buffer(oclContext,
			CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
			sizeof(PathGPU::Material) * materialsCount,
			mats);
	deviceDesc->AllocMemory(materialsBuff->getInfo<CL_MEM_SIZE>());

	delete[] mats;

	//--------------------------------------------------------------------------
	// Translate mesh material indices
	//--------------------------------------------------------------------------

	const unsigned int meshCount = renderEngine->scene->objectMaterials.size();
	unsigned int *meshMats = new unsigned int[meshCount];
	for (unsigned int i = 0; i < meshCount; ++i) {
		Material *m = renderEngine->scene->objectMaterials[i];

		// Look for the index
		unsigned int index = 0;
		for (unsigned int j = 0; j < materialsCount; ++j) {
			if (m == renderEngine->scene->materials[j]) {
				index = j;
				break;
			}
		}

		meshMats[i] = index;
	}

	cerr << "[PathGPUDeviceRenderThread::" << threadIndex << "] Mesh material index buffer size: " << (sizeof(unsigned int) * meshCount / 1024) << "Kbytes" << endl;
	meshMatsBuff = new cl::Buffer(oclContext,
			CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
			sizeof(unsigned int) * meshCount,
			meshMats);
	deviceDesc->AllocMemory(meshMatsBuff->getInfo<CL_MEM_SIZE>());

	delete[] meshMats;

	//--------------------------------------------------------------------------
	// Check if there is an infinite light source
	//--------------------------------------------------------------------------

	InfiniteLight *infiniteLight = NULL;
	if (renderEngine->scene->infiniteLight)
		infiniteLight = renderEngine->scene->infiniteLight;
	else {
		// Look for the infinite light

		for (unsigned int i = 0; i < renderEngine->scene->lights.size(); ++i) {
			LightSource *l = renderEngine->scene->lights[i];

			if ((l->GetType() == TYPE_IL_BF) || (l->GetType() == TYPE_IL_PORTAL) ||
					(l->GetType() == TYPE_IL_IS)) {
				infiniteLight = (InfiniteLight *)l;
				break;
			}
		}
	}

	if (infiniteLight) {
		const TextureMap *texMap = infiniteLight->GetTexture()->GetTexMap();
		const unsigned int pixelCount = texMap->GetWidth() * texMap->GetHeight();

		cerr << "[PathGPUDeviceRenderThread::" << threadIndex << "] InfiniteLight buffer size: " << (sizeof(Spectrum) * pixelCount / 1024) << "Kbytes" << endl;
		infiniteLightBuff = new cl::Buffer(oclContext,
				CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
				sizeof(Spectrum) * pixelCount,
				(void *)texMap->GetPixels());
		deviceDesc->AllocMemory(infiniteLightBuff->getInfo<CL_MEM_SIZE>());
	} else
		infiniteLightBuff = NULL;

	//--------------------------------------------------------------------------
	// Translate mesh normals
	//--------------------------------------------------------------------------

	const unsigned int normalsCount = renderEngine->scene->dataSet->GetTotalVertexCount();
	Normal *normals = new Normal[normalsCount];
	unsigned int nIndex = 0;
	for (unsigned int i = 0; i < renderEngine->scene->objects.size(); ++i) {
		ExtMesh *mesh = renderEngine->scene->objects[i];

		for (unsigned int j = 0; j < mesh->GetTotalVertexCount(); ++j)
			normals[nIndex++] = mesh->GetNormal(j);
	}

	cerr << "[PathGPUDeviceRenderThread::" << threadIndex << "] Normals buffer size: " << (sizeof(Normal) * normalsCount / 1024) << "Kbytes" << endl;
	normalsBuff = new cl::Buffer(oclContext,
			CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
			sizeof(Normal) * normalsCount,
			normals);
	deviceDesc->AllocMemory(normalsBuff->getInfo<CL_MEM_SIZE>());
	delete[] normals;

	//--------------------------------------------------------------------------
	// Translate mesh indices
	//--------------------------------------------------------------------------

	const TriangleMesh *preprocessedMesh;
	switch (renderEngine->scene->dataSet->GetAcceleratorType()) {
		case ACCEL_BVH:
			preprocessedMesh = ((BVHAccel *)renderEngine->scene->dataSet->GetAccelerator())->GetPreprocessedMesh();
			break;
		case ACCEL_QBVH:
			preprocessedMesh = ((QBVHAccel *)renderEngine->scene->dataSet->GetAccelerator())->GetPreprocessedMesh();
			break;
		default:
			throw runtime_error("ACCEL_MQBVH is not yet supported by PathGPURenderEngine");
	}

	cerr << "[PathGPUDeviceRenderThread::" << threadIndex << "] Triangles buffer size: " << (sizeof(Triangle) * trianglesCount / 1024) << "Kbytes" << endl;
	trianglesBuff = new cl::Buffer(oclContext,
			CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
			sizeof(Triangle) * trianglesCount,
			(void *)preprocessedMesh->GetTriangles());
	deviceDesc->AllocMemory(trianglesBuff->getInfo<CL_MEM_SIZE>());

	//--------------------------------------------------------------------------
	// Compile kernels
	//--------------------------------------------------------------------------

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
			" -D PARAM_CLIP_HITHER=" << renderEngine->scene->camera->GetClipHither() <<
			" -D PARAM_SEED=" << seed <<
			" -D PARAM_MAX_PATH_DEPTH=" << renderEngine->maxPathDepth <<
			" -D PARAM_RR_DEPTH=" << renderEngine->rrDepth <<
			" -D PARAM_RR_CAP=" << renderEngine->rrImportanceCap <<
			" -D PARAM_SAMPLE_PER_PIXEL=" << renderEngine->samplePerPixel
			;

	if (infiniteLight) {
		ss <<
				" -D PARAM_HAVE_INFINITELIGHT=1" <<
				" -D PARAM_IL_GAIN_R=" << infiniteLight->GetGain().r <<
				" -D PARAM_IL_GAIN_G=" << infiniteLight->GetGain().g <<
				" -D PARAM_IL_GAIN_B=" << infiniteLight->GetGain().b <<
				" -D PARAM_IL_SHIFT_U=" << infiniteLight->GetShiftU() <<
				" -D PARAM_IL_SHIFT_V=" << infiniteLight->GetShiftV() <<
				" -D PARAM_IL_WIDTH=" << infiniteLight->GetTexture()->GetTexMap()->GetWidth() <<
				" -D PARAM_IL_HEIGHT=" << infiniteLight->GetTexture()->GetTexMap()->GetHeight()
				;
	}

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
	// Initialize
	//--------------------------------------------------------------------------

	// Clear the frame buffer
	cl::CommandQueue &oclQueue = intersectionDevice->GetOpenCLQueue();
	initFBKernel->setArg(0, *frameBufferBuff);
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
	advancePathKernel->setArg(3, *frameBufferBuff);
	advancePathKernel->setArg(4, *materialsBuff);
	advancePathKernel->setArg(5, *meshMatsBuff);
	advancePathKernel->setArg(6, *meshIDBuff);
	advancePathKernel->setArg(7, *triIDBuff);
	advancePathKernel->setArg(8, *normalsBuff);
	advancePathKernel->setArg(9, *trianglesBuff);
	if (infiniteLight)
		advancePathKernel->setArg(10, *infiniteLightBuff);

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
	deviceDesc->FreeMemory(frameBufferBuff->getInfo<CL_MEM_SIZE>());
	delete frameBufferBuff;
	deviceDesc->FreeMemory(materialsBuff->getInfo<CL_MEM_SIZE>());
	delete materialsBuff;
	deviceDesc->FreeMemory(meshIDBuff->getInfo<CL_MEM_SIZE>());
	delete meshIDBuff;
	deviceDesc->FreeMemory(triIDBuff->getInfo<CL_MEM_SIZE>());
	delete triIDBuff;
	deviceDesc->FreeMemory(meshMatsBuff->getInfo<CL_MEM_SIZE>());
	delete meshMatsBuff;
	deviceDesc->FreeMemory(normalsBuff->getInfo<CL_MEM_SIZE>());
	delete normalsBuff;
	deviceDesc->FreeMemory(trianglesBuff->getInfo<CL_MEM_SIZE>());
	delete trianglesBuff;
	if (infiniteLightBuff) {
		deviceDesc->FreeMemory(infiniteLightBuff->getInfo<CL_MEM_SIZE>());
		delete infiniteLightBuff;
	}

	delete initKernel;
	delete initFBKernel;
	delete advancePathKernel;

	PathGPURenderThread::Stop();
}

void PathGPUDeviceRenderThread::RenderThreadImpl(PathGPUDeviceRenderThread *renderThread) {
	cerr << "[PathGPUDeviceRenderThread::" << renderThread->threadIndex << "] Rendering thread started" << endl;

	try {
		cl::CommandQueue &oclQueue = renderThread->intersectionDevice->GetOpenCLQueue();
		const unsigned int pixelCount = renderThread->renderEngine->film->GetWidth() * renderThread->renderEngine->film->GetHeight();

		//const double refreshInterval = 1.0;
		const double refreshInterval = 3.0;

		// -refreshInterval is a trick to set the first frame buffer refresh after 1 sec
		double startTime = WallClockTime() - (refreshInterval / 2.0);
		while (!boost::this_thread::interruption_requested()) {
			if(renderThread->threadIndex == 0)
				cerr<< "[DEBUG] =================================" << endl;

			// Async. transfer of the frame buffer
			oclQueue.enqueueReadBuffer(
				*(renderThread->frameBufferBuff),
				CL_FALSE,
				0,
				sizeof(PathGPU::Pixel) * pixelCount,
				renderThread->frameBuffer);

			for(unsigned int j = 0;;++j) {
				cl::Event event;
				for (unsigned int i = 0; i < 32; ++i) {
					// Trace rays
					renderThread->intersectionDevice->EnqueueTraceRayBuffer(*(renderThread->raysBuff),
							*(renderThread->hitsBuff), PATHGPU_PATH_COUNT);

					// Advance paths
					if (i == 0)
						oclQueue.enqueueNDRangeKernel(*(renderThread->advancePathKernel), cl::NullRange,
							cl::NDRange(PATHGPU_PATH_COUNT), cl::NDRange(renderThread->advancePathWorkGroupSize), NULL, &event);
					else
						oclQueue.enqueueNDRangeKernel(*(renderThread->advancePathKernel), cl::NullRange,
							cl::NDRange(PATHGPU_PATH_COUNT), cl::NDRange(renderThread->advancePathWorkGroupSize));
				}
				oclQueue.flush();

				event.wait();
				const double elapsedTime = WallClockTime() - startTime;

				if(renderThread->threadIndex == 0)
					cerr<< "[DEBUG] =" << j << "="<< elapsedTime * 1000.0 << "ms" << endl;

				if ((elapsedTime > refreshInterval) || boost::this_thread::interruption_requested())
					break;
			}

			startTime = WallClockTime();
		}

		cerr << "[PathGPUDeviceRenderThread::" << renderThread->threadIndex << "] Rendering thread halted" << endl;
	} catch (boost::thread_interrupted) {
		cerr << "[PathGPUDeviceRenderThread::" << renderThread->threadIndex << "] Rendering thread halted" << endl;
	} catch (cl::Error err) {
		cerr << "[PathGPUDeviceRenderThread::" << renderThread->threadIndex << "] Rendering thread ERROR: " << err.what() << "(" << err.err() << ")" << endl;
	}
}

//------------------------------------------------------------------------------
// PathGPURenderEngine
//------------------------------------------------------------------------------

PathGPURenderEngine::PathGPURenderEngine(SLGScene *scn, Film *flm, boost::mutex *filmMutex,
		vector<IntersectionDevice *> intersectionDevices, const Properties &cfg) :
		RenderEngine(scn, flm, filmMutex) {
	samplePerPixel = max(1, cfg.GetInt("path.sampler.spp", cfg.GetInt("sampler.spp", 4)));
	samplePerPixel *= samplePerPixel;
	maxPathDepth = cfg.GetInt("path.maxdepth", 3);
	rrDepth = cfg.GetInt("path.russianroulette.depth", 2);
	rrImportanceCap = cfg.GetFloat("path.russianroulette.cap", 0.125f);

	startTime = 0.0;
	samplesCount = 0;

	sampleBuffer = film->GetFreeSampleBuffer();

	// Look for OpenCL devices
	for (size_t i = 0; i < intersectionDevices.size(); ++i) {
		if (intersectionDevices[i]->GetType() == DEVICE_TYPE_OPENCL)
			oclIntersectionDevices.push_back((OpenCLIntersectionDevice *)intersectionDevices[i]);
	}

	cerr << "Found "<< oclIntersectionDevices.size() << " OpenCL intersection devices for PathGPU render engine" << endl;

	if (oclIntersectionDevices.size() < 1)
		throw runtime_error("Unable to find an OpenCL intersection device for PathGPU render engine");

	const unsigned int seedBase = (unsigned long)(WallClockTime() / 1000.0);

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

	samplesCount = 0;
	startTime = WallClockTime();

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

unsigned int PathGPURenderEngine::GetThreadCount() const {
	return renderThreads.size();
}

void PathGPURenderEngine::UpdateFilm() {
	boost::unique_lock<boost::mutex> lock(*filmMutex);

	elapsedTime = WallClockTime() - startTime;
	film->Reset();

	const unsigned int imgWidth = film->GetWidth();
	const unsigned int pixelCount = imgWidth * film->GetHeight();
	unsigned long long totalCount = 0;
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

			totalCount += count;
		}

		if (sampleBuffer->GetSampleCount() > 0) {
			// Splat all samples on the film
			film->SplatSampleBuffer(true, sampleBuffer);
			sampleBuffer = film->GetFreeSampleBuffer();
		}
	}

	samplesCount = totalCount;
}

unsigned int PathGPURenderEngine::GetPass() const {
	return samplesCount / (film->GetWidth() * film->GetHeight());
}

#endif
