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

#include "slg.h"
#include "pathocl/pathocl.h"
#include "pathocl/kernels/kernels.h"
#include "renderconfig.h"

#include "luxrays/core/geometry/transform.h"
#include "luxrays/accelerators/mqbvhaccel.h"
#include "luxrays/accelerators/bvhaccel.h"
#include "luxrays/opencl/utils.h"
#include "luxrays/opencl/intersectiondevice.h"
#include "luxrays/kernels/kernels.h"

#if defined(__APPLE__)
//OSX version detection
#include <sys/utsname.h>
#endif

using namespace std;
using namespace luxrays;
using namespace luxrays::sdl;
using namespace luxrays::utils;

namespace slg {

//------------------------------------------------------------------------------
// PathOCLRenderThread
//------------------------------------------------------------------------------

PathOCLRenderThread::PathOCLRenderThread(const u_int index,
		const float samplStart, OpenCLIntersectionDevice *device,
		PathOCLRenderEngine *re) {
	intersectionDevice = device;
	samplingStart = samplStart;

	renderThread = NULL;

	threadIndex = index;
	renderEngine = re;
	started = false;
	editMode = false;
	frameBuffer = NULL;
	alphaFrameBuffer = NULL;

	kernelsParameters = "";
	initKernel = NULL;
	initFBKernel = NULL;
	advancePathsKernel = NULL;

	raysBuff = NULL;
	hitsBuff = NULL;
	tasksBuff = NULL;
	taskStatsBuff = NULL;
	frameBufferBuff = NULL;
	alphaFrameBufferBuff = NULL;
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
	texMapDescBuff = NULL;
	meshTexMapsBuff = NULL;
	meshTexMapsInfoBuff = NULL;
	meshBumpMapsBuff = NULL;
	meshBumpMapsInfoBuff = NULL;
	meshNormalMapsBuff = NULL;
	meshNormalMapsInfoBuff = NULL;
	uvsBuff = NULL;

	gpuTaskStats = new slg::ocl::GPUTaskStats[renderEngine->taskCount];

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
	delete advancePathsKernel;

	delete[] frameBuffer;
	delete[] alphaFrameBuffer;
	delete[] gpuTaskStats;

	delete kernelCache;
}

void PathOCLRenderThread::AllocOCLBufferRO(cl::Buffer **buff, void *src, const size_t size, const string &desc) {
	if (*buff) {
		// Check the size of the already allocated buffer

		if (size == (*buff)->getInfo<CL_MEM_SIZE>()) {
			// I can reuse the buffer; just update the content

			//SLG_LOG("[PathOCLRenderThread::" << threadIndex << "] " << desc << " buffer updated for size: " << (size / 1024) << "Kbytes");
			cl::CommandQueue &oclQueue = intersectionDevice->GetOpenCLQueue();
			oclQueue.enqueueWriteBuffer(**buff, CL_FALSE, 0, size, src);
			return;
		} else {
			// Free the buffer
			intersectionDevice->FreeMemory((*buff)->getInfo<CL_MEM_SIZE>());
			delete *buff;
		}
	}

	cl::Context &oclContext = intersectionDevice->GetOpenCLContext();

	SLG_LOG("[PathOCLRenderThread::" << threadIndex << "] " << desc << " buffer size: " << (size / 1024) << "Kbytes");
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
			//SLG_LOG("[PathOCLRenderThread::" << threadIndex << "] " << desc << " buffer reused for size: " << (size / 1024) << "Kbytes");
			return;
		} else {
			// Free the buffer
			intersectionDevice->FreeMemory((*buff)->getInfo<CL_MEM_SIZE>());
			delete *buff;
		}
	}

	cl::Context &oclContext = intersectionDevice->GetOpenCLContext();

	SLG_LOG("[PathOCLRenderThread::" << threadIndex << "] " << desc << " buffer size: " << (size / 1024) << "Kbytes");
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
	frameBuffer = new slg::ocl::Pixel[frameBufferPixelCount];

	for (u_int i = 0; i < frameBufferPixelCount; ++i) {
		frameBuffer[i].c.r = 0.f;
		frameBuffer[i].c.g = 0.f;
		frameBuffer[i].c.b = 0.f;
		frameBuffer[i].count = 0.f;
	}

	AllocOCLBufferRW(&frameBufferBuff, sizeof(slg::ocl::Pixel) * frameBufferPixelCount, "FrameBuffer");

	delete[] alphaFrameBuffer;
	alphaFrameBuffer = NULL;

	// Check if the film has an alpha channel
	if (renderEngine->film->IsAlphaChannelEnabled()) {
		alphaFrameBuffer = new slg::ocl::AlphaPixel[frameBufferPixelCount];

		for (u_int i = 0; i < frameBufferPixelCount; ++i)
			alphaFrameBuffer[i].alpha = 0.f;

		AllocOCLBufferRW(&alphaFrameBufferBuff, sizeof(slg::ocl::AlphaPixel) * frameBufferPixelCount, "Alpha Channel FrameBuffer");
	}
}

