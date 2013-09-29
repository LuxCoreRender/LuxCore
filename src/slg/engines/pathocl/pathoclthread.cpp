/***************************************************************************
 *   Copyright (C) 1998-2013 by authors (see AUTHORS.txt)                  *
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

#include <boost/lexical_cast.hpp>

#include "slg/slg.h"
#include "slg/engines/pathocl/pathocl.h"
#include "slg/engines/pathocl/rtpathocl.h"
#include "slg/kernels/kernels.h"
#include "slg/renderconfig.h"

#include "luxrays/core/geometry/transform.h"
#include "luxrays/utils/ocl.h"
#include "luxrays/core/oclintersectiondevice.h"
#include "luxrays/kernels/kernels.h"

using namespace std;
using namespace luxrays;
using namespace slg;

#define SOBOL_MAXDEPTH 6

//------------------------------------------------------------------------------
// PathOCLRenderThread
//------------------------------------------------------------------------------

PathOCLRenderThread::PathOCLRenderThread(const u_int index,
		OpenCLIntersectionDevice *device, PathOCLRenderEngine *re) {
	intersectionDevice = device;

	renderThread = NULL;

	threadIndex = index;
	renderEngine = re;
	started = false;
	editMode = false;
	film = NULL;
	gpuTaskStats = NULL;

	kernelsParameters = "";
	initKernel = NULL;
	initFilmKernel = NULL;
	advancePathsKernel = NULL;

	clearFBKernel = NULL;
	clearSBKernel = NULL;
	mergeFBKernel = NULL;
	normalizeFBKernel = NULL;
	applyBlurFilterXR1Kernel = NULL;
	applyBlurFilterYR1Kernel = NULL;
	toneMapLinearKernel = NULL;
	updateScreenBufferKernel = NULL;

	raysBuff = NULL;
	hitsBuff = NULL;
	tasksBuff = NULL;
	samplesBuff = NULL;
	sampleDataBuff = NULL;
	taskStatsBuff = NULL;

	// Film buffers
	channel_ALPHA_Buff = NULL;
	channel_DEPTH_Buff = NULL;
	channel_POSITION_Buff = NULL;
	channel_GEOMETRY_NORMAL_Buff = NULL;
	channel_SHADING_NORMAL_Buff = NULL;
	channel_MATERIAL_ID_Buff = NULL;
	channel_DIRECT_DIFFUSE_Buff = NULL;
	channel_DIRECT_GLOSSY_Buff = NULL;
	channel_EMISSION_Buff = NULL;
	channel_INDIRECT_DIFFUSE_Buff = NULL;
	channel_INDIRECT_GLOSSY_Buff = NULL;
	channel_INDIRECT_SPECULAR_Buff = NULL;
	channel_MATERIAL_ID_MASK_Buff = NULL;
	channel_DIRECT_SHADOW_MASK_Buff = NULL;
	channel_INDIRECT_SHADOW_MASK_Buff = NULL;
	channel_UV_Buff = NULL;

	// Scene buffers
	materialsBuff = NULL;
	texturesBuff = NULL;
	meshDescsBuff = NULL;
	meshMatsBuff = NULL;
	infiniteLightBuff = NULL;
	infiniteLightDistributionBuff = NULL;
	sunLightBuff = NULL;
	skyLightBuff = NULL;
	lightsDistributionBuff = NULL;
	vertsBuff = NULL;
	normalsBuff = NULL;
	uvsBuff = NULL;
	colsBuff = NULL;
	alphasBuff = NULL;
	trianglesBuff = NULL;
	cameraBuff = NULL;
	triLightDefsBuff = NULL;
	meshTriLightDefsOffsetBuff = NULL;
	imageMapDescsBuff = NULL;

	// Check the kind of kernel cache to use
	std::string type = re->renderConfig->cfg.GetString("opencl.kernelcache", "PERSISTENT");
	if (type == "PERSISTENT")
		kernelCache = new oclKernelPersistentCache("SLG_" SLG_VERSION_MAJOR "." SLG_VERSION_MINOR);
	else if (type == "VOLATILE")
		kernelCache = new oclKernelVolatileCache();
	else if (type == "NONE")
		kernelCache = new oclKernelDummyCache();
	else
		throw std::runtime_error("Unknown opencl.kernelcache type: " + type);
}

PathOCLRenderThread::~PathOCLRenderThread() {
	if (editMode)
		EndEdit(EditActionList());
	if (started)
		Stop();

	delete initKernel;
	delete initFilmKernel;
	delete advancePathsKernel;
	delete clearFBKernel;
	delete clearSBKernel;
	delete mergeFBKernel;
	delete normalizeFBKernel;
	delete applyBlurFilterXR1Kernel;
	delete applyBlurFilterYR1Kernel;
	delete toneMapLinearKernel;
	delete updateScreenBufferKernel;

	delete film;
	delete[] gpuTaskStats;

	delete kernelCache;
}

void PathOCLRenderThread::AllocOCLBufferRO(cl::Buffer **buff, void *src, const size_t size, const string &desc) {
	// Check if the buffer is too big
	if (intersectionDevice->GetDeviceDesc()->GetMaxMemoryAllocSize() < size) {
		stringstream ss;
		ss << "The " << desc << " buffer is too big for " << intersectionDevice->GetName() << 
				" device (i.e. CL_DEVICE_MAX_MEM_ALLOC_SIZE=" <<
				intersectionDevice->GetDeviceDesc()->GetMaxMemoryAllocSize() <<
				"): try to reduce related parameters";
		throw std::runtime_error(ss.str());
	}

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

	SLG_LOG("[PathOCLRenderThread::" << threadIndex << "] " << desc << " buffer size: " <<
			(size < 10000 ? size : (size / 1024)) << (size < 10000 ? "bytes" : "Kbytes"));
	*buff = new cl::Buffer(oclContext,
			CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
			size, src);
	intersectionDevice->AllocMemory((*buff)->getInfo<CL_MEM_SIZE>());
}

void PathOCLRenderThread::AllocOCLBufferRW(cl::Buffer **buff, const size_t size, const string &desc) {
	// Check if the buffer is too big
	if (intersectionDevice->GetDeviceDesc()->GetMaxMemoryAllocSize() < size) {
		stringstream ss;
		ss << "The " << desc << " buffer is too big for " << intersectionDevice->GetName() << 
				" device (i.e. CL_DEVICE_MAX_MEM_ALLOC_SIZE=" <<
				intersectionDevice->GetDeviceDesc()->GetMaxMemoryAllocSize() <<
				"): try to reduce related parameters";
		throw std::runtime_error(ss.str());
	}

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
 
	SLG_LOG("[PathOCLRenderThread::" << threadIndex << "] " << desc << " buffer size: " <<
			(size < 10000 ? size : (size / 1024)) << (size < 10000 ? "bytes" : "Kbytes"));
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

void PathOCLRenderThread::InitFilm() {
	const Film *engineFilm = renderEngine->film;
	const u_int filmPixelCount = engineFilm->GetWidth() * engineFilm->GetHeight();

	// Delete previous allocated Film
	delete film;

	// Allocate the new Film
	film = new Film(engineFilm->GetWidth(), engineFilm->GetHeight());
	film->CopyDynamicSettings(*engineFilm);
	film->Init();

	//--------------------------------------------------------------------------
	for (u_int i = 0; i < channel_RADIANCE_PER_PIXEL_NORMALIZEDs_Buff.size(); ++i)
		FreeOCLBuffer(&channel_RADIANCE_PER_PIXEL_NORMALIZEDs_Buff[i]);

	if (film->GetRadianceGroupCount() > 8)
			throw runtime_error("PathOCL supports only up to 8 Radiance Groups");

	channel_RADIANCE_PER_PIXEL_NORMALIZEDs_Buff.resize(film->GetRadianceGroupCount());
	for (u_int i = 0; i < channel_RADIANCE_PER_PIXEL_NORMALIZEDs_Buff.size(); ++i) {
		AllocOCLBufferRW(&channel_RADIANCE_PER_PIXEL_NORMALIZEDs_Buff[i],
				sizeof(float[4]) * filmPixelCount, "RADIANCE_PER_PIXEL_NORMALIZEDs[" + ToString(i) + "]");
	}
	//--------------------------------------------------------------------------
	if (film->HasChannel(Film::ALPHA))
		AllocOCLBufferRW(&channel_ALPHA_Buff, sizeof(float[2]) * filmPixelCount, "ALPHA");
	else
		FreeOCLBuffer(&channel_ALPHA_Buff);
	//--------------------------------------------------------------------------
	if (film->HasChannel(Film::DEPTH))
		AllocOCLBufferRW(&channel_DEPTH_Buff, sizeof(float) * filmPixelCount, "DEPTH");
	else
		FreeOCLBuffer(&channel_DEPTH_Buff);
	//--------------------------------------------------------------------------
	if (film->HasChannel(Film::POSITION))
		AllocOCLBufferRW(&channel_POSITION_Buff, sizeof(float[3]) * filmPixelCount, "POSITION");
	else
		FreeOCLBuffer(&channel_POSITION_Buff);
	//--------------------------------------------------------------------------
	if (film->HasChannel(Film::GEOMETRY_NORMAL))
		AllocOCLBufferRW(&channel_GEOMETRY_NORMAL_Buff, sizeof(float[3]) * filmPixelCount, "GEOMETRY_NORMAL");
	else
		FreeOCLBuffer(&channel_GEOMETRY_NORMAL_Buff);
	//--------------------------------------------------------------------------
	if (film->HasChannel(Film::SHADING_NORMAL))
		AllocOCLBufferRW(&channel_SHADING_NORMAL_Buff, sizeof(float[3]) * filmPixelCount, "SHADING_NORMAL");
	else
		FreeOCLBuffer(&channel_SHADING_NORMAL_Buff);
	//--------------------------------------------------------------------------
	if (film->HasChannel(Film::MATERIAL_ID))
		AllocOCLBufferRW(&channel_MATERIAL_ID_Buff, sizeof(u_int) * filmPixelCount, "MATERIAL_ID");
	else
		FreeOCLBuffer(&channel_MATERIAL_ID_Buff);
	//--------------------------------------------------------------------------
	if (film->HasChannel(Film::DIRECT_DIFFUSE))
		AllocOCLBufferRW(&channel_DIRECT_DIFFUSE_Buff, sizeof(float[4]) * filmPixelCount, "DIRECT_DIFFUSE");
	else
		FreeOCLBuffer(&channel_DIRECT_DIFFUSE_Buff);
	//--------------------------------------------------------------------------
	if (film->HasChannel(Film::DIRECT_GLOSSY))
		AllocOCLBufferRW(&channel_DIRECT_GLOSSY_Buff, sizeof(float[4]) * filmPixelCount, "DIRECT_GLOSSY");
	else
		FreeOCLBuffer(&channel_DIRECT_GLOSSY_Buff);
	//--------------------------------------------------------------------------
	if (film->HasChannel(Film::EMISSION))
		AllocOCLBufferRW(&channel_EMISSION_Buff, sizeof(float[4]) * filmPixelCount, "EMISSION");
	else
		FreeOCLBuffer(&channel_EMISSION_Buff);
	//--------------------------------------------------------------------------
	if (film->HasChannel(Film::INDIRECT_DIFFUSE))
		AllocOCLBufferRW(&channel_INDIRECT_DIFFUSE_Buff, sizeof(float[4]) * filmPixelCount, "INDIRECT_DIFFUSE");
	else
		FreeOCLBuffer(&channel_INDIRECT_DIFFUSE_Buff);
	//--------------------------------------------------------------------------
	if (film->HasChannel(Film::INDIRECT_GLOSSY))
		AllocOCLBufferRW(&channel_INDIRECT_GLOSSY_Buff, sizeof(float[4]) * filmPixelCount, "INDIRECT_GLOSSY");
	else
		FreeOCLBuffer(&channel_INDIRECT_GLOSSY_Buff);
	//--------------------------------------------------------------------------
	if (film->HasChannel(Film::INDIRECT_SPECULAR))
		AllocOCLBufferRW(&channel_INDIRECT_SPECULAR_Buff, sizeof(float[4]) * filmPixelCount, "INDIRECT_SPECULAR");
	else
		FreeOCLBuffer(&channel_INDIRECT_SPECULAR_Buff);
	//--------------------------------------------------------------------------
	if (film->HasChannel(Film::MATERIAL_ID_MASK)) {
		if (film->GetMaskMaterialIDCount() > 1)
			throw runtime_error("PathOCL supports only 1 MATERIAL_ID_MASK");
		else
			AllocOCLBufferRW(&channel_MATERIAL_ID_MASK_Buff,
					sizeof(float[2]) * filmPixelCount, "MATERIAL_ID_MASK");
	} else
		FreeOCLBuffer(&channel_MATERIAL_ID_MASK_Buff);
	//--------------------------------------------------------------------------
	if (film->HasChannel(Film::DIRECT_SHADOW_MASK))
		AllocOCLBufferRW(&channel_DIRECT_SHADOW_MASK_Buff, sizeof(float[2]) * filmPixelCount, "DIRECT_SHADOW_MASK");
	else
		FreeOCLBuffer(&channel_DIRECT_SHADOW_MASK_Buff);
	//--------------------------------------------------------------------------
	if (film->HasChannel(Film::INDIRECT_SHADOW_MASK))
		AllocOCLBufferRW(&channel_INDIRECT_SHADOW_MASK_Buff, sizeof(float[2]) * filmPixelCount, "INDIRECT_SHADOW_MASK");
	else
		FreeOCLBuffer(&channel_INDIRECT_SHADOW_MASK_Buff);
	//--------------------------------------------------------------------------
	if (film->HasChannel(Film::UV))
		AllocOCLBufferRW(&channel_UV_Buff, sizeof(float[2]) * filmPixelCount, "UV");
	else
		FreeOCLBuffer(&channel_UV_Buff);
}

void PathOCLRenderThread::InitCamera() {
	AllocOCLBufferRO(&cameraBuff, &renderEngine->compiledScene->camera,
			sizeof(slg::ocl::Camera), "Camera");
}

void PathOCLRenderThread::InitGeometry() {
	CompiledScene *cscene = renderEngine->compiledScene;

	if (cscene->normals.size() > 0)
		AllocOCLBufferRO(&normalsBuff, &cscene->normals[0],
				sizeof(Normal) * cscene->normals.size(), "Normals");
	else
		FreeOCLBuffer(&normalsBuff);

	if (cscene->uvs.size() > 0)
		AllocOCLBufferRO(&uvsBuff, &cscene->uvs[0],
			sizeof(UV) * cscene->uvs.size(), "UVs");
	else
		FreeOCLBuffer(&uvsBuff);

	if (cscene->cols.size() > 0)
		AllocOCLBufferRO(&colsBuff, &cscene->cols[0],
			sizeof(Spectrum) * cscene->cols.size(), "Colors");
	else
		FreeOCLBuffer(&colsBuff);

	if (cscene->alphas.size() > 0)
		AllocOCLBufferRO(&alphasBuff, &cscene->alphas[0],
			sizeof(float) * cscene->alphas.size(), "Alphas");
	else
		FreeOCLBuffer(&alphasBuff);

	AllocOCLBufferRO(&vertsBuff, &cscene->verts[0],
		sizeof(Point) * cscene->verts.size(), "Vertices");

	AllocOCLBufferRO(&trianglesBuff, &cscene->tris[0],
		sizeof(Triangle) * cscene->tris.size(), "Triangles");

	AllocOCLBufferRO(&meshDescsBuff, &cscene->meshDescs[0],
			sizeof(slg::ocl::Mesh) * cscene->meshDescs.size(), "Mesh description");
}

void PathOCLRenderThread::InitMaterials() {
	const size_t materialsCount = renderEngine->compiledScene->mats.size();
	AllocOCLBufferRO(&materialsBuff, &renderEngine->compiledScene->mats[0],
			sizeof(slg::ocl::Material) * materialsCount, "Materials");

	const u_int meshCount = renderEngine->compiledScene->meshMats.size();
	AllocOCLBufferRO(&meshMatsBuff, &renderEngine->compiledScene->meshMats[0],
			sizeof(u_int) * meshCount, "Mesh material index");
}

void PathOCLRenderThread::InitTextures() {
	const size_t texturesCount = renderEngine->compiledScene->texs.size();
	AllocOCLBufferRO(&texturesBuff, &renderEngine->compiledScene->texs[0],
			sizeof(slg::ocl::Texture) * texturesCount, "Textures");
}

void PathOCLRenderThread::InitTriangleAreaLights() {
	CompiledScene *cscene = renderEngine->compiledScene;

	if (cscene->triLightDefs.size() > 0) {
		AllocOCLBufferRO(&triLightDefsBuff, &cscene->triLightDefs[0],
			sizeof(slg::ocl::TriangleLight) * cscene->triLightDefs.size(), "Triangle AreaLights");
		AllocOCLBufferRO(&meshTriLightDefsOffsetBuff, &cscene->meshTriLightDefsOffset[0],
			sizeof(u_int) * cscene->meshTriLightDefsOffset.size(), "Triangle AreaLights offsets");
	} else {
		FreeOCLBuffer(&triLightDefsBuff);
		FreeOCLBuffer(&meshTriLightDefsOffsetBuff);
	}
}

void PathOCLRenderThread::InitInfiniteLight() {
	CompiledScene *cscene = renderEngine->compiledScene;

	if (cscene->infiniteLight) {
		AllocOCLBufferRO(&infiniteLightBuff, cscene->infiniteLight,
			sizeof(slg::ocl::InfiniteLight), "InfiniteLight");
		AllocOCLBufferRO(&infiniteLightDistributionBuff, cscene->infiniteLightDistribution,
			cscene->infiniteLightDistributionSize, "InfiniteLight Distribution");
	} else {
		FreeOCLBuffer(&infiniteLightBuff);
		FreeOCLBuffer(&infiniteLightDistributionBuff);
	}
}

void PathOCLRenderThread::InitSunLight() {
	CompiledScene *cscene = renderEngine->compiledScene;

	if (cscene->sunLight)
		AllocOCLBufferRO(&sunLightBuff, cscene->sunLight,
			sizeof(slg::ocl::SunLight), "SunLight");
	else
		FreeOCLBuffer(&sunLightBuff);
}

void PathOCLRenderThread::InitSkyLight() {
	CompiledScene *cscene = renderEngine->compiledScene;

	if (cscene->skyLight)
		AllocOCLBufferRO(&skyLightBuff, cscene->skyLight,
			sizeof(slg::ocl::SkyLight), "SkyLight");
	else
		FreeOCLBuffer(&skyLightBuff);
}

void PathOCLRenderThread::InitLightsDistribution() {
	CompiledScene *cscene = renderEngine->compiledScene;

	AllocOCLBufferRO(&lightsDistributionBuff, cscene->lightsDistribution,
		cscene->lightsDistributionSize, "LightsDistribution");
}

void PathOCLRenderThread::InitImageMaps() {
	CompiledScene *cscene = renderEngine->compiledScene;

	if (cscene->imageMapDescs.size() > 0) {
		AllocOCLBufferRO(&imageMapDescsBuff, &cscene->imageMapDescs[0],
				sizeof(slg::ocl::ImageMap) * cscene->imageMapDescs.size(), "ImageMaps description");

		// Free unused pages
		for (u_int i = cscene->imageMapMemBlocks.size(); i < imageMapsBuff.size(); ++i)
			FreeOCLBuffer(&imageMapsBuff[i]);
		imageMapsBuff.resize(cscene->imageMapMemBlocks.size(), NULL);

		for (u_int i = 0; i < imageMapsBuff.size(); ++i) {
			AllocOCLBufferRO(&(imageMapsBuff[i]), &(cscene->imageMapMemBlocks[i][0]),
					sizeof(float) * cscene->imageMapMemBlocks[i].size(), "ImageMaps");
		}
	} else {
		FreeOCLBuffer(&imageMapDescsBuff);
		for (u_int i = 0; i < imageMapsBuff.size(); ++i)
			FreeOCLBuffer(&imageMapsBuff[i]);
		imageMapsBuff.resize(0);
	}
}

void PathOCLRenderThread::CompileKernel(cl::Program *program, cl::Kernel **kernel,
		size_t *workgroupSize, const std::string &name) {
	delete *kernel;
	SLG_LOG("[PathOCLRenderThread::" << threadIndex << "] Compiling " << name << " Kernel");
	*kernel = new cl::Kernel(*program, name.c_str());
	
	if (intersectionDevice->GetDeviceDesc()->GetForceWorkGroupSize() > 0)
		*workgroupSize = intersectionDevice->GetDeviceDesc()->GetForceWorkGroupSize();
	else {
		cl::Device &oclDevice = intersectionDevice->GetOpenCLDevice();
		(*kernel)->getWorkGroupInfo<size_t>(oclDevice, CL_KERNEL_WORK_GROUP_SIZE, workgroupSize);
		SLG_LOG("[PathOCLRenderThread::" << threadIndex << "] " << name << " workgroup size: " << *workgroupSize);
	}
}

void PathOCLRenderThread::InitKernels() {
	//--------------------------------------------------------------------------
	// Allocate GPU task buffers
	//--------------------------------------------------------------------------

	InitGPUTaskBuffer();

	//--------------------------------------------------------------------------
	// Allocate sample buffers
	//--------------------------------------------------------------------------

	InitSamplesBuffer();

	//--------------------------------------------------------------------------
	// Allocate sample data buffers
	//--------------------------------------------------------------------------

	InitSampleDataBuffer();

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
			" -D LUXRAYS_OPENCL_KERNEL" <<
			" -D SLG_OPENCL_KERNEL" <<
			" -D PARAM_TASK_COUNT=" << renderEngine->taskCount <<
			" -D PARAM_RAY_EPSILON_MIN=" << MachineEpsilon::GetMin() << "f"
			" -D PARAM_RAY_EPSILON_MAX=" << MachineEpsilon::GetMax() << "f"
			" -D PARAM_MAX_PATH_DEPTH=" << renderEngine->maxPathDepth <<
			" -D PARAM_RR_DEPTH=" << renderEngine->rrDepth <<
			" -D PARAM_RR_CAP=" << renderEngine->rrImportanceCap << "f"
			" -D PARAM_LIGHT_WORLD_RADIUS_SCALE=" << slg::LIGHT_WORLD_RADIUS_SCALE << "f"
			;

	switch (intersectionDevice->GetAccelerator()->GetType()) {
		case ACCEL_BVH:
			ss << " -D PARAM_ACCEL_BVH";
			break;
		case ACCEL_QBVH:
			ss << " -D PARAM_ACCEL_QBVH";
			break;
		case ACCEL_MQBVH:
			ss << " -D PARAM_ACCEL_MQBVH";
			break;
		case ACCEL_MBVH:
			ss << " -D PARAM_ACCEL_MBVH";
			break;
		default:
			throw new std::runtime_error("Unknown accelerator in PathOCLRenderThread::InitKernels()");
	}

	// Film related parameters
	for (u_int i = 0; i < channel_RADIANCE_PER_PIXEL_NORMALIZEDs_Buff.size(); ++i)
		ss << " -D PARAM_FILM_RADIANCE_GROUP_" << i;
	ss << " -D PARAM_FILM_RADIANCE_GROUP_COUNT=" << channel_RADIANCE_PER_PIXEL_NORMALIZEDs_Buff.size();
	if (film->HasChannel(Film::ALPHA))
		ss << " -D PARAM_FILM_CHANNELS_HAS_ALPHA";
	if (film->HasChannel(Film::DEPTH))
		ss << " -D PARAM_FILM_CHANNELS_HAS_DEPTH";
	if (film->HasChannel(Film::POSITION))
		ss << " -D PARAM_FILM_CHANNELS_HAS_POSITION";
	if (film->HasChannel(Film::GEOMETRY_NORMAL))
		ss << " -D PARAM_FILM_CHANNELS_HAS_GEOMETRY_NORMAL";
	if (film->HasChannel(Film::SHADING_NORMAL))
		ss << " -D PARAM_FILM_CHANNELS_HAS_SHADING_NORMAL";
	if (film->HasChannel(Film::MATERIAL_ID))
		ss << " -D PARAM_FILM_CHANNELS_HAS_MATERIAL_ID";
	if (film->HasChannel(Film::DIRECT_DIFFUSE))
		ss << " -D PARAM_FILM_CHANNELS_HAS_DIRECT_DIFFUSE";
	if (film->HasChannel(Film::DIRECT_GLOSSY))
		ss << " -D PARAM_FILM_CHANNELS_HAS_DIRECT_GLOSSY";
	if (film->HasChannel(Film::EMISSION))
		ss << " -D PARAM_FILM_CHANNELS_HAS_EMISSION";
	if (film->HasChannel(Film::INDIRECT_DIFFUSE))
		ss << " -D PARAM_FILM_CHANNELS_HAS_INDIRECT_DIFFUSE";
	if (film->HasChannel(Film::INDIRECT_GLOSSY))
		ss << " -D PARAM_FILM_CHANNELS_HAS_INDIRECT_GLOSSY";
	if (film->HasChannel(Film::INDIRECT_SPECULAR))
		ss << " -D PARAM_FILM_CHANNELS_HAS_INDIRECT_SPECULAR";
	if (film->HasChannel(Film::MATERIAL_ID_MASK)) {
		ss << " -D PARAM_FILM_CHANNELS_HAS_MATERIAL_ID_MASK" <<
				" -D PARAM_FILM_MASK_MATERIAL_ID=" << renderEngine->film->GetMaskMaterialID(0);
	}
	if (film->HasChannel(Film::DIRECT_SHADOW_MASK))
		ss << " -D PARAM_FILM_CHANNELS_HAS_DIRECT_SHADOW_MASK";
	if (film->HasChannel(Film::INDIRECT_SHADOW_MASK))
		ss << " -D PARAM_FILM_CHANNELS_HAS_INDIRECT_SHADOW_MASK";
	if (film->HasChannel(Film::UV))
		ss << " -D PARAM_FILM_CHANNELS_HAS_UV";

	if (normalsBuff)
		ss << " -D PARAM_HAS_NORMALS_BUFFER";
	if (uvsBuff)
		ss << " -D PARAM_HAS_UVS_BUFFER";
	if (colsBuff)
		ss << " -D PARAM_HAS_COLS_BUFFER";
	if (alphasBuff)
		ss << " -D PARAM_HAS_ALPHAS_BUFFER";

	if (cscene->IsTextureCompiled(CONST_FLOAT))
		ss << " -D PARAM_ENABLE_TEX_CONST_FLOAT";
	if (cscene->IsTextureCompiled(CONST_FLOAT3))
		ss << " -D PARAM_ENABLE_TEX_CONST_FLOAT3";
	if (cscene->IsTextureCompiled(IMAGEMAP))
		ss << " -D PARAM_ENABLE_TEX_IMAGEMAP";
	if (cscene->IsTextureCompiled(SCALE_TEX))
		ss << " -D PARAM_ENABLE_TEX_SCALE";
	if (cscene->IsTextureCompiled(FRESNEL_APPROX_N))
		ss << " -D PARAM_ENABLE_FRESNEL_APPROX_N";
	if (cscene->IsTextureCompiled(FRESNEL_APPROX_K))
		ss << " -D PARAM_ENABLE_FRESNEL_APPROX_K";
	if (cscene->IsTextureCompiled(CHECKERBOARD2D))
		ss << " -D PARAM_ENABLE_CHECKERBOARD2D";
	if (cscene->IsTextureCompiled(CHECKERBOARD3D))
		ss << " -D PARAM_ENABLE_CHECKERBOARD3D";
	if (cscene->IsTextureCompiled(MIX_TEX))
		ss << " -D PARAM_ENABLE_TEX_MIX";
	if (cscene->IsTextureCompiled(FBM_TEX))
		ss << " -D PARAM_ENABLE_FBM_TEX";
	if (cscene->IsTextureCompiled(MARBLE))
		ss << " -D PARAM_ENABLE_MARBLE";
	if (cscene->IsTextureCompiled(DOTS))
		ss << " -D PARAM_ENABLE_DOTS";
	if (cscene->IsTextureCompiled(BRICK))
		ss << " -D PARAM_ENABLE_BRICK";
	if (cscene->IsTextureCompiled(ADD_TEX))
		ss << " -D PARAM_ENABLE_TEX_ADD";
	if (cscene->IsTextureCompiled(WINDY))
		ss << " -D PARAM_ENABLE_WINDY";
	if (cscene->IsTextureCompiled(WRINKLED))
		ss << " -D PARAM_ENABLE_WRINKLED";
	if (cscene->IsTextureCompiled(UV_TEX))
		ss << " -D PARAM_ENABLE_TEX_UV";
	if (cscene->IsTextureCompiled(BAND_TEX))
		ss << " -D PARAM_ENABLE_TEX_BAND";
	if (cscene->IsTextureCompiled(HITPOINTCOLOR))
		ss << " -D PARAM_ENABLE_TEX_HITPOINTCOLOR";
	if (cscene->IsTextureCompiled(HITPOINTALPHA))
		ss << " -D PARAM_ENABLE_TEX_HITPOINTALPHA";
	if (cscene->IsTextureCompiled(HITPOINTGREY))
		ss << " -D PARAM_ENABLE_TEX_HITPOINTGREY";

	if (cscene->IsMaterialCompiled(MATTE))
		ss << " -D PARAM_ENABLE_MAT_MATTE";
	if (cscene->IsMaterialCompiled(MIRROR))
		ss << " -D PARAM_ENABLE_MAT_MIRROR";
	if (cscene->IsMaterialCompiled(GLASS))
		ss << " -D PARAM_ENABLE_MAT_GLASS";
	if (cscene->IsMaterialCompiled(METAL))
		ss << " -D PARAM_ENABLE_MAT_METAL";
	if (cscene->IsMaterialCompiled(ARCHGLASS))
		ss << " -D PARAM_ENABLE_MAT_ARCHGLASS";
	if (cscene->IsMaterialCompiled(MIX))
		ss << " -D PARAM_ENABLE_MAT_MIX";
	if (cscene->IsMaterialCompiled(NULLMAT))
		ss << " -D PARAM_ENABLE_MAT_NULL";
	if (cscene->IsMaterialCompiled(MATTETRANSLUCENT))
		ss << " -D PARAM_ENABLE_MAT_MATTETRANSLUCENT";
	if (cscene->IsMaterialCompiled(GLOSSY2)) {
		ss << " -D PARAM_ENABLE_MAT_GLOSSY2";

		if (cscene->IsMaterialCompiled(GLOSSY2_ANISOTROPIC))
			ss << " -D PARAM_ENABLE_MAT_GLOSSY2_ANISOTROPIC";
		if (cscene->IsMaterialCompiled(GLOSSY2_ABSORPTION))
			ss << " -D PARAM_ENABLE_MAT_GLOSSY2_ABSORPTION";
		if (cscene->IsMaterialCompiled(GLOSSY2_INDEX))
			ss << " -D PARAM_ENABLE_MAT_GLOSSY2_INDEX";
		if (cscene->IsMaterialCompiled(GLOSSY2_MULTIBOUNCE))
			ss << " -D PARAM_ENABLE_MAT_GLOSSY2_MULTIBOUNCE";
	}
	if (cscene->IsMaterialCompiled(METAL2)) {
		ss << " -D PARAM_ENABLE_MAT_METAL2";
		if (cscene->IsMaterialCompiled(METAL2_ANISOTROPIC))
			ss << " -D PARAM_ENABLE_MAT_METAL2_ANISOTROPIC";
	}
	if (cscene->IsMaterialCompiled(ROUGHGLASS)) {
		ss << " -D PARAM_ENABLE_MAT_ROUGHGLASS";
		if (cscene->IsMaterialCompiled(ROUGHGLASS_ANISOTROPIC))
			ss << " -D PARAM_ENABLE_MAT_ROUGHGLASS_ANISOTROPIC";
	}

	if (cscene->RequiresPassThrough())
		ss << " -D PARAM_HAS_PASSTHROUGH";
	
	if (cscene->camera.lensRadius > 0.f)
		ss << " -D PARAM_CAMERA_HAS_DOF";
	if (cscene->enableHorizStereo) {
		ss << " -D PARAM_CAMERA_ENABLE_HORIZ_STEREO";

		if (cscene->enableOculusRiftBarrel)
			ss << " -D PARAM_CAMERA_ENABLE_OCULUSRIFT_BARREL";
	}

	if (infiniteLightBuff)
		ss << " -D PARAM_HAS_INFINITELIGHT";

	if (skyLightBuff)
		ss << " -D PARAM_HAS_SKYLIGHT";

	if (sunLightBuff)
		ss << " -D PARAM_HAS_SUNLIGHT";

	if (triLightDefsBuff)
		ss << " -D PARAM_DL_LIGHT_COUNT=" << renderEngine->compiledScene->triLightDefs.size();
	else
		ss << " -D PARAM_DL_LIGHT_COUNT=0";

	if (imageMapDescsBuff) {
		ss << " -D PARAM_HAS_IMAGEMAPS";
		if (imageMapsBuff.size() > 8)
			throw std::runtime_error("Too many memory pages required for image maps");
		for (u_int i = 0; i < imageMapsBuff.size(); ++i)
			ss << " -D PARAM_IMAGEMAPS_PAGE_" << i;
		ss << " -D PARAM_IMAGEMAPS_COUNT=" << imageMapsBuff.size();
	}

	if (renderEngine->compiledScene->useBumpMapping)
		ss << " -D PARAM_HAS_BUMPMAPS";
	if (renderEngine->compiledScene->useNormalMapping)
		ss << " -D PARAM_HAS_NORMALMAPS";

	const slg::ocl::Filter *filter = renderEngine->filter;
	switch (filter->type) {
		case slg::ocl::FILTER_NONE:
			ss << " -D PARAM_IMAGE_FILTER_TYPE=0";
			break;
		case slg::ocl::FILTER_BOX:
			ss << " -D PARAM_IMAGE_FILTER_TYPE=1" <<
					" -D PARAM_IMAGE_FILTER_WIDTH_X=" << filter->box.widthX << "f" <<
					" -D PARAM_IMAGE_FILTER_WIDTH_Y=" << filter->box.widthY << "f";
			break;
		case slg::ocl::FILTER_GAUSSIAN:
			ss << " -D PARAM_IMAGE_FILTER_TYPE=2" <<
					" -D PARAM_IMAGE_FILTER_WIDTH_X=" << filter->gaussian.widthX << "f" <<
					" -D PARAM_IMAGE_FILTER_WIDTH_Y=" << filter->gaussian.widthY << "f" <<
					" -D PARAM_IMAGE_FILTER_GAUSSIAN_ALPHA=" << filter->gaussian.alpha << "f";
			break;
		case slg::ocl::FILTER_MITCHELL:
			ss << " -D PARAM_IMAGE_FILTER_TYPE=3" <<
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

	const slg::ocl::Sampler *sampler = renderEngine->sampler;
	switch (sampler->type) {
		case slg::ocl::RANDOM:
			ss << " -D PARAM_SAMPLER_TYPE=0";
			break;
		case slg::ocl::METROPOLIS:
			ss << " -D PARAM_SAMPLER_TYPE=1" <<
					" -D PARAM_SAMPLER_METROPOLIS_LARGE_STEP_RATE=" << sampler->metropolis.largeMutationProbability << "f" <<
					" -D PARAM_SAMPLER_METROPOLIS_IMAGE_MUTATION_RANGE=" << sampler->metropolis.imageMutationRange << "f" <<
					" -D PARAM_SAMPLER_METROPOLIS_MAX_CONSECUTIVE_REJECT=" << sampler->metropolis.maxRejects;
			break;
		case slg::ocl::SOBOL:
			ss << " -D PARAM_SAMPLER_TYPE=2" <<
					" -D PARAM_SAMPLER_SOBOL_STARTOFFSET=" << SOBOL_STARTOFFSET <<
					" -D PARAM_SAMPLER_SOBOL_MAXDEPTH=" << max(SOBOL_MAXDEPTH, renderEngine->maxPathDepth);
			break;
		default:
			assert (false);
	}

	if (renderEngine->film->HasChannel(Film::ALPHA))
		ss << " -D PARAM_ENABLE_ALPHA_CHANNEL";

	// Some information about our place in the universe...
	ss << " -D PARAM_DEVICE_INDEX=" << threadIndex;
	ss << " -D PARAM_DEVICE_COUNT=" << renderEngine->intersectionDevices.size();

	//--------------------------------------------------------------------------
	// PARAMs used only by RTPATHOCL
	//--------------------------------------------------------------------------

	// Tonemapping is done on device only by RTPATHOCL
	if (renderEngine->film->GetToneMapParams()->GetType() == TONEMAP_LINEAR) {
		const LinearToneMapParams *ltm = (const LinearToneMapParams *)renderEngine->film->GetToneMapParams();
		ss << " -D PARAM_TONEMAP_LINEAR_SCALE=" << ltm->scale << "f";
	} else
		ss << " -D PARAM_TONEMAP_LINEAR_SCALE=1.f";

	ss << " -D PARAM_GAMMA=" << renderEngine->film->GetGamma() << "f";
	
	//--------------------------------------------------------------------------

	// Check the OpenCL vendor and use some specific compiler options
#if defined(__APPLE__)
	ss << " -D __APPLE_CL__";
#endif
	//--------------------------------------------------------------------------

	const double tStart = WallClockTime();

	// Check if I have to recompile the kernels
	string newKernelParameters = ss.str();
	if (kernelsParameters != newKernelParameters) {
		kernelsParameters = newKernelParameters;

		string sobolLookupTable;
		if (sampler->type == slg::ocl::SOBOL) {
			// Generate the Sobol vectors
			SLG_LOG("[PathOCLRenderThread::" << threadIndex << "] Sobol table size: " << sampleDimensions * SOBOL_BITS);
			u_int *directions = new u_int[sampleDimensions * SOBOL_BITS];

			SobolGenerateDirectionVectors(directions, sampleDimensions);

			stringstream sobolTableSS;
			sobolTableSS << "#line 2 \"Sobol Table in pathoclthread.cpp\"\n";
			sobolTableSS << "__constant uint SOBOL_DIRECTIONS[" << sampleDimensions * SOBOL_BITS << "] = {\n";
			for (u_int i = 0; i < sampleDimensions * SOBOL_BITS; ++i) {
				if (i != 0)
					sobolTableSS << ", ";
				sobolTableSS << directions[i] << "u";
			}
			sobolTableSS << "};\n";

			sobolLookupTable = sobolTableSS.str();

			delete directions;
		}

		// Compile sources
		stringstream ssKernel;
		ssKernel <<
			// OpenCL LuxRays Types
			luxrays::ocl::KernelSource_luxrays_types <<
			luxrays::ocl::KernelSource_uv_types <<
			luxrays::ocl::KernelSource_point_types <<
			luxrays::ocl::KernelSource_vector_types <<
			luxrays::ocl::KernelSource_normal_types <<
			luxrays::ocl::KernelSource_triangle_types <<
			luxrays::ocl::KernelSource_ray_types <<
			luxrays::ocl::KernelSource_bbox_types <<
			luxrays::ocl::KernelSource_epsilon_types <<
			luxrays::ocl::KernelSource_spectrum_types <<
			luxrays::ocl::KernelSource_frame_types <<
			luxrays::ocl::KernelSource_matrix4x4_types <<
			luxrays::ocl::KernelSource_transform_types <<
			// OpenCL LuxRays Funcs
			luxrays::ocl::KernelSource_epsilon_funcs <<
			luxrays::ocl::KernelSource_utils_funcs <<
			luxrays::ocl::KernelSource_vector_funcs <<
			luxrays::ocl::KernelSource_ray_funcs <<
			luxrays::ocl::KernelSource_bbox_funcs <<
			luxrays::ocl::KernelSource_spectrum_funcs <<
			luxrays::ocl::KernelSource_frame_funcs <<
			luxrays::ocl::KernelSource_matrix4x4_funcs <<
			luxrays::ocl::KernelSource_transform_funcs <<
			// OpenCL SLG Types
			slg::ocl::KernelSource_randomgen_types <<
			slg::ocl::KernelSource_trianglemesh_types <<
			slg::ocl::KernelSource_hitpoint_types <<
			slg::ocl::KernelSource_mapping_types <<
			slg::ocl::KernelSource_texture_types <<
			slg::ocl::KernelSource_material_types <<
			slg::ocl::KernelSource_bsdf_types <<
			slg::ocl::KernelSource_film_types <<
			slg::ocl::KernelSource_filter_types <<
			slg::ocl::KernelSource_sampler_types <<
			slg::ocl::KernelSource_camera_types <<
			slg::ocl::KernelSource_light_types <<
			// OpenCL SLG Types
			slg::ocl::KernelSource_mc_funcs <<
			slg::ocl::KernelSource_randomgen_funcs <<
			slg::ocl::KernelSource_triangle_funcs <<
			slg::ocl::KernelSource_trianglemesh_funcs <<
			slg::ocl::KernelSource_mapping_funcs <<
			slg::ocl::KernelSource_texture_funcs <<
			slg::ocl::KernelSource_materialdefs_funcs <<
			slg::ocl::KernelSource_material_funcs <<
			slg::ocl::KernelSource_camera_funcs <<
			slg::ocl::KernelSource_light_funcs <<
			slg::ocl::KernelSource_filter_funcs <<
			slg::ocl::KernelSource_film_funcs <<
			sobolLookupTable <<
			slg::ocl::KernelSource_sampler_funcs <<
			slg::ocl::KernelSource_bsdf_funcs <<
			slg::ocl::KernelSource_scene_funcs <<
			// SLG Kernels
			slg::ocl::KernelSource_datatypes <<
			slg::ocl::KernelSource_pathocl_kernels <<
			slg::ocl::KernelSource_rtpathocl_kernels;
		string kernelSource = ssKernel.str();

		SLG_LOG("[PathOCLRenderThread::" << threadIndex << "] Defined symbols: " << kernelsParameters);
		SLG_LOG("[PathOCLRenderThread::" << threadIndex << "] Compiling kernels ");

		// Some debug code to write the OpenCL kernel source to a file
		/*const std::string kernelFileName = "kernel_source_device_" + ToString(threadIndex) + ".txt";
		std::ofstream kernelFile(kernelFileName.c_str());
		string kernelDefs = kernelsParameters;
		boost::replace_all(kernelDefs, "-D", "\n#define");
		boost::replace_all(kernelDefs, "=", " ");
		kernelFile << kernelDefs << std::endl << std::endl << kernelSource << std::endl;
		kernelFile.close();*/

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

		CompileKernel(program, &initKernel, &initWorkGroupSize, "Init");

		//--------------------------------------------------------------------------
		// InitFB kernel
		//--------------------------------------------------------------------------

		CompileKernel(program, &initFilmKernel, &initFilmWorkGroupSize, "InitFilm");

		//----------------------------------------------------------------------
		// AdvancePaths kernel
		//----------------------------------------------------------------------

		CompileKernel(program, &advancePathsKernel, &advancePathsWorkGroupSize, "AdvancePaths");

		//----------------------------------------------------------------------
		// The following kernels are used only by RTPathOCL
		//----------------------------------------------------------------------

		if (dynamic_cast<RTPathOCLRenderEngine *>(renderEngine)) {
			//------------------------------------------------------------------
			// ClearFrameBuffer kernel
			//------------------------------------------------------------------

			CompileKernel(program, &clearFBKernel, &clearFBWorkGroupSize, "ClearFrameBuffer");

			//------------------------------------------------------------------
			// InitRTFrameBuffer kernel
			//------------------------------------------------------------------

			CompileKernel(program, &clearSBKernel, &clearSBWorkGroupSize, "ClearScreenBuffer");

			//------------------------------------------------------------------
			// MergeFrameBuffer kernel
			//------------------------------------------------------------------

			CompileKernel(program, &mergeFBKernel, &mergeFBWorkGroupSize, "MergeFrameBuffer");

			//------------------------------------------------------------------
			// NormalizeFrameBuffer kernel
			//------------------------------------------------------------------

			CompileKernel(program, &normalizeFBKernel, &normalizeFBWorkGroupSize, "NormalizeFrameBuffer");

			//------------------------------------------------------------------
			// Gaussian blur frame buffer filter kernel
			//------------------------------------------------------------------

			CompileKernel(program, &applyBlurFilterXR1Kernel, &applyBlurFilterXR1WorkGroupSize, "ApplyGaussianBlurFilterXR1");
			CompileKernel(program, &applyBlurFilterYR1Kernel, &applyBlurFilterYR1WorkGroupSize, "ApplyGaussianBlurFilterYR1");

			//------------------------------------------------------------------
			// ToneMapLinear kernel
			//------------------------------------------------------------------

			CompileKernel(program, &toneMapLinearKernel, &toneMapLinearWorkGroupSize, "ToneMapLinear");

			//------------------------------------------------------------------
			// UpdateScreenBuffer kernel
			//------------------------------------------------------------------

			CompileKernel(program, &updateScreenBufferKernel, &updateScreenBufferWorkGroupSize, "UpdateScreenBuffer");
		}

		//----------------------------------------------------------------------

		const double tEnd = WallClockTime();
		SLG_LOG("[PathOCLRenderThread::" << threadIndex << "] Kernels compilation time: " << int((tEnd - tStart) * 1000.0) << "ms");

		delete program;
	} else
		SLG_LOG("[PathOCLRenderThread::" << threadIndex << "] Using cached kernels");
}

