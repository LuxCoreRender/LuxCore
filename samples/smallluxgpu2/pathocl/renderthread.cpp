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

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <string.h>
#include <string>
#include <sstream>
#include <stdexcept>

#include <boost/thread/mutex.hpp>

#include "smalllux.h"

#include "pathocl/pathocl.h"
#include "pathocl/kernels/kernels.h"
#include "renderconfig.h"
#include "luxrays/core/geometry/transform.h"
#include "luxrays/accelerators/mqbvhaccel.h"
#include "luxrays/accelerators/bvhaccel.h"
#include "luxrays/core/pixel/samplebuffer.h"

//------------------------------------------------------------------------------
// PathOCLRenderThread
//------------------------------------------------------------------------------

PathOCLRenderThread::PathOCLRenderThread(const unsigned int index, const unsigned int seedBase,
		const float samplStart, OpenCLIntersectionDevice *device,
		PathOCLRenderEngine *re) {
	intersectionDevice = device;
	samplingStart = samplStart;
	seed = seedBase;
	reportedPermissionError = false;

	renderThread = NULL;

	threadIndex = index;
	renderEngine = re;
	started = false;
	editMode = false;
	frameBuffer = NULL;

	kernelsParameters = "";
	initKernel = NULL;
	initFBKernel = NULL;
	samplerKernel = NULL;
	advancePathsKernel = NULL;

	raysBuff = NULL;
	hitsBuff = NULL;
	tasksBuff = NULL;
	taskStatsBuff = NULL;
	frameBufferBuff = NULL;
	materialsBuff = NULL;
	meshIDBuff = NULL;
	triangleIDBuff = NULL;
	meshDescsBuff = NULL;
	meshMatsBuff = NULL;
	infiniteLightBuff = NULL;
	infiniteLightMapBuff = NULL;
	sunLightBuff = NULL;
	skyLightBuff = NULL;
	vertsBuff = NULL;
	normalsBuff = NULL;
	trianglesBuff = NULL;
	colorsBuff = NULL;
	cameraBuff = NULL;
	triLightsBuff = NULL;
	texMapRGBBuff = NULL;
	texMapAlphaBuff = NULL;
	texMapDescBuff = NULL;
	meshTexsBuff = NULL;
	meshBumpsBuff = NULL;
	meshBumpsScaleBuff = NULL;
	uvsBuff = NULL;

	gpuTaskStats = new PathOCL::GPUTaskStats[renderEngine->taskCount];
}

PathOCLRenderThread::~PathOCLRenderThread() {
	if (editMode)
		EndEdit(EditActionList());
	if (started)
		Stop();

	delete initKernel;
	delete initFBKernel;
	delete samplerKernel;
	delete advancePathsKernel;

	delete[] frameBuffer;
	delete[] gpuTaskStats;
}

void PathOCLRenderThread::AllocOCLBufferRO(cl::Buffer **buff, void *src, const size_t size, const string &desc) {
	const OpenCLDeviceDescription *deviceDesc = intersectionDevice->GetDeviceDesc();
	if (*buff) {
		// Check the size of the already allocated buffer

		if (size == (*buff)->getInfo<CL_MEM_SIZE>()) {
			// I can reuse the buffer; just update the content

			//cerr << "[PathOCLRenderThread::" << threadIndex << "] " << desc << " buffer updated for size: " << (size / 1024) << "Kbytes" << endl;
			cl::CommandQueue &oclQueue = intersectionDevice->GetOpenCLQueue();
			oclQueue.enqueueWriteBuffer(**buff, CL_FALSE, 0, size, src);
			return;
		}
	}

	cl::Context &oclContext = intersectionDevice->GetOpenCLContext();

	cerr << "[PathOCLRenderThread::" << threadIndex << "] " << desc << " buffer size: " << (size / 1024) << "Kbytes" << endl;
	*buff = new cl::Buffer(oclContext,
			CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
			size, src);
	deviceDesc->AllocMemory((*buff)->getInfo<CL_MEM_SIZE>());
}

void PathOCLRenderThread::AllocOCLBufferRW(cl::Buffer **buff, const size_t size, const string &desc) {
	const OpenCLDeviceDescription *deviceDesc = intersectionDevice->GetDeviceDesc();
	if (*buff) {
		// Check the size of the already allocated buffer

		if (size == (*buff)->getInfo<CL_MEM_SIZE>()) {
			// I can reuse the buffer
			//cerr << "[PathOCLRenderThread::" << threadIndex << "] " << desc << " buffer reused for size: " << (size / 1024) << "Kbytes" << endl;
			return;
		}
	}

	cl::Context &oclContext = intersectionDevice->GetOpenCLContext();

	cerr << "[PathOCLRenderThread::" << threadIndex << "] " << desc << " buffer size: " << (size / 1024) << "Kbytes" << endl;
	*buff = new cl::Buffer(oclContext,
			CL_MEM_READ_WRITE,
			size);
	deviceDesc->AllocMemory((*buff)->getInfo<CL_MEM_SIZE>());
}

void PathOCLRenderThread::FreeOCLBuffer(cl::Buffer **buff) {
	if (*buff) {
		const OpenCLDeviceDescription *deviceDesc = intersectionDevice->GetDeviceDesc();

		deviceDesc->FreeMemory((*buff)->getInfo<CL_MEM_SIZE>());
		delete *buff;
		*buff = NULL;
	}
}

void PathOCLRenderThread::InitFrameBuffer() {
	//--------------------------------------------------------------------------
	// FrameBuffer definition
	//--------------------------------------------------------------------------

	frameBufferPixelCount =	(renderEngine->film->GetWidth() + 2) * (renderEngine->film->GetHeight() + 2);

	// Delete previous allocated frameBuffer
	delete[] frameBuffer;
	frameBuffer = new PathOCL::Pixel[frameBufferPixelCount];

	for (unsigned int i = 0; i < frameBufferPixelCount; ++i) {
		frameBuffer[i].c.r = 0.f;
		frameBuffer[i].c.g = 0.f;
		frameBuffer[i].c.b = 0.f;
		frameBuffer[i].count = 0.f;
	}

	AllocOCLBufferRW(&frameBufferBuff, sizeof(PathOCL::Pixel) * frameBufferPixelCount, "FrameBuffer");
}

void PathOCLRenderThread::InitCamera() {
	AllocOCLBufferRO(&cameraBuff, &renderEngine->compiledScene->camera,
			sizeof(PathOCL::Camera), "Camera");
}

void PathOCLRenderThread::InitGeometry() {
	Scene *scene = renderEngine->renderConfig->scene;
	CompiledScene *cscene = renderEngine->compiledScene;

	const unsigned int trianglesCount = scene->dataSet->GetTotalTriangleCount();
	AllocOCLBufferRO(&meshIDBuff, (void *)cscene->meshIDs,
			sizeof(unsigned int) * trianglesCount, "MeshIDs");

	AllocOCLBufferRO(&colorsBuff, &cscene->colors[0],
		sizeof(Spectrum) * cscene->colors.size(), "Colors");

	AllocOCLBufferRO(&normalsBuff, &cscene->normals[0],
		sizeof(Normal) * cscene->normals.size(), "Normals");

	if (cscene->uvs.size() > 0)
		AllocOCLBufferRO(&uvsBuff, &cscene->uvs[0],
			sizeof(UV) * cscene->uvs.size(), "UVs");
	else
		uvsBuff = NULL;

	AllocOCLBufferRO(&vertsBuff, &cscene->verts[0],
		sizeof(Point) * cscene->verts.size(), "Vertices");

	AllocOCLBufferRO(&trianglesBuff, &cscene->tris[0],
		sizeof(Triangle) * cscene->tris.size(), "Triangles");

	// Check the used accelerator type
	if (scene->dataSet->GetAcceleratorType() == ACCEL_MQBVH) {
		// MQBVH geometry must be defined in a specific way.

		AllocOCLBufferRO(&triangleIDBuff, (void *)cscene->triangleIDs,
				sizeof(unsigned int) * trianglesCount, "TriangleIDs");

		AllocOCLBufferRO(&meshDescsBuff, &cscene->meshDescs[0],
				sizeof(PathOCL::Mesh) * cscene->meshDescs.size(), "Mesh description");
	} else {
		triangleIDBuff = NULL;
		meshDescsBuff = NULL;
	}
}