void PathOCLRenderThread::InitCamera() {
	AllocOCLBufferRO(&cameraBuff, &renderEngine->compiledScene->camera,
			sizeof(luxrays::ocl::Camera), "Camera");
}

void PathOCLRenderThread::InitGeometry() {
	Scene *scene = renderEngine->renderConfig->scene;
	CompiledScene *cscene = renderEngine->compiledScene;

	const u_int trianglesCount = scene->dataSet->GetTotalTriangleCount();
	AllocOCLBufferRO(&meshIDBuff, (void *)cscene->meshIDs,
			sizeof(u_int) * trianglesCount, "MeshIDs");

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
				sizeof(u_int) * cscene->meshDescs.size(), "First mesh triangle offset");

		AllocOCLBufferRO(&meshDescsBuff, &cscene->meshDescs[0],
				sizeof(luxrays::ocl::Mesh) * cscene->meshDescs.size(), "Mesh description");
	} else {
		triangleIDBuff = NULL;
		meshDescsBuff = NULL;
	}
}

void PathOCLRenderThread::InitMaterials() {
//	const size_t materialsCount = renderEngine->compiledScene->mats.size();
//	AllocOCLBufferRO(&materialsBuff, &renderEngine->compiledScene->mats[0],
//			sizeof(slg::ocl::Material) * materialsCount, "Materials");
//
//	const u_int meshCount = renderEngine->compiledScene->meshMats.size();
//	AllocOCLBufferRO(&meshMatsBuff, &renderEngine->compiledScene->meshMats[0],
//			sizeof(u_int) * meshCount, "Mesh material index");
}

void PathOCLRenderThread::InitAreaLights() {
//	CompiledScene *cscene = renderEngine->compiledScene;
//
//	if (cscene->areaLights.size() > 0) {
//		AllocOCLBufferRO(&areaLightsBuff, &cscene->areaLights[0],
//			sizeof(slg::ocl::TriangleLight) * cscene->areaLights.size(), "AreaLights");
//	} else
//		areaLightsBuff = NULL;
}

void PathOCLRenderThread::InitInfiniteLight() {
//	CompiledScene *cscene = renderEngine->compiledScene;
//
//	if (cscene->infiniteLight) {
//		AllocOCLBufferRO(&infiniteLightBuff, cscene->infiniteLight,
//			sizeof(slg::ocl::InfiniteLight), "InfiniteLight");
//
//		const u_int pixelCount = cscene->infiniteLight->width * cscene->infiniteLight->height;
//		AllocOCLBufferRO(&infiniteLightMapBuff, (void *)cscene->infiniteLightMap,
//			sizeof(Spectrum) * pixelCount, "InfiniteLight map");
//	} else {
//		infiniteLightBuff = NULL;
//		infiniteLightMapBuff = NULL;
//	}
}

void PathOCLRenderThread::InitSunLight() {
//	CompiledScene *cscene = renderEngine->compiledScene;
//
//	if (cscene->sunLight)
//		AllocOCLBufferRO(&sunLightBuff, cscene->sunLight,
//			sizeof(slg::ocl::SunLight), "SunLight");
//	else
//		sunLightBuff = NULL;
}

void PathOCLRenderThread::InitSkyLight() {
//	CompiledScene *cscene = renderEngine->compiledScene;
//
//	if (cscene->skyLight)
//		AllocOCLBufferRO(&skyLightBuff, cscene->skyLight,
//			sizeof(slg::ocl::SkyLight), "SkyLight");
//	else
//		skyLightBuff = NULL;
}

