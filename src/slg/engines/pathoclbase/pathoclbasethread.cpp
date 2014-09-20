/***************************************************************************
 * Copyright 1998-2013 by authors (see AUTHORS.txt)                        *
 *                                                                         *
 *   This file is part of LuxRender.                                       *
 *                                                                         *
 * Licensed under the Apache License, Version 2.0 (the "License");         *
 * you may not use this file except in compliance with the License.        *
 * You may obtain a copy of the License at                                 *
 *                                                                         *
 *     http://www.apache.org/licenses/LICENSE-2.0                          *
 *                                                                         *
 * Unless required by applicable law or agreed to in writing, software     *
 * distributed under the License is distributed on an "AS IS" BASIS,       *
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.*
 * See the License for the specific language governing permissions and     *
 * limitations under the License.                                          *
 ***************************************************************************/

#if !defined(LUXRAYS_DISABLE_OPENCL)

#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string/replace.hpp>

#include "luxrays/core/geometry/transform.h"
#include "luxrays/utils/ocl.h"
#include "luxrays/core/oclintersectiondevice.h"
#include "luxrays/kernels/kernels.h"

#include "slg/slg.h"
#include "slg/kernels/kernels.h"
#include "slg/renderconfig.h"
#include "slg/engines/pathoclbase/pathoclbase.h"

#if defined(__APPLE__)
//OSX version detection
#include <sys/sysctl.h>
#endif

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// PathOCLBaseRenderThread
//------------------------------------------------------------------------------

PathOCLBaseRenderThread::PathOCLBaseRenderThread(const u_int index,
		OpenCLIntersectionDevice *device, PathOCLBaseRenderEngine *re) {
	intersectionDevice = device;

	renderThread = NULL;

	threadIndex = index;
	renderEngine = re;
	started = false;
	editMode = false;
	threadFilm = NULL;

	filmClearKernel = NULL;

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
	channel_RAYCOUNT_Buff = NULL;
	channel_BY_MATERIAL_ID_Buff = NULL;

	// Scene buffers
	materialsBuff = NULL;
	texturesBuff = NULL;
	meshDescsBuff = NULL;
	meshMatsBuff = NULL;
	lightsBuff = NULL;
	envLightIndicesBuff = NULL;
	lightsDistributionBuff = NULL;
	infiniteLightDistributionsBuff = NULL;
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
	string type = renderEngine->renderConfig->cfg.Get(Property("opencl.kernelcache")("PERSISTENT")).Get<string>();
	if (type == "PERSISTENT")
		kernelCache = new oclKernelPersistentCache("SLG_" SLG_VERSION_MAJOR "." SLG_VERSION_MINOR);
	else if (type == "VOLATILE")
		kernelCache = new oclKernelVolatileCache();
	else if (type == "NONE")
		kernelCache = new oclKernelDummyCache();
	else
		throw runtime_error("Unknown opencl.kernelcache type: " + type);
}

PathOCLBaseRenderThread::~PathOCLBaseRenderThread() {
	if (editMode)
		EndSceneEdit(EditActionList());
	if (started)
		Stop();

	delete filmClearKernel;
	delete threadFilm;
	delete kernelCache;
}

u_int PathOCLBaseRenderThread::SetFilmKernelArgs(cl::Kernel &kernel, u_int argIndex) {
	// Film parameters
	kernel.setArg(argIndex++, threadFilm->GetWidth());
	kernel.setArg(argIndex++, threadFilm->GetHeight());
	for (u_int i = 0; i < channel_RADIANCE_PER_PIXEL_NORMALIZEDs_Buff.size(); ++i)
		kernel.setArg(argIndex++, *(channel_RADIANCE_PER_PIXEL_NORMALIZEDs_Buff[i]));
	if (threadFilm->HasChannel(Film::ALPHA))
		kernel.setArg(argIndex++, *channel_ALPHA_Buff);
	if (threadFilm->HasChannel(Film::DEPTH))
		kernel.setArg(argIndex++, *channel_DEPTH_Buff);
	if (threadFilm->HasChannel(Film::POSITION))
		kernel.setArg(argIndex++, *channel_POSITION_Buff);
	if (threadFilm->HasChannel(Film::GEOMETRY_NORMAL))
		kernel.setArg(argIndex++, *channel_GEOMETRY_NORMAL_Buff);
	if (threadFilm->HasChannel(Film::SHADING_NORMAL))
		kernel.setArg(argIndex++, *channel_SHADING_NORMAL_Buff);
	if (threadFilm->HasChannel(Film::MATERIAL_ID))
		kernel.setArg(argIndex++, *channel_MATERIAL_ID_Buff);
	if (threadFilm->HasChannel(Film::DIRECT_DIFFUSE))
		kernel.setArg(argIndex++, *channel_DIRECT_DIFFUSE_Buff);
	if (threadFilm->HasChannel(Film::DIRECT_GLOSSY))
		kernel.setArg(argIndex++, *channel_DIRECT_GLOSSY_Buff);
	if (threadFilm->HasChannel(Film::EMISSION))
		kernel.setArg(argIndex++, *channel_EMISSION_Buff);
	if (threadFilm->HasChannel(Film::INDIRECT_DIFFUSE))
		kernel.setArg(argIndex++, *channel_INDIRECT_DIFFUSE_Buff);
	if (threadFilm->HasChannel(Film::INDIRECT_GLOSSY))
		kernel.setArg(argIndex++, *channel_INDIRECT_GLOSSY_Buff);
	if (threadFilm->HasChannel(Film::INDIRECT_SPECULAR))
		kernel.setArg(argIndex++, *channel_INDIRECT_SPECULAR_Buff);
	if (threadFilm->HasChannel(Film::MATERIAL_ID_MASK))
		kernel.setArg(argIndex++, *channel_MATERIAL_ID_MASK_Buff);
	if (threadFilm->HasChannel(Film::DIRECT_SHADOW_MASK))
		kernel.setArg(argIndex++, *channel_DIRECT_SHADOW_MASK_Buff);
	if (threadFilm->HasChannel(Film::INDIRECT_SHADOW_MASK))
		kernel.setArg(argIndex++, *channel_INDIRECT_SHADOW_MASK_Buff);
	if (threadFilm->HasChannel(Film::UV))
		kernel.setArg(argIndex++, *channel_UV_Buff);
	if (threadFilm->HasChannel(Film::RAYCOUNT))
		kernel.setArg(argIndex++, *channel_RAYCOUNT_Buff);
	if (threadFilm->HasChannel(Film::BY_MATERIAL_ID))
		kernel.setArg(argIndex++, *channel_BY_MATERIAL_ID_Buff);

	return argIndex;
}

size_t PathOCLBaseRenderThread::GetOpenCLHitPointSize() const {
	// HitPoint memory size
	size_t hitPointSize = sizeof(Vector) + sizeof(Point) + sizeof(UV) + 2 * sizeof(Normal);
	if (renderEngine->compiledScene->IsTextureCompiled(HITPOINTCOLOR) ||
			renderEngine->compiledScene->IsTextureCompiled(HITPOINTGREY))
		hitPointSize += sizeof(Spectrum);
	if (renderEngine->compiledScene->IsTextureCompiled(HITPOINTALPHA))
		hitPointSize += sizeof(float);
	if (renderEngine->compiledScene->RequiresPassThrough())
		hitPointSize += sizeof(float);
	// Volume fields
	if (renderEngine->compiledScene->HasVolumes())
		hitPointSize += 2 * sizeof(u_int) + 2 * sizeof(u_int) +
				sizeof(int);

	return hitPointSize;
}