void PathOCLRenderThread::InitMaterials() {
	const size_t materialsCount = renderEngine->compiledScene->mats.size();
	AllocOCLBufferRO(&materialsBuff, &renderEngine->compiledScene->mats[0],
			sizeof(PathOCL::Material) * materialsCount, "Materials");

	const unsigned int meshCount = renderEngine->compiledScene->meshMats.size();
	AllocOCLBufferRO(&meshMatsBuff, &renderEngine->compiledScene->meshMats[0],
			sizeof(unsigned int) * meshCount, "Mesh material index");
}

void PathOCLRenderThread::InitKernels() {
	//--------------------------------------------------------------------------
	// Compile kernels
	//--------------------------------------------------------------------------

	CompiledScene *cscene = renderEngine->compiledScene;
	cl::Context &oclContext = intersectionDevice->GetOpenCLContext();
	cl::Device &oclDevice = intersectionDevice->GetOpenCLDevice();
	const OpenCLDeviceDescription *deviceDesc = intersectionDevice->GetDeviceDesc();

	// Set #define symbols
	stringstream ss;
	ss.precision(6);
	ss << scientific <<
			" -D PARAM_TASK_COUNT=" << renderEngine->taskCount <<
			" -D PARAM_IMAGE_WIDTH=" << renderEngine->film->GetWidth() <<
			" -D PARAM_IMAGE_HEIGHT=" << renderEngine->film->GetHeight() <<
			" -D PARAM_RAY_EPSILON=" << RAY_EPSILON << "f" <<
			" -D PARAM_SEED=" << seed <<
			" -D PARAM_MAX_PATH_DEPTH=" << renderEngine->maxPathDepth <<
			" -D PARAM_RR_DEPTH=" << renderEngine->rrDepth <<
			" -D PARAM_RR_CAP=" << renderEngine->rrImportanceCap << "f"
			;

	switch (renderEngine->renderConfig->scene->dataSet->GetAcceleratorType()) {
		case ACCEL_BVH:
			ss << " -D PARAM_ACCEL_BVH";
			break;
		case ACCEL_QBVH:
			ss << " -D PARAM_ACCEL_QBVH";
			break;
		case ACCEL_MQBVH:
			ss << " -D PARAM_ACCEL_MQBVH";
			break;
		default:
			assert (false);
	}

	if (cscene->enable_MAT_MATTE)
		ss << " -D PARAM_ENABLE_MAT_MATTE";
	if (cscene->enable_MAT_AREALIGHT)
		ss << " -D PARAM_ENABLE_MAT_AREALIGHT";
	if (cscene->enable_MAT_MIRROR)
		ss << " -D PARAM_ENABLE_MAT_MIRROR";
	if (cscene->enable_MAT_GLASS)
		ss << " -D PARAM_ENABLE_MAT_GLASS";
	if (cscene->enable_MAT_MATTEMIRROR)
		ss << " -D PARAM_ENABLE_MAT_MATTEMIRROR";
	if (cscene->enable_MAT_METAL)
		ss << " -D PARAM_ENABLE_MAT_METAL";
	if (cscene->enable_MAT_MATTEMETAL)
		ss << " -D PARAM_ENABLE_MAT_MATTEMETAL";
	if (cscene->enable_MAT_ALLOY)
		ss << " -D PARAM_ENABLE_MAT_ALLOY";
	if (cscene->enable_MAT_ARCHGLASS)
		ss << " -D PARAM_ENABLE_MAT_ARCHGLASS";

	if (cscene->camera.lensRadius > 0.f)
		ss << " -D PARAM_CAMERA_HAS_DOF";


	if (infiniteLightBuff)
		ss << " -D PARAM_HAS_INFINITELIGHT";

	if (skyLightBuff)
		ss << " -D PARAM_HAS_SKYLIGHT";

	if (sunLightBuff) {
		ss << " -D PARAM_HAS_SUNLIGHT";

		if (!triLightsBuff) {
			ss <<
				" -D PARAM_DIRECT_LIGHT_SAMPLING" <<
				" -D PARAM_DL_LIGHT_COUNT=0"
				;
		}
	}

	if (triLightsBuff) {
		ss <<
				" -D PARAM_DIRECT_LIGHT_SAMPLING" <<
				" -D PARAM_DL_LIGHT_COUNT=" << areaLightCount
				;
	}

	if (texMapRGBBuff || texMapAlphaBuff)
		ss << " -D PARAM_HAS_TEXTUREMAPS";
	if (texMapAlphaBuff)
		ss << " -D PARAM_HAS_ALPHA_TEXTUREMAPS";
	if (meshBumpsBuff)
		ss << " -D PARAM_HAS_BUMPMAPS";

	const PathOCL::Filter *filter = renderEngine->filter;
	switch (filter->type) {
		case PathOCL::NONE:
			ss << " -D PARAM_IMAGE_FILTER_TYPE=0";
			break;
		case PathOCL::BOX:
			ss << " -D PARAM_IMAGE_FILTER_TYPE=1" <<
					" -D PARAM_IMAGE_FILTER_WIDTH_X=" << filter->widthX << "f" <<
					" -D PARAM_IMAGE_FILTER_WIDTH_Y=" << filter->widthY << "f";
			break;
		case PathOCL::GAUSSIAN:
			ss << " -D PARAM_IMAGE_FILTER_TYPE=2" <<
					" -D PARAM_IMAGE_FILTER_WIDTH_X=" << filter->widthX << "f" <<
					" -D PARAM_IMAGE_FILTER_WIDTH_Y=" << filter->widthY << "f" <<
					" -D PARAM_IMAGE_FILTER_GAUSSIAN_ALPHA=" << ((PathOCL::GaussianFilter *)filter)->alpha << "f";
			break;
		case PathOCL::MITCHELL:
			ss << " -D PARAM_IMAGE_FILTER_TYPE=3" <<
					" -D PARAM_IMAGE_FILTER_WIDTH_X=" << filter->widthX << "f" <<
					" -D PARAM_IMAGE_FILTER_WIDTH_Y=" << filter->widthY << "f" <<
					" -D PARAM_IMAGE_FILTER_MITCHELL_B=" << ((PathOCL::MitchellFilter *)filter)->B << "f" <<
					" -D PARAM_IMAGE_FILTER_MITCHELL_C=" << ((PathOCL::MitchellFilter *)filter)->C << "f";
			break;
		default:
			assert (false);
	}

	if (renderEngine->usePixelAtomics)
		ss << " -D PARAM_USE_PIXEL_ATOMICS";

	const PathOCL::Sampler *sampler = renderEngine->sampler;
	switch (sampler->type) {
		case PathOCL::INLINED_RANDOM:
			ss << " -D PARAM_SAMPLER_TYPE=0";
			break;
		case PathOCL::RANDOM:
			ss << " -D PARAM_SAMPLER_TYPE=1";
			break;
		case PathOCL::METROPOLIS:
			ss << " -D PARAM_SAMPLER_TYPE=2" <<
					" -D PARAM_SAMPLER_METROPOLIS_LARGE_STEP_RATE=" << ((PathOCL::MetropolisSampler *)sampler)->largeStepRate << "f" <<
					" -D PARAM_SAMPLER_METROPOLIS_MAX_CONSECUTIVE_REJECT=" << ((PathOCL::MetropolisSampler *)sampler)->maxConsecutiveReject;
			break;
		case PathOCL::STRATIFIED:
			ss << " -D PARAM_SAMPLER_TYPE=3" <<
					" -D PARAM_SAMPLER_STRATIFIED_X_SAMPLES=" << ((PathOCL::StratifiedSampler *)sampler)->xSamples <<
					" -D PARAM_SAMPLER_STRATIFIED_Y_SAMPLES=" << ((PathOCL::StratifiedSampler *)sampler)->ySamples;
			break;
		default:
			assert (false);
	}

	// Check the OpenCL vendor and use some specific compiler options
	if (deviceDesc->IsAMDPlatform())
		ss << " -fno-alias";

#if defined(__APPLE__)
	ss << " -D __APPLE__";
#endif

	//--------------------------------------------------------------------------

	const double tStart = WallClockTime();

	// Check if I have to recompile the kernels
	string newKernelParameters = ss.str();
	if (kernelsParameters != newKernelParameters) {
		kernelsParameters = newKernelParameters;

		// Compile sources
		stringstream ssKernel;
		ssKernel << KernelSource_PathOCL_kernel_datatypes << KernelSource_PathOCL_kernel_core <<
				 KernelSource_PathOCL_kernel_filters << KernelSource_PathOCL_kernel_scene <<
				KernelSource_PathOCL_kernel_samplers << KernelSource_PathOCL_kernels;
		string kernelSource = ssKernel.str();

		cl::Program::Sources source(1, std::make_pair(kernelSource.c_str(), kernelSource.length()));
		cl::Program program = cl::Program(oclContext, source);

		try {
			cerr << "[PathOCLRenderThread::" << threadIndex << "] Defined symbols: " << kernelsParameters << endl;
			cerr << "[PathOCLRenderThread::" << threadIndex << "] Compiling kernels " << endl;

			VECTOR_CLASS<cl::Device> buildDevice;
			buildDevice.push_back(oclDevice);
			program.build(buildDevice, kernelsParameters.c_str());
		} catch (cl::Error err) {
			cl::STRING_CLASS strError = program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(oclDevice);
			cerr << "[PathOCLRenderThread::" << threadIndex << "] PathOCL compilation error:\n" << strError.c_str() << endl;

			throw err;
		}

		//----------------------------------------------------------------------
		// Init kernel
		//----------------------------------------------------------------------

		delete initKernel;
		cerr << "[PathOCLRenderThread::" << threadIndex << "] Compiling Init Kernel" << endl;
		initKernel = new cl::Kernel(program, "Init");
		initKernel->getWorkGroupInfo<size_t>(oclDevice, CL_KERNEL_WORK_GROUP_SIZE, &initWorkGroupSize);
		cerr << "[PathOCLRenderThread::" << threadIndex << "] PathOCL Init kernel work group size: " << initWorkGroupSize << endl;

		initKernel->getWorkGroupInfo<size_t>(oclDevice, CL_KERNEL_WORK_GROUP_SIZE, &initWorkGroupSize);
		cerr << "[PathOCLRenderThread::" << threadIndex << "] Suggested work group size: " << initWorkGroupSize << endl;

		if (intersectionDevice->GetForceWorkGroupSize() > 0) {
			initWorkGroupSize = intersectionDevice->GetForceWorkGroupSize();
			cerr << "[PathOCLRenderThread::" << threadIndex << "] Forced work group size: " << initWorkGroupSize << endl;
		} else if (renderEngine->sampler->type == PathOCL::STRATIFIED) {
			// Resize the workgroup to have enough local memory
			size_t localMem = oclDevice.getInfo<CL_DEVICE_LOCAL_MEM_SIZE>();

			while ((initWorkGroupSize > 64) && (stratifiedDataSize * initWorkGroupSize > localMem))
				initWorkGroupSize /= 2;

			if (stratifiedDataSize * initWorkGroupSize > localMem)
				throw std::runtime_error("Not enough local memory to run, try to reduce path.sampler.xsamples and path.sampler.xsamples values");

			cerr << "[PathOCLRenderThread::" << threadIndex << "] Cap work group size to: " << initWorkGroupSize << endl;
		}

		//--------------------------------------------------------------------------
		// InitFB kernel
		//--------------------------------------------------------------------------

		delete initFBKernel;
		initFBKernel = new cl::Kernel(program, "InitFrameBuffer");
		initFBKernel->getWorkGroupInfo<size_t>(oclDevice, CL_KERNEL_WORK_GROUP_SIZE, &initFBWorkGroupSize);
		cerr << "[PathOCLRenderThread::" << threadIndex << "] PathOCL InitFrameBuffer kernel work group size: " << initFBWorkGroupSize << endl;

		initFBKernel->getWorkGroupInfo<size_t>(oclDevice, CL_KERNEL_WORK_GROUP_SIZE, &initFBWorkGroupSize);
		cerr << "[PathOCLRenderThread::" << threadIndex << "] Suggested work group size: " << initFBWorkGroupSize << endl;

		if (intersectionDevice->GetForceWorkGroupSize() > 0) {
			initFBWorkGroupSize = intersectionDevice->GetForceWorkGroupSize();
			cerr << "[PathOCLRenderThread::" << threadIndex << "] Forced work group size: " << initFBWorkGroupSize << endl;
		}

		//----------------------------------------------------------------------
		// Sampler kernel
		//----------------------------------------------------------------------

		delete samplerKernel;
		cerr << "[PathOCLRenderThread::" << threadIndex << "] Compiling Sampler Kernel" << endl;
		samplerKernel = new cl::Kernel(program, "Sampler");
		samplerKernel->getWorkGroupInfo<size_t>(oclDevice, CL_KERNEL_WORK_GROUP_SIZE, &samplerWorkGroupSize);
		cerr << "[PathOCLRenderThread::" << threadIndex << "] PathOCL Sampler kernel work group size: " << samplerWorkGroupSize << endl;

		samplerKernel->getWorkGroupInfo<size_t>(oclDevice, CL_KERNEL_WORK_GROUP_SIZE, &samplerWorkGroupSize);
		cerr << "[PathOCLRenderThread::" << threadIndex << "] Suggested work group size: " << samplerWorkGroupSize << endl;

		if (intersectionDevice->GetForceWorkGroupSize() > 0) {
			samplerWorkGroupSize = intersectionDevice->GetForceWorkGroupSize();
			cerr << "[PathOCLRenderThread::" << threadIndex << "] Forced work group size: " << samplerWorkGroupSize << endl;
		} else if (renderEngine->sampler->type == PathOCL::STRATIFIED) {
			// Resize the workgroup to have enough local memory
			size_t localMem = oclDevice.getInfo<CL_DEVICE_LOCAL_MEM_SIZE>();

			while ((samplerWorkGroupSize > 64) && (stratifiedDataSize * samplerWorkGroupSize > localMem))
				samplerWorkGroupSize /= 2;

			if (stratifiedDataSize * samplerWorkGroupSize > localMem)
				throw std::runtime_error("Not enough local memory to run, try to reduce path.sampler.xsamples and path.sampler.xsamples values");

			cerr << "[PathOCLRenderThread::" << threadIndex << "] Cap work group size to: " << samplerWorkGroupSize << endl;
		}

		//----------------------------------------------------------------------
		// AdvancePaths kernel
		//----------------------------------------------------------------------

		delete advancePathsKernel;
		cerr << "[PathOCLRenderThread::" << threadIndex << "] Compiling AdvancePaths Kernel" << endl;
		advancePathsKernel = new cl::Kernel(program, "AdvancePaths");
		advancePathsKernel->getWorkGroupInfo<size_t>(oclDevice, CL_KERNEL_WORK_GROUP_SIZE, &advancePathsWorkGroupSize);
		cerr << "[PathOCLRenderThread::" << threadIndex << "] PathOCL AdvancePaths kernel work group size: " << advancePathsWorkGroupSize << endl;

		advancePathsKernel->getWorkGroupInfo<size_t>(oclDevice, CL_KERNEL_WORK_GROUP_SIZE, &advancePathsWorkGroupSize);
		cerr << "[PathOCLRenderThread::" << threadIndex << "] Suggested work group size: " << advancePathsWorkGroupSize << endl;

		if (intersectionDevice->GetForceWorkGroupSize() > 0) {
			advancePathsWorkGroupSize = intersectionDevice->GetForceWorkGroupSize();
			cerr << "[PathOCLRenderThread::" << threadIndex << "] Forced work group size: " << advancePathsWorkGroupSize << endl;
		}

		//----------------------------------------------------------------------

		const double tEnd = WallClockTime();
		cerr  << "[PathOCLRenderThread::" << threadIndex << "] Kernels compilation time: " << int((tEnd - tStart) * 1000.0) << "ms" << endl;
	} else
		cerr << "[PathOCLRenderThread::" << threadIndex << "] Using cached kernels" << endl;
}