void PathOCLRenderThread::InitTextureMaps() {
//	CompiledScene *cscene = renderEngine->compiledScene;
//
//	if ((cscene->totRGBTexMem > 0) || (cscene->totAlphaTexMem > 0)) {
//		if (cscene->totRGBTexMem > 0) {
//			texMapRGBBuff.resize(cscene->rgbTexMemBlocks.size());
//			for (u_int i = 0; i < cscene->rgbTexMemBlocks.size(); ++i) {
//				AllocOCLBufferRO(&(texMapRGBBuff[i]), &(cscene->rgbTexMemBlocks[i][0]),
//						sizeof(Spectrum) * cscene->rgbTexMemBlocks[i].size(), "TexMaps");
//			}
//		} else
//			texMapRGBBuff.resize(0);
//
//		if (cscene->totAlphaTexMem > 0) {
//			texMapAlphaBuff.resize(cscene->alphaTexMemBlocks.size());
//			for (u_int i = 0; i < cscene->alphaTexMemBlocks.size(); ++i) {
//				AllocOCLBufferRO(&(texMapAlphaBuff[i]), &(cscene->alphaTexMemBlocks[i][0]),
//						sizeof(float) * cscene->alphaTexMemBlocks[i].size(), "TexMaps Alpha Channel");
//			}
//		} else
//			texMapAlphaBuff.resize(0);
//
//		AllocOCLBufferRO(&texMapDescBuff, &cscene->gpuTexMaps[0],
//				sizeof(slg::ocl::TexMap) * cscene->gpuTexMaps.size(), "TexMaps description");
//
//		const u_int meshCount = renderEngine->compiledScene->meshMats.size();
//		AllocOCLBufferRO(&meshTexMapsBuff, &cscene->meshTexMaps[0],
//				sizeof(u_int) * meshCount, "Mesh TexMaps index");
//		AllocOCLBufferRO(&meshTexMapsInfoBuff, &cscene->meshTexMapsInfo[0],
//			sizeof(slg::ocl::TexMapInfo) * meshCount, "Mesh TexMaps info");
//
//		if (cscene->meshBumpMaps.size() > 0) {
//			AllocOCLBufferRO(&meshBumpMapsBuff, &cscene->meshBumpMaps[0],
//				sizeof(u_int) * meshCount, "Mesh BumpMaps index");
//			AllocOCLBufferRO(&meshBumpMapsInfoBuff, &cscene->meshBumpMapsInfo[0],
//				sizeof(slg::ocl::BumpMapInfo) * meshCount, "Mesh BumpMaps info");
//		} else {
//			meshBumpMapsBuff = NULL;
//			meshBumpMapsInfoBuff = NULL;
//		}
//
//		if (cscene->meshNormalMaps.size() > 0) {
//			AllocOCLBufferRO(&meshNormalMapsBuff, &cscene->meshNormalMaps[0],
//				sizeof(u_int) * meshCount, "Mesh NormalMaps index");
//			AllocOCLBufferRO(&meshNormalMapsInfoBuff, &cscene->meshNormalMapsInfo[0],
//				sizeof(slg::ocl::NormalMapInfo) * meshCount, "Mesh NormalMaps info");
//		} else {
//			meshNormalMapsBuff = NULL;
//			meshNormalMapsInfoBuff = NULL;
//		}
//	} else {
//		texMapRGBBuff.resize(0);
//		texMapAlphaBuff.resize(0);
//		texMapDescBuff = NULL;
//		meshTexMapsBuff = NULL;
//		meshTexMapsInfoBuff = NULL;
//		meshBumpMapsBuff = NULL;
//		meshBumpMapsInfoBuff = NULL;
//		meshNormalMapsBuff = NULL;
//		meshNormalMapsInfoBuff = NULL;
//	}
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
//			" -D PARAM_RAY_EPSILON=" << renderEngine->epsilon << "f" <<
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

//	if (cscene->enable_MAT_MATTE)
//		ss << " -D PARAM_ENABLE_MAT_MATTE";
//	if (cscene->enable_MAT_AREALIGHT)
//		ss << " -D PARAM_ENABLE_MAT_AREALIGHT";
//	if (cscene->enable_MAT_MIRROR)
//		ss << " -D PARAM_ENABLE_MAT_MIRROR";
//	if (cscene->enable_MAT_GLASS)
//		ss << " -D PARAM_ENABLE_MAT_GLASS";
//	if (cscene->enable_MAT_MATTEMIRROR)
//		ss << " -D PARAM_ENABLE_MAT_MATTEMIRROR";
//	if (cscene->enable_MAT_METAL)
//		ss << " -D PARAM_ENABLE_MAT_METAL";
//	if (cscene->enable_MAT_MATTEMETAL)
//		ss << " -D PARAM_ENABLE_MAT_MATTEMETAL";
//	if (cscene->enable_MAT_ALLOY)
//		ss << " -D PARAM_ENABLE_MAT_ALLOY";
//	if (cscene->enable_MAT_ARCHGLASS)
//		ss << " -D PARAM_ENABLE_MAT_ARCHGLASS";

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

//	if (areaLightsBuff) {
//		ss <<
//				" -D PARAM_DIRECT_LIGHT_SAMPLING" <<
//				" -D PARAM_DL_LIGHT_COUNT=" << renderEngine->compiledScene->areaLights.size()
//				;
//	}
//
//	if ((texMapRGBBuff.size() > 0) || (texMapAlphaBuff.size() > 0)) {
//		ss << " -D PARAM_HAS_TEXTUREMAPS";
//		for (u_int i = 0; i < cscene->rgbTexMemBlocks.size(); ++i)
//			ss << " -D PARAM_TEXTUREMAPS_RGB_PAGE_" << i;
//		if (cscene->rgbTexMemBlocks.size() > 5)
//			throw std::runtime_error("Too many memory pages required for RGB channels of a texture maps");
//	}
//	if (texMapAlphaBuff.size() > 0) {
//		ss << " -D PARAM_HAS_ALPHA_TEXTUREMAPS";
//		for (u_int i = 0; i < cscene->alphaTexMemBlocks.size(); ++i)
//			ss << " -D PARAM_TEXTUREMAPS_ALPHA_PAGE_" << i;
//		if (cscene->alphaTexMemBlocks.size() > 5)
//			throw std::runtime_error("Too many memory pages required for alpha channel of a texture maps");
//	}
//	if (meshBumpMapsBuff)
//		ss << " -D PARAM_HAS_BUMPMAPS";
//	if (meshNormalMapsBuff)
//		ss << " -D PARAM_HAS_NORMALMAPS";

	const luxrays::ocl::Filter *filter = renderEngine->filter;
	switch (filter->type) {
		case luxrays::ocl::FILTER_NONE:
			ss << " -D PARAM_IMAGE_FILTER_TYPE=NONE";
			break;
		case luxrays::ocl::FILTER_BOX:
			ss << " -D PARAM_IMAGE_FILTER_TYPE=BOX" <<
					" -D PARAM_IMAGE_FILTER_WIDTH_X=" << filter->box.widthX << "f" <<
					" -D PARAM_IMAGE_FILTER_WIDTH_Y=" << filter->box.widthY << "f";
			break;
		case luxrays::ocl::FILTER_GAUSSIAN:
			ss << " -D PARAM_IMAGE_FILTER_TYPE=GAUSSIAN" <<
					" -D PARAM_IMAGE_FILTER_WIDTH_X=" << filter->gaussian.widthX << "f" <<
					" -D PARAM_IMAGE_FILTER_WIDTH_Y=" << filter->gaussian.widthY << "f" <<
					" -D PARAM_IMAGE_FILTER_GAUSSIAN_ALPHA=" << filter->gaussian.alpha << "f";
			break;
		case luxrays::ocl::FILTER_MITCHELL:
			ss << " -D PARAM_IMAGE_FILTER_TYPE=MITCHELL" <<
					" -D PARAM_IMAGE_FILTER_WIDTH_X=" << filter->mitchell.widthX << "f" <<
					" -D PARAM_IMAGE_FILTER_WIDTH_Y=" << filter->mitchell.widthY << "f" <<
					" -D PARAM_IMAGE_FILTER_MITCHELL_B=" << filter->mitchell.B << "f" <<
					" -D PARAM_IMAGE_FILTER_MITCHELL_C=" << filter->mitchell.C << "f";
			break;
		default:
			assert (false);
	}

	if (renderEngine->usePixelAtomics)
		ss << " -D PARAM_USE_PIXEL_ATOMICS";

	const luxrays::ocl::Sampler *sampler = renderEngine->sampler;
	switch (sampler->type) {
		case luxrays::ocl::RANDOM:
			ss << " -D PARAM_SAMPLER_TYPE=RANDOM";
			break;
		case luxrays::ocl::METROPOLIS:
			ss << " -D PARAM_SAMPLER_TYPE=METROPOLIS" <<
					" -D PARAM_SAMPLER_METROPOLIS_LARGE_STEP_RATE=" << sampler->metropolis.largeMutationProbability << "f" <<
					" -D PARAM_SAMPLER_METROPOLIS_IMAGE_MUTATION_RANGE=" << sampler->metropolis.imageMutationRange << "f" <<
					" -D PARAM_SAMPLER_METROPOLIS_MAX_CONSECUTIVE_REJECT=" << sampler->metropolis.maxRejects;
			break;
		default:
			assert (false);
	}

	if (renderEngine->film->IsAlphaChannelEnabled())
		ss << " -D PARAM_ENABLE_ALPHA_CHANNEL";

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
			luxrays::ocl::KernelSource_SamplerTypes <<
			luxrays::ocl::KernelSource_FilterTypes <<
			luxrays::ocl::KernelSource_CameraTypes <<
			luxrays::ocl::KernelSource_TriangleMeshTypes <<
			slg::ocl::KernelSource_PathOCL_DataTypes <<
			slg::ocl::KernelSource_PathOCL_Kernels;
		string kernelSource = ssKernel.str();

		SLG_LOG("[PathOCLRenderThread::" << threadIndex << "] Defined symbols: " << kernelsParameters);
		SLG_LOG("[PathOCLRenderThread::" << threadIndex << "] Compiling kernels ");

		bool cached;
		cl::STRING_CLASS error;
		cl::Program *program = kernelCache->Compile(oclContext, oclDevice,
				kernelsParameters, kernelSource,
				&cached, &error);

		if (!program) {
			SLG_LOG("[PathOCLRenderThread::" << threadIndex << "] PathOCL kernel compilation error" << std::endl << error);

			throw std::runtime_error("PathOCL kernel compilation error");
		}

		if (cached) {
			SLG_LOG("[PathOCLRenderThread::" << threadIndex << "] Kernels cached");
		} else {
			SLG_LOG("[PathOCLRenderThread::" << threadIndex << "] Kernels not cached");
		}

		//----------------------------------------------------------------------
		// Init kernel
		//----------------------------------------------------------------------

		delete initKernel;
		SLG_LOG("[PathOCLRenderThread::" << threadIndex << "] Compiling Init Kernel");
		initKernel = new cl::Kernel(*program, "Init");
		initKernel->getWorkGroupInfo<size_t>(oclDevice, CL_KERNEL_WORK_GROUP_SIZE, &initWorkGroupSize);
		if (intersectionDevice->GetForceWorkGroupSize() > 0)
			initWorkGroupSize = intersectionDevice->GetForceWorkGroupSize();

		//--------------------------------------------------------------------------
		// InitFB kernel
		//--------------------------------------------------------------------------

		delete initFBKernel;
		initFBKernel = new cl::Kernel(*program, "InitFrameBuffer");
		initFBKernel->getWorkGroupInfo<size_t>(oclDevice, CL_KERNEL_WORK_GROUP_SIZE, &initFBWorkGroupSize);
		if (intersectionDevice->GetForceWorkGroupSize() > 0)
			initFBWorkGroupSize = intersectionDevice->GetForceWorkGroupSize();

		//----------------------------------------------------------------------
		// AdvancePaths kernel
		//----------------------------------------------------------------------

		delete advancePathsKernel;
		SLG_LOG("[PathOCLRenderThread::" << threadIndex << "] Compiling AdvancePaths Kernel");
		advancePathsKernel = new cl::Kernel(*program, "AdvancePaths");
		advancePathsKernel->getWorkGroupInfo<size_t>(oclDevice, CL_KERNEL_WORK_GROUP_SIZE, &advancePathsWorkGroupSize);
		if (intersectionDevice->GetForceWorkGroupSize() > 0)
			advancePathsWorkGroupSize = intersectionDevice->GetForceWorkGroupSize();

		//----------------------------------------------------------------------

		const double tEnd = WallClockTime();
		SLG_LOG("[PathOCLRenderThread::" << threadIndex << "] Kernels compilation time: " << int((tEnd - tStart) * 1000.0) << "ms");

		delete program;
	} else
		SLG_LOG("[PathOCLRenderThread::" << threadIndex << "] Using cached kernels");
}