size_t PathOCLBaseRenderThread::GetOpenCLBSDFSize() const {
	// Add BSDF memory size
	size_t bsdfSize = GetOpenCLHitPointSize();
	// Add BSDF.materialIndex memory size
	bsdfSize += sizeof(u_int);
	// Add BSDF.triangleLightSourceIndex memory size
	if (renderEngine->compiledScene->lightTypeCounts[TYPE_TRIANGLE] > 0)
		bsdfSize += sizeof(u_int);
	// Add BSDF.Frame memory size
	bsdfSize += sizeof(slg::ocl::Frame);
	// Add BSDF.isVolume memory size
	if (renderEngine->compiledScene->HasVolumes())
		bsdfSize += sizeof(int);

	return bsdfSize;
}

size_t PathOCLBaseRenderThread::GetOpenCLSampleResultSize() const {
	//--------------------------------------------------------------------------
	// SampleResult size
	//--------------------------------------------------------------------------

	// SampleResult.filmX and SampleResult.filmY
	size_t sampleResultSize = 2 * sizeof(float);
	// SampleResult.radiancePerPixelNormalized[PARAM_FILM_RADIANCE_GROUP_COUNT]
	sampleResultSize += sizeof(slg::ocl::Spectrum) * threadFilm->GetRadianceGroupCount();
	if (threadFilm->HasChannel(Film::ALPHA))
		sampleResultSize += sizeof(float);
	if (threadFilm->HasChannel(Film::DEPTH))
		sampleResultSize += sizeof(float);
	if (threadFilm->HasChannel(Film::POSITION))
		sampleResultSize += sizeof(Point);
	if (threadFilm->HasChannel(Film::GEOMETRY_NORMAL))
		sampleResultSize += sizeof(Normal);
	if (threadFilm->HasChannel(Film::SHADING_NORMAL))
		sampleResultSize += sizeof(Normal);
	if (threadFilm->HasChannel(Film::MATERIAL_ID))
		sampleResultSize += sizeof(u_int);
	if (threadFilm->HasChannel(Film::DIRECT_DIFFUSE))
		sampleResultSize += sizeof(Spectrum);
	if (threadFilm->HasChannel(Film::DIRECT_GLOSSY))
		sampleResultSize += sizeof(Spectrum);
	if (threadFilm->HasChannel(Film::EMISSION))
		sampleResultSize += sizeof(Spectrum);
	if (threadFilm->HasChannel(Film::INDIRECT_DIFFUSE))
		sampleResultSize += sizeof(Spectrum);
	if (threadFilm->HasChannel(Film::INDIRECT_GLOSSY))
		sampleResultSize += sizeof(Spectrum);
	if (threadFilm->HasChannel(Film::INDIRECT_SPECULAR))
		sampleResultSize += sizeof(Spectrum);
	if (threadFilm->HasChannel(Film::MATERIAL_ID_MASK))
		sampleResultSize += sizeof(float);
	if (threadFilm->HasChannel(Film::DIRECT_SHADOW_MASK))
		sampleResultSize += sizeof(float);
	if (threadFilm->HasChannel(Film::INDIRECT_SHADOW_MASK))
		sampleResultSize += sizeof(float);
	if (threadFilm->HasChannel(Film::UV))
		sampleResultSize += sizeof(UV);
	if (threadFilm->HasChannel(Film::RAYCOUNT))
		sampleResultSize += sizeof(Film::RAYCOUNT);

	sampleResultSize += sizeof(BSDFEvent) +
			2 * sizeof(int);
	
	return sampleResultSize;
}