void PathOCLRenderThread::InitRender() {
	Scene *scene = renderEngine->renderConfig->scene;

	cl::Context &oclContext = intersectionDevice->GetOpenCLContext();
	cl::Device &oclDevice = intersectionDevice->GetOpenCLDevice();
	const OpenCLDeviceDescription *deviceDesc = intersectionDevice->GetDeviceDesc();

	double tStart, tEnd;

	//--------------------------------------------------------------------------
	// FrameBuffer definition
	//--------------------------------------------------------------------------

	InitFrameBuffer();

	//--------------------------------------------------------------------------
	// Allocate buffers
	//--------------------------------------------------------------------------

	const unsigned int taskCount = renderEngine->taskCount;

	tStart = WallClockTime();

	cerr << "[PathOCLRenderThread::" << threadIndex << "] Ray buffer size: " << (sizeof(Ray) * taskCount / 1024) << "Kbytes" << endl;
	raysBuff = new cl::Buffer(oclContext,
			CL_MEM_READ_WRITE,
			sizeof(Ray) * taskCount);
	deviceDesc->AllocMemory(raysBuff->getInfo<CL_MEM_SIZE>());

	cerr << "[PathOCLRenderThread::" << threadIndex << "] RayHit buffer size: " << (sizeof(RayHit) * taskCount / 1024) << "Kbytes" << endl;
	hitsBuff = new cl::Buffer(oclContext,
			CL_MEM_READ_WRITE,
			sizeof(RayHit) * taskCount);
	deviceDesc->AllocMemory(hitsBuff->getInfo<CL_MEM_SIZE>());

	tEnd = WallClockTime();
	cerr << "[PathOCLRenderThread::" << threadIndex << "] OpenCL buffer creation time: " << int((tEnd - tStart) * 1000.0) << "ms" << endl;

	//--------------------------------------------------------------------------
	// Camera definition
	//--------------------------------------------------------------------------

	InitCamera();

	//--------------------------------------------------------------------------
	// Scene geometry
	//--------------------------------------------------------------------------

	InitGeometry();

	//--------------------------------------------------------------------------
	// Translate material definitions
	//--------------------------------------------------------------------------

	InitMaterials();

	//--------------------------------------------------------------------------
	// Translate area lights
	//--------------------------------------------------------------------------

	tStart = WallClockTime();

	// Count the area lights
	areaLightCount = 0;
	for (unsigned int i = 0; i < scene->lights.size(); ++i) {
		if (scene->lights[i]->IsAreaLight())
			++areaLightCount;
	}

	if (areaLightCount > 0) {
		PathOCL::TriangleLight *tals = new PathOCL::TriangleLight[areaLightCount];

		unsigned int index = 0;
		for (unsigned int i = 0; i < scene->lights.size(); ++i) {
			if (scene->lights[i]->IsAreaLight()) {
				const TriangleLight *tl = (TriangleLight *)scene->lights[i];

				const ExtMesh *mesh = scene->objects[tl->GetMeshIndex()];
				const Triangle *tri = &(mesh->GetTriangles()[tl->GetTriIndex()]);
				tals[index].v0 = mesh->GetVertex(tri->v[0]);
				tals[index].v1 = mesh->GetVertex(tri->v[1]);
				tals[index].v2 = mesh->GetVertex(tri->v[2]);

				tals[index].normal = mesh->GetNormal(tri->v[0]);

				tals[index].area = tl->GetArea();

				AreaLightMaterial *alm = (AreaLightMaterial *)tl->GetMaterial();
				tals[index].gain_r = alm->GetGain().r;
				tals[index].gain_g = alm->GetGain().g;
				tals[index].gain_b = alm->GetGain().b;

				++index;
			}
		}

		cerr << "[PathOCLRenderThread::" << threadIndex << "] Triangle lights buffer size: " << (sizeof(PathOCL::TriangleLight) * areaLightCount / 1024) << "Kbytes" << endl;
		triLightsBuff = new cl::Buffer(oclContext,
				CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
				sizeof(PathOCL::TriangleLight) * areaLightCount,
				tals);
		deviceDesc->AllocMemory(triLightsBuff->getInfo<CL_MEM_SIZE>());
		delete[] tals;
	} else
		triLightsBuff = NULL;

	tEnd = WallClockTime();
	cerr << "[PathOCLRenderThread::" << threadIndex << "] Area lights translation time: " << int((tEnd - tStart) * 1000.0) << "ms" << endl;

	//--------------------------------------------------------------------------
	// Check if there is an infinite light source
	//--------------------------------------------------------------------------

	tStart = WallClockTime();

	InfiniteLight *infiniteLight = NULL;
	if (scene->infiniteLight && (
			(scene->infiniteLight->GetType() == TYPE_IL_BF) ||
			(scene->infiniteLight->GetType() == TYPE_IL_PORTAL) ||
			(scene->infiniteLight->GetType() == TYPE_IL_IS)))
		infiniteLight = scene->infiniteLight;
	else {
		// Look for the infinite light

		for (unsigned int i = 0; i < scene->lights.size(); ++i) {
			LightSource *l = scene->lights[i];

			if ((l->GetType() == TYPE_IL_BF) || (l->GetType() == TYPE_IL_PORTAL) ||
					(l->GetType() == TYPE_IL_IS)) {
				infiniteLight = (InfiniteLight *)l;
				break;
			}
		}
	}

	if (infiniteLight) {
		PathOCL::InfiniteLight il;

		il.gain = infiniteLight->GetGain();
		il.shiftU = infiniteLight->GetShiftU();
		il.shiftV = infiniteLight->GetShiftV();
		const TextureMap *texMap = infiniteLight->GetTexture()->GetTexMap();
		il.width = texMap->GetWidth();
		il.height = texMap->GetHeight();

		cerr << "[PathOCLRenderThread::" << threadIndex << "] InfiniteLight buffer size: " << (sizeof(PathOCL::InfiniteLight) / 1024) << "Kbytes" << endl;
		infiniteLightBuff = new cl::Buffer(oclContext,
				CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
				sizeof(PathOCL::InfiniteLight),
				(void *)&il);
		deviceDesc->AllocMemory(infiniteLightBuff->getInfo<CL_MEM_SIZE>());

		const unsigned int pixelCount = il.width * il.height;
		cerr << "[PathOCLRenderThread::" << threadIndex << "] InfiniteLight Map buffer size: " << (sizeof(Spectrum) * pixelCount / 1024) << "Kbytes" << endl;
		infiniteLightMapBuff = new cl::Buffer(oclContext,
				CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
				sizeof(Spectrum) * pixelCount,
				(void *)texMap->GetPixels());
		deviceDesc->AllocMemory(infiniteLightMapBuff->getInfo<CL_MEM_SIZE>());
	} else {
		infiniteLightBuff = NULL;
		infiniteLightMapBuff = NULL;
	}

	tEnd = WallClockTime();
	cerr << "[PathOCLRenderThread::" << threadIndex << "] Infinitelight translation time: " << int((tEnd - tStart) * 1000.0) << "ms" << endl;

	//--------------------------------------------------------------------------
	// Check if there is an sun light source
	//--------------------------------------------------------------------------

	SunLight *sunLight = NULL;

	// Look for the sun light
	for (unsigned int i = 0; i < scene->lights.size(); ++i) {
		LightSource *l = scene->lights[i];

		if (l->GetType() == TYPE_SUN) {
			sunLight = (SunLight *)l;
			break;
		}
	}

	if (sunLight) {
		PathOCL::SunLight sl;

		sl.sundir = sunLight->GetDir();
		sl.gain = sunLight->GetGain();
		sl.turbidity = sunLight->GetTubidity();
		sl.relSize= sunLight->GetRelSize();
		float tmp;
		sunLight->GetInitData(&sl.x, &sl.y, &tmp, &tmp, &tmp,
				&sl.cosThetaMax, &tmp, &sl.suncolor);

		cerr << "[PathOCLRenderThread::" << threadIndex << "] SunLight buffer size: " << (sizeof(PathOCL::SunLight) / 1024) << "Kbytes" << endl;
		sunLightBuff = new cl::Buffer(oclContext,
				CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
				sizeof(PathOCL::SunLight),
				(void *)&sl);
		deviceDesc->AllocMemory(sunLightBuff->getInfo<CL_MEM_SIZE>());
	} else
		sunLightBuff = NULL;

	//--------------------------------------------------------------------------
	// Check if there is an sky light source
	//--------------------------------------------------------------------------

	SkyLight *skyLight = NULL;

	if (scene->infiniteLight && (scene->infiniteLight->GetType() == TYPE_IL_SKY))
		skyLight = (SkyLight *)scene->infiniteLight;
	else {
		// Look for the sky light
		for (unsigned int i = 0; i < scene->lights.size(); ++i) {
			LightSource *l = scene->lights[i];

			if (l->GetType() == TYPE_IL_SKY) {
				skyLight = (SkyLight *)l;
				break;
			}
		}
	}

	if (skyLight) {
		PathOCL::SkyLight sl;

		sl.gain = skyLight->GetGain();
		skyLight->GetInitData(&sl.thetaS, &sl.phiS,
				&sl.zenith_Y, &sl.zenith_x, &sl.zenith_y,
				sl.perez_Y, sl.perez_x, sl.perez_y);

		cerr << "[PathOCLRenderThread::" << threadIndex << "] SkyLight buffer size: " << (sizeof(PathOCL::SkyLight) / 1024) << "Kbytes" << endl;
		skyLightBuff = new cl::Buffer(oclContext,
				CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
				sizeof(PathOCL::SkyLight),
				(void *)&sl);
		deviceDesc->AllocMemory(skyLightBuff->getInfo<CL_MEM_SIZE>());
	} else
		skyLightBuff = NULL;

	if (!skyLight && !sunLight && !infiniteLight && (areaLightCount == 0))
		throw runtime_error("There are no light sources supported by PathOCL in the scene");

	//--------------------------------------------------------------------------
	// Translate mesh texture maps
	//--------------------------------------------------------------------------

	tStart = WallClockTime();

	std::vector<TextureMap *> tms;
	scene->texMapCache->GetTexMaps(tms);
	// Calculate the amount of ram to allocate
	unsigned int totRGBTexMem = 0;
	unsigned int totAlphaTexMem = 0;

	for (unsigned int i = 0; i < tms.size(); ++i) {
		TextureMap *tm = tms[i];
		const unsigned int pixelCount = tm->GetWidth() * tm->GetHeight();

		totRGBTexMem += pixelCount;
		if (tm->HasAlpha())
			totAlphaTexMem += pixelCount;
	}

	// Allocate texture map memory
	if ((totRGBTexMem > 0) || (totAlphaTexMem > 0)) {
		PathOCL::TexMap *gpuTexMap = new PathOCL::TexMap[tms.size()];

		if (totRGBTexMem > 0) {
			unsigned int rgbOffset = 0;
			Spectrum *rgbTexMem = new Spectrum[totRGBTexMem];

			for (unsigned int i = 0; i < tms.size(); ++i) {
				TextureMap *tm = tms[i];
				const unsigned int pixelCount = tm->GetWidth() * tm->GetHeight();

				memcpy(&rgbTexMem[rgbOffset], tm->GetPixels(), pixelCount * sizeof(Spectrum));
				gpuTexMap[i].rgbOffset = rgbOffset;
				rgbOffset += pixelCount;
			}

			cerr << "[PathOCLRenderThread::" << threadIndex << "] TexMap buffer size: " << (sizeof(Spectrum) * totRGBTexMem / 1024) << "Kbytes" << endl;
			texMapRGBBuff = new cl::Buffer(oclContext,
					CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
					sizeof(Spectrum) * totRGBTexMem,
					rgbTexMem);
			deviceDesc->AllocMemory(texMapRGBBuff->getInfo<CL_MEM_SIZE>());
			delete[] rgbTexMem;
		} else
			texMapRGBBuff = NULL;

		if (totAlphaTexMem > 0) {
			unsigned int alphaOffset = 0;
			float *alphaTexMem = new float[totAlphaTexMem];

			for (unsigned int i = 0; i < tms.size(); ++i) {
				TextureMap *tm = tms[i];
				const unsigned int pixelCount = tm->GetWidth() * tm->GetHeight();

				if (tm->HasAlpha()) {
					memcpy(&alphaTexMem[alphaOffset], tm->GetAlphas(), pixelCount * sizeof(float));
					gpuTexMap[i].alphaOffset = alphaOffset;
					alphaOffset += pixelCount;
				} else
					gpuTexMap[i].alphaOffset = 0xffffffffu;
			}

			cerr << "[PathOCLRenderThread::" << threadIndex << "] TexMap buffer size: " << (sizeof(float) * totAlphaTexMem / 1024) << "Kbytes" << endl;
			texMapAlphaBuff = new cl::Buffer(oclContext,
					CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
					sizeof(float) * totAlphaTexMem,
					alphaTexMem);
			deviceDesc->AllocMemory(texMapAlphaBuff->getInfo<CL_MEM_SIZE>());
			delete[] alphaTexMem;
		} else
			texMapAlphaBuff = NULL;

		//----------------------------------------------------------------------

		// Translate texture map description
		for (unsigned int i = 0; i < tms.size(); ++i) {
			TextureMap *tm = tms[i];
			gpuTexMap[i].width = tm->GetWidth();
			gpuTexMap[i].height = tm->GetHeight();
		}

		cerr << "[PathOCLRenderThread::" << threadIndex << "] TexMap indices buffer size: " << (sizeof(PathOCL::TexMap) * tms.size() / 1024) << "Kbytes" << endl;
		texMapDescBuff = new cl::Buffer(oclContext,
				CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
				sizeof(PathOCL::TexMap) * tms.size(),
				gpuTexMap);
		deviceDesc->AllocMemory(texMapDescBuff->getInfo<CL_MEM_SIZE>());
		delete[] gpuTexMap;

		//----------------------------------------------------------------------

		// Translate mesh texture indices
		const unsigned int meshCount = renderEngine->compiledScene->meshMats.size();
		unsigned int *meshTexs = new unsigned int[meshCount];
		for (unsigned int i = 0; i < meshCount; ++i) {
			TexMapInstance *t = scene->objectTexMaps[i];

			if (t) {
				// Look for the index
				unsigned int index = 0;
				for (unsigned int j = 0; j < tms.size(); ++j) {
					if (t->GetTexMap() == tms[j]) {
						index = j;
						break;
					}
				}

				meshTexs[i] = index;
			} else
				meshTexs[i] = 0xffffffffu;
		}

		cerr << "[PathOCLRenderThread::" << threadIndex << "] Mesh texture maps index buffer size: " << (sizeof(unsigned int) * meshCount / 1024) << "Kbytes" << endl;
		meshTexsBuff = new cl::Buffer(oclContext,
				CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
				sizeof(unsigned int) * meshCount,
				meshTexs);
		deviceDesc->AllocMemory(meshTexsBuff->getInfo<CL_MEM_SIZE>());
		delete[] meshTexs;

		//----------------------------------------------------------------------

		// Translate mesh bump map indices
		bool hasBumpMapping = false;
		unsigned int *meshBumps = new unsigned int[meshCount];
		for (unsigned int i = 0; i < meshCount; ++i) {
			BumpMapInstance *bm = scene->objectBumpMaps[i];

			if (bm) {
				// Look for the index
				unsigned int index = 0;
				for (unsigned int j = 0; j < tms.size(); ++j) {
					if (bm->GetTexMap() == tms[j]) {
						index = j;
						break;
					}
				}

				meshBumps[i] = index;
				hasBumpMapping = true;
			} else
				meshBumps[i] = 0xffffffffu;
		}

		if (hasBumpMapping) {
			cerr << "[PathOCLRenderThread::" << threadIndex << "] Mesh bump maps index buffer size: " << (sizeof(unsigned int) * meshCount / 1024) << "Kbytes" << endl;
			meshBumpsBuff = new cl::Buffer(oclContext,
					CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
					sizeof(unsigned int) * meshCount,
					meshBumps);
			deviceDesc->AllocMemory(meshTexsBuff->getInfo<CL_MEM_SIZE>());

			float *scales = new float[meshCount];
			for (unsigned int i = 0; i < meshCount; ++i) {
				BumpMapInstance *bm = scene->objectBumpMaps[i];

				if (bm)
					scales[i] = bm->GetScale();
				else
					scales[i] = 1.f;
			}

			cerr << "[PathOCLRenderThread::" << threadIndex << "] Mesh bump maps scale buffer size: " << (sizeof(float) * meshCount / 1024) << "Kbytes" << endl;
			meshBumpsScaleBuff = new cl::Buffer(oclContext,
					CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
					sizeof(float) * meshCount,
					scales);
			deviceDesc->AllocMemory(meshTexsBuff->getInfo<CL_MEM_SIZE>());
			delete[] scales;
		} else {
			meshBumpsBuff = NULL;
			meshBumpsScaleBuff = NULL;
		}

		delete[] meshBumps;

		tEnd = WallClockTime();
		cerr << "[PathOCLRenderThread::" << threadIndex << "] Texture maps translation time: " << int((tEnd - tStart) * 1000.0) << "ms" << endl;
	} else {
		texMapRGBBuff = NULL;
		texMapAlphaBuff = NULL;
		texMapDescBuff = NULL;
		meshTexsBuff = NULL;
		meshBumpsBuff = NULL;
		meshBumpsScaleBuff = NULL;
	}

	//--------------------------------------------------------------------------
	// Allocate GPU task buffers
	//--------------------------------------------------------------------------

	// TODO: clenup all this mess

	const size_t gpuTaksSizePart1 =
		// Seed size
		sizeof(PathOCL::Seed);

	const size_t uDataEyePathVertexSize =
		// IDX_SCREEN_X, IDX_SCREEN_Y
		sizeof(float) * 2 +
		// IDX_DOF_X, IDX_DOF_Y
		((scene->camera->lensRadius > 0.f) ? (sizeof(float) * 2) : 0);
	const size_t uDataPerPathVertexSize =
		// IDX_TEX_ALPHA,
		((texMapAlphaBuff) ? sizeof(float) : 0) +
		// IDX_BSDF_X, IDX_BSDF_Y, IDX_BSDF_Z
		sizeof(float) * 3 +
		// IDX_DIRECTLIGHT_X, IDX_DIRECTLIGHT_Y, IDX_DIRECTLIGHT_Z
		(((areaLightCount > 0) || sunLight) ? (sizeof(float) * 3) : 0) +
		// IDX_RR
		sizeof(float);
	const size_t uDataSize = (renderEngine->sampler->type == PathOCL::INLINED_RANDOM) ?
		// Only IDX_SCREEN_X, IDX_SCREEN_Y
		(sizeof(float) * 2) :
		((renderEngine->sampler->type == PathOCL::METROPOLIS) ?
			(sizeof(float) * 2 + sizeof(unsigned int) * 5 + sizeof(Spectrum) + 2 * (uDataEyePathVertexSize + uDataPerPathVertexSize * renderEngine->maxPathDepth)) :
			(uDataEyePathVertexSize + uDataPerPathVertexSize * renderEngine->maxPathDepth));

	size_t sampleSize =
		// uint pixelIndex;
		((renderEngine->sampler->type == PathOCL::METROPOLIS) ? 0 : sizeof(unsigned int)) +
		uDataSize +
		// Spectrum radiance;
		sizeof(Spectrum);

	stratifiedDataSize = 0;
	if (renderEngine->sampler->type == PathOCL::STRATIFIED) {
		PathOCL::StratifiedSampler *s = (PathOCL::StratifiedSampler *)renderEngine->sampler;
		stratifiedDataSize =
				// stratifiedScreen2D
				sizeof(float) * s->xSamples * s->ySamples * 2 +
				// stratifiedDof2D
				((scene->camera->lensRadius > 0.f) ? (sizeof(float) * s->xSamples * s->ySamples * 2) : 0) +
				// stratifiedAlpha1D
				((texMapAlphaBuff) ? (sizeof(float) * s->xSamples) : 0) +
				// stratifiedBSDF2D
				sizeof(float) * s->xSamples * s->ySamples * 2 +
				// stratifiedBSDF1D
				sizeof(float) * s->xSamples +
				// stratifiedLight2D
				// stratifiedLight1D
				(((areaLightCount > 0) || sunLight) ? (sizeof(float) * s->xSamples * s->ySamples * 2 + sizeof(float) * s->xSamples) : 0);

		sampleSize += stratifiedDataSize;
	}

	const size_t gpuTaksSizePart2 = sampleSize;

	const size_t gpuTaksSizePart3 =
		// PathState size
		(((areaLightCount > 0) || sunLight) ? sizeof(PathOCL::PathStateDL) : sizeof(PathOCL::PathState));

	const size_t gpuTaksSize = gpuTaksSizePart1 + gpuTaksSizePart2 + gpuTaksSizePart3;
	cerr << "[PathOCLRenderThread::" << threadIndex << "] Size of a GPUTask: " << gpuTaksSize <<
			"bytes (" << gpuTaksSizePart1 << " + " << gpuTaksSizePart2 << " + " << gpuTaksSizePart3 << ")" << endl;
	cerr << "[PathOCLRenderThread::" << threadIndex << "] Tasks buffer size: " << (gpuTaksSize * taskCount / 1024) << "Kbytes" << endl;

	// Check if the task buffer is too big
	if (oclDevice.getInfo<CL_DEVICE_MAX_MEM_ALLOC_SIZE>() < gpuTaksSize * taskCount) {
		stringstream ss;
		ss << "The GPUTask buffer is too big for this device (i.e. CL_DEVICE_MAX_MEM_ALLOC_SIZE=" <<
				oclDevice.getInfo<CL_DEVICE_MAX_MEM_ALLOC_SIZE>() <<
				"): try to reduce opencl.task.count and/or path.maxdepth and/or to change Sampler";
		throw std::runtime_error(ss.str());
	}

	tasksBuff = new cl::Buffer(oclContext,
			CL_MEM_READ_WRITE,
			gpuTaksSize * taskCount);
	deviceDesc->AllocMemory(tasksBuff->getInfo<CL_MEM_SIZE>());

	//--------------------------------------------------------------------------
	// Allocate GPU task statistic buffers
	//--------------------------------------------------------------------------

	cerr << "[PathOCLRenderThread::" << threadIndex << "] Task Stats buffer size: " << (sizeof(PathOCL::GPUTaskStats) * taskCount / 1024) << "Kbytes" << endl;
	taskStatsBuff = new cl::Buffer(oclContext,
			CL_MEM_READ_WRITE,
			sizeof(PathOCL::GPUTaskStats) * taskCount);
	deviceDesc->AllocMemory(taskStatsBuff->getInfo<CL_MEM_SIZE>());

	//--------------------------------------------------------------------------
	// Compile kernels
	//--------------------------------------------------------------------------

	InitKernels();

	//--------------------------------------------------------------------------
	// Initialize
	//--------------------------------------------------------------------------

	// Set kernel arguments
	SetKernelArgs();

	cl::CommandQueue &oclQueue = intersectionDevice->GetOpenCLQueue();

	// Clear the frame buffer
	oclQueue.enqueueNDRangeKernel(*initFBKernel, cl::NullRange,
			cl::NDRange(RoundUp<unsigned int>(frameBufferPixelCount, initFBWorkGroupSize)),
			cl::NDRange(initFBWorkGroupSize));

	// Initialize the tasks buffer
	oclQueue.enqueueNDRangeKernel(*initKernel, cl::NullRange,
			cl::NDRange(taskCount), cl::NDRange(initWorkGroupSize));
	oclQueue.finish();

	// Reset statistics in order to be more accurate
	intersectionDevice->ResetPerformaceStats();
}