void PathOCLRenderThread::InitRender() {
//	Scene *scene = renderEngine->renderConfig->scene;

	cl::Context &oclContext = intersectionDevice->GetOpenCLContext();

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

//	const u_int areaLightCount = renderEngine->compiledScene->areaLights.size();
//	if (!skyLightBuff && !sunLightBuff && !infiniteLightBuff && (areaLightCount == 0))
//		throw runtime_error("There are no light sources supported by PathOCL in the scene");

	//--------------------------------------------------------------------------
	// Translate mesh texture maps
	//--------------------------------------------------------------------------

	InitTextureMaps();

	//--------------------------------------------------------------------------
	// Allocate Ray/RayHit buffers
	//--------------------------------------------------------------------------

	const u_int taskCount = renderEngine->taskCount;

	tStart = WallClockTime();

	SLG_LOG("[PathOCLRenderThread::" << threadIndex << "] Ray buffer size: " << (sizeof(Ray) * taskCount / 1024) << "Kbytes");
	raysBuff = new cl::Buffer(oclContext,
			CL_MEM_READ_WRITE,
			sizeof(Ray) * taskCount);
	intersectionDevice->AllocMemory(raysBuff->getInfo<CL_MEM_SIZE>());

	SLG_LOG("[PathOCLRenderThread::" << threadIndex << "] RayHit buffer size: " << (sizeof(RayHit) * taskCount / 1024) << "Kbytes");
	hitsBuff = new cl::Buffer(oclContext,
			CL_MEM_READ_WRITE,
			sizeof(RayHit) * taskCount);
	intersectionDevice->AllocMemory(hitsBuff->getInfo<CL_MEM_SIZE>());

	tEnd = WallClockTime();
	SLG_LOG("[PathOCLRenderThread::" << threadIndex << "] OpenCL buffer creation time: " << int((tEnd - tStart) * 1000.0) << "ms");

	//--------------------------------------------------------------------------
	// Allocate GPU task buffers
	//--------------------------------------------------------------------------

//	// TODO: clenup all this mess
//
//	const size_t gpuTaksSizePart1 =
//		// Seed size
//		sizeof(slg::ocl::Seed);
//
//	const size_t uDataEyePathVertexSize =
//		// IDX_SCREEN_X, IDX_SCREEN_Y
//		sizeof(float) * 2 +
//		// IDX_DOF_X, IDX_DOF_Y
//		((scene->camera->lensRadius > 0.f) ? (sizeof(float) * 2) : 0);
//	const size_t uDataPerPathVertexSize =
//		// IDX_TEX_ALPHA,
//		((texMapAlphaBuff.size() > 0) ? sizeof(float) : 0) +
//		// IDX_BSDF_X, IDX_BSDF_Y, IDX_BSDF_Z
//		sizeof(float) * 3 +
//		// IDX_DIRECTLIGHT_X, IDX_DIRECTLIGHT_Y, IDX_DIRECTLIGHT_Z
//		(((areaLightCount > 0) || sunLightBuff) ? (sizeof(float) * 3) : 0) +
//		// IDX_RR
//		sizeof(float);
//	const size_t uDataSize = (renderEngine->sampler->type == slg::ocl::INLINED_RANDOM) ?
//		// Only IDX_SCREEN_X, IDX_SCREEN_Y
//		(sizeof(float) * 2) :
//		((renderEngine->sampler->type == slg::ocl::METROPOLIS) ?
//			(sizeof(float) * 2 + sizeof(u_int) * 5 + sizeof(Spectrum) + 2 * (uDataEyePathVertexSize + uDataPerPathVertexSize * renderEngine->maxPathDepth)) :
//			(uDataEyePathVertexSize + uDataPerPathVertexSize * renderEngine->maxPathDepth));
//
//	size_t sampleSize =
//		// uint pixelIndex;
//		((renderEngine->sampler->type == slg::ocl::METROPOLIS) ? 0 : sizeof(u_int)) +
//		uDataSize +
//		// Spectrum radiance;
//		sizeof(Spectrum) +
//		// float currentAlpha;
//		(((renderEngine->sampler->type == slg::ocl::METROPOLIS) && alphaFrameBufferBuff) ? sizeof(float) : 0);
//
//	stratifiedDataSize = 0;
//	if (renderEngine->sampler->type == slg::ocl::STRATIFIED) {
//		slg::ocl::StratifiedSampler *s = (slg::ocl::StratifiedSampler *)renderEngine->sampler;
//		stratifiedDataSize =
//				// stratifiedScreen2D
//				sizeof(float) * s->xSamples * s->ySamples * 2 +
//				// stratifiedDof2D
//				((scene->camera->lensRadius > 0.f) ? (sizeof(float) * s->xSamples * s->ySamples * 2) : 0) +
//				// stratifiedAlpha1D
//				((texMapAlphaBuff.size() > 0) ? (sizeof(float) * s->xSamples) : 0) +
//				// stratifiedBSDF2D
//				sizeof(float) * s->xSamples * s->ySamples * 2 +
//				// stratifiedBSDF1D
//				sizeof(float) * s->xSamples +
//				// stratifiedLight2D
//				// stratifiedLight1D
//				(((areaLightCount > 0) || sunLightBuff) ? (sizeof(float) * s->xSamples * s->ySamples * 2 + sizeof(float) * s->xSamples) : 0);
//
//		sampleSize += stratifiedDataSize;
//	}
//
//	const size_t gpuTaksSizePart2 = sampleSize;
//
//	const size_t gpuTaksSizePart3 =
//		// PathState size
//		(((areaLightCount > 0) || sunLightBuff) ? sizeof(slg::ocl::PathStateDL) : sizeof(slg::ocl::PathState)) +
//		//u_int diffuseVertexCount;
//		((renderEngine->maxDiffusePathVertexCount < renderEngine->maxPathDepth) ? sizeof(u_int) : 0) +
//		//uint vertexCount;
//		//float alpha;
//		(alphaFrameBufferBuff ? (sizeof(u_int) + sizeof(float)) : 0);
//
//	const size_t gpuTaksSize = gpuTaksSizePart1 + gpuTaksSizePart2 + gpuTaksSizePart3;
//	SLG_LOG("[PathOCLRenderThread::" << threadIndex << "] Size of a GPUTask: " << gpuTaksSize <<
//			"bytes (" << gpuTaksSizePart1 << " + " << gpuTaksSizePart2 << " + " << gpuTaksSizePart3 << ")");
//	SLG_LOG("[PathOCLRenderThread::" << threadIndex << "] Tasks buffer size: " << (gpuTaksSize * taskCount / 1024) << "Kbytes");
//
//	// Check if the task buffer is too big
//	if (intersectionDevice->GetDeviceDesc()->GetMaxMemoryAllocSize() < gpuTaksSize * taskCount) {
//		stringstream ss;
//		ss << "The GPUTask buffer is too big for this device (i.e. CL_DEVICE_MAX_MEM_ALLOC_SIZE=" <<
//				intersectionDevice->GetDeviceDesc()->GetMaxMemoryAllocSize() <<
//				"): try to reduce opencl.task.count and/or path.maxdepth and/or to change Sampler";
//		throw std::runtime_error(ss.str());
//	}
//
//	tasksBuff = new cl::Buffer(oclContext,
//			CL_MEM_READ_WRITE,
//			gpuTaksSize * taskCount);
//	intersectionDevice->AllocMemory(tasksBuff->getInfo<CL_MEM_SIZE>());

	//--------------------------------------------------------------------------
	// Allocate GPU task statistic buffers
	//--------------------------------------------------------------------------

	SLG_LOG("[PathOCLRenderThread::" << threadIndex << "] Task Stats buffer size: " << (sizeof(slg::ocl::GPUTaskStats) * taskCount / 1024) << "Kbytes");
	taskStatsBuff = new cl::Buffer(oclContext,
			CL_MEM_READ_WRITE,
			sizeof(slg::ocl::GPUTaskStats) * taskCount);
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
			cl::NDRange(RoundUp<u_int>(frameBufferPixelCount, initFBWorkGroupSize)),
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
//	CompiledScene *cscene = renderEngine->compiledScene;

	//--------------------------------------------------------------------------
	// advancePathsKernel
	//--------------------------------------------------------------------------
	u_int argIndex = 0;
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
//	if (infiniteLightBuff) {
//		advancePathsKernel->setArg(argIndex++, *infiniteLightBuff);
//		advancePathsKernel->setArg(argIndex++, *infiniteLightMapBuff);
//	}
//	if (sunLightBuff)
//		advancePathsKernel->setArg(argIndex++, *sunLightBuff);
//	if (skyLightBuff)
//		advancePathsKernel->setArg(argIndex++, *skyLightBuff);
//	if (areaLightsBuff)
//		advancePathsKernel->setArg(argIndex++, *areaLightsBuff);
//	for (u_int i = 0; i < cscene->rgbTexMemBlocks.size(); ++i)
//		advancePathsKernel->setArg(argIndex++, *(texMapRGBBuff[i]));
//	for (u_int i = 0; i < cscene->alphaTexMemBlocks.size(); ++i)
//		advancePathsKernel->setArg(argIndex++, *(texMapAlphaBuff[i]));
//	if ((cscene->rgbTexMemBlocks.size() > 0) || (cscene->alphaTexMemBlocks.size() > 0)) {
//		advancePathsKernel->setArg(argIndex++, *texMapDescBuff);
//		advancePathsKernel->setArg(argIndex++, *meshTexMapsBuff);
//		advancePathsKernel->setArg(argIndex++, *meshTexMapsInfoBuff);
//		if (meshBumpMapsBuff) {
//			advancePathsKernel->setArg(argIndex++, *meshBumpMapsBuff);
//			advancePathsKernel->setArg(argIndex++, *meshBumpMapsInfoBuff);
//		}
//		if (meshNormalMapsBuff) {
//			advancePathsKernel->setArg(argIndex++, *meshNormalMapsBuff);
//			advancePathsKernel->setArg(argIndex++, *meshNormalMapsInfoBuff);
//		}
//		advancePathsKernel->setArg(argIndex++, *uvsBuff);
//	}
//	if (alphaFrameBufferBuff)
//		advancePathsKernel->setArg(argIndex++, *alphaFrameBufferBuff);

	//--------------------------------------------------------------------------
	// initFBKernel
	//--------------------------------------------------------------------------

	initFBKernel->setArg(0, *frameBufferBuff);
	if (alphaFrameBufferBuff)
		initFBKernel->setArg(1, *alphaFrameBufferBuff);

	//--------------------------------------------------------------------------
	// initKernel
	//--------------------------------------------------------------------------

	argIndex = 0;
	initKernel->setArg(argIndex++, renderEngine->seedBase + threadIndex * renderEngine->taskCount);
	initKernel->setArg(argIndex++, *tasksBuff);
	initKernel->setArg(argIndex++, *taskStatsBuff);
	initKernel->setArg(argIndex++, *raysBuff);
	if (cameraBuff)
		initKernel->setArg(argIndex++, *cameraBuff);
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

		// Check if I have to transfer the alpha channel too
		if (alphaFrameBufferBuff) {
			oclQueue.enqueueReadBuffer(
				*alphaFrameBufferBuff,
				CL_TRUE,
				0,
				alphaFrameBufferBuff->getInfo<CL_MEM_SIZE>(),
				alphaFrameBuffer);
		}
	}

	FreeOCLBuffer(&raysBuff);
	FreeOCLBuffer(&hitsBuff);
	FreeOCLBuffer(&tasksBuff);
	FreeOCLBuffer(&taskStatsBuff);
	FreeOCLBuffer(&frameBufferBuff);
	FreeOCLBuffer(&alphaFrameBufferBuff);
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
	for (u_int i = 0; i < texMapRGBBuff.size(); ++i)
		FreeOCLBuffer(&texMapRGBBuff[i]);
	for (u_int i = 0; i < texMapAlphaBuff.size(); ++i)
		FreeOCLBuffer(&texMapAlphaBuff[i]);
	if ((texMapAlphaBuff.size() > 0) || (texMapAlphaBuff.size() > 0)) {
		FreeOCLBuffer(&texMapDescBuff);
		FreeOCLBuffer(&meshTexMapsBuff);
		FreeOCLBuffer(&meshTexMapsInfoBuff);

		if (meshBumpMapsBuff) {
			FreeOCLBuffer(&meshBumpMapsBuff);
			FreeOCLBuffer(&meshBumpMapsInfoBuff);
		}

		if (meshNormalMapsBuff) {
			FreeOCLBuffer(&meshNormalMapsBuff);
			FreeOCLBuffer(&meshNormalMapsInfoBuff);
		}

		FreeOCLBuffer(&uvsBuff);
	}

	started = false;

	// frameBuffer is delete on the destructor to allow image saving after
	// the rendering is finished
}

