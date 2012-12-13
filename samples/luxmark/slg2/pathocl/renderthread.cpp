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

#if defined(__APPLE__)
//OSX version detection
#include <sys/utsname.h>
#endif

#include <boost/thread/mutex.hpp>

#include "smalllux.h"

#include "pathocl/pathocl.h"
#include "pathocl/kernels/kernels.h"
#include "renderconfig.h"
#include "luxrays/core/geometry/transform.h"
#include "luxrays/accelerators/mqbvhaccel.h"
#include "luxrays/accelerators/bvhaccel.h"
#include "luxrays/opencl/utils.h"

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
	cameraBuff = NULL;
	areaLightsBuff = NULL;
	texMapRGBBuff = NULL;
	texMapAlphaBuff = NULL;
	texMapDescBuff = NULL;
	meshTexsBuff = NULL;
	meshBumpsBuff = NULL;
	meshBumpsScaleBuff = NULL;
	meshNormalMapsBuff = NULL;
	uvsBuff = NULL;

	gpuTaskStats = new PathOCL::GPUTaskStats[renderEngine->taskCount];
	memset(gpuTaskStats, 0, sizeof(PathOCL::GPUTaskStats) * renderEngine->taskCount);

	// Check the kind of kernel cache to use
	std::string type = re->renderConfig->cfg.GetString("opencl.kernelcache", "NONE");
	if (type == "PERSISTENT")
		kernelCache = new luxrays::utils::oclKernelPersistentCache("slg_" + string(SLG_VERSION_MAJOR) + "." + string(SLG_VERSION_MINOR));
	else if (type == "VOLATILE")
		kernelCache = new luxrays::utils::oclKernelVolatileCache();
	else if (type == "NONE")
		kernelCache = new luxrays::utils::oclKernelDummyCache();
	else
		throw std::runtime_error("Unknown opencl.kernelcache type: " + type);

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

	delete kernelCache;
}

void PathOCLRenderThread::AllocOCLBufferRO(cl::Buffer **buff, void *src, const size_t size, const string &desc) {
	if (*buff) {
		// Check the size of the already allocated buffer

		if (size == (*buff)->getInfo<CL_MEM_SIZE>()) {
			// I can reuse the buffer; just update the content

			//LM_LOG_ENGINE("[PathOCLRenderThread::" << threadIndex << "] " << desc << " buffer updated for size: " << (size / 1024) << "Kbytes");
			cl::CommandQueue &oclQueue = intersectionDevice->GetOpenCLQueue();
			oclQueue.enqueueWriteBuffer(**buff, CL_FALSE, 0, size, src);
			return;
		}
	}

	cl::Context &oclContext = intersectionDevice->GetOpenCLContext();

	LM_LOG_ENGINE("[PathOCLRenderThread::" << threadIndex << "] " << desc << " buffer size: " << (size / 1024) << "Kbytes");
	*buff = new cl::Buffer(oclContext,
			CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
			size, src);
	intersectionDevice->AllocMemory((*buff)->getInfo<CL_MEM_SIZE>());
}

void PathOCLRenderThread::AllocOCLBufferRW(cl::Buffer **buff, const size_t size, const string &desc) {
	if (*buff) {
		// Check the size of the already allocated buffer

		if (size == (*buff)->getInfo<CL_MEM_SIZE>()) {
			// I can reuse the buffer
			//LM_LOG_ENGINE("[PathOCLRenderThread::" << threadIndex << "] " << desc << " buffer reused for size: " << (size / 1024) << "Kbytes");
			return;
		}
	}

	cl::Context &oclContext = intersectionDevice->GetOpenCLContext();

	LM_LOG_ENGINE("[PathOCLRenderThread::" << threadIndex << "] " << desc << " buffer size: " << (size / 1024) << "Kbytes");
	*buff = new cl::Buffer(oclContext,
			CL_MEM_READ_WRITE,
			size);
	intersectionDevice->AllocMemory((*buff)->getInfo<CL_MEM_SIZE>());
}