void PathOCLRenderThread::InitGPUTaskBuffer() {
	const u_int taskCount = renderEngine->taskCount;
	const u_int triAreaLightCount = renderEngine->compiledScene->triLightDefs.size();
	const bool hasPassThrough = renderEngine->compiledScene->RequiresPassThrough();

	// Add Seed memory size
	size_t gpuTaksSize = sizeof(slg::ocl::Seed);

	// Add PathStateBase memory size
	gpuTaksSize += sizeof(int) + sizeof(u_int) + sizeof(Spectrum);

	// Add PathStateBase.BSDF.HitPoint memory size
	size_t hitPointSize = sizeof(Vector) + sizeof(Point) + sizeof(UV) + 2 * sizeof(Normal);
	if (renderEngine->compiledScene->IsTextureCompiled(HITPOINTCOLOR) ||
			renderEngine->compiledScene->IsTextureCompiled(HITPOINTGREY))
		hitPointSize += sizeof(Spectrum);
	if (renderEngine->compiledScene->IsTextureCompiled(HITPOINTALPHA))
		hitPointSize += sizeof(float);
	if (hasPassThrough)
		hitPointSize += sizeof(float);

	// Add PathStateBase.BSDF memory size
	size_t bsdfSize = hitPointSize;
	// Add PathStateBase.BSDF.materialIndex memory size
	bsdfSize += sizeof(u_int);
	// Add PathStateBase.BSDF.triangleLightSourceIndex memory size
	if (triAreaLightCount > 0)
		bsdfSize += sizeof(u_int);
	// Add PathStateBase.BSDF.Frame memory size
	bsdfSize += sizeof(slg::ocl::Frame);
	gpuTaksSize += bsdfSize;

	// Add PathStateDirectLight memory size
	gpuTaksSize += sizeof(Spectrum) + sizeof(u_int) + 2 * sizeof(BSDFEvent) + sizeof(float);
	// Add PathStateDirectLight.tmpHitPoint memory size
	if (triAreaLightCount > 0)
		gpuTaksSize += hitPointSize;

	// Add PathStateDirectLightPassThrough memory size
	if (hasPassThrough)
		gpuTaksSize += sizeof(float) + bsdfSize;

	SLG_LOG("[PathOCLRenderThread::" << threadIndex << "] Size of a GPUTask: " << gpuTaksSize << "bytes");
	AllocOCLBufferRW(&tasksBuff, gpuTaksSize * taskCount, "GPUTask");
}