void PathOCLBaseRenderThread::AllocOCLBufferRO(cl::Buffer **buff, void *src, const size_t size, const string &desc) {
	// Check if the buffer is too big
	if (intersectionDevice->GetDeviceDesc()->GetMaxMemoryAllocSize() < size) {
		stringstream ss;
		ss << "The " << desc << " buffer is too big for " << intersectionDevice->GetName() <<
				" device (i.e. CL_DEVICE_MAX_MEM_ALLOC_SIZE=" <<
				intersectionDevice->GetDeviceDesc()->GetMaxMemoryAllocSize() <<
				"): try to reduce related parameters";
		throw runtime_error(ss.str());
	}

	if (*buff) {
		// Check the size of the already allocated buffer

		if (size == (*buff)->getInfo<CL_MEM_SIZE>()) {
			// I can reuse the buffer; just update the content

			//SLG_LOG("[PathOCLBaseRenderThread::" << threadIndex << "] " << desc << " buffer updated for size: " << (size / 1024) << "Kbytes");
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

	SLG_LOG("[PathOCLBaseRenderThread::" << threadIndex << "] " << desc << " buffer size: " <<
			(size < 10000 ? size : (size / 1024)) << (size < 10000 ? "bytes" : "Kbytes"));
	*buff = new cl::Buffer(oclContext,
			CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
			size, src);
	intersectionDevice->AllocMemory((*buff)->getInfo<CL_MEM_SIZE>());
}

void PathOCLBaseRenderThread::AllocOCLBufferRW(cl::Buffer **buff, const size_t size, const string &desc) {
	// Check if the buffer is too big
	if (intersectionDevice->GetDeviceDesc()->GetMaxMemoryAllocSize() < size) {
		stringstream ss;
		ss << "The " << desc << " buffer is too big for " << intersectionDevice->GetName() <<
				" device (i.e. CL_DEVICE_MAX_MEM_ALLOC_SIZE=" <<
				intersectionDevice->GetDeviceDesc()->GetMaxMemoryAllocSize() <<
				"): try to reduce related parameters";
		throw runtime_error(ss.str());
	}

	if (*buff) {
		// Check the size of the already allocated buffer

		if (size == (*buff)->getInfo<CL_MEM_SIZE>()) {
			// I can reuse the buffer
			//SLG_LOG("[PathOCLBaseRenderThread::" << threadIndex << "] " << desc << " buffer reused for size: " << (size / 1024) << "Kbytes");
			return;
		} else {
			// Free the buffer
			intersectionDevice->FreeMemory((*buff)->getInfo<CL_MEM_SIZE>());
			delete *buff;
		}
	}

	cl::Context &oclContext = intersectionDevice->GetOpenCLContext();

	SLG_LOG("[PathOCLBaseRenderThread::" << threadIndex << "] " << desc << " buffer size: " <<
			(size < 10000 ? size : (size / 1024)) << (size < 10000 ? "bytes" : "Kbytes"));
	*buff = new cl::Buffer(oclContext,
			CL_MEM_READ_WRITE,
			size);
	intersectionDevice->AllocMemory((*buff)->getInfo<CL_MEM_SIZE>());
}

void PathOCLBaseRenderThread::FreeOCLBuffer(cl::Buffer **buff) {
	if (*buff) {

		intersectionDevice->FreeMemory((*buff)->getInfo<CL_MEM_SIZE>());
		delete *buff;
		*buff = NULL;
	}
}

void PathOCLBaseRenderThread::InitFilm() {
	const Film *engineFilm = renderEngine->film;
	u_int filmWidth, filmHeight;
	GetThreadFilmSize(&filmWidth, &filmHeight);
	const u_int filmPixelCount = filmWidth * filmHeight;

	// Delete previous allocated Film
	delete threadFilm;

	// Allocate the new Film
	threadFilm = new Film(filmWidth, filmHeight);
	threadFilm->CopyDynamicSettings(*engineFilm);
	threadFilm->Init();

	//--------------------------------------------------------------------------
	for (u_int i = 0; i < channel_RADIANCE_PER_PIXEL_NORMALIZEDs_Buff.size(); ++i)
		FreeOCLBuffer(&channel_RADIANCE_PER_PIXEL_NORMALIZEDs_Buff[i]);

	if (threadFilm->GetRadianceGroupCount() > 8)
		throw runtime_error("PathOCL supports only up to 8 Radiance Groups");

	channel_RADIANCE_PER_PIXEL_NORMALIZEDs_Buff.resize(threadFilm->GetRadianceGroupCount());
	for (u_int i = 0; i < channel_RADIANCE_PER_PIXEL_NORMALIZEDs_Buff.size(); ++i) {
		AllocOCLBufferRW(&channel_RADIANCE_PER_PIXEL_NORMALIZEDs_Buff[i],
				sizeof(float[4]) * filmPixelCount, "RADIANCE_PER_PIXEL_NORMALIZEDs[" + ToString(i) + "]");
	}
	//--------------------------------------------------------------------------
	if (threadFilm->HasChannel(Film::ALPHA))
		AllocOCLBufferRW(&channel_ALPHA_Buff, sizeof(float[2]) * filmPixelCount, "ALPHA");
	else
		FreeOCLBuffer(&channel_ALPHA_Buff);
	//--------------------------------------------------------------------------
	if (threadFilm->HasChannel(Film::DEPTH))
		AllocOCLBufferRW(&channel_DEPTH_Buff, sizeof(float) * filmPixelCount, "DEPTH");
	else
		FreeOCLBuffer(&channel_DEPTH_Buff);
	//--------------------------------------------------------------------------
	if (threadFilm->HasChannel(Film::POSITION))
		AllocOCLBufferRW(&channel_POSITION_Buff, sizeof(float[3]) * filmPixelCount, "POSITION");
	else
		FreeOCLBuffer(&channel_POSITION_Buff);
	//--------------------------------------------------------------------------
	if (threadFilm->HasChannel(Film::GEOMETRY_NORMAL))
		AllocOCLBufferRW(&channel_GEOMETRY_NORMAL_Buff, sizeof(float[3]) * filmPixelCount, "GEOMETRY_NORMAL");
	else
		FreeOCLBuffer(&channel_GEOMETRY_NORMAL_Buff);
	//--------------------------------------------------------------------------
	if (threadFilm->HasChannel(Film::SHADING_NORMAL))
		AllocOCLBufferRW(&channel_SHADING_NORMAL_Buff, sizeof(float[3]) * filmPixelCount, "SHADING_NORMAL");
	else
		FreeOCLBuffer(&channel_SHADING_NORMAL_Buff);
	//--------------------------------------------------------------------------
	if (threadFilm->HasChannel(Film::MATERIAL_ID))
		AllocOCLBufferRW(&channel_MATERIAL_ID_Buff, sizeof(u_int) * filmPixelCount, "MATERIAL_ID");
	else
		FreeOCLBuffer(&channel_MATERIAL_ID_Buff);
	//--------------------------------------------------------------------------
	if (threadFilm->HasChannel(Film::DIRECT_DIFFUSE))
		AllocOCLBufferRW(&channel_DIRECT_DIFFUSE_Buff, sizeof(float[4]) * filmPixelCount, "DIRECT_DIFFUSE");
	else
		FreeOCLBuffer(&channel_DIRECT_DIFFUSE_Buff);
	//--------------------------------------------------------------------------
	if (threadFilm->HasChannel(Film::DIRECT_GLOSSY))
		AllocOCLBufferRW(&channel_DIRECT_GLOSSY_Buff, sizeof(float[4]) * filmPixelCount, "DIRECT_GLOSSY");
	else
		FreeOCLBuffer(&channel_DIRECT_GLOSSY_Buff);
	//--------------------------------------------------------------------------
	if (threadFilm->HasChannel(Film::EMISSION))
		AllocOCLBufferRW(&channel_EMISSION_Buff, sizeof(float[4]) * filmPixelCount, "EMISSION");
	else
		FreeOCLBuffer(&channel_EMISSION_Buff);
	//--------------------------------------------------------------------------
	if (threadFilm->HasChannel(Film::INDIRECT_DIFFUSE))
		AllocOCLBufferRW(&channel_INDIRECT_DIFFUSE_Buff, sizeof(float[4]) * filmPixelCount, "INDIRECT_DIFFUSE");
	else
		FreeOCLBuffer(&channel_INDIRECT_DIFFUSE_Buff);
	//--------------------------------------------------------------------------
	if (threadFilm->HasChannel(Film::INDIRECT_GLOSSY))
		AllocOCLBufferRW(&channel_INDIRECT_GLOSSY_Buff, sizeof(float[4]) * filmPixelCount, "INDIRECT_GLOSSY");
	else
		FreeOCLBuffer(&channel_INDIRECT_GLOSSY_Buff);
	//--------------------------------------------------------------------------
	if (threadFilm->HasChannel(Film::INDIRECT_SPECULAR))
		AllocOCLBufferRW(&channel_INDIRECT_SPECULAR_Buff, sizeof(float[4]) * filmPixelCount, "INDIRECT_SPECULAR");
	else
		FreeOCLBuffer(&channel_INDIRECT_SPECULAR_Buff);
	//--------------------------------------------------------------------------
	if (threadFilm->HasChannel(Film::MATERIAL_ID_MASK)) {
		if (threadFilm->GetMaskMaterialIDCount() > 1)
			throw runtime_error("PathOCL supports only 1 MATERIAL_ID_MASK");
		else
			AllocOCLBufferRW(&channel_MATERIAL_ID_MASK_Buff,
					sizeof(float[2]) * filmPixelCount, "MATERIAL_ID_MASK");
	} else
		FreeOCLBuffer(&channel_MATERIAL_ID_MASK_Buff);
	//--------------------------------------------------------------------------
	if (threadFilm->HasChannel(Film::DIRECT_SHADOW_MASK))
		AllocOCLBufferRW(&channel_DIRECT_SHADOW_MASK_Buff, sizeof(float[2]) * filmPixelCount, "DIRECT_SHADOW_MASK");
	else
		FreeOCLBuffer(&channel_DIRECT_SHADOW_MASK_Buff);
	//--------------------------------------------------------------------------
	if (threadFilm->HasChannel(Film::INDIRECT_SHADOW_MASK))
		AllocOCLBufferRW(&channel_INDIRECT_SHADOW_MASK_Buff, sizeof(float[2]) * filmPixelCount, "INDIRECT_SHADOW_MASK");
	else
		FreeOCLBuffer(&channel_INDIRECT_SHADOW_MASK_Buff);
	//--------------------------------------------------------------------------
	if (threadFilm->HasChannel(Film::UV))
		AllocOCLBufferRW(&channel_UV_Buff, sizeof(float[2]) * filmPixelCount, "UV");
	else
		FreeOCLBuffer(&channel_UV_Buff);
	if (threadFilm->HasChannel(Film::RAYCOUNT))
		AllocOCLBufferRW(&channel_RAYCOUNT_Buff, sizeof(float) * filmPixelCount, "RAYCOUNT");
	else
		FreeOCLBuffer(&channel_RAYCOUNT_Buff);
	//--------------------------------------------------------------------------
	if (threadFilm->HasChannel(Film::BY_MATERIAL_ID)) {
		if (threadFilm->GetByMaterialIDCount() > 1)
			throw runtime_error("PathOCL supports only 1 BY_MATERIAL_ID");
		else
			AllocOCLBufferRW(&channel_BY_MATERIAL_ID_Buff,
					sizeof(float[4]) * filmPixelCount, "BY_MATERIAL_ID");
	} else
		FreeOCLBuffer(&channel_BY_MATERIAL_ID_Buff);
}

void PathOCLBaseRenderThread::InitCamera() {
	AllocOCLBufferRO(&cameraBuff, &renderEngine->compiledScene->camera,
			sizeof(slg::ocl::Camera), "Camera");
}

void PathOCLBaseRenderThread::InitGeometry() {
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

void PathOCLBaseRenderThread::InitMaterials() {
	const size_t materialsCount = renderEngine->compiledScene->mats.size();
	AllocOCLBufferRO(&materialsBuff, &renderEngine->compiledScene->mats[0],
			sizeof(slg::ocl::Material) * materialsCount, "Materials");

	const u_int meshCount = renderEngine->compiledScene->meshMats.size();
	AllocOCLBufferRO(&meshMatsBuff, &renderEngine->compiledScene->meshMats[0],
			sizeof(u_int) * meshCount, "Mesh material index");
}

void PathOCLBaseRenderThread::InitTextures() {
	const size_t texturesCount = renderEngine->compiledScene->texs.size();
	AllocOCLBufferRO(&texturesBuff, &renderEngine->compiledScene->texs[0],
			sizeof(slg::ocl::Texture) * texturesCount, "Textures");
}

void PathOCLBaseRenderThread::InitLights() {
	CompiledScene *cscene = renderEngine->compiledScene;

	AllocOCLBufferRO(&lightsBuff, &cscene->lightDefs[0],
		sizeof(slg::ocl::LightSource) * cscene->lightDefs.size(), "Lights");
	if (cscene->envLightIndices.size() > 0) {
		AllocOCLBufferRO(&envLightIndicesBuff, &cscene->envLightIndices[0],
				sizeof(slg::ocl::LightSource) * cscene->envLightIndices.size(), "Env. light indices");
	} else
		FreeOCLBuffer(&envLightIndicesBuff);

	AllocOCLBufferRO(&meshTriLightDefsOffsetBuff, &cscene->meshTriLightDefsOffset[0],
		sizeof(u_int) * cscene->meshTriLightDefsOffset.size(), "Light offsets");

	if (cscene->infiniteLightDistributions.size() > 0) {
		AllocOCLBufferRO(&infiniteLightDistributionsBuff, &cscene->infiniteLightDistributions[0],
			sizeof(float) * cscene->infiniteLightDistributions.size(), "InfiniteLight distributions");
	} else
		FreeOCLBuffer(&infiniteLightDistributionsBuff);

	AllocOCLBufferRO(&lightsDistributionBuff, cscene->lightsDistribution,
		cscene->lightsDistributionSize, "LightsDistribution");
}

void PathOCLBaseRenderThread::InitImageMaps() {
	CompiledScene *cscene = renderEngine->compiledScene;

	if (cscene->imageMapDescs.size() > 0) {
		AllocOCLBufferRO(&imageMapDescsBuff, &cscene->imageMapDescs[0],
				sizeof(slg::ocl::ImageMap) * cscene->imageMapDescs.size(), "ImageMap descriptions");

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

void PathOCLBaseRenderThread::CompileKernel(cl::Program *program, cl::Kernel **kernel,
		size_t *workgroupSize, const string &name) {
	delete *kernel;
	SLG_LOG("[PathOCLBaseRenderThread::" << threadIndex << "] Compiling " << name << " Kernel");
	*kernel = new cl::Kernel(*program, name.c_str());

	if (intersectionDevice->GetDeviceDesc()->GetForceWorkGroupSize() > 0)
		*workgroupSize = intersectionDevice->GetDeviceDesc()->GetForceWorkGroupSize();
	else {
		cl::Device &oclDevice = intersectionDevice->GetOpenCLDevice();
		(*kernel)->getWorkGroupInfo<size_t>(oclDevice, CL_KERNEL_WORK_GROUP_SIZE, workgroupSize);
		SLG_LOG("[PathOCLBaseRenderThread::" << threadIndex << "] " << name << " workgroup size: " << *workgroupSize);
	}
}

void PathOCLBaseRenderThread::InitKernels() {
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
			" -D RENDER_ENGINE_" << RenderEngine::RenderEngineType2String(renderEngine->GetEngineType()) <<
			" -D PARAM_RAY_EPSILON_MIN=" << MachineEpsilon::GetMin() << "f"
			" -D PARAM_RAY_EPSILON_MAX=" << MachineEpsilon::GetMax() << "f"
			" -D PARAM_LIGHT_WORLD_RADIUS_SCALE=" << LIGHT_WORLD_RADIUS_SCALE << "f"
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
			throw new runtime_error("Unknown accelerator in PathOCLBaseRenderThread::InitKernels()");
	}

	// Film related parameters
	for (u_int i = 0; i < channel_RADIANCE_PER_PIXEL_NORMALIZEDs_Buff.size(); ++i)
		ss << " -D PARAM_FILM_RADIANCE_GROUP_" << i;
	ss << " -D PARAM_FILM_RADIANCE_GROUP_COUNT=" << channel_RADIANCE_PER_PIXEL_NORMALIZEDs_Buff.size();
	if (threadFilm->HasChannel(Film::ALPHA))
		ss << " -D PARAM_FILM_CHANNELS_HAS_ALPHA";
	if (threadFilm->HasChannel(Film::DEPTH))
		ss << " -D PARAM_FILM_CHANNELS_HAS_DEPTH";
	if (threadFilm->HasChannel(Film::POSITION))
		ss << " -D PARAM_FILM_CHANNELS_HAS_POSITION";
	if (threadFilm->HasChannel(Film::GEOMETRY_NORMAL))
		ss << " -D PARAM_FILM_CHANNELS_HAS_GEOMETRY_NORMAL";
	if (threadFilm->HasChannel(Film::SHADING_NORMAL))
		ss << " -D PARAM_FILM_CHANNELS_HAS_SHADING_NORMAL";
	if (threadFilm->HasChannel(Film::MATERIAL_ID))
		ss << " -D PARAM_FILM_CHANNELS_HAS_MATERIAL_ID";
	if (threadFilm->HasChannel(Film::DIRECT_DIFFUSE))
		ss << " -D PARAM_FILM_CHANNELS_HAS_DIRECT_DIFFUSE";
	if (threadFilm->HasChannel(Film::DIRECT_GLOSSY))
		ss << " -D PARAM_FILM_CHANNELS_HAS_DIRECT_GLOSSY";
	if (threadFilm->HasChannel(Film::EMISSION))
		ss << " -D PARAM_FILM_CHANNELS_HAS_EMISSION";
	if (threadFilm->HasChannel(Film::INDIRECT_DIFFUSE))
		ss << " -D PARAM_FILM_CHANNELS_HAS_INDIRECT_DIFFUSE";
	if (threadFilm->HasChannel(Film::INDIRECT_GLOSSY))
		ss << " -D PARAM_FILM_CHANNELS_HAS_INDIRECT_GLOSSY";
	if (threadFilm->HasChannel(Film::INDIRECT_SPECULAR))
		ss << " -D PARAM_FILM_CHANNELS_HAS_INDIRECT_SPECULAR";
	if (threadFilm->HasChannel(Film::MATERIAL_ID_MASK)) {
		ss << " -D PARAM_FILM_CHANNELS_HAS_MATERIAL_ID_MASK" <<
				" -D PARAM_FILM_MASK_MATERIAL_ID=" << threadFilm->GetMaskMaterialID(0);
	}
	if (threadFilm->HasChannel(Film::DIRECT_SHADOW_MASK))
		ss << " -D PARAM_FILM_CHANNELS_HAS_DIRECT_SHADOW_MASK";
	if (threadFilm->HasChannel(Film::INDIRECT_SHADOW_MASK))
		ss << " -D PARAM_FILM_CHANNELS_HAS_INDIRECT_SHADOW_MASK";
	if (threadFilm->HasChannel(Film::UV))
		ss << " -D PARAM_FILM_CHANNELS_HAS_UV";
	if (threadFilm->HasChannel(Film::RAYCOUNT))
		ss << " -D PARAM_FILM_CHANNELS_HAS_RAYCOUNT";
	if (threadFilm->HasChannel(Film::BY_MATERIAL_ID)) {
		ss << " -D PARAM_FILM_CHANNELS_HAS_BY_MATERIAL_ID" <<
				" -D PARAM_FILM_BY_MATERIAL_ID=" << threadFilm->GetByMaterialID(0);
	}

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
	if (cscene->IsTextureCompiled(BLENDER_BLEND))
		ss << " -D PARAM_ENABLE_BLENDER_BLEND";
 	if (cscene->IsTextureCompiled(BLENDER_CLOUDS))
 		ss << " -D PARAM_ENABLE_BLENDER_CLOUDS";
	if (cscene->IsTextureCompiled(BLENDER_DISTORTED_NOISE))
		ss << " -D PARAM_ENABLE_BLENDER_DISTORTED_NOISE";
	if (cscene->IsTextureCompiled(BLENDER_MAGIC))
		ss << " -D PARAM_ENABLE_BLENDER_MAGIC";
	if (cscene->IsTextureCompiled(BLENDER_MARBLE))
		ss << " -D PARAM_ENABLE_BLENDER_MARBLE";
	if (cscene->IsTextureCompiled(BLENDER_MUSGRAVE))
		ss << " -D PARAM_ENABLE_BLENDER_MUSGRAVE";
	if (cscene->IsTextureCompiled(BLENDER_STUCCI))
		ss << " -D PARAM_ENABLE_BLENDER_STUCCI";
 	if (cscene->IsTextureCompiled(BLENDER_WOOD))
 		ss << " -D PARAM_ENABLE_BLENDER_WOOD";
	if (cscene->IsTextureCompiled(BLENDER_VORONOI))
		ss << " -D PARAM_ENABLE_BLENDER_VORONOI";
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
	if (cscene->IsTextureCompiled(NORMALMAP_TEX))
		ss << " -D PARAM_ENABLE_TEX_NORMALMAP";

	if (cscene->IsMaterialCompiled(MATTE))
		ss << " -D PARAM_ENABLE_MAT_MATTE";
	if (cscene->IsMaterialCompiled(ROUGHMATTE))
		ss << " -D PARAM_ENABLE_MAT_ROUGHMATTE";
	if (cscene->IsMaterialCompiled(VELVET))
		ss << " -D PARAM_ENABLE_MAT_VELVET";
	if (cscene->IsMaterialCompiled(MIRROR))
		ss << " -D PARAM_ENABLE_MAT_MIRROR";
	if (cscene->IsMaterialCompiled(GLASS))
		ss << " -D PARAM_ENABLE_MAT_GLASS";
	if (cscene->IsMaterialCompiled(ARCHGLASS))
		ss << " -D PARAM_ENABLE_MAT_ARCHGLASS";
	if (cscene->IsMaterialCompiled(MIX))
		ss << " -D PARAM_ENABLE_MAT_MIX";
	if (cscene->IsMaterialCompiled(NULLMAT))
		ss << " -D PARAM_ENABLE_MAT_NULL";
	if (cscene->IsMaterialCompiled(MATTETRANSLUCENT))
		ss << " -D PARAM_ENABLE_MAT_MATTETRANSLUCENT";
	if (cscene->IsMaterialCompiled(ROUGHMATTETRANSLUCENT))
		ss << " -D PARAM_ENABLE_MAT_ROUGHMATTETRANSLUCENT";
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
	if (cscene->IsMaterialCompiled(CLOTH))
		ss << " -D PARAM_ENABLE_MAT_CLOTH";
	if (cscene->IsMaterialCompiled(CARPAINT))
		ss << " -D PARAM_ENABLE_MAT_CARPAINT";
	if (cscene->IsMaterialCompiled(CLEAR_VOL))
		ss << " -D PARAM_ENABLE_MAT_CLEAR_VOL";
	if (cscene->IsMaterialCompiled(HOMOGENEOUS_VOL))
		ss << " -D PARAM_ENABLE_MAT_HOMOGENEOUS_VOL";
	if (cscene->IsMaterialCompiled(HETEROGENEOUS_VOL))
		ss << " -D PARAM_ENABLE_MAT_HETEROGENEOUS_VOL";

	if (cscene->RequiresPassThrough())
		ss << " -D PARAM_HAS_PASSTHROUGH";

	if (cscene->camera.lensRadius > 0.f)
		ss << " -D PARAM_CAMERA_HAS_DOF";
	if (cscene->enableCameraHorizStereo) {
		ss << " -D PARAM_CAMERA_ENABLE_HORIZ_STEREO";

		if (cscene->enableOculusRiftBarrel)
			ss << " -D PARAM_CAMERA_ENABLE_OCULUSRIFT_BARREL";
	}
	if (cscene->enableCameraClippingPlane)
		ss << " -D PARAM_CAMERA_ENABLE_CLIPPING_PLANE";

	if (renderEngine->compiledScene->lightTypeCounts[TYPE_IL] > 0)
		ss << " -D PARAM_HAS_INFINITELIGHT";
	if (renderEngine->compiledScene->lightTypeCounts[TYPE_IL_CONSTANT] > 0)
		ss << " -D PARAM_HAS_CONSTANTINFINITELIGHT";
	if (renderEngine->compiledScene->lightTypeCounts[TYPE_IL_SKY] > 0)
		ss << " -D PARAM_HAS_SKYLIGHT";
	if (renderEngine->compiledScene->lightTypeCounts[TYPE_IL_SKY2] > 0)
		ss << " -D PARAM_HAS_SKYLIGHT2";
	if (renderEngine->compiledScene->lightTypeCounts[TYPE_SUN] > 0)
		ss << " -D PARAM_HAS_SUNLIGHT";
	if (renderEngine->compiledScene->lightTypeCounts[TYPE_SHARPDISTANT] > 0)
		ss << " -D PARAM_HAS_SHARPDISTANTLIGHT";
	if (renderEngine->compiledScene->lightTypeCounts[TYPE_DISTANT] > 0)
		ss << " -D PARAM_HAS_DISTANTLIGHT";
	if (renderEngine->compiledScene->lightTypeCounts[TYPE_POINT] > 0)
		ss << " -D PARAM_HAS_POINTLIGHT";
	if (renderEngine->compiledScene->lightTypeCounts[TYPE_MAPPOINT] > 0)
		ss << " -D PARAM_HAS_MAPPOINTLIGHT";
	if (renderEngine->compiledScene->lightTypeCounts[TYPE_SPOT] > 0)
		ss << " -D PARAM_HAS_SPOTLIGHT";
	if (renderEngine->compiledScene->lightTypeCounts[TYPE_PROJECTION] > 0)
		ss << " -D PARAM_HAS_PROJECTIONLIGHT";
	if (renderEngine->compiledScene->lightTypeCounts[TYPE_LASER] > 0)
		ss << " -D PARAM_HAS_LASERLIGHT";
	ss << " -D PARAM_TRIANGLE_LIGHT_COUNT=" << renderEngine->compiledScene->lightTypeCounts[TYPE_TRIANGLE];
	ss << " -D PARAM_LIGHT_COUNT=" << renderEngine->compiledScene->lightDefs.size();

	if (renderEngine->compiledScene->hasInfiniteLights)
		ss << " -D PARAM_HAS_INFINITELIGHTS";
	if (renderEngine->compiledScene->hasEnvLights)
		ss << " -D PARAM_HAS_ENVLIGHTS";

	if (imageMapDescsBuff) {
		ss << " -D PARAM_HAS_IMAGEMAPS";
		if (imageMapsBuff.size() > 8)
			throw runtime_error("Too many memory pages required for image maps");
		for (u_int i = 0; i < imageMapsBuff.size(); ++i)
			ss << " -D PARAM_IMAGEMAPS_PAGE_" << i;
		ss << " -D PARAM_IMAGEMAPS_COUNT=" << imageMapsBuff.size();
	}

	if (renderEngine->compiledScene->HasBumpMaps())
		ss << " -D PARAM_HAS_BUMPMAPS";

	if (renderEngine->compiledScene->HasVolumes()) {
		ss << " -D PARAM_HAS_VOLUMES";
		ss << " -D SCENE_DEFAULT_VOLUME_INDEX=" << renderEngine->compiledScene->defaultWorldVolumeIndex;
	}

	// Some information about our place in the universe...
	ss << " -D PARAM_DEVICE_INDEX=" << threadIndex;
	ss << " -D PARAM_DEVICE_COUNT=" << renderEngine->intersectionDevices.size();

	if (!renderEngine->useDynamicCodeGenerationForTextures)
		ss << " -D PARAM_DIASBLE_TEX_DYNAMIC_EVALUATION";
	if (!renderEngine->useDynamicCodeGenerationForMaterials)
		ss << " -D PARAM_DIASBLE_MAT_DYNAMIC_EVALUATION";

	ss << AdditionalKernelOptions();

	//--------------------------------------------------------------------------

	// Check the OpenCL vendor and use some specific compiler options

#if defined(__APPLE__)
	// Starting with 10.10 (darwin 14.x.x), opencl mix() function is fixed
	char darwin_ver[10];
	size_t len = sizeof(darwin_ver);
	sysctlbyname("kern.osrelease", &darwin_ver, &len, NULL, 0);
	if(darwin_ver[0] == '1' && darwin_ver[1] < '4') {
		ss << " -D __APPLE_CL__";
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
			AdditionalKernelDefinitions() <<
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
			luxrays::ocl::KernelSource_color_types <<
			luxrays::ocl::KernelSource_frame_types <<
			luxrays::ocl::KernelSource_matrix4x4_types <<
			luxrays::ocl::KernelSource_quaternion_types <<
			luxrays::ocl::KernelSource_transform_types <<
			luxrays::ocl::KernelSource_motionsystem_types <<
			// OpenCL LuxRays Funcs
			luxrays::ocl::KernelSource_epsilon_funcs <<
			luxrays::ocl::KernelSource_utils_funcs <<
			luxrays::ocl::KernelSource_vector_funcs <<
			luxrays::ocl::KernelSource_ray_funcs <<
			luxrays::ocl::KernelSource_bbox_funcs <<
			luxrays::ocl::KernelSource_color_funcs <<
			luxrays::ocl::KernelSource_frame_funcs <<
			luxrays::ocl::KernelSource_matrix4x4_funcs <<
			luxrays::ocl::KernelSource_quaternion_funcs <<
			luxrays::ocl::KernelSource_transform_funcs <<
			luxrays::ocl::KernelSource_motionsystem_funcs <<
			// OpenCL SLG Types
			slg::ocl::KernelSource_randomgen_types <<
			slg::ocl::KernelSource_trianglemesh_types <<
			slg::ocl::KernelSource_hitpoint_types <<
			slg::ocl::KernelSource_mapping_types <<
			slg::ocl::KernelSource_texture_types <<
			slg::ocl::KernelSource_bsdf_types <<
			slg::ocl::KernelSource_material_types <<
			slg::ocl::KernelSource_volume_types <<
			slg::ocl::KernelSource_film_types <<
			slg::ocl::KernelSource_filter_types <<
			slg::ocl::KernelSource_sampler_types <<
			slg::ocl::KernelSource_camera_types <<
			slg::ocl::KernelSource_light_types <<
			// OpenCL SLG Funcs
			slg::ocl::KernelSource_mc_funcs <<
			slg::ocl::KernelSource_randomgen_funcs <<
			slg::ocl::KernelSource_triangle_funcs <<
			slg::ocl::KernelSource_trianglemesh_funcs <<
			slg::ocl::KernelSource_mapping_funcs <<
			slg::ocl::KernelSource_texture_noise_funcs <<
			slg::ocl::KernelSource_texture_blender_noise_funcs <<
			slg::ocl::KernelSource_texture_blender_noise_funcs2 <<
			slg::ocl::KernelSource_texture_blender_funcs <<
			slg::ocl::KernelSource_texture_funcs;

		if (renderEngine->useDynamicCodeGenerationForTextures) {
			// Generate the code to evaluate the textures
			ssKernel <<
				"#line 2 \"Texture evaluation code form CompiledScene::GetTexturesEvaluationSourceCode()\"\n" <<
				cscene->GetTexturesEvaluationSourceCode() <<
				"\n";
		}

		ssKernel <<
			slg::ocl::KernelSource_texture_bump_funcs <<
			slg::ocl::KernelSource_materialdefs_funcs_generic <<
			slg::ocl::KernelSource_materialdefs_funcs_archglass <<
			slg::ocl::KernelSource_materialdefs_funcs_carpaint <<
			slg::ocl::KernelSource_materialdefs_funcs_clearvol <<
			slg::ocl::KernelSource_materialdefs_funcs_cloth <<
			slg::ocl::KernelSource_materialdefs_funcs_glass <<
			slg::ocl::KernelSource_materialdefs_funcs_glossy2 <<
			slg::ocl::KernelSource_materialdefs_funcs_heterogeneousvol <<
			slg::ocl::KernelSource_materialdefs_funcs_homogeneousvol <<
			slg::ocl::KernelSource_materialdefs_funcs_matte <<
			slg::ocl::KernelSource_materialdefs_funcs_matte_translucent <<
			slg::ocl::KernelSource_materialdefs_funcs_metal2 <<
			slg::ocl::KernelSource_materialdefs_funcs_mirror <<
			slg::ocl::KernelSource_materialdefs_funcs_null <<
			slg::ocl::KernelSource_materialdefs_funcs_roughglass <<
			slg::ocl::KernelSource_materialdefs_funcs_roughmatte_translucent <<
			slg::ocl::KernelSource_materialdefs_funcs_velvet <<
			slg::ocl::KernelSource_material_funcs <<
			// KernelSource_materialdefs_funcs_mix must always be the last one
			slg::ocl::KernelSource_materialdefs_funcs_mix;

		if (renderEngine->useDynamicCodeGenerationForMaterials) {
			// Generate the code to evaluate the textures
			ssKernel <<
				"#line 2 \"Material evaluation code form CompiledScene::GetMaterialsEvaluationSourceCode()\"\n" <<
				cscene->GetMaterialsEvaluationSourceCode() <<
				"\n";
		}

		ssKernel <<
			slg::ocl::KernelSource_bsdfutils_funcs << // Must be before volumeinfo_funcs
			slg::ocl::KernelSource_volume_funcs <<
			slg::ocl::KernelSource_volumeinfo_funcs <<
			slg::ocl::KernelSource_camera_funcs <<
			slg::ocl::KernelSource_light_funcs <<
			slg::ocl::KernelSource_filter_funcs <<
			slg::ocl::KernelSource_film_funcs <<
			slg::ocl::KernelSource_sampler_funcs <<
			slg::ocl::KernelSource_bsdf_funcs <<
			slg::ocl::KernelSource_scene_funcs <<
			// PathOCL Funcs
			slg::ocl::KernelSource_pathoclbase_funcs;

		ssKernel << AdditionalKernelSources();

		string kernelSource = ssKernel.str();

		SLG_LOG("[PathOCLBaseRenderThread::" << threadIndex << "] Defined symbols: " << kernelsParameters);
		SLG_LOG("[PathOCLBaseRenderThread::" << threadIndex << "] Compiling kernels ");

		if (renderEngine->writeKernelsToFile) {
			// Some debug code to write the OpenCL kernel source to a file
			const string kernelFileName = "kernel_source_device_" + ToString(threadIndex) + ".cl";
			ofstream kernelFile(kernelFileName.c_str());
			string kernelDefs = kernelsParameters;
			boost::replace_all(kernelDefs, "-D", "\n#define");
			boost::replace_all(kernelDefs, "=", " ");
			kernelFile << kernelDefs << endl << endl << kernelSource << endl;
			kernelFile.close();
		}

		bool cached;
		cl::STRING_CLASS error;
		cl::Program *program = kernelCache->Compile(oclContext, oclDevice,
				kernelsParameters, kernelSource,
				&cached, &error);

		if (!program) {
			SLG_LOG("[PathOCLBaseRenderThread::" << threadIndex << "] PathOCL kernel compilation error" << endl << error);

			throw runtime_error("PathOCLBase kernel compilation error");
		}

		if (cached) {
			SLG_LOG("[PathOCLBaseRenderThread::" << threadIndex << "] Kernels cached");
		} else {
			SLG_LOG("[PathOCLBaseRenderThread::" << threadIndex << "] Kernels not cached");
		}

		// Film clear kernel
		CompileKernel(program, &filmClearKernel, &filmClearWorkGroupSize, "Film_Clear");

		// Additional kernels
		CompileAdditionalKernels(program);

		const double tEnd = WallClockTime();
		SLG_LOG("[PathOCLBaseRenderThread::" << threadIndex << "] Kernels compilation time: " << int((tEnd - tStart) * 1000.0) << "ms");

		delete program;
	} else
		SLG_LOG("[PathOCLBaseRenderThread::" << threadIndex << "] Using cached kernels");
}

void PathOCLBaseRenderThread::InitRender() {
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
	// Light definitions
	//--------------------------------------------------------------------------

	InitLights();

	AdditionalInit();

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

	// Clear the film
	const u_int filmPixelCount = threadFilm->GetWidth() * threadFilm->GetHeight();
	oclQueue.enqueueNDRangeKernel(*filmClearKernel, cl::NullRange,
			cl::NDRange(RoundUp<u_int>(filmPixelCount, filmClearWorkGroupSize)),
			cl::NDRange(filmClearWorkGroupSize));

	oclQueue.finish();

	// Reset statistics in order to be more accurate
	intersectionDevice->ResetPerformaceStats();
}

void PathOCLBaseRenderThread::SetKernelArgs() {
	// Set OpenCL kernel arguments

	{
		// OpenCL kernel setArg() is the only no thread safe function in OpenCL 1.1 so
		// I need to use a mutex here
		boost::unique_lock<boost::mutex> lock(renderEngine->setKernelArgsMutex);

		//--------------------------------------------------------------------------
		// initFilmKernel
		//--------------------------------------------------------------------------

		SetFilmKernelArgs(*filmClearKernel, 0);
	}

	SetAdditionalKernelArgs();
}

void PathOCLBaseRenderThread::Start() {
	started = true;

	InitRender();
	StartRenderThread();
}

void PathOCLBaseRenderThread::Interrupt() {
	if (renderThread)
		renderThread->interrupt();
}

void PathOCLBaseRenderThread::Stop() {
	StopRenderThread();

	TransferFilm(intersectionDevice->GetOpenCLQueue());

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
	FreeOCLBuffer(&channel_RAYCOUNT_Buff);
	FreeOCLBuffer(&channel_BY_MATERIAL_ID_Buff);

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
	FreeOCLBuffer(&lightsBuff);
	FreeOCLBuffer(&envLightIndicesBuff);
	FreeOCLBuffer(&lightsDistributionBuff);
	FreeOCLBuffer(&infiniteLightDistributionsBuff);
	FreeOCLBuffer(&cameraBuff);
	FreeOCLBuffer(&triLightDefsBuff);
	FreeOCLBuffer(&meshTriLightDefsOffsetBuff);
	FreeOCLBuffer(&imageMapDescsBuff);
	for (u_int i = 0; i < imageMapsBuff.size(); ++i)
		FreeOCLBuffer(&imageMapsBuff[i]);
	imageMapsBuff.resize(0);

	started = false;

	// Film is deleted in the destructor to allow image saving after
	// the rendering is finished
}

void PathOCLBaseRenderThread::StartRenderThread() {
	// Create the thread for the rendering
	renderThread = new boost::thread(&PathOCLBaseRenderThread::RenderThreadImpl, this);
}

void PathOCLBaseRenderThread::StopRenderThread() {
	if (renderThread) {
		renderThread->interrupt();
		renderThread->join();
		delete renderThread;
		renderThread = NULL;
	}
}

void PathOCLBaseRenderThread::BeginSceneEdit() {
	StopRenderThread();
}

void PathOCLBaseRenderThread::EndSceneEdit(const EditActionList &editActions) {
	//--------------------------------------------------------------------------
	// Update OpenCL buffers
	//--------------------------------------------------------------------------

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

	if  (editActions.Has(LIGHTS_EDIT)) {
		// Update Scene Lights
		InitLights();
	}

	// A material types edit can enable/disable PARAM_HAS_PASSTHROUGH parameter
	// and change the size of the structure allocated
	if (editActions.Has(MATERIAL_TYPES_EDIT))
		AdditionalInit();

	//--------------------------------------------------------------------------
	// Recompile Kernels if required
	//--------------------------------------------------------------------------

	if (editActions.Has(MATERIAL_TYPES_EDIT))
		InitKernels();

	if (editActions.HasAnyAction()) {
		SetKernelArgs();

		//--------------------------------------------------------------------------
		// Execute initialization kernels
		//--------------------------------------------------------------------------

		cl::CommandQueue &oclQueue = intersectionDevice->GetOpenCLQueue();

		// Clear the frame buffer
		const u_int filmPixelCount = threadFilm->GetWidth() * threadFilm->GetHeight();
		oclQueue.enqueueNDRangeKernel(*filmClearKernel, cl::NullRange,
			cl::NDRange(RoundUp<u_int>(filmPixelCount, filmClearWorkGroupSize)),
			cl::NDRange(filmClearWorkGroupSize));
	}

	// Reset statistics in order to be more accurate
	intersectionDevice->ResetPerformaceStats();

	StartRenderThread();
}

bool PathOCLBaseRenderThread::HasDone() const {
	return (renderThread == NULL) || (renderThread->timed_join(boost::posix_time::seconds(0)));
}

void PathOCLBaseRenderThread::WaitForDone() const {
	if (renderThread)
		renderThread->join();
}

void PathOCLBaseRenderThread::TransferFilm(cl::CommandQueue &oclQueue) {
	// Async. transfer of the Film buffers

	for (u_int i = 0; i < channel_RADIANCE_PER_PIXEL_NORMALIZEDs_Buff.size(); ++i) {
		if (channel_RADIANCE_PER_PIXEL_NORMALIZEDs_Buff[i]) {
			oclQueue.enqueueReadBuffer(
				*(channel_RADIANCE_PER_PIXEL_NORMALIZEDs_Buff[i]),
				CL_FALSE,
				0,
				channel_RADIANCE_PER_PIXEL_NORMALIZEDs_Buff[i]->getInfo<CL_MEM_SIZE>(),
				threadFilm->channel_RADIANCE_PER_PIXEL_NORMALIZEDs[i]->GetPixels());
		}
	}

	if (channel_ALPHA_Buff) {
		oclQueue.enqueueReadBuffer(
			*channel_ALPHA_Buff,
			CL_FALSE,
			0,
			channel_ALPHA_Buff->getInfo<CL_MEM_SIZE>(),
			threadFilm->channel_ALPHA->GetPixels());
	}
	if (channel_DEPTH_Buff) {
		oclQueue.enqueueReadBuffer(
			*channel_DEPTH_Buff,
			CL_FALSE,
			0,
			channel_DEPTH_Buff->getInfo<CL_MEM_SIZE>(),
			threadFilm->channel_DEPTH->GetPixels());
	}
	if (channel_POSITION_Buff) {
		oclQueue.enqueueReadBuffer(
			*channel_POSITION_Buff,
			CL_FALSE,
			0,
			channel_POSITION_Buff->getInfo<CL_MEM_SIZE>(),
			threadFilm->channel_POSITION->GetPixels());
	}
	if (channel_GEOMETRY_NORMAL_Buff) {
		oclQueue.enqueueReadBuffer(
			*channel_GEOMETRY_NORMAL_Buff,
			CL_FALSE,
			0,
			channel_GEOMETRY_NORMAL_Buff->getInfo<CL_MEM_SIZE>(),
			threadFilm->channel_GEOMETRY_NORMAL->GetPixels());
	}
	if (channel_SHADING_NORMAL_Buff) {
		oclQueue.enqueueReadBuffer(
			*channel_SHADING_NORMAL_Buff,
			CL_FALSE,
			0,
			channel_SHADING_NORMAL_Buff->getInfo<CL_MEM_SIZE>(),
			threadFilm->channel_SHADING_NORMAL->GetPixels());
	}
	if (channel_MATERIAL_ID_Buff) {
		oclQueue.enqueueReadBuffer(
			*channel_MATERIAL_ID_Buff,
			CL_FALSE,
			0,
			channel_MATERIAL_ID_Buff->getInfo<CL_MEM_SIZE>(),
			threadFilm->channel_MATERIAL_ID->GetPixels());
	}
	if (channel_DIRECT_DIFFUSE_Buff) {
		oclQueue.enqueueReadBuffer(
			*channel_DIRECT_DIFFUSE_Buff,
			CL_FALSE,
			0,
			channel_DIRECT_DIFFUSE_Buff->getInfo<CL_MEM_SIZE>(),
			threadFilm->channel_DIRECT_DIFFUSE->GetPixels());
	}
	if (channel_MATERIAL_ID_Buff) {
		oclQueue.enqueueReadBuffer(
			*channel_MATERIAL_ID_Buff,
			CL_FALSE,
			0,
			channel_MATERIAL_ID_Buff->getInfo<CL_MEM_SIZE>(),
			threadFilm->channel_MATERIAL_ID->GetPixels());
	}
	if (channel_DIRECT_GLOSSY_Buff) {
		oclQueue.enqueueReadBuffer(
			*channel_DIRECT_GLOSSY_Buff,
			CL_FALSE,
			0,
			channel_DIRECT_GLOSSY_Buff->getInfo<CL_MEM_SIZE>(),
			threadFilm->channel_DIRECT_GLOSSY->GetPixels());
	}
	if (channel_EMISSION_Buff) {
		oclQueue.enqueueReadBuffer(
			*channel_EMISSION_Buff,
			CL_FALSE,
			0,
			channel_EMISSION_Buff->getInfo<CL_MEM_SIZE>(),
			threadFilm->channel_EMISSION->GetPixels());
	}
	if (channel_INDIRECT_DIFFUSE_Buff) {
		oclQueue.enqueueReadBuffer(
			*channel_INDIRECT_DIFFUSE_Buff,
			CL_FALSE,
			0,
			channel_INDIRECT_DIFFUSE_Buff->getInfo<CL_MEM_SIZE>(),
			threadFilm->channel_INDIRECT_DIFFUSE->GetPixels());
	}
	if (channel_INDIRECT_GLOSSY_Buff) {
		oclQueue.enqueueReadBuffer(
			*channel_INDIRECT_GLOSSY_Buff,
			CL_FALSE,
			0,
			channel_INDIRECT_GLOSSY_Buff->getInfo<CL_MEM_SIZE>(),
			threadFilm->channel_INDIRECT_GLOSSY->GetPixels());
	}
	if (channel_INDIRECT_SPECULAR_Buff) {
		oclQueue.enqueueReadBuffer(
			*channel_INDIRECT_SPECULAR_Buff,
			CL_FALSE,
			0,
			channel_INDIRECT_SPECULAR_Buff->getInfo<CL_MEM_SIZE>(),
			threadFilm->channel_INDIRECT_SPECULAR->GetPixels());
	}
	if (channel_MATERIAL_ID_MASK_Buff) {
		oclQueue.enqueueReadBuffer(
			*channel_MATERIAL_ID_MASK_Buff,
			CL_FALSE,
			0,
			channel_MATERIAL_ID_MASK_Buff->getInfo<CL_MEM_SIZE>(),
			threadFilm->channel_MATERIAL_ID_MASKs[0]->GetPixels());
	}
	if (channel_DIRECT_SHADOW_MASK_Buff) {
		oclQueue.enqueueReadBuffer(
			*channel_DIRECT_SHADOW_MASK_Buff,
			CL_FALSE,
			0,
			channel_DIRECT_SHADOW_MASK_Buff->getInfo<CL_MEM_SIZE>(),
			threadFilm->channel_DIRECT_SHADOW_MASK->GetPixels());
	}
	if (channel_INDIRECT_SHADOW_MASK_Buff) {
		oclQueue.enqueueReadBuffer(
			*channel_INDIRECT_SHADOW_MASK_Buff,
			CL_FALSE,
			0,
			channel_INDIRECT_SHADOW_MASK_Buff->getInfo<CL_MEM_SIZE>(),
			threadFilm->channel_INDIRECT_SHADOW_MASK->GetPixels());
	}
	if (channel_UV_Buff) {
		oclQueue.enqueueReadBuffer(
			*channel_UV_Buff,
			CL_FALSE,
			0,
			channel_UV_Buff->getInfo<CL_MEM_SIZE>(),
			threadFilm->channel_UV->GetPixels());
	}
	if (channel_RAYCOUNT_Buff) {
		oclQueue.enqueueReadBuffer(
			*channel_RAYCOUNT_Buff,
			CL_FALSE,
			0,
			channel_RAYCOUNT_Buff->getInfo<CL_MEM_SIZE>(),
			threadFilm->channel_RAYCOUNT->GetPixels());
	}
	if (channel_BY_MATERIAL_ID_Buff) {
		oclQueue.enqueueReadBuffer(
			*channel_BY_MATERIAL_ID_Buff,
			CL_FALSE,
			0,
			channel_BY_MATERIAL_ID_Buff->getInfo<CL_MEM_SIZE>(),
			threadFilm->channel_BY_MATERIAL_IDs[0]->GetPixels());
	}
}

#endif