void PathOCLRenderThread::SetKernelArgs() {
	// Set OpenCL kernel arguments

	//--------------------------------------------------------------------------
	// initFBKernel
	//--------------------------------------------------------------------------
	unsigned int argIndex = 0;
	samplerKernel->setArg(argIndex++, *tasksBuff);
	samplerKernel->setArg(argIndex++, *taskStatsBuff);
	samplerKernel->setArg(argIndex++, *raysBuff);
	if (cameraBuff)
		samplerKernel->setArg(argIndex++, *cameraBuff);
	if (renderEngine->sampler->type == PathOCL::STRATIFIED)
		samplerKernel->setArg(argIndex++, samplerWorkGroupSize * stratifiedDataSize, NULL);

	argIndex = 0;
	advancePathsKernel->setArg(argIndex++, *tasksBuff);
	advancePathsKernel->setArg(argIndex++, *raysBuff);
	advancePathsKernel->setArg(argIndex++, *hitsBuff);
	advancePathsKernel->setArg(argIndex++, *frameBufferBuff);
	advancePathsKernel->setArg(argIndex++, *materialsBuff);
	advancePathsKernel->setArg(argIndex++, *meshMatsBuff);
	advancePathsKernel->setArg(argIndex++, *meshIDBuff);
	if (triangleIDBuff)
		advancePathsKernel->setArg(argIndex++, *triangleIDBuff);
	if (meshDescsBuff)
		advancePathsKernel->setArg(argIndex++, *meshDescsBuff);
	advancePathsKernel->setArg(argIndex++, *colorsBuff);
	advancePathsKernel->setArg(argIndex++, *normalsBuff);
	advancePathsKernel->setArg(argIndex++, *vertsBuff);
	advancePathsKernel->setArg(argIndex++, *trianglesBuff);
	if (cameraBuff)
		advancePathsKernel->setArg(argIndex++, *cameraBuff);
	if (infiniteLightBuff) {
		advancePathsKernel->setArg(argIndex++, *infiniteLightBuff);
		advancePathsKernel->setArg(argIndex++, *infiniteLightMapBuff);
	}
	if (sunLightBuff)
		advancePathsKernel->setArg(argIndex++, *sunLightBuff);
	if (skyLightBuff)
		advancePathsKernel->setArg(argIndex++, *skyLightBuff);
	if (triLightsBuff)
		advancePathsKernel->setArg(argIndex++, *triLightsBuff);
	if (texMapRGBBuff)
		advancePathsKernel->setArg(argIndex++, *texMapRGBBuff);
	if (texMapAlphaBuff)
		advancePathsKernel->setArg(argIndex++, *texMapAlphaBuff);
	if (texMapRGBBuff || texMapAlphaBuff) {
		advancePathsKernel->setArg(argIndex++, *texMapDescBuff);
		advancePathsKernel->setArg(argIndex++, *meshTexsBuff);
		if (meshBumpsBuff) {
			advancePathsKernel->setArg(argIndex++, *meshBumpsBuff);
			advancePathsKernel->setArg(argIndex++, *meshBumpsScaleBuff);
		}
		advancePathsKernel->setArg(argIndex++, *uvsBuff);
	}

	//--------------------------------------------------------------------------
	// initFBKernel
	//--------------------------------------------------------------------------

	initFBKernel->setArg(0, *frameBufferBuff);

	//--------------------------------------------------------------------------
	// initKernel
	//--------------------------------------------------------------------------

	argIndex = 0;
	initKernel->setArg(argIndex++, *tasksBuff);
	initKernel->setArg(argIndex++, *taskStatsBuff);
	initKernel->setArg(argIndex++, *raysBuff);
	if (cameraBuff)
		initKernel->setArg(argIndex++, *cameraBuff);
	if (renderEngine->sampler->type == PathOCL::STRATIFIED)
		initKernel->setArg(argIndex++, initWorkGroupSize * stratifiedDataSize, NULL);
}