void PathOCLRenderThread::InitSamplesBuffer() {
	const u_int taskCount = renderEngine->taskCount;

	//--------------------------------------------------------------------------
	// SampleResult size
	//--------------------------------------------------------------------------

	// SampleResult.filmX and SampleResult.filmY
	size_t sampleResultSize = 2 * sizeof(float);
	// SampleResult.radiancePerPixelNormalized[PARAM_FILM_RADIANCE_GROUP_COUNT]
	sampleResultSize += sizeof(slg::ocl::Spectrum) * renderEngine->film->GetRadianceGroupCount();
	if (film->HasChannel(Film::ALPHA))
		sampleResultSize += sizeof(float);
	if (film->HasChannel(Film::DEPTH))
		sampleResultSize += sizeof(float);
	if (film->HasChannel(Film::POSITION))
		sampleResultSize += sizeof(Point);
	if (film->HasChannel(Film::GEOMETRY_NORMAL))
		sampleResultSize += sizeof(Normal);
	if (film->HasChannel(Film::SHADING_NORMAL))
		sampleResultSize += sizeof(Normal);
	if (film->HasChannel(Film::MATERIAL_ID))
		sampleResultSize += sizeof(u_int);
	if (film->HasChannel(Film::DIRECT_DIFFUSE))
		sampleResultSize += sizeof(Spectrum);
	if (film->HasChannel(Film::DIRECT_GLOSSY))
		sampleResultSize += sizeof(Spectrum);
	if (film->HasChannel(Film::EMISSION))
		sampleResultSize += sizeof(Spectrum);
	if (film->HasChannel(Film::INDIRECT_DIFFUSE))
		sampleResultSize += sizeof(Spectrum);
	if (film->HasChannel(Film::INDIRECT_GLOSSY))
		sampleResultSize += sizeof(Spectrum);
	if (film->HasChannel(Film::INDIRECT_SPECULAR))
		sampleResultSize += sizeof(Spectrum);
	if (film->HasChannel(Film::MATERIAL_ID_MASK))
		sampleResultSize += sizeof(float);
	if (film->HasChannel(Film::DIRECT_SHADOW_MASK))
		sampleResultSize += sizeof(float);
	if (film->HasChannel(Film::INDIRECT_SHADOW_MASK))
		sampleResultSize += sizeof(float);
	if (film->HasChannel(Film::UV))
		sampleResultSize += sizeof(UV);
	
	//--------------------------------------------------------------------------
	// Sample size
	//--------------------------------------------------------------------------
	size_t sampleSize = sampleResultSize;

	// Add Sample memory size
	if (renderEngine->sampler->type == slg::ocl::RANDOM) {
		// Nothing to add
	} else if (renderEngine->sampler->type == slg::ocl::METROPOLIS) {
		sampleSize += 2 * sizeof(float) + 5 * sizeof(u_int) + sampleResultSize;		
	} else if (renderEngine->sampler->type == slg::ocl::SOBOL) {
		sampleSize += 2 * sizeof(float) + 2 * sizeof(u_int);
	} else
		throw std::runtime_error("Unknown sampler.type: " + boost::lexical_cast<std::string>(renderEngine->sampler->type));

	SLG_LOG("[PathOCLRenderThread::" << threadIndex << "] Size of a Sample: " << sampleSize << "bytes");
	AllocOCLBufferRW(&samplesBuff, sampleSize * taskCount, "Sample");
}