void PathOCLRenderThread::FreeOCLBuffer(cl::Buffer **buff) {
	if (*buff) {

		intersectionDevice->FreeMemory((*buff)->getInfo<CL_MEM_SIZE>());
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

		AllocOCLBufferRO(&triangleIDBuff, (void *)cscene->meshFirstTriangleOffset,
				sizeof(unsigned int) * cscene->meshDescs.size(), "First mesh triangle offset");

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

void PathOCLRenderThread::InitAreaLights() {
	CompiledScene *cscene = renderEngine->compiledScene;

	if (cscene->areaLights.size() > 0) {
		AllocOCLBufferRO(&areaLightsBuff, &cscene->areaLights[0],
			sizeof(PathOCL::TriangleLight) * cscene->areaLights.size(), "AreaLights");
	} else
		areaLightsBuff = NULL;
}

void PathOCLRenderThread::InitInfiniteLight() {
	CompiledScene *cscene = renderEngine->compiledScene;

	if (cscene->infiniteLight) {
		AllocOCLBufferRO(&infiniteLightBuff, cscene->infiniteLight,
			sizeof(PathOCL::InfiniteLight), "InfiniteLight");

		const unsigned int pixelCount = cscene->infiniteLight->width * cscene->infiniteLight->height;
		AllocOCLBufferRO(&infiniteLightMapBuff, (void *)cscene->infiniteLightMap,
			sizeof(Spectrum) * pixelCount, "InfiniteLight map");
	} else {
		infiniteLightBuff = NULL;
		infiniteLightMapBuff = NULL;
	}
}

void PathOCLRenderThread::InitSunLight() {
	CompiledScene *cscene = renderEngine->compiledScene;

	if (cscene->sunLight)
		AllocOCLBufferRO(&sunLightBuff, cscene->sunLight,
			sizeof(PathOCL::SunLight), "SunLight");
	else
		sunLightBuff = NULL;
}

void PathOCLRenderThread::InitSkyLight() {
	CompiledScene *cscene = renderEngine->compiledScene;

	if (cscene->skyLight)
		AllocOCLBufferRO(&skyLightBuff, cscene->skyLight,
			sizeof(PathOCL::SkyLight), "SkyLight");
	else
		skyLightBuff = NULL;
}

void PathOCLRenderThread::InitTextureMaps() {
	CompiledScene *cscene = renderEngine->compiledScene;

	if ((cscene->totRGBTexMem > 0) || (cscene->totAlphaTexMem > 0)) {
		if (cscene->totRGBTexMem > 0)
			AllocOCLBufferRO(&texMapRGBBuff, cscene->rgbTexMem,
				sizeof(Spectrum) * cscene->totRGBTexMem, "TexMaps");
		else
			texMapRGBBuff = NULL;

		if (cscene->totAlphaTexMem > 0)
			AllocOCLBufferRO(&texMapAlphaBuff, cscene->alphaTexMem,
				sizeof(float) * cscene->totAlphaTexMem, "TexMaps Alpha Channel");
		else
			texMapAlphaBuff = NULL;

		AllocOCLBufferRO(&texMapDescBuff, &cscene->gpuTexMaps[0],
				sizeof(PathOCL::TexMap) * cscene->gpuTexMaps.size(), "TexMaps description");

		const unsigned int meshCount = renderEngine->compiledScene->meshMats.size();
		AllocOCLBufferRO(&meshTexsBuff, cscene->meshTexs,
				sizeof(unsigned int) * meshCount, "Mesh TexMaps index");

		if (cscene->meshBumps) {
			AllocOCLBufferRO(&meshBumpsBuff, cscene->meshBumps,
				sizeof(unsigned int) * meshCount, "Mesh BumpMaps index");

			AllocOCLBufferRO(&meshBumpsScaleBuff, cscene->bumpMapScales,
				sizeof(float) * meshCount, "Mesh BumpMaps scales");
		} else {
			meshBumpsBuff = NULL;
			meshBumpsScaleBuff = NULL;
		}

		if (cscene->meshNormalMaps)
			AllocOCLBufferRO(&meshNormalMapsBuff, cscene->meshNormalMaps,
				sizeof(unsigned int) * meshCount, "Mesh NormalMaps index");
		else
			meshNormalMapsBuff = NULL;
	} else {
		texMapRGBBuff = NULL;
		texMapAlphaBuff = NULL;
		texMapDescBuff = NULL;
		meshTexsBuff = NULL;
		meshBumpsBuff = NULL;
		meshBumpsScaleBuff = NULL;
		meshNormalMapsBuff = NULL;
	}
}

void PathOCLRenderThread::InitKernels() {
	//--------------------------------------------------------------------------
	// Compile kernels
	//--------------------------------------------------------------------------

	CompiledScene *cscene = renderEngine->compiledScene;
	cl::Context &oclContext = intersectionDevice->GetOpenCLContext();
	cl::Device &oclDevice = intersectionDevice->GetOpenCLDevice();

	// Set #define symbols
	stringstream ss;
	ss.precision(6);
	ss << scientific <<
			" -D PARAM_TASK_COUNT=" << renderEngine->taskCount <<
			" -D PARAM_IMAGE_WIDTH=" << renderEngine->film->GetWidth() <<
			" -D PARAM_IMAGE_HEIGHT=" << renderEngine->film->GetHeight() <<
			" -D PARAM_RAY_EPSILON=" << renderEngine->epsilon << "f" <<
			" -D PARAM_SEED=" << seed <<
			" -D PARAM_MAX_PATH_DEPTH=" << renderEngine->maxPathDepth <<
			" -D PARAM_MAX_DIFFUSE_PATH_VERTEX_COUNT=" << renderEngine->maxDiffusePathVertexCount <<
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

		if (!areaLightsBuff) {
			ss <<
				" -D PARAM_DIRECT_LIGHT_SAMPLING" <<
				" -D PARAM_DL_LIGHT_COUNT=0"
				;
		}
	}

	if (areaLightsBuff) {
		ss <<
				" -D PARAM_DIRECT_LIGHT_SAMPLING" <<
				" -D PARAM_DL_LIGHT_COUNT=" << renderEngine->compiledScene->areaLights.size()
				;
	}

	if (texMapRGBBuff || texMapAlphaBuff)
		ss << " -D PARAM_HAS_TEXTUREMAPS";
	if (texMapAlphaBuff)
		ss << " -D PARAM_HAS_ALPHA_TEXTUREMAPS";
	if (meshBumpsBuff)
		ss << " -D PARAM_HAS_BUMPMAPS";
	if (meshNormalMapsBuff)
		ss << " -D PARAM_HAS_NORMALMAPS";

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
					" -D PARAM_SAMPLER_METROPOLIS_MAX_CONSECUTIVE_REJECT=" << ((PathOCL::MetropolisSampler *)sampler)->maxConsecutiveReject <<
					" -D PARAM_SAMPLER_METROPOLIS_IMAGE_MUTATION_RANGE=" << ((PathOCL::MetropolisSampler *)sampler)->imageMutationRate << "f";
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
	
#if defined(__APPLE__) // OSX version detection
	{
	struct utsname retval;
		uname(&retval);
	if(retval.release[0] == '1' && retval.release[1] < '1') // result < darwin 11
		ss << " -D __APPLE_FIX__";
	}
#endif

	//--------------------------------------------------------------------------

	const double tStart = WallClockTime();

	// Check if I have to recompile the kernels
	string newKernelParameters = ss.str();
	if (kernelsParameters != newKernelParameters) {
		kernelsParameters = newKernelParameters;

		// Compile sources
		stringstream ssKernel;
		ssKernel <<
			_LUXRAYS_UV_OCLDEFINE
			_LUXRAYS_SPECTRUM_OCLDEFINE
			_LUXRAYS_POINT_OCLDEFINE
			_LUXRAYS_VECTOR_OCLDEFINE
			_LUXRAYS_TRIANGLE_OCLDEFINE
			_LUXRAYS_RAY_OCLDEFINE
			_LUXRAYS_RAYHIT_OCLDEFINE <<
			KernelSource_PathOCL_kernel_datatypes <<
			KernelSource_PathOCL_kernel_core <<
			KernelSource_PathOCL_kernel_filters <<
			KernelSource_PathOCL_kernel_scene <<
			KernelSource_PathOCL_kernel_samplers <<
			KernelSource_PathOCL_kernels;
		string kernelSource = ssKernel.str();

		LM_LOG_ENGINE("[PathOCLRenderThread::" << threadIndex << "] Defined symbols: " << kernelsParameters);
		LM_LOG_ENGINE("[PathOCLRenderThread::" << threadIndex << "] Compiling kernels ");

		bool cached;
		cl::STRING_CLASS error;
		cl::Program *program = kernelCache->Compile(oclContext, oclDevice,
				kernelsParameters, kernelSource,
				&cached, &error);

		if (!program) {
			LM_LOG_ENGINE("[PathOCLRenderThread::" << threadIndex << "] PathOCL kernel compilation error" << std::endl << error);

			throw std::runtime_error("PathOCL kernel compilation error");
		}

		if (cached) {
			LM_LOG_ENGINE("[PathOCLRenderThread::" << threadIndex << "] Kernels cached");
		} else {
			LM_LOG_ENGINE("[PathOCLRenderThread::" << threadIndex << "] Kernels not cached");
		}

		//----------------------------------------------------------------------
		// Init kernel
		//----------------------------------------------------------------------

		delete initKernel;
		LM_LOG_ENGINE("[PathOCLRenderThread::" << threadIndex << "] Compiling Init Kernel");
		initKernel = new cl::Kernel(*program, "Init");
		initKernel->getWorkGroupInfo<size_t>(oclDevice, CL_KERNEL_WORK_GROUP_SIZE, &initWorkGroupSize);

		if (intersectionDevice->GetForceWorkGroupSize() > 0)
			initWorkGroupSize = intersectionDevice->GetForceWorkGroupSize();
		else if (renderEngine->sampler->type == PathOCL::STRATIFIED) {
			// Resize the workgroup to have enough local memory
			size_t localMem = oclDevice.getInfo<CL_DEVICE_LOCAL_MEM_SIZE>();

			while ((initWorkGroupSize > 64) && (stratifiedDataSize * initWorkGroupSize > localMem))
				initWorkGroupSize /= 2;

			if (stratifiedDataSize * initWorkGroupSize > localMem)
				throw std::runtime_error("Not enough local memory to run, try to reduce path.sampler.xsamples and path.sampler.xsamples values");

			LM_LOG_ENGINE("[PathOCLRenderThread::" << threadIndex << "] Cap work group size to: " << initWorkGroupSize);
		}

		//--------------------------------------------------------------------------
		// InitFB kernel
		//--------------------------------------------------------------------------

		delete initFBKernel;
		initFBKernel = new cl::Kernel(*program, "InitFrameBuffer");
		initFBKernel->getWorkGroupInfo<size_t>(oclDevice, CL_KERNEL_WORK_GROUP_SIZE, &initFBWorkGroupSize);
		if (intersectionDevice->GetForceWorkGroupSize() > 0)
			initFBWorkGroupSize = intersectionDevice->GetForceWorkGroupSize();

		//----------------------------------------------------------------------
		// Sampler kernel
		//----------------------------------------------------------------------

		delete samplerKernel;
		LM_LOG_ENGINE("[PathOCLRenderThread::" << threadIndex << "] Compiling Sampler Kernel");
		samplerKernel = new cl::Kernel(*program, "Sampler");
		samplerKernel->getWorkGroupInfo<size_t>(oclDevice, CL_KERNEL_WORK_GROUP_SIZE, &samplerWorkGroupSize);

		if (intersectionDevice->GetForceWorkGroupSize() > 0)
			samplerWorkGroupSize = intersectionDevice->GetForceWorkGroupSize();
		else if (renderEngine->sampler->type == PathOCL::STRATIFIED) {
			// Resize the workgroup to have enough local memory
			size_t localMem = oclDevice.getInfo<CL_DEVICE_LOCAL_MEM_SIZE>();

			while ((samplerWorkGroupSize > 64) && (stratifiedDataSize * samplerWorkGroupSize > localMem))
				samplerWorkGroupSize /= 2;

			if (stratifiedDataSize * samplerWorkGroupSize > localMem)
				throw std::runtime_error("Not enough local memory to run, try to reduce path.sampler.xsamples and path.sampler.xsamples values");

			LM_LOG_ENGINE("[PathOCLRenderThread::" << threadIndex << "] Cap work group size to: " << samplerWorkGroupSize);
		}

		//----------------------------------------------------------------------
		// AdvancePaths kernel
		//----------------------------------------------------------------------

		delete advancePathsKernel;
		LM_LOG_ENGINE("[PathOCLRenderThread::" << threadIndex << "] Compiling AdvancePaths Kernel");
		advancePathsKernel = new cl::Kernel(*program, "AdvancePaths");
		advancePathsKernel->getWorkGroupInfo<size_t>(oclDevice, CL_KERNEL_WORK_GROUP_SIZE, &advancePathsWorkGroupSize);
		if (intersectionDevice->GetForceWorkGroupSize() > 0)
			advancePathsWorkGroupSize = intersectionDevice->GetForceWorkGroupSize();

		//----------------------------------------------------------------------

		const double tEnd = WallClockTime();
		LM_LOG_ENGINE("[PathOCLRenderThread::" << threadIndex << "] Kernels compilation time: " << int((tEnd - tStart) * 1000.0) << "ms");

		delete program;
	} else
		LM_LOG_ENGINE("[PathOCLRenderThread::" << threadIndex << "] Using cached kernels");
}

void PathOCLRenderThread::InitRender() {
	Scene *scene = renderEngine->renderConfig->scene;

	cl::Context &oclContext = intersectionDevice->GetOpenCLContext();
	cl::Device &oclDevice = intersectionDevice->GetOpenCLDevice();

	double tStart, tEnd;

	//--------------------------------------------------------------------------
	// FrameBuffer definition
	//--------------------------------------------------------------------------

	InitFrameBuffer();

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

	InitAreaLights();

	//--------------------------------------------------------------------------
	// Check if there is an infinite light source
	//--------------------------------------------------------------------------

	InitInfiniteLight();

	//--------------------------------------------------------------------------
	// Check if there is an sun light source
	//--------------------------------------------------------------------------

	InitSunLight();

	//--------------------------------------------------------------------------
	// Check if there is an sky light source
	//--------------------------------------------------------------------------

	InitSkyLight();

	const unsigned int areaLightCount = renderEngine->compiledScene->areaLights.size();
	if (!skyLightBuff && !sunLightBuff && !infiniteLightBuff && (areaLightCount == 0))
		throw runtime_error("There are no light sources supported by PathOCL in the scene");

	//--------------------------------------------------------------------------
	// Translate mesh texture maps
	//--------------------------------------------------------------------------

	InitTextureMaps();

	//--------------------------------------------------------------------------
	// Allocate Ray/RayHit buffers
	//--------------------------------------------------------------------------

	const unsigned int taskCount = renderEngine->taskCount;

	tStart = WallClockTime();

	LM_LOG_ENGINE("[PathOCLRenderThread::" << threadIndex << "] Ray buffer size: " << (sizeof(Ray) * taskCount / 1024) << "Kbytes");
	raysBuff = new cl::Buffer(oclContext,
			CL_MEM_READ_WRITE,
			sizeof(Ray) * taskCount);
	intersectionDevice->AllocMemory(raysBuff->getInfo<CL_MEM_SIZE>());

	LM_LOG_ENGINE("[PathOCLRenderThread::" << threadIndex << "] RayHit buffer size: " << (sizeof(RayHit) * taskCount / 1024) << "Kbytes");
	hitsBuff = new cl::Buffer(oclContext,
			CL_MEM_READ_WRITE,
			sizeof(RayHit) * taskCount);
	intersectionDevice->AllocMemory(hitsBuff->getInfo<CL_MEM_SIZE>());

	tEnd = WallClockTime();
	LM_LOG_ENGINE("[PathOCLRenderThread::" << threadIndex << "] OpenCL buffer creation time: " << int((tEnd - tStart) * 1000.0) << "ms");

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
		(((areaLightCount > 0) || sunLightBuff) ? (sizeof(float) * 3) : 0) +
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
				(((areaLightCount > 0) || sunLightBuff) ? (sizeof(float) * s->xSamples * s->ySamples * 2 + sizeof(float) * s->xSamples) : 0);

		sampleSize += stratifiedDataSize;
	}

	const size_t gpuTaksSizePart2 = sampleSize;

	const size_t gpuTaksSizePart3 =
		// PathState size
		((((areaLightCount > 0) || sunLightBuff) ? sizeof(PathOCL::PathStateDL) : sizeof(PathOCL::PathState)) +
			//unsigned int diffuseVertexCount;
			((renderEngine->maxDiffusePathVertexCount < renderEngine->maxPathDepth) ? sizeof(unsigned int) : 0));

	const size_t gpuTaksSize = gpuTaksSizePart1 + gpuTaksSizePart2 + gpuTaksSizePart3;
	LM_LOG_ENGINE("[PathOCLRenderThread::" << threadIndex << "] Size of a GPUTask: " << gpuTaksSize <<
			"bytes (" << gpuTaksSizePart1 << " + " << gpuTaksSizePart2 << " + " << gpuTaksSizePart3 << ")");
	LM_LOG_ENGINE("[PathOCLRenderThread::" << threadIndex << "] Tasks buffer size: " << (gpuTaksSize * taskCount / 1024) << "Kbytes");

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
	intersectionDevice->AllocMemory(tasksBuff->getInfo<CL_MEM_SIZE>());

	//--------------------------------------------------------------------------
	// Allocate GPU task statistic buffers
	//--------------------------------------------------------------------------

	LM_LOG_ENGINE("[PathOCLRenderThread::" << threadIndex << "] Task Stats buffer size: " << (sizeof(PathOCL::GPUTaskStats) * taskCount / 1024) << "Kbytes");
	taskStatsBuff = new cl::Buffer(oclContext,
			CL_MEM_READ_WRITE,
			sizeof(PathOCL::GPUTaskStats) * taskCount);
	intersectionDevice->AllocMemory(taskStatsBuff->getInfo<CL_MEM_SIZE>());

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
	if (areaLightsBuff)
		advancePathsKernel->setArg(argIndex++, *areaLightsBuff);
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
		if (meshNormalMapsBuff)
			advancePathsKernel->setArg(argIndex++, *meshNormalMapsBuff);
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

	if (frameBuffer) {
		// Transfer of the frame buffer
		cl::CommandQueue &oclQueue = intersectionDevice->GetOpenCLQueue();
		oclQueue.enqueueReadBuffer(
			*frameBufferBuff,
			CL_TRUE,
			0,
			frameBufferBuff->getInfo<CL_MEM_SIZE>(),
			frameBuffer);
	}

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
	FreeOCLBuffer(&areaLightsBuff);
	FreeOCLBuffer(&texMapRGBBuff);
	FreeOCLBuffer(&texMapAlphaBuff);
	if (texMapRGBBuff || texMapAlphaBuff) {
		FreeOCLBuffer(&texMapDescBuff);
		FreeOCLBuffer(&meshTexsBuff);

		if (meshBumpsBuff) {
			FreeOCLBuffer(&meshBumpsBuff);
			FreeOCLBuffer(&meshBumpsScaleBuff);
		}

		if (meshNormalMapsBuff)
			FreeOCLBuffer(&meshNormalMapsBuff);

		FreeOCLBuffer(&uvsBuff);
	}

	started = false;

	// frameBuffer is delete on the destructor to allow image saving after
	// the rendering is finished
}