void PathOCLRenderThread::Start() {
	started = true;

	InitRender();
	StartRenderThread();
}

void PathOCLRenderThread::Interrupt() {
	if (renderThread)
		renderThread->interrupt();
}

void PathOCLRenderThread::Stop() {
	StopRenderThread();

	// Transfer of the frame buffer
	cl::CommandQueue &oclQueue = intersectionDevice->GetOpenCLQueue();
	oclQueue.enqueueReadBuffer(
		*frameBufferBuff,
		CL_TRUE,
		0,
		frameBufferBuff->getInfo<CL_MEM_SIZE>(),
		frameBuffer);

	FreeOCLBuffer(&raysBuff);
	FreeOCLBuffer(&hitsBuff);
	FreeOCLBuffer(&tasksBuff);
	FreeOCLBuffer(&taskStatsBuff);
	FreeOCLBuffer(&frameBufferBuff);
	FreeOCLBuffer(&materialsBuff);
	FreeOCLBuffer(&meshIDBuff);
	FreeOCLBuffer(&triangleIDBuff);
	FreeOCLBuffer(&meshDescsBuff);
	FreeOCLBuffer(&meshMatsBuff);
	FreeOCLBuffer(&colorsBuff);
	FreeOCLBuffer(&normalsBuff);
	FreeOCLBuffer(&trianglesBuff);
	FreeOCLBuffer(&vertsBuff);
	if (infiniteLightBuff) {
		FreeOCLBuffer(&infiniteLightBuff);
		FreeOCLBuffer(&infiniteLightMapBuff);
	}
	FreeOCLBuffer(&sunLightBuff);
	FreeOCLBuffer(&skyLightBuff);
	FreeOCLBuffer(&cameraBuff);
	FreeOCLBuffer(&triLightsBuff);
	FreeOCLBuffer(&texMapRGBBuff);
	FreeOCLBuffer(&texMapAlphaBuff);
	if (texMapRGBBuff || texMapAlphaBuff) {
		FreeOCLBuffer(&texMapDescBuff);
		FreeOCLBuffer(&meshTexsBuff);

		if (meshBumpsBuff) {
			FreeOCLBuffer(&meshBumpsBuff);
			FreeOCLBuffer(&meshBumpsScaleBuff);
		}

		FreeOCLBuffer(&uvsBuff);
	}

	started = false;

	// frameBuffer is delete on the destructor to allow image saving after
	// the rendering is finished
}