void PathOCLRenderThread::InitSampleDataBuffer() {
	Scene *scene = renderEngine->renderConfig->scene;
	const u_int taskCount = renderEngine->taskCount;
	const bool hasPassThrough = renderEngine->compiledScene->RequiresPassThrough();

	const size_t eyePathVertexDimension =
		// IDX_SCREEN_X, IDX_SCREEN_Y
		2 +
		// IDX_EYE_PASSTROUGHT
		(hasPassThrough ? 1 : 0) +
		// IDX_DOF_X, IDX_DOF_Y
		((scene->camera->lensRadius > 0.f) ? 2 : 0);
	const size_t PerPathVertexDimension =
		// IDX_PASSTHROUGH,
		(hasPassThrough ? 1 : 0) +
		// IDX_BSDF_X, IDX_BSDF_Y
		2 +
		// IDX_DIRECTLIGHT_X, IDX_DIRECTLIGHT_Y, IDX_DIRECTLIGHT_Z, IDX_DIRECTLIGHT_W, IDX_DIRECTLIGHT_A
		4 + (hasPassThrough ? 1 : 0) +
		// IDX_RR
		1;
	sampleDimensions = eyePathVertexDimension + PerPathVertexDimension * renderEngine->maxPathDepth;

	size_t uDataSize;
	if ((renderEngine->sampler->type == slg::ocl::RANDOM) ||
			(renderEngine->sampler->type == slg::ocl::SOBOL)) {
		// Only IDX_SCREEN_X, IDX_SCREEN_Y
		uDataSize = sizeof(float) * 2;
		
		if (renderEngine->sampler->type == slg::ocl::SOBOL) {
			// Limit the number of dimension where I use Sobol sequence (after, I switch
			// to Random sampler.
			sampleDimensions = eyePathVertexDimension + PerPathVertexDimension * max(SOBOL_MAXDEPTH, renderEngine->maxPathDepth);
		}
	} else if (renderEngine->sampler->type == slg::ocl::METROPOLIS) {
		// Metropolis needs 2 sets of samples, the current and the proposed mutation
		uDataSize = 2 * sizeof(float) * sampleDimensions;
	} else
		throw std::runtime_error("Unknown sampler.type: " + boost::lexical_cast<std::string>(renderEngine->sampler->type));

	SLG_LOG("[PathOCLRenderThread::" << threadIndex << "] Sample dimensions: " << sampleDimensions);
	SLG_LOG("[PathOCLRenderThread::" << threadIndex << "] Size of a SampleData: " << uDataSize << "bytes");

	// TOFIX
	AllocOCLBufferRW(&sampleDataBuff, uDataSize * taskCount + 1, "SampleData"); // +1 to avoid METROPOLIS + Intel\AMD OpenCL crash
}