void PathOCLRenderThread::StartRenderThread() {
	// Create the thread for the rendering
	renderThread = new boost::thread(&PathOCLRenderThread::RenderThreadImpl, this);
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
		// Resize the Frame Buffer
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
			cl::NDRange(RoundUp<u_int>(frameBufferPixelCount, initFBWorkGroupSize)),
			cl::NDRange(initFBWorkGroupSize));

		// Initialize the tasks buffer
		oclQueue.enqueueNDRangeKernel(*initKernel, cl::NullRange,
				cl::NDRange(renderEngine->taskCount), cl::NDRange(initWorkGroupSize));
	}

	// Reset statistics in order to be more accurate
	intersectionDevice->ResetPerformaceStats();

	StartRenderThread();
}

void PathOCLRenderThread::RenderThreadImpl() {
	//SLG_LOG("[PathOCLRenderThread::" << threadIndex << "] Rendering thread started");

	cl::CommandQueue &oclQueue = intersectionDevice->GetOpenCLQueue();
	const u_int taskCount = renderEngine->taskCount;

	try {
		double startTime = WallClockTime();
		while (!boost::this_thread::interruption_requested()) {
			/*if(threadIndex == 0)
				cerr<< "[DEBUG] =================================");*/

			// Async. transfer of the frame buffer
			oclQueue.enqueueReadBuffer(
				*frameBufferBuff,
				CL_FALSE,
				0,
				frameBufferBuff->getInfo<CL_MEM_SIZE>(),
				frameBuffer);

			// Check if I have to transfer the alpha channel too
			if (alphaFrameBufferBuff) {
				// Async. transfer of the alpha channel
				oclQueue.enqueueReadBuffer(
					*alphaFrameBufferBuff,
					CL_FALSE,
					0,
					alphaFrameBufferBuff->getInfo<CL_MEM_SIZE>(),
					alphaFrameBuffer);
			}

			// Async. transfer of GPU task statistics
			oclQueue.enqueueReadBuffer(
				*(taskStatsBuff),
				CL_FALSE,
				0,
				sizeof(slg::ocl::GPUTaskStats) * taskCount,
				gpuTaskStats);

			for (;;) {
				cl::Event event;

				// Decide how many kernels to enqueue
				const u_int screenRefreshInterval = renderEngine->renderConfig->GetScreenRefreshInterval();

				u_int iterations;
				if (screenRefreshInterval <= 100)
					iterations = 1;
				else if (screenRefreshInterval <= 500)
					iterations = 2;
				else if (screenRefreshInterval <= 1000)
					iterations = 4;
				else
					iterations = 8;

				for (u_int i = 0; i < iterations; ++i) {
					// Advance to next path state
					oclQueue.enqueueNDRangeKernel(*advancePathsKernel, cl::NullRange,
							cl::NDRange(taskCount), cl::NDRange(advancePathsWorkGroupSize));

					// Trace rays
					intersectionDevice->EnqueueTraceRayBuffer(*raysBuff,
								*(hitsBuff), taskCount, NULL, NULL);
				}
				oclQueue.flush();

				event.wait();
				const double elapsedTime = WallClockTime() - startTime;

				/*if(threadIndex == 0)
					cerr<< "[DEBUG] Elapsed time: " << elapsedTime * 1000.0 <<
							"ms (screenRefreshInterval: " << renderEngine->screenRefreshInterval << ")");*/

				if ((elapsedTime * 1000.0 > (double)screenRefreshInterval) ||
						boost::this_thread::interruption_requested())
					break;
			}

			startTime = WallClockTime();
		}

		//SLG_LOG("[PathOCLRenderThread::" << threadIndex << "] Rendering thread halted");
	} catch (boost::thread_interrupted) {
		SLG_LOG("[PathOCLRenderThread::" << threadIndex << "] Rendering thread halted");
	} catch (cl::Error err) {
		SLG_LOG("[PathOCLRenderThread::" << threadIndex << "] Rendering thread ERROR: " << err.what() <<
				"(" << luxrays::utils::oclErrorString(err.err()) << ")");
	}

	oclQueue.enqueueReadBuffer(
			*frameBufferBuff,
			CL_TRUE,
			0,
			frameBufferBuff->getInfo<CL_MEM_SIZE>(),
			frameBuffer);

	// Check if I have to transfer the alpha channel too
	if (alphaFrameBufferBuff) {
		oclQueue.enqueueReadBuffer(
			*alphaFrameBufferBuff,
			CL_TRUE,
			0,
			alphaFrameBufferBuff->getInfo<CL_MEM_SIZE>(),
			alphaFrameBuffer);
	}
}

}

#endif