void PathOCLRenderThread::StartRenderThread() {
	// Create the thread for the rendering
	renderThread = new boost::thread(boost::bind(PathOCLRenderThread::RenderThreadImpl, this));

	// Set renderThread priority
	bool res = SetThreadRRPriority(renderThread);
	if (res && !reportedPermissionError) {
		cerr << "[PathOCLRenderThread::" << threadIndex << "] Failed to set ray intersection thread priority (you probably need root/administrator permission to set thread realtime priority)" << endl;
		reportedPermissionError = true;
	}
}

void PathOCLRenderThread::StopRenderThread() {
	if (renderThread) {
		renderThread->interrupt();
		renderThread->join();
		delete renderThread;
		renderThread = NULL;
	}
}

void PathOCLRenderThread::BeginEdit() {
	StopRenderThread();
}

void PathOCLRenderThread::EndEdit(const EditActionList &editActions) {
	cl::CommandQueue &oclQueue = intersectionDevice->GetOpenCLQueue();

	//--------------------------------------------------------------------------
	// Update OpenCL buffers
	//--------------------------------------------------------------------------

	if (editActions.Has(FILM_EDIT)) {
		// Resize the Framebuffer
		InitFrameBuffer();
	}

	if (editActions.Has(CAMERA_EDIT)) {
		// Update Camera
		InitCamera();
	}

	if (editActions.Has(GEOMETRY_EDIT)) {
		// Update Scene Geometry
		InitGeometry();
	}

	if (editActions.Has(MATERIALS_EDIT)) {
		// Update Scene Materials
		InitMaterials();
	}

	//--------------------------------------------------------------------------
	// Recompile Kernels if required
	//--------------------------------------------------------------------------

	// TODO: optimize the case when MATERIALS_EDIT doesn't change the type
	// of materials used
	if (editActions.Has(FILM_EDIT) || editActions.Has(MATERIALS_EDIT))
		InitKernels();

	if (editActions.Size() > 0)
		SetKernelArgs();

	//--------------------------------------------------------------------------
	// Execute initialization kernels
	//--------------------------------------------------------------------------

	if (editActions.Size() > 0) {
		// Clear the frame buffer
		oclQueue.enqueueNDRangeKernel(*initFBKernel, cl::NullRange,
			cl::NDRange(RoundUp<unsigned int>(frameBufferPixelCount, initFBWorkGroupSize)),
			cl::NDRange(initFBWorkGroupSize));

		// Initialize the tasks buffer
		oclQueue.enqueueNDRangeKernel(*initKernel, cl::NullRange,
				cl::NDRange(renderEngine->taskCount), cl::NDRange(initWorkGroupSize));
	}

	// Reset statistics in order to be more accurate
	intersectionDevice->ResetPerformaceStats();

	StartRenderThread();
}