void PathOCLRenderThread::InitRender() {
	const u_int taskCount = renderEngine->taskCount;

	// In case renderEngine->taskCount has changed
	delete gpuTaskStats;
	gpuTaskStats = new slg::ocl::GPUTaskStats[renderEngine->taskCount];

	//--------------------------------------------------------------------------
	// Film definition
	//--------------------------------------------------------------------------

	InitFilm();

	//--------------------------------------------------------------------------
	// Camera definition
	//--------------------------------------------------------------------------

	InitCamera();

	//--------------------------------------------------------------------------
	// Scene geometry
	//--------------------------------------------------------------------------

	InitGeometry();

	//--------------------------------------------------------------------------
	// Image maps
	//--------------------------------------------------------------------------

	InitImageMaps();

	//--------------------------------------------------------------------------
	// Texture definitions
	//--------------------------------------------------------------------------

	InitTextures();

	//--------------------------------------------------------------------------
	// Material definitions
	//--------------------------------------------------------------------------

	InitMaterials();

	//--------------------------------------------------------------------------
	// Translate triangle area lights
	//--------------------------------------------------------------------------

	InitTriangleAreaLights();

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
	
	InitLightsDistribution();

	const u_int triAreaLightCount = renderEngine->compiledScene->triLightDefs.size();
	if (!skyLightBuff && !sunLightBuff && !infiniteLightBuff && (triAreaLightCount == 0))
		throw runtime_error("There are no light sources supported by PathOCL in the scene");

	//--------------------------------------------------------------------------
	// Allocate Ray/RayHit buffers
	//--------------------------------------------------------------------------

	AllocOCLBufferRW(&raysBuff, sizeof(Ray) * taskCount, "Ray");
	AllocOCLBufferRW(&hitsBuff, sizeof(RayHit) * taskCount, "RayHit");

	//--------------------------------------------------------------------------
	// Allocate GPU task statistic buffers
	//--------------------------------------------------------------------------

	AllocOCLBufferRW(&taskStatsBuff, sizeof(slg::ocl::GPUTaskStats) * taskCount, "GPUTask Stats");

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
	const u_int filmPixelCount = renderEngine->film->GetWidth() * renderEngine->film->GetHeight();
	oclQueue.enqueueNDRangeKernel(*initFilmKernel, cl::NullRange,
			cl::NDRange(RoundUp<u_int>(filmPixelCount, initFilmWorkGroupSize)),
			cl::NDRange(initFilmWorkGroupSize));

	// Initialize the tasks buffer
	oclQueue.enqueueNDRangeKernel(*initKernel, cl::NullRange,
			cl::NDRange(RoundUp<u_int>(taskCount, initWorkGroupSize)),
			cl::NDRange(initWorkGroupSize));
	oclQueue.finish();

	// Reset statistics in order to be more accurate
	intersectionDevice->ResetPerformaceStats();
}