void PathOCLRenderThread::StartRenderThread() {
	// Create the thread for the rendering
	renderThread = new boost::thread(boost::bind(PathOCLRenderThread::RenderThreadImpl, this));
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

	if (editActions.Has(MATERIALS_EDIT) || editActions.Has(MATERIAL_TYPES_EDIT)) {
		// Update Scene Materials
		InitMaterials();
	}

	if  (editActions.Has(AREALIGHTS_EDIT)) {
		// Update Scene Area Lights
		InitAreaLights();
	}

	if  (editActions.Has(INFINITELIGHT_EDIT)) {
		// Update Scene Infinite Light
		InitInfiniteLight();
	}

	if  (editActions.Has(SUNLIGHT_EDIT)) {
		// Update Scene Sun Light
		InitSunLight();
	}

	if  (editActions.Has(SKYLIGHT_EDIT)) {
		// Update Scene Sun Light
		InitSkyLight();
	}

	//--------------------------------------------------------------------------
	// Recompile Kernels if required
	//--------------------------------------------------------------------------

	if (editActions.Has(FILM_EDIT) || editActions.Has(MATERIAL_TYPES_EDIT))
		InitKernels();

	if (editActions.Size() > 0)
		SetKernelArgs();

	//--------------------------------------------------------------------------
	// Execute initialization kernels
	//--------------------------------------------------------------------------

	if (editActions.Size() > 0) {
		cl::CommandQueue &oclQueue = intersectionDevice->GetOpenCLQueue();

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
	//LM_LOG_ENGINE("[PathOCLRenderThread::" << renderThread->threadIndex << "] Rendering thread started");
	cl::CommandQueue &oclQueue = renderThread->intersectionDevice->GetOpenCLQueue();
	const unsigned int taskCount = renderThread->renderEngine->taskCount;

	oclQueue.finish();
	// Wait for the signal to start the rendering
	renderThread->renderEngine->renderStartBarrier->wait();

	try {
		double startTime = WallClockTime();
		while (!boost::this_thread::interruption_requested()) {
			/*if(renderThread->threadIndex == 0)
				cerr<< "[DEBUG] =================================");*/

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
								*(renderThread->hitsBuff), taskCount, NULL, NULL);

					// Advance to next path state
					oclQueue.enqueueNDRangeKernel(*(renderThread->advancePathsKernel), cl::NullRange,
							cl::NDRange(taskCount), cl::NDRange(renderThread->advancePathsWorkGroupSize));
				}
				oclQueue.flush();

				event.wait();
				const double elapsedTime = WallClockTime() - startTime;

				/*if(renderThread->threadIndex == 0)
					cerr<< "[DEBUG] Elapsed time: " << elapsedTime * 1000.0 <<
							"ms (screenRefreshInterval: " << renderThread->renderEngine->screenRefreshInterval << ")");*/

				if ((elapsedTime * 1000.0 > (double)screenRefreshInterval) ||
						boost::this_thread::interruption_requested())
					break;
			}

			startTime = WallClockTime();
		}

		//LM_LOG_ENGINE("[PathOCLRenderThread::" << renderThread->threadIndex << "] Rendering thread halted");
	} catch (boost::thread_interrupted) {
		LM_LOG_ENGINE("[PathOCLRenderThread::" << renderThread->threadIndex << "] Rendering thread halted");
	} catch (cl::Error err) {
		LM_LOG_ENGINE("[PathOCLRenderThread::" << renderThread->threadIndex << "] Rendering thread ERROR: " << err.what() <<
				"(" << luxrays::utils::oclErrorString(err.err()) << ")");
	}

	oclQueue.enqueueReadBuffer(
			*(renderThread->frameBufferBuff),
			CL_TRUE,
			0,
			renderThread->frameBufferBuff->getInfo<CL_MEM_SIZE>(),
			renderThread->frameBuffer);
}