void PathOCLRenderThread::RenderThreadImpl(PathOCLRenderThread *renderThread) {
	cerr << "[PathOCLRenderThread::" << renderThread->threadIndex << "] Rendering thread started" << endl;

	cl::CommandQueue &oclQueue = renderThread->intersectionDevice->GetOpenCLQueue();
	const unsigned int taskCount = renderThread->renderEngine->taskCount;

	try {
		double startTime = WallClockTime();
		while (!boost::this_thread::interruption_requested()) {
			/*if(renderThread->threadIndex == 0)
				cerr<< "[DEBUG] =================================" << endl;*/

			// Async. transfer of the frame buffer
			oclQueue.enqueueReadBuffer(
				*(renderThread->frameBufferBuff),
				CL_FALSE,
				0,
				renderThread->frameBufferBuff->getInfo<CL_MEM_SIZE>(),
				renderThread->frameBuffer);

			// Async. transfer of GPU task statistics
			oclQueue.enqueueReadBuffer(
				*(renderThread->taskStatsBuff),
				CL_FALSE,
				0,
				sizeof(PathOCL::GPUTaskStats) * taskCount,
				renderThread->gpuTaskStats);

			for (;;) {
				cl::Event event;

				// Decide how many kernels to enqueue
				const unsigned int screenRefreshInterval = renderThread->renderEngine->renderConfig->GetScreenRefreshInterval();

				unsigned int iterations;
				if (screenRefreshInterval <= 100)
					iterations = 1;
				else if (screenRefreshInterval <= 500)
					iterations = 2;
				else if (screenRefreshInterval <= 1000)
					iterations = 4;
				else
					iterations = 8;

				for (unsigned int i = 0; i < iterations; ++i) {
					// Generate the samples and paths
					if (i == 0)
						oclQueue.enqueueNDRangeKernel(*(renderThread->samplerKernel), cl::NullRange,
								cl::NDRange(taskCount), cl::NDRange(renderThread->samplerWorkGroupSize),
								NULL, &event);
					else
						oclQueue.enqueueNDRangeKernel(*(renderThread->samplerKernel), cl::NullRange,
								cl::NDRange(taskCount), cl::NDRange(renderThread->samplerWorkGroupSize));

					// Trace rays
					renderThread->intersectionDevice->EnqueueTraceRayBuffer(*(renderThread->raysBuff),
								*(renderThread->hitsBuff), taskCount);

					// Advance to next path state
					oclQueue.enqueueNDRangeKernel(*(renderThread->advancePathsKernel), cl::NullRange,
							cl::NDRange(taskCount), cl::NDRange(renderThread->advancePathsWorkGroupSize));
				}
				oclQueue.flush();

				event.wait();
				const double elapsedTime = WallClockTime() - startTime;

				/*if(renderThread->threadIndex == 0)
					cerr<< "[DEBUG] Elapsed time: " << elapsedTime * 1000.0 <<
							"ms (screenRefreshInterval: " << renderThread->renderEngine->screenRefreshInterval << ")" << endl;*/

				if ((elapsedTime * 1000.0 > (double)screenRefreshInterval) ||
						boost::this_thread::interruption_requested())
					break;
			}

			startTime = WallClockTime();
		}

		cerr << "[PathOCLRenderThread::" << renderThread->threadIndex << "] Rendering thread halted" << endl;
	} catch (boost::thread_interrupted) {
		cerr << "[PathOCLRenderThread::" << renderThread->threadIndex << "] Rendering thread halted" << endl;
	} catch (cl::Error err) {
		cerr << "[PathOCLRenderThread::" << renderThread->threadIndex << "] Rendering thread ERROR: " << err.what() << "(" << err.err() << ")" << endl;
	}

	oclQueue.enqueueReadBuffer(
			*(renderThread->frameBufferBuff),
			CL_TRUE,
			0,
			renderThread->frameBufferBuff->getInfo<CL_MEM_SIZE>(),
			renderThread->frameBuffer);
}