void PathOCLRenderThread::SetKernelArgs() {
	// Set OpenCL kernel arguments

	// OpenCL kernel setArg() is the only no thread safe function in OpenCL 1.1 so
	// I need to use a mutex here
	boost::unique_lock<boost::mutex> lock(renderEngine->setKernelArgsMutex);

	//--------------------------------------------------------------------------
	// advancePathsKernel
	//--------------------------------------------------------------------------

	CompiledScene *cscene = renderEngine->compiledScene;

	u_int argIndex = 0;
	advancePathsKernel->setArg(argIndex++, *tasksBuff);
	advancePathsKernel->setArg(argIndex++, *taskStatsBuff);
	advancePathsKernel->setArg(argIndex++, *samplesBuff);
	advancePathsKernel->setArg(argIndex++, *sampleDataBuff);
	advancePathsKernel->setArg(argIndex++, *raysBuff);
	advancePathsKernel->setArg(argIndex++, *hitsBuff);

	// Film parameters
	advancePathsKernel->setArg(argIndex++, renderEngine->film->GetWidth());
	advancePathsKernel->setArg(argIndex++, renderEngine->film->GetHeight());
	for (u_int i = 0; i < channel_RADIANCE_PER_PIXEL_NORMALIZEDs_Buff.size(); ++i)
		advancePathsKernel->setArg(argIndex++, *(channel_RADIANCE_PER_PIXEL_NORMALIZEDs_Buff[i]));
	if (film->HasChannel(Film::ALPHA))
		advancePathsKernel->setArg(argIndex++, *channel_ALPHA_Buff);
	if (film->HasChannel(Film::DEPTH))
		advancePathsKernel->setArg(argIndex++, *channel_DEPTH_Buff);
	if (film->HasChannel(Film::POSITION))
		advancePathsKernel->setArg(argIndex++, *channel_POSITION_Buff);
	if (film->HasChannel(Film::GEOMETRY_NORMAL))
		advancePathsKernel->setArg(argIndex++, *channel_GEOMETRY_NORMAL_Buff);
	if (film->HasChannel(Film::SHADING_NORMAL))
		advancePathsKernel->setArg(argIndex++, *channel_SHADING_NORMAL_Buff);
	if (film->HasChannel(Film::MATERIAL_ID))
		advancePathsKernel->setArg(argIndex++, *channel_MATERIAL_ID_Buff);
	if (film->HasChannel(Film::DIRECT_DIFFUSE))
		advancePathsKernel->setArg(argIndex++, *channel_DIRECT_DIFFUSE_Buff);

	if (film->HasChannel(Film::DIRECT_GLOSSY))
		advancePathsKernel->setArg(argIndex++, *channel_DIRECT_GLOSSY_Buff);
	if (film->HasChannel(Film::EMISSION))
		advancePathsKernel->setArg(argIndex++, *channel_EMISSION_Buff);
	if (film->HasChannel(Film::INDIRECT_DIFFUSE))
		advancePathsKernel->setArg(argIndex++, *channel_INDIRECT_DIFFUSE_Buff);
	if (film->HasChannel(Film::INDIRECT_GLOSSY))
		advancePathsKernel->setArg(argIndex++, *channel_INDIRECT_GLOSSY_Buff);
	if (film->HasChannel(Film::INDIRECT_SPECULAR))
		advancePathsKernel->setArg(argIndex++, *channel_INDIRECT_SPECULAR_Buff);
	if (film->HasChannel(Film::MATERIAL_ID_MASK))
		advancePathsKernel->setArg(argIndex++, *channel_MATERIAL_ID_MASK_Buff);
	if (film->HasChannel(Film::DIRECT_SHADOW_MASK))
		advancePathsKernel->setArg(argIndex++, *channel_DIRECT_SHADOW_MASK_Buff);
	if (film->HasChannel(Film::INDIRECT_SHADOW_MASK))
		advancePathsKernel->setArg(argIndex++, *channel_INDIRECT_SHADOW_MASK_Buff);
	if (film->HasChannel(Film::UV))
		advancePathsKernel->setArg(argIndex++, *channel_UV_Buff);

	// Scene parameters
	advancePathsKernel->setArg(argIndex++, cscene->worldBSphere.center.x);
	advancePathsKernel->setArg(argIndex++, cscene->worldBSphere.center.y);
	advancePathsKernel->setArg(argIndex++, cscene->worldBSphere.center.z);
	advancePathsKernel->setArg(argIndex++, cscene->worldBSphere.rad);
	advancePathsKernel->setArg(argIndex++, *materialsBuff);
	advancePathsKernel->setArg(argIndex++, *texturesBuff);
	advancePathsKernel->setArg(argIndex++, *meshMatsBuff);
	advancePathsKernel->setArg(argIndex++, *meshDescsBuff);
	advancePathsKernel->setArg(argIndex++, *vertsBuff);
	if (normalsBuff)
		advancePathsKernel->setArg(argIndex++, *normalsBuff);
	if (uvsBuff)
		advancePathsKernel->setArg(argIndex++, *uvsBuff);
	if (colsBuff)
		advancePathsKernel->setArg(argIndex++, *colsBuff);
	if (alphasBuff)
		advancePathsKernel->setArg(argIndex++, *alphasBuff);
	advancePathsKernel->setArg(argIndex++, *trianglesBuff);
	advancePathsKernel->setArg(argIndex++, *cameraBuff);
	advancePathsKernel->setArg(argIndex++, *lightsDistributionBuff);
	if (infiniteLightBuff) {
		advancePathsKernel->setArg(argIndex++, *infiniteLightBuff);
		advancePathsKernel->setArg(argIndex++, *infiniteLightDistributionBuff);
	}
	if (sunLightBuff)
		advancePathsKernel->setArg(argIndex++, *sunLightBuff);
	if (skyLightBuff)
		advancePathsKernel->setArg(argIndex++, *skyLightBuff);
	if (triLightDefsBuff) {
		advancePathsKernel->setArg(argIndex++, *triLightDefsBuff);
		advancePathsKernel->setArg(argIndex++, *meshTriLightDefsOffsetBuff);
	}
	if (imageMapDescsBuff) {
		advancePathsKernel->setArg(argIndex++, *imageMapDescsBuff);

		for (u_int i = 0; i < imageMapsBuff.size(); ++i)
			advancePathsKernel->setArg(argIndex++, *(imageMapsBuff[i]));
	}

	//--------------------------------------------------------------------------
	// initFilmKernel
	//--------------------------------------------------------------------------

	argIndex = 0;
	initFilmKernel->setArg(argIndex++, renderEngine->film->GetWidth());
	initFilmKernel->setArg(argIndex++, renderEngine->film->GetHeight());
	for (u_int i = 0; i < channel_RADIANCE_PER_PIXEL_NORMALIZEDs_Buff.size(); ++i)
		initFilmKernel->setArg(argIndex++, *(channel_RADIANCE_PER_PIXEL_NORMALIZEDs_Buff[i]));

	if (film->HasChannel(Film::ALPHA))
		initFilmKernel->setArg(argIndex++, *channel_ALPHA_Buff);
	if (film->HasChannel(Film::DEPTH))
		initFilmKernel->setArg(argIndex++, *channel_DEPTH_Buff);
	if (film->HasChannel(Film::POSITION))
		initFilmKernel->setArg(argIndex++, *channel_POSITION_Buff);
	if (film->HasChannel(Film::GEOMETRY_NORMAL))
		initFilmKernel->setArg(argIndex++, *channel_GEOMETRY_NORMAL_Buff);
	if (film->HasChannel(Film::SHADING_NORMAL))
		initFilmKernel->setArg(argIndex++, *channel_SHADING_NORMAL_Buff);
	if (film->HasChannel(Film::MATERIAL_ID))
		initFilmKernel->setArg(argIndex++, *channel_MATERIAL_ID_Buff);
	if (film->HasChannel(Film::DIRECT_DIFFUSE))
		initFilmKernel->setArg(argIndex++, *channel_DIRECT_DIFFUSE_Buff);
	if (film->HasChannel(Film::DIRECT_GLOSSY))
		initFilmKernel->setArg(argIndex++, *channel_DIRECT_GLOSSY_Buff);
	if (film->HasChannel(Film::EMISSION))
		initFilmKernel->setArg(argIndex++, *channel_EMISSION_Buff);
	if (film->HasChannel(Film::INDIRECT_DIFFUSE))
		initFilmKernel->setArg(argIndex++, *channel_INDIRECT_DIFFUSE_Buff);
	if (film->HasChannel(Film::INDIRECT_GLOSSY))
		initFilmKernel->setArg(argIndex++, *channel_INDIRECT_GLOSSY_Buff);
	if (film->HasChannel(Film::INDIRECT_SPECULAR))
		initFilmKernel->setArg(argIndex++, *channel_INDIRECT_SPECULAR_Buff);
	if (film->HasChannel(Film::MATERIAL_ID_MASK))
		initFilmKernel->setArg(argIndex++, *channel_MATERIAL_ID_MASK_Buff);
	if (film->HasChannel(Film::DIRECT_SHADOW_MASK))
		initFilmKernel->setArg(argIndex++, *channel_DIRECT_SHADOW_MASK_Buff);
	if (film->HasChannel(Film::INDIRECT_SHADOW_MASK))
		initFilmKernel->setArg(argIndex++, *channel_INDIRECT_SHADOW_MASK_Buff);
	if (film->HasChannel(Film::UV))
		initFilmKernel->setArg(argIndex++, *channel_UV_Buff);

	//--------------------------------------------------------------------------
	// initKernel
	//--------------------------------------------------------------------------

	argIndex = 0;
	initKernel->setArg(argIndex++, renderEngine->seedBase + threadIndex * renderEngine->taskCount);
	initKernel->setArg(argIndex++, *tasksBuff);
	initKernel->setArg(argIndex++, *taskStatsBuff);
	initKernel->setArg(argIndex++, *samplesBuff);
	initKernel->setArg(argIndex++, *sampleDataBuff);
	initKernel->setArg(argIndex++, *raysBuff);
	initKernel->setArg(argIndex++, *cameraBuff);
	initKernel->setArg(argIndex++, renderEngine->film->GetWidth());
	initKernel->setArg(argIndex++, renderEngine->film->GetHeight());
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

	TransferFilm(intersectionDevice->GetOpenCLQueue());

	FreeOCLBuffer(&raysBuff);
	FreeOCLBuffer(&hitsBuff);
	FreeOCLBuffer(&tasksBuff);
	FreeOCLBuffer(&samplesBuff);
	FreeOCLBuffer(&sampleDataBuff);
	FreeOCLBuffer(&taskStatsBuff);
	
	// Film buffers
	for (u_int i = 0; i < channel_RADIANCE_PER_PIXEL_NORMALIZEDs_Buff.size(); ++i)
		FreeOCLBuffer(&channel_RADIANCE_PER_PIXEL_NORMALIZEDs_Buff[i]);
	channel_RADIANCE_PER_PIXEL_NORMALIZEDs_Buff.clear();
	FreeOCLBuffer(&channel_ALPHA_Buff);
	FreeOCLBuffer(&channel_DEPTH_Buff);
	FreeOCLBuffer(&channel_POSITION_Buff);
	FreeOCLBuffer(&channel_GEOMETRY_NORMAL_Buff);
	FreeOCLBuffer(&channel_SHADING_NORMAL_Buff);
	FreeOCLBuffer(&channel_MATERIAL_ID_Buff);
	FreeOCLBuffer(&channel_DIRECT_DIFFUSE_Buff);
	FreeOCLBuffer(&channel_DIRECT_GLOSSY_Buff);
	FreeOCLBuffer(&channel_EMISSION_Buff);
	FreeOCLBuffer(&channel_INDIRECT_DIFFUSE_Buff);
	FreeOCLBuffer(&channel_INDIRECT_GLOSSY_Buff);
	FreeOCLBuffer(&channel_INDIRECT_SPECULAR_Buff);
	FreeOCLBuffer(&channel_MATERIAL_ID_MASK_Buff);
	FreeOCLBuffer(&channel_DIRECT_SHADOW_MASK_Buff);
	FreeOCLBuffer(&channel_INDIRECT_SHADOW_MASK_Buff);
	FreeOCLBuffer(&channel_UV_Buff);

	// Scene buffers
	FreeOCLBuffer(&materialsBuff);
	FreeOCLBuffer(&texturesBuff);
	FreeOCLBuffer(&meshDescsBuff);
	FreeOCLBuffer(&meshMatsBuff);
	FreeOCLBuffer(&normalsBuff);
	FreeOCLBuffer(&uvsBuff);
	FreeOCLBuffer(&colsBuff);
	FreeOCLBuffer(&alphasBuff);
	FreeOCLBuffer(&trianglesBuff);
	FreeOCLBuffer(&vertsBuff);
	FreeOCLBuffer(&infiniteLightBuff);
	FreeOCLBuffer(&infiniteLightDistributionBuff);
	FreeOCLBuffer(&sunLightBuff);
	FreeOCLBuffer(&skyLightBuff);
	FreeOCLBuffer(&cameraBuff);
	FreeOCLBuffer(&triLightDefsBuff);
	FreeOCLBuffer(&meshTriLightDefsOffsetBuff);
	FreeOCLBuffer(&imageMapDescsBuff);
	for (u_int i = 0; i < imageMapsBuff.size(); ++i)
		FreeOCLBuffer(&imageMapsBuff[i]);
	imageMapsBuff.resize(0);

	started = false;

	// Film is delete on the destructor to allow image saving after
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
		// Update Film
		InitFilm();
	}

	if (editActions.Has(CAMERA_EDIT)) {
		// Update Camera
		InitCamera();
	}

	if (editActions.Has(GEOMETRY_EDIT)) {
		// Update Scene Geometry
		InitGeometry();
	}

	if (editActions.Has(IMAGEMAPS_EDIT)) {
		// Update Image Maps
		InitImageMaps();
	}

	if (editActions.Has(MATERIALS_EDIT) || editActions.Has(MATERIAL_TYPES_EDIT)) {
		// Update Scene Textures and Materials
		InitTextures();
		InitMaterials();
	}

	if  (editActions.Has(AREALIGHTS_EDIT)) {
		// Update Scene Area Lights
		InitTriangleAreaLights();
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

	if (editActions.Has(GEOMETRY_EDIT) ||
			editActions.Has(AREALIGHTS_EDIT) ||
			editActions.Has(INFINITELIGHT_EDIT) ||
			editActions.Has(INFINITELIGHT_EDIT) ||
			editActions.Has(INFINITELIGHT_EDIT)) {
		// Update Scene light power distribution for direct light sampling
		InitLightsDistribution();
	}

	//--------------------------------------------------------------------------
	// Recompile Kernels if required
	//--------------------------------------------------------------------------

	if (editActions.Has(FILM_EDIT) || editActions.Has(MATERIAL_TYPES_EDIT))
		InitKernels();

	if (editActions.HasAnyAction()) {
		SetKernelArgs();

		//--------------------------------------------------------------------------
		// Execute initialization kernels
		//--------------------------------------------------------------------------

		cl::CommandQueue &oclQueue = intersectionDevice->GetOpenCLQueue();

		// Clear the frame buffer
		const u_int filmPixelCount = renderEngine->film->GetWidth() * renderEngine->film->GetHeight();
		oclQueue.enqueueNDRangeKernel(*initFilmKernel, cl::NullRange,
			cl::NDRange(RoundUp<u_int>(filmPixelCount, initFilmWorkGroupSize)),
			cl::NDRange(initFilmWorkGroupSize));

		// Initialize the tasks buffer
		oclQueue.enqueueNDRangeKernel(*initKernel, cl::NullRange,
				cl::NDRange(RoundUp<u_int>(renderEngine->taskCount, initWorkGroupSize)),
				cl::NDRange(initWorkGroupSize));
	}

	// Reset statistics in order to be more accurate
	intersectionDevice->ResetPerformaceStats();

	StartRenderThread();
}

void PathOCLRenderThread::TransferFilm(cl::CommandQueue &oclQueue) {
	// Async. transfer of the Film buffers

	for (u_int i = 0; i < channel_RADIANCE_PER_PIXEL_NORMALIZEDs_Buff.size(); ++i) {
		if (channel_RADIANCE_PER_PIXEL_NORMALIZEDs_Buff[i]) {
			oclQueue.enqueueReadBuffer(
				*(channel_RADIANCE_PER_PIXEL_NORMALIZEDs_Buff[i]),
				CL_FALSE,
				0,
				channel_RADIANCE_PER_PIXEL_NORMALIZEDs_Buff[i]->getInfo<CL_MEM_SIZE>(),
				film->channel_RADIANCE_PER_PIXEL_NORMALIZEDs[i]->GetPixels());
		}
	}

	if (channel_ALPHA_Buff) {
		oclQueue.enqueueReadBuffer(
			*channel_ALPHA_Buff,
			CL_FALSE,
			0,
			channel_ALPHA_Buff->getInfo<CL_MEM_SIZE>(),
			film->channel_ALPHA->GetPixels());
	}
	if (channel_DEPTH_Buff) {
		oclQueue.enqueueReadBuffer(
			*channel_DEPTH_Buff,
			CL_FALSE,
			0,
			channel_DEPTH_Buff->getInfo<CL_MEM_SIZE>(),
			film->channel_DEPTH->GetPixels());
	}
	if (channel_POSITION_Buff) {
		oclQueue.enqueueReadBuffer(
			*channel_POSITION_Buff,
			CL_FALSE,
			0,
			channel_POSITION_Buff->getInfo<CL_MEM_SIZE>(),
			film->channel_POSITION->GetPixels());
	}
	if (channel_GEOMETRY_NORMAL_Buff) {
		oclQueue.enqueueReadBuffer(
			*channel_GEOMETRY_NORMAL_Buff,
			CL_FALSE,
			0,
			channel_GEOMETRY_NORMAL_Buff->getInfo<CL_MEM_SIZE>(),
			film->channel_GEOMETRY_NORMAL->GetPixels());
	}
	if (channel_SHADING_NORMAL_Buff) {
		oclQueue.enqueueReadBuffer(
			*channel_SHADING_NORMAL_Buff,
			CL_FALSE,
			0,
			channel_SHADING_NORMAL_Buff->getInfo<CL_MEM_SIZE>(),
			film->channel_SHADING_NORMAL->GetPixels());
	}
	if (channel_MATERIAL_ID_Buff) {
		oclQueue.enqueueReadBuffer(
			*channel_MATERIAL_ID_Buff,
			CL_FALSE,
			0,
			channel_MATERIAL_ID_Buff->getInfo<CL_MEM_SIZE>(),
			film->channel_MATERIAL_ID->GetPixels());
	}
	if (channel_DIRECT_DIFFUSE_Buff) {
		oclQueue.enqueueReadBuffer(
			*channel_DIRECT_DIFFUSE_Buff,
			CL_FALSE,
			0,
			channel_DIRECT_DIFFUSE_Buff->getInfo<CL_MEM_SIZE>(),
			film->channel_DIRECT_DIFFUSE->GetPixels());
	}
	if (channel_MATERIAL_ID_Buff) {
		oclQueue.enqueueReadBuffer(
			*channel_MATERIAL_ID_Buff,
			CL_FALSE,
			0,
			channel_MATERIAL_ID_Buff->getInfo<CL_MEM_SIZE>(),
			film->channel_MATERIAL_ID->GetPixels());
	}
	if (channel_DIRECT_GLOSSY_Buff) {
		oclQueue.enqueueReadBuffer(
			*channel_DIRECT_GLOSSY_Buff,
			CL_FALSE,
			0,
			channel_DIRECT_GLOSSY_Buff->getInfo<CL_MEM_SIZE>(),
			film->channel_DIRECT_GLOSSY->GetPixels());
	}
	if (channel_EMISSION_Buff) {
		oclQueue.enqueueReadBuffer(
			*channel_EMISSION_Buff,
			CL_FALSE,
			0,
			channel_EMISSION_Buff->getInfo<CL_MEM_SIZE>(),
			film->channel_EMISSION->GetPixels());
	}
	if (channel_INDIRECT_DIFFUSE_Buff) {
		oclQueue.enqueueReadBuffer(
			*channel_INDIRECT_DIFFUSE_Buff,
			CL_FALSE,
			0,
			channel_INDIRECT_DIFFUSE_Buff->getInfo<CL_MEM_SIZE>(),
			film->channel_INDIRECT_DIFFUSE->GetPixels());
	}
	if (channel_INDIRECT_GLOSSY_Buff) {
		oclQueue.enqueueReadBuffer(
			*channel_INDIRECT_GLOSSY_Buff,
			CL_FALSE,
			0,
			channel_INDIRECT_GLOSSY_Buff->getInfo<CL_MEM_SIZE>(),
			film->channel_INDIRECT_GLOSSY->GetPixels());
	}
	if (channel_INDIRECT_SPECULAR_Buff) {
		oclQueue.enqueueReadBuffer(
			*channel_INDIRECT_SPECULAR_Buff,
			CL_FALSE,
			0,
			channel_INDIRECT_SPECULAR_Buff->getInfo<CL_MEM_SIZE>(),
			film->channel_INDIRECT_SPECULAR->GetPixels());
	}
	if (channel_MATERIAL_ID_MASK_Buff) {
		oclQueue.enqueueReadBuffer(
			*channel_MATERIAL_ID_MASK_Buff,
			CL_FALSE,
			0,
			channel_MATERIAL_ID_MASK_Buff->getInfo<CL_MEM_SIZE>(),
			film->channel_MATERIAL_ID_MASKs[0]->GetPixels());
	}
	if (channel_DIRECT_SHADOW_MASK_Buff) {
		oclQueue.enqueueReadBuffer(
			*channel_DIRECT_SHADOW_MASK_Buff,
			CL_FALSE,
			0,
			channel_DIRECT_SHADOW_MASK_Buff->getInfo<CL_MEM_SIZE>(),
			film->channel_DIRECT_SHADOW_MASK->GetPixels());
	}
	if (channel_INDIRECT_SHADOW_MASK_Buff) {
		oclQueue.enqueueReadBuffer(
			*channel_INDIRECT_SHADOW_MASK_Buff,
			CL_FALSE,
			0,
			channel_INDIRECT_SHADOW_MASK_Buff->getInfo<CL_MEM_SIZE>(),
			film->channel_INDIRECT_SHADOW_MASK->GetPixels());
	}
	if (channel_UV_Buff) {
		oclQueue.enqueueReadBuffer(
			*channel_UV_Buff,
			CL_FALSE,
			0,
			channel_UV_Buff->getInfo<CL_MEM_SIZE>(),
			film->channel_UV->GetPixels());
	}
}

void PathOCLRenderThread::RenderThreadImpl() {
	//SLG_LOG("[PathOCLRenderThread::" << threadIndex << "] Rendering thread started");

	cl::CommandQueue &oclQueue = intersectionDevice->GetOpenCLQueue();
	const u_int taskCount = renderEngine->taskCount;

	try {
		// signed int in order to avoid problems with underflows (when computing
		// iterations - 1)
		int iterations = 1;

		double startTime = WallClockTime();
		while (!boost::this_thread::interruption_requested()) {
			/*if (threadIndex == 0)
				cerr << "[DEBUG] =================================";*/

			// Async. transfer of the Film buffers
			TransferFilm(oclQueue);

			// Async. transfer of GPU task statistics
			oclQueue.enqueueReadBuffer(
				*(taskStatsBuff),
				CL_FALSE,
				0,
				sizeof(slg::ocl::GPUTaskStats) * taskCount,
				gpuTaskStats);

			// Decide the target refresh time based on screen refresh interval
			const u_int screenRefreshInterval = renderEngine->renderConfig->GetScreenRefreshInterval();
			double targetTime;
			if (screenRefreshInterval <= 100)
				targetTime = 0.025; // 25 ms
			else if (screenRefreshInterval <= 500)
				targetTime = 0.050; // 50 ms
			else
				targetTime = 0.075; // 75 ms

			for (;;) {
				cl::Event event;

				const double t1 = WallClockTime();
				for (int i = 0; i < iterations; ++i) {
					// Trace rays
					intersectionDevice->EnqueueTraceRayBuffer(*raysBuff,
							*(hitsBuff), taskCount, NULL, (i == 0) ? &event : NULL);

					// Advance to next path state
					oclQueue.enqueueNDRangeKernel(*advancePathsKernel, cl::NullRange,
							cl::NDRange(RoundUp<u_int>(taskCount, advancePathsWorkGroupSize)),
							cl::NDRange(advancePathsWorkGroupSize));
				}
				oclQueue.flush();

				event.wait();
				const double t2 = WallClockTime();

				/*if (threadIndex == 0)
					cerr << "[DEBUG] Delta time: " << (t2 - t1) * 1000.0 <<
							"ms (screenRefreshInterval: " << screenRefreshInterval <<
							" iterations: " << iterations << ")\n";*/

				// Check if I have to adjust the number of kernel enqueued
				if (t2 - t1 > targetTime)
					iterations = Max(iterations - 1, 1);
				else
					iterations = Min(iterations + 1, 128);

				// Check if it is time to refresh the screen
				if (((t2 - startTime) * 1000.0 > (double)screenRefreshInterval) ||
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
				"(" << oclErrorString(err.err()) << ")");
	}

	TransferFilm(oclQueue);
	oclQueue.finish();
}

#endif
