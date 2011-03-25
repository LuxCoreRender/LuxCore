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
#include <string>
#include <sstream>
#include <stdexcept>

#include <boost/thread/mutex.hpp>

#include <GL/glew.h>

#include "smalllux.h"

#if !defined(LUXRAYS_DISABLE_OPENCL)

#include "pathgpu2/pathgpu2.h"
#include "pathgpu2/kernels/kernels.h"
#include "renderconfig.h"
#include "luxrays/accelerators/mqbvhaccel.h"
#include "luxrays/accelerators/bvhaccel.h"
#include "luxrays/core/pixel/samplebuffer.h"

// TODO: add a check for PARAM_IMAGE_WIDTH * PARAM_IMAGE_HEIGHT > PARAM_TASK_COUNT

//------------------------------------------------------------------------------
// PathGPU2RenderThread
//------------------------------------------------------------------------------

PathGPU2RenderThread::PathGPU2RenderThread(const unsigned int index, const unsigned int seedBase,
		const float samplStart, OpenCLIntersectionDevice *device,
		PathGPU2RenderEngine *re) {
	intersectionDevice = device;
	samplingStart = samplStart;
	seed = seedBase;
	reportedPermissionError = false;

	renderThread = NULL;

	threadIndex = index;
	renderEngine = re;
	started = false;
	frameBuffer = NULL;

	kernelsParameters = "";
	initKernel = NULL;
	initFBKernel = NULL;
	randomSamplerKernel = NULL;
	generateRaysKernel = NULL;
	advancePathsKernel = NULL;
	collectResultsKernel = NULL;
}

PathGPU2RenderThread::~PathGPU2RenderThread() {
	if (started)
		Stop();

	delete initKernel;
	delete initFBKernel;
	delete randomSamplerKernel;
	delete generateRaysKernel;
	delete advancePathsKernel;
	delete collectResultsKernel;

	delete[] frameBuffer;
}

static void AppendMatrixDefinition(stringstream &ss, const char *paramName, const Matrix4x4 &m) {
	for (unsigned int i = 0; i < 4; ++i) {
		for (unsigned int j = 0; j < 4; ++j)
			ss << " -D " << paramName << "_" << i << j << "=" << m.m[i][j] << "f";
	}
}

void PathGPU2RenderThread::Start() {
	started = true;

	InitRender();

	// Create the thread for the rendering
	renderThread = new boost::thread(boost::bind(PathGPU2RenderThread::RenderThreadImpl, this));

	// Set renderThread priority
	bool res = SetThreadRRPriority(renderThread);
	if (res && !reportedPermissionError) {
		cerr << "[PathGPU2RenderThread::" << threadIndex << "] Failed to set ray intersection thread priority (you probably need root/administrator permission to set thread realtime priority)" << endl;
		reportedPermissionError = true;
	}
}

void PathGPU2RenderThread::InitRender() {
	const unsigned int pixelCount = renderEngine->film->GetWidth() * renderEngine->film->GetHeight();

	// Delete previous allocated frameBuffer
	delete[] frameBuffer;
	frameBuffer = new PathGPU2::Pixel[pixelCount];

	for (unsigned int i = 0; i < pixelCount; ++i) {
		frameBuffer[i].c.r = 0.f;
		frameBuffer[i].c.g = 0.f;
		frameBuffer[i].c.b = 0.f;
		frameBuffer[i].count = 0.f;
	}

	SLGScene *scene = renderEngine->scene;

	// Check the used accelerator type
	if (scene->dataSet->GetAcceleratorType() == ACCEL_MQBVH)
		throw runtime_error("ACCEL_MQBVH is not yet supported by PathGPU2RenderEngine");

	const unsigned int startLine = Clamp<unsigned int>(
			renderEngine->film->GetHeight() * samplingStart,
			0, renderEngine->film->GetHeight() - 1);

	cl::Context &oclContext = intersectionDevice->GetOpenCLContext();
	cl::Device &oclDevice = intersectionDevice->GetOpenCLDevice();

	double tStart, tEnd;

	//--------------------------------------------------------------------------
	// Allocate buffers
	//--------------------------------------------------------------------------

	const OpenCLDeviceDescription *deviceDesc = intersectionDevice->GetDeviceDesc();

	tStart = WallClockTime();

	cerr << "[PathGPU2RenderThread::" << threadIndex << "] Ray buffer size: " << (sizeof(Ray) * PATHGPU2_TASK_COUNT / 1024) << "Kbytes" << endl;
	raysBuff = new cl::Buffer(oclContext,
			CL_MEM_READ_WRITE,
			sizeof(Ray) * PATHGPU2_TASK_COUNT);
	deviceDesc->AllocMemory(raysBuff->getInfo<CL_MEM_SIZE>());

	cerr << "[PathGPU2RenderThread::" << threadIndex << "] RayHit buffer size: " << (sizeof(RayHit) * PATHGPU2_TASK_COUNT / 1024) << "Kbytes" << endl;
	hitsBuff = new cl::Buffer(oclContext,
			CL_MEM_READ_WRITE,
			sizeof(RayHit) * PATHGPU2_TASK_COUNT);
	deviceDesc->AllocMemory(hitsBuff->getInfo<CL_MEM_SIZE>());

	cerr << "[PathGPU2RenderThread::" << threadIndex << "] FrameBuffer buffer size: " << (sizeof(PathGPU2::Pixel) * renderEngine->film->GetWidth() * renderEngine->film->GetHeight() / 1024) << "Kbytes" << endl;
	frameBufferBuff = new cl::Buffer(oclContext,
			CL_MEM_READ_WRITE,
			sizeof(PathGPU2::Pixel) * renderEngine->film->GetWidth() * renderEngine->film->GetHeight());
	deviceDesc->AllocMemory(frameBufferBuff->getInfo<CL_MEM_SIZE>());

	const unsigned int trianglesCount = scene->dataSet->GetTotalTriangleCount();
	cerr << "[PathGPU2RenderThread::" << threadIndex << "] MeshIDs buffer size: " << (sizeof(unsigned int) * trianglesCount / 1024) << "Kbytes" << endl;
	meshIDBuff = new cl::Buffer(oclContext,
			CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
			sizeof(unsigned int) * trianglesCount,
			(void *)scene->dataSet->GetMeshIDTable());
	deviceDesc->AllocMemory(meshIDBuff->getInfo<CL_MEM_SIZE>());

	tEnd = WallClockTime();
	cerr << "[PathGPU2RenderThread::" << threadIndex << "] OpenCL buffer creation time: " << int((tEnd - tStart) * 1000.0) << "ms" << endl;

	//--------------------------------------------------------------------------
	// Translate material definitions
	//--------------------------------------------------------------------------

	tStart = WallClockTime();

	bool enable_MAT_MATTE = false;
	bool enable_MAT_AREALIGHT = false;
	bool enable_MAT_MIRROR = false;
	bool enable_MAT_GLASS = false;
	bool enable_MAT_MATTEMIRROR = false;
	bool enable_MAT_METAL = false;
	bool enable_MAT_MATTEMETAL = false;
	bool enable_MAT_ALLOY = false;
	bool enable_MAT_ARCHGLASS = false;
	const unsigned int materialsCount = scene->materials.size();
	PathGPU2::Material *mats = new PathGPU2::Material[materialsCount];
	for (unsigned int i = 0; i < materialsCount; ++i) {
		Material *m = scene->materials[i];
		PathGPU2::Material *gpum = &mats[i];

		switch (m->GetType()) {
			case MATTE: {
				enable_MAT_MATTE = true;
				MatteMaterial *mm = (MatteMaterial *)m;

				gpum->type = MAT_MATTE;
				gpum->param.matte.r = mm->GetKd().r;
				gpum->param.matte.g = mm->GetKd().g;
				gpum->param.matte.b = mm->GetKd().b;
				break;
			}
			case AREALIGHT: {
				enable_MAT_AREALIGHT = true;
				AreaLightMaterial *alm = (AreaLightMaterial *)m;

				gpum->type = MAT_AREALIGHT;
				gpum->param.areaLight.gain_r = alm->GetGain().r;
				gpum->param.areaLight.gain_g = alm->GetGain().g;
				gpum->param.areaLight.gain_b = alm->GetGain().b;
				break;
			}
			case MIRROR: {
				enable_MAT_MIRROR = true;
				MirrorMaterial *mm = (MirrorMaterial *)m;

				gpum->type = MAT_MIRROR;
				gpum->param.mirror.r = mm->GetKr().r;
				gpum->param.mirror.g = mm->GetKr().g;
				gpum->param.mirror.b = mm->GetKr().b;
				gpum->param.mirror.specularBounce = mm->HasSpecularBounceEnabled();
				break;
			}
			case GLASS: {
				enable_MAT_GLASS = true;
				GlassMaterial *gm = (GlassMaterial *)m;

				gpum->type = MAT_GLASS;
				gpum->param.glass.refl_r = gm->GetKrefl().r;
				gpum->param.glass.refl_g = gm->GetKrefl().g;
				gpum->param.glass.refl_b = gm->GetKrefl().b;

				gpum->param.glass.refrct_r = gm->GetKrefrct().r;
				gpum->param.glass.refrct_g = gm->GetKrefrct().g;
				gpum->param.glass.refrct_b = gm->GetKrefrct().b;

				gpum->param.glass.ousideIor = gm->GetOutsideIOR();
				gpum->param.glass.ior = gm->GetIOR();
				gpum->param.glass.R0 = gm->GetR0();
				gpum->param.glass.reflectionSpecularBounce = gm->HasReflSpecularBounceEnabled();
				gpum->param.glass.transmitionSpecularBounce = gm->HasRefrctSpecularBounceEnabled();
				break;
			}
			case MATTEMIRROR: {
				enable_MAT_MATTEMIRROR = true;
				MatteMirrorMaterial *mmm = (MatteMirrorMaterial *)m;

				gpum->type = MAT_MATTEMIRROR;
				gpum->param.matteMirror.matte.r = mmm->GetMatte().GetKd().r;
				gpum->param.matteMirror.matte.g = mmm->GetMatte().GetKd().g;
				gpum->param.matteMirror.matte.b = mmm->GetMatte().GetKd().b;

				gpum->param.matteMirror.mirror.r = mmm->GetMirror().GetKr().r;
				gpum->param.matteMirror.mirror.g = mmm->GetMirror().GetKr().g;
				gpum->param.matteMirror.mirror.b = mmm->GetMirror().GetKr().b;
				gpum->param.matteMirror.mirror.specularBounce = mmm->GetMirror().HasSpecularBounceEnabled();

				gpum->param.matteMirror.matteFilter = mmm->GetMatteFilter();
				gpum->param.matteMirror.totFilter = mmm->GetTotFilter();
				gpum->param.matteMirror.mattePdf = mmm->GetMattePdf();
				gpum->param.matteMirror.mirrorPdf = mmm->GetMirrorPdf();
				break;
			}
			case METAL: {
				enable_MAT_METAL = true;
				MetalMaterial *mm = (MetalMaterial *)m;

				gpum->type = MAT_METAL;
				gpum->param.metal.r = mm->GetKr().r;
				gpum->param.metal.g = mm->GetKr().g;
				gpum->param.metal.b = mm->GetKr().b;
				gpum->param.metal.exponent = mm->GetExp();
				gpum->param.metal.specularBounce = mm->HasSpecularBounceEnabled();
				break;
			}
			case MATTEMETAL: {
				enable_MAT_MATTEMETAL = true;
				MatteMetalMaterial *mmm = (MatteMetalMaterial *)m;

				gpum->type = MAT_MATTEMETAL;
				gpum->param.matteMetal.matte.r = mmm->GetMatte().GetKd().r;
				gpum->param.matteMetal.matte.g = mmm->GetMatte().GetKd().g;
				gpum->param.matteMetal.matte.b = mmm->GetMatte().GetKd().b;

				gpum->param.matteMetal.metal.r = mmm->GetMetal().GetKr().r;
				gpum->param.matteMetal.metal.g = mmm->GetMetal().GetKr().g;
				gpum->param.matteMetal.metal.b = mmm->GetMetal().GetKr().b;
				gpum->param.matteMetal.metal.exponent = mmm->GetMetal().GetExp();
				gpum->param.matteMetal.metal.specularBounce = mmm->GetMetal().HasSpecularBounceEnabled();

				gpum->param.matteMetal.matteFilter = mmm->GetMatteFilter();
				gpum->param.matteMetal.totFilter = mmm->GetTotFilter();
				gpum->param.matteMetal.mattePdf = mmm->GetMattePdf();
				gpum->param.matteMetal.metalPdf = mmm->GetMetalPdf();
				break;
			}
			case ALLOY: {
				enable_MAT_ALLOY = true;
				AlloyMaterial *am = (AlloyMaterial *)m;

				gpum->type = MAT_ALLOY;
				gpum->param.alloy.refl_r= am->GetKrefl().r;
				gpum->param.alloy.refl_g = am->GetKrefl().g;
				gpum->param.alloy.refl_b = am->GetKrefl().b;

				gpum->param.alloy.diff_r = am->GetKd().r;
				gpum->param.alloy.diff_g = am->GetKd().g;
				gpum->param.alloy.diff_b = am->GetKd().b;

				gpum->param.alloy.exponent = am->GetExp();
				gpum->param.alloy.R0 = am->GetR0();
				gpum->param.alloy.specularBounce = am->HasSpecularBounceEnabled();
				break;
			}
			case ARCHGLASS: {
				enable_MAT_ARCHGLASS = true;
				ArchGlassMaterial *agm = (ArchGlassMaterial *)m;

				gpum->type = MAT_ARCHGLASS;
				gpum->param.archGlass.refl_r = agm->GetKrefl().r;
				gpum->param.archGlass.refl_g = agm->GetKrefl().g;
				gpum->param.archGlass.refl_b = agm->GetKrefl().b;

				gpum->param.archGlass.refrct_r = agm->GetKrefrct().r;
				gpum->param.archGlass.refrct_g = agm->GetKrefrct().g;
				gpum->param.archGlass.refrct_b = agm->GetKrefrct().b;

				gpum->param.archGlass.transFilter = agm->GetTransFilter();
				gpum->param.archGlass.totFilter = agm->GetTotFilter();
				gpum->param.archGlass.reflPdf = agm->GetReflPdf();
				gpum->param.archGlass.transPdf = agm->GetTransPdf();
				break;
			}
			default: {
				enable_MAT_MATTE = true;
				gpum->type = MAT_MATTE;
				gpum->param.matte.r = 0.75f;
				gpum->param.matte.g = 0.75f;
				gpum->param.matte.b = 0.75f;
				break;
			}
		}
	}

	cerr << "[PathGPU2RenderThread::" << threadIndex << "] Materials buffer size: " << (sizeof(PathGPU2::Material) * materialsCount / 1024) << "Kbytes" << endl;
	materialsBuff = new cl::Buffer(oclContext,
			CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
			sizeof(PathGPU2::Material) * materialsCount,
			mats);
	deviceDesc->AllocMemory(materialsBuff->getInfo<CL_MEM_SIZE>());

	delete[] mats;

	tEnd = WallClockTime();
	cerr << "[PathGPU2RenderThread::" << threadIndex << "] Material translation time: " << int((tEnd - tStart) * 1000.0) << "ms" << endl;

	//--------------------------------------------------------------------------
	// Translate area lights
	//--------------------------------------------------------------------------

	tStart = WallClockTime();

	// Count the area lights
	unsigned int areaLightCount = 0;
	for (unsigned int i = 0; i < scene->lights.size(); ++i) {
		if (scene->lights[i]->IsAreaLight())
			++areaLightCount;
	}

	if (areaLightCount > 0) {
		PathGPU2::TriangleLight *tals = new PathGPU2::TriangleLight[areaLightCount];

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

		cerr << "[PathGPU2RenderThread::" << threadIndex << "] Triangle lights buffer size: " << (sizeof(PathGPU2::TriangleLight) * areaLightCount / 1024) << "Kbytes" << endl;
		triLightsBuff = new cl::Buffer(oclContext,
				CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
				sizeof(PathGPU2::TriangleLight) * areaLightCount,
				tals);
		deviceDesc->AllocMemory(triLightsBuff->getInfo<CL_MEM_SIZE>());

		delete[] tals;
	} else
		triLightsBuff = NULL;

	tEnd = WallClockTime();
	cerr << "[PathGPU2RenderThread::" << threadIndex << "] Area lights translation time: " << int((tEnd - tStart) * 1000.0) << "ms" << endl;

	//--------------------------------------------------------------------------
	// Translate mesh material indices
	//--------------------------------------------------------------------------

	tStart = WallClockTime();

	const unsigned int meshCount = scene->objectMaterials.size();
	unsigned int *meshMats = new unsigned int[meshCount];
	for (unsigned int i = 0; i < meshCount; ++i) {
		Material *m = scene->objectMaterials[i];

		// Look for the index
		unsigned int index = 0;
		for (unsigned int j = 0; j < materialsCount; ++j) {
			if (m == scene->materials[j]) {
				index = j;
				break;
			}
		}

		meshMats[i] = index;
	}

	cerr << "[PathGPU2RenderThread::" << threadIndex << "] Mesh material index buffer size: " << (sizeof(unsigned int) * meshCount / 1024) << "Kbytes" << endl;
	meshMatsBuff = new cl::Buffer(oclContext,
			CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
			sizeof(unsigned int) * meshCount,
			meshMats);
	deviceDesc->AllocMemory(meshMatsBuff->getInfo<CL_MEM_SIZE>());

	delete[] meshMats;

	tEnd = WallClockTime();
	cerr << "[PathGPU2RenderThread::" << threadIndex << "] Material indices translation time: " << int((tEnd - tStart) * 1000.0) << "ms" << endl;

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
		const TextureMap *texMap = infiniteLight->GetTexture()->GetTexMap();
		const unsigned int pixelCount = texMap->GetWidth() * texMap->GetHeight();

		cerr << "[PathGPU2RenderThread::" << threadIndex << "] InfiniteLight buffer size: " << (sizeof(Spectrum) * pixelCount / 1024) << "Kbytes" << endl;
		infiniteLightBuff = new cl::Buffer(oclContext,
				CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
				sizeof(Spectrum) * pixelCount,
				(void *)texMap->GetPixels());
		deviceDesc->AllocMemory(infiniteLightBuff->getInfo<CL_MEM_SIZE>());
	} else
		infiniteLightBuff = NULL;

	if (!infiniteLight && (areaLightCount == 0))
		throw runtime_error("There are no light sources supported by PathGPU2 in the scene (i.e. sun is not yet supported)");

	tEnd = WallClockTime();
	cerr << "[PathGPU2RenderThread::" << threadIndex << "] Infinitelight translation time: " << int((tEnd - tStart) * 1000.0) << "ms" << endl;

	//--------------------------------------------------------------------------
	// Translate mesh colors
	//--------------------------------------------------------------------------

	tStart = WallClockTime();

	const unsigned int colorsCount = scene->dataSet->GetTotalVertexCount();
	Spectrum *colors = new Spectrum[colorsCount];
	unsigned int cIndex = 0;
	for (unsigned int i = 0; i < scene->objects.size(); ++i) {
		ExtMesh *mesh = scene->objects[i];

		for (unsigned int j = 0; j < mesh->GetTotalVertexCount(); ++j) {
			if (mesh->HasColors())
				colors[cIndex++] = mesh->GetColor(j);
			else
				colors[cIndex++] = Spectrum(1.f, 1.f, 1.f);
		}
	}

	cerr << "[PathGPU2RenderThread::" << threadIndex << "] Colors buffer size: " << (sizeof(Spectrum) * colorsCount / 1024) << "Kbytes" << endl;
	colorsBuff = new cl::Buffer(oclContext,
			CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
			sizeof(Spectrum) * colorsCount,
			colors);
	deviceDesc->AllocMemory(colorsBuff->getInfo<CL_MEM_SIZE>());
	delete[] colors;

	//--------------------------------------------------------------------------
	// Translate mesh normals
	//--------------------------------------------------------------------------

	const unsigned int normalsCount = scene->dataSet->GetTotalVertexCount();
	Normal *normals = new Normal[normalsCount];
	unsigned int nIndex = 0;
	for (unsigned int i = 0; i < scene->objects.size(); ++i) {
		ExtMesh *mesh = scene->objects[i];

		for (unsigned int j = 0; j < mesh->GetTotalVertexCount(); ++j)
			normals[nIndex++] = mesh->GetNormal(j);
	}

	cerr << "[PathGPU2RenderThread::" << threadIndex << "] Normals buffer size: " << (sizeof(Normal) * normalsCount / 1024) << "Kbytes" << endl;
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
	switch (scene->dataSet->GetAcceleratorType()) {
		case ACCEL_BVH:
			preprocessedMesh = ((BVHAccel *)scene->dataSet->GetAccelerator())->GetPreprocessedMesh();
			break;
		case ACCEL_QBVH:
			preprocessedMesh = ((QBVHAccel *)scene->dataSet->GetAccelerator())->GetPreprocessedMesh();
			break;
		default:
			throw runtime_error("ACCEL_MQBVH is not yet supported by PathGPU2RenderEngine");
	}

	cerr << "[PathGPU2RenderThread::" << threadIndex << "] Triangles buffer size: " << (sizeof(Triangle) * trianglesCount / 1024) << "Kbytes" << endl;
	trianglesBuff = new cl::Buffer(oclContext,
			CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
			sizeof(Triangle) * trianglesCount,
			(void *)preprocessedMesh->GetTriangles());
	deviceDesc->AllocMemory(trianglesBuff->getInfo<CL_MEM_SIZE>());

	tEnd = WallClockTime();
	cerr << "[PathGPU2RenderThread::" << threadIndex << "] Mesh information translation time: " << int((tEnd - tStart) * 1000.0) << "ms" << endl;

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
		PathGPU2::TexMap *gpuTexMap = new PathGPU2::TexMap[tms.size()];

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

			cerr << "[PathGPU2RenderThread::" << threadIndex << "] TexMap buffer size: " << (sizeof(Spectrum) * totRGBTexMem / 1024) << "Kbytes" << endl;
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

			cerr << "[PathGPU2RenderThread::" << threadIndex << "] TexMap buffer size: " << (sizeof(float) * totAlphaTexMem / 1024) << "Kbytes" << endl;
			texMapAlphaBuff = new cl::Buffer(oclContext,
					CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
					sizeof(float) * totAlphaTexMem,
					alphaTexMem);
			deviceDesc->AllocMemory(texMapAlphaBuff->getInfo<CL_MEM_SIZE>());

			delete[] alphaTexMem;
		} else
			texMapAlphaBuff = NULL;

		// Translate texture map description
		for (unsigned int i = 0; i < tms.size(); ++i) {
			TextureMap *tm = tms[i];
			gpuTexMap[i].width = tm->GetWidth();
			gpuTexMap[i].height = tm->GetHeight();
		}

		cerr << "[PathGPU2RenderThread::" << threadIndex << "] TexMap indices buffer size: " << (sizeof(PathGPU2::TexMap) * tms.size() / 1024) << "Kbytes" << endl;
		texMapDescBuff = new cl::Buffer(oclContext,
				CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
				sizeof(PathGPU2::TexMap) * tms.size(),
				gpuTexMap);
		deviceDesc->AllocMemory(texMapDescBuff->getInfo<CL_MEM_SIZE>());
		delete[] gpuTexMap;

		// Translate mesh texture indices
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

		cerr << "[PathGPU2RenderThread::" << threadIndex << "] Mesh texture maps index buffer size: " << (sizeof(unsigned int) * meshCount / 1024) << "Kbytes" << endl;
		meshTexsBuff = new cl::Buffer(oclContext,
				CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
				sizeof(unsigned int) * meshCount,
				meshTexs);
		deviceDesc->AllocMemory(meshTexsBuff->getInfo<CL_MEM_SIZE>());

		delete[] meshTexs;

		// Translate vertex uvs
		const unsigned int uvsCount = scene->dataSet->GetTotalVertexCount();
		UV *uvs = new UV[uvsCount];
		unsigned int uvIndex = 0;
		for (unsigned int i = 0; i < scene->objects.size(); ++i) {
			ExtMesh *mesh = scene->objects[i];

			for (unsigned int j = 0; j < mesh->GetTotalVertexCount(); ++j) {
				if (mesh->HasUVs())
					uvs[uvIndex++] = mesh->GetUV(j);
				else
					uvs[uvIndex++] = UV(0.f, 0.f);
			}
		}

		cerr << "[PathGPU2RenderThread::" << threadIndex << "] UVs buffer size: " << (sizeof(UV) * uvsCount / 1024) << "Kbytes" << endl;
		uvsBuff = new cl::Buffer(oclContext,
				CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
				sizeof(UV) * uvsCount,
				uvs);
		deviceDesc->AllocMemory(uvsBuff->getInfo<CL_MEM_SIZE>());
		delete[] uvs;

		tEnd = WallClockTime();
		cerr << "[PathGPU2RenderThread::" << threadIndex << "] Texture maps translation time: " << int((tEnd - tStart) * 1000.0) << "ms" << endl;
	} else {
		texMapRGBBuff = NULL;
		texMapAlphaBuff = NULL;
		texMapDescBuff = NULL;
		meshTexsBuff = NULL;
		uvsBuff = NULL;
	}

	//--------------------------------------------------------------------------
	// Allocate GPU tasks buffer
	//--------------------------------------------------------------------------

	cerr << "[PathGPU2RenderThread::" << threadIndex << "] Sample count: " << PATHGPU2_SAMPLE_COUNT << endl;
	const size_t gpuTaksSizePart1 =
		// Seed size
		sizeof(PathGPU2::Seed);

	const size_t uDataSize =
		// IDX_SCREEN_X, IDX_SCREEN_Y
		sizeof(float) * 2 +
		// IDX_DOF_X, IDX_DOF_Y
		((scene->camera->lensRadius > 0.f) ? (sizeof(float) * 2) : 0) +
		(
			// IDX_TEX_ALPHA,
			((texMapAlphaBuff) ? sizeof(float) : 0) +
			// IDX_BSDF_X, IDX_BSDF_Y, IDX_BSDF_Z
			sizeof(float) * 3 +
			// IDX_DIRECTLIGHT_X, IDX_DIRECTLIGHT_Y, IDX_DIRECTLIGHT_Z, IDX_DIRECTLIGHT_W
			((areaLightCount > 0) ? (sizeof(float) * 4) : 0)
		) * renderEngine->maxPathDepth;

	const size_t sampleSize =
		// int pixelIndex;
		sizeof(unsigned int) +
		uDataSize +
		// Spectrum radiance;
		sizeof(Spectrum);

	const size_t gpuTaksSizePart2 = sampleSize * PATHGPU2_SAMPLE_COUNT + sizeof(unsigned int) * 2;

	const size_t gpuTaksSizePart3 =
		// PathState size
		((areaLightCount > 0) ? sizeof(PathGPU2::PathStateDL) : sizeof(PathGPU2::PathState));

	const size_t gpuTaksSize = gpuTaksSizePart1 + gpuTaksSizePart2 + gpuTaksSizePart3;
	cerr << "[PathGPU2RenderThread::" << threadIndex << "] Size of a GPUTask: " << gpuTaksSize <<
			" (" << gpuTaksSizePart1 << "+" << gpuTaksSizePart2 << "[" << sampleSize << "*" << PATHGPU2_SAMPLE_COUNT << "]+" << gpuTaksSizePart3 << ")bytes" << endl;
	cerr << "[PathGPU2RenderThread::" << threadIndex << "] Tasks buffer size: " << (gpuTaksSize * PATHGPU2_TASK_COUNT / 1024) << "Kbytes" << endl;
	tasksBuff = new cl::Buffer(oclContext,
			CL_MEM_READ_WRITE,
			gpuTaksSize * PATHGPU2_TASK_COUNT);
	deviceDesc->AllocMemory(tasksBuff->getInfo<CL_MEM_SIZE>());

	//--------------------------------------------------------------------------
	// Compile kernels
	//--------------------------------------------------------------------------

	// Set #define symbols
	stringstream ss;
	ss.precision(6);
	ss << scientific <<
			" -D PARAM_STARTLINE=" << startLine <<
			" -D PARAM_TASK_COUNT=" << PATHGPU2_TASK_COUNT <<
			" -D PARAM_IMAGE_WIDTH=" << renderEngine->film->GetWidth() <<
			" -D PARAM_IMAGE_HEIGHT=" << renderEngine->film->GetHeight() <<
			" -D PARAM_RAY_EPSILON=" << RAY_EPSILON << "f" <<
			" -D PARAM_CLIP_YON=" << scene->camera->GetClipYon() << "f" <<
			" -D PARAM_CLIP_HITHER=" << scene->camera->GetClipHither() << "f" <<
			" -D PARAM_SEED=" << seed <<
			" -D PARAM_MAX_PATH_DEPTH=" << renderEngine->maxPathDepth <<
			" -D PARAM_RR_DEPTH=" << renderEngine->rrDepth <<
			" -D PARAM_RR_CAP=" << renderEngine->rrImportanceCap << "f" <<
			" -D PARAM_SAMPLE_COUNT=" << PATHGPU2_SAMPLE_COUNT
			;

	if (enable_MAT_MATTE)
		ss << " -D PARAM_ENABLE_MAT_MATTE";
	if (enable_MAT_AREALIGHT)
		ss << " -D PARAM_ENABLE_MAT_AREALIGHT";
	if (enable_MAT_MIRROR)
		ss << " -D PARAM_ENABLE_MAT_MIRROR";
	if (enable_MAT_GLASS)
		ss << " -D PARAM_ENABLE_MAT_GLASS";
	if (enable_MAT_MATTEMIRROR)
		ss << " -D PARAM_ENABLE_MAT_MATTEMIRROR";
	if (enable_MAT_METAL)
		ss << " -D PARAM_ENABLE_MAT_METAL";
	if (enable_MAT_MATTEMETAL)
		ss << " -D PARAM_ENABLE_MAT_MATTEMETAL";
	if (enable_MAT_ALLOY)
		ss << " -D PARAM_ENABLE_MAT_ALLOY";
	if (enable_MAT_ARCHGLASS)
		ss << " -D PARAM_ENABLE_MAT_ARCHGLASS";
	if (scene->camera->lensRadius > 0.f) {
		ss <<
				" -D PARAM_CAMERA_HAS_DOF"
				" -D PARAM_CAMERA_LENS_RADIUS=" << scene->camera->lensRadius << "f" <<
				" -D PARAM_CAMERA_FOCAL_DISTANCE=" << scene->camera->focalDistance << "f";
	}

	if (infiniteLight) {
		ss <<
				" -D PARAM_HAVE_INFINITELIGHT" <<
				" -D PARAM_IL_GAIN_R=" << infiniteLight->GetGain().r << "f" <<
				" -D PARAM_IL_GAIN_G=" << infiniteLight->GetGain().g << "f" <<
				" -D PARAM_IL_GAIN_B=" << infiniteLight->GetGain().b << "f" <<
				" -D PARAM_IL_SHIFT_U=" << infiniteLight->GetShiftU() << "f" <<
				" -D PARAM_IL_SHIFT_V=" << infiniteLight->GetShiftV() << "f" <<
				" -D PARAM_IL_WIDTH=" << infiniteLight->GetTexture()->GetTexMap()->GetWidth() <<
				" -D PARAM_IL_HEIGHT=" << infiniteLight->GetTexture()->GetTexMap()->GetHeight()
				;
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

	if (renderEngine->dynamicCamera) {
		tStart = WallClockTime();

		// Dynamic camera mode uses a buffer to transfer camera position/orientation
		ss << " -D PARAM_CAMERA_DYNAMIC";

		cerr << "[PathGPU2RenderThread::" << threadIndex << "] Camera buffer size: " << (sizeof(float) * 4 * 4 * 2 / 1024) << "Kbytes" << endl;
		float data[4 * 4 * 2];
		memcpy(&data[0], scene->camera->GetRasterToCameraMatrix().m, 4 * 4 * sizeof(float));
		memcpy(&data[4 * 4], scene->camera->GetCameraToWorldMatrix().m, 4 * 4 * sizeof(float));

		cameraBuff = new cl::Buffer(oclContext,
				CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
				sizeof(float) * 4 * 4 * 2,
				(void *)data);
		deviceDesc->AllocMemory(cameraBuff->getInfo<CL_MEM_SIZE>());
	} else {
		cameraBuff = NULL;
		AppendMatrixDefinition(ss, "PARAM_RASTER2CAMERA", scene->camera->GetRasterToCameraMatrix());
		AppendMatrixDefinition(ss, "PARAM_CAMERA2WORLD", scene->camera->GetCameraToWorldMatrix());
	}

	// Check if I have to recompile the kernels
	string newKernelParameters = ss.str();
	if (kernelsParameters != newKernelParameters) {
		kernelsParameters = newKernelParameters;

		// Compile sources
		cl::Program::Sources source(1, std::make_pair(KernelSource_PathGPU2.c_str(), KernelSource_PathGPU2.length()));
		cl::Program program = cl::Program(oclContext, source);

		try {
			cerr << "[PathGPU2RenderThread::" << threadIndex << "] Defined symbols: " << kernelsParameters << endl;
			cerr << "[PathGPU2RenderThread::" << threadIndex << "] Compiling kernels " << endl;

			VECTOR_CLASS<cl::Device> buildDevice;
			buildDevice.push_back(oclDevice);
			program.build(buildDevice, kernelsParameters.c_str());
		} catch (cl::Error err) {
			cl::STRING_CLASS strError = program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(oclDevice);
			cerr << "[PathGPU2RenderThread::" << threadIndex << "] PathGPU2 compilation error:\n" << strError.c_str() << endl;

			throw err;
		}

		//----------------------------------------------------------------------
		// Init kernel
		//----------------------------------------------------------------------

		delete initKernel;
		cerr << "[PathGPURenderThread::" << threadIndex << "] Compiling Init Kernel" << endl;
		initKernel = new cl::Kernel(program, "Init");
		initKernel->getWorkGroupInfo<size_t>(oclDevice, CL_KERNEL_WORK_GROUP_SIZE, &initWorkGroupSize);
		cerr << "[PathGPURenderThread::" << threadIndex << "] PathGPU Init kernel work group size: " << initWorkGroupSize << endl;

		initKernel->getWorkGroupInfo<size_t>(oclDevice, CL_KERNEL_WORK_GROUP_SIZE, &initWorkGroupSize);
		cerr << "[PathGPURenderThread::" << threadIndex << "] Suggested work group size: " << initWorkGroupSize << endl;

		if (intersectionDevice->GetForceWorkGroupSize() > 0) {
			initWorkGroupSize = intersectionDevice->GetForceWorkGroupSize();
			cerr << "[PathGPURenderThread::" << threadIndex << "] Forced work group size: " << initWorkGroupSize << endl;
		}

		//--------------------------------------------------------------------------
		// InitFB kernel
		//--------------------------------------------------------------------------

		delete initFBKernel;
		initFBKernel = new cl::Kernel(program, "InitFrameBuffer");
		initFBKernel->getWorkGroupInfo<size_t>(oclDevice, CL_KERNEL_WORK_GROUP_SIZE, &initFBWorkGroupSize);
		cerr << "[PathGPU2RenderThread::" << threadIndex << "] PathGPU2 InitFrameBuffer kernel work group size: " << initFBWorkGroupSize << endl;

		initFBKernel->getWorkGroupInfo<size_t>(oclDevice, CL_KERNEL_WORK_GROUP_SIZE, &initFBWorkGroupSize);
		cerr << "[PathGPU2RenderThread::" << threadIndex << "] Suggested work group size: " << initFBWorkGroupSize << endl;

		if (intersectionDevice->GetForceWorkGroupSize() > 0) {
			initFBWorkGroupSize = intersectionDevice->GetForceWorkGroupSize();
			cerr << "[PathGPU2RenderThread::" << threadIndex << "] Forced work group size: " << initFBWorkGroupSize << endl;
		}

		//----------------------------------------------------------------------
		// RandomSampler kernel
		//----------------------------------------------------------------------

		delete randomSamplerKernel;
		cerr << "[PathGPURenderThread::" << threadIndex << "] Compiling RandomSampler Kernel" << endl;
		randomSamplerKernel = new cl::Kernel(program, "RandomSampler");
		randomSamplerKernel->getWorkGroupInfo<size_t>(oclDevice, CL_KERNEL_WORK_GROUP_SIZE, &randomSamplerWorkGroupSize);
		cerr << "[PathGPURenderThread::" << threadIndex << "] PathGPU RandomSampler kernel work group size: " << randomSamplerWorkGroupSize << endl;

		randomSamplerKernel->getWorkGroupInfo<size_t>(oclDevice, CL_KERNEL_WORK_GROUP_SIZE, &randomSamplerWorkGroupSize);
		cerr << "[PathGPURenderThread::" << threadIndex << "] Suggested work group size: " << randomSamplerWorkGroupSize << endl;

		if (intersectionDevice->GetForceWorkGroupSize() > 0) {
			randomSamplerWorkGroupSize = intersectionDevice->GetForceWorkGroupSize();
			cerr << "[PathGPURenderThread::" << threadIndex << "] Forced work group size: " << randomSamplerWorkGroupSize << endl;
		}

		//----------------------------------------------------------------------
		// GenerateRays kernel
		//----------------------------------------------------------------------

		delete generateRaysKernel;
		cerr << "[PathGPURenderThread::" << threadIndex << "] Compiling GenerateRays Kernel" << endl;
		generateRaysKernel = new cl::Kernel(program, "GenerateRays");
		generateRaysKernel->getWorkGroupInfo<size_t>(oclDevice, CL_KERNEL_WORK_GROUP_SIZE, &generateRaysWorkGroupSize);
		cerr << "[PathGPURenderThread::" << threadIndex << "] PathGPU GenerateRays kernel work group size: " << generateRaysWorkGroupSize << endl;

		generateRaysKernel->getWorkGroupInfo<size_t>(oclDevice, CL_KERNEL_WORK_GROUP_SIZE, &generateRaysWorkGroupSize);
		cerr << "[PathGPURenderThread::" << threadIndex << "] Suggested work group size: " << generateRaysWorkGroupSize << endl;

		if (intersectionDevice->GetForceWorkGroupSize() > 0) {
			generateRaysWorkGroupSize = intersectionDevice->GetForceWorkGroupSize();
			cerr << "[PathGPURenderThread::" << threadIndex << "] Forced work group size: " << generateRaysWorkGroupSize << endl;
		}

		//----------------------------------------------------------------------
		// AdvancePaths kernel
		//----------------------------------------------------------------------

		delete advancePathsKernel;
		cerr << "[PathGPURenderThread::" << threadIndex << "] Compiling AdvancePaths Kernel" << endl;
		advancePathsKernel = new cl::Kernel(program, "AdvancePaths");
		advancePathsKernel->getWorkGroupInfo<size_t>(oclDevice, CL_KERNEL_WORK_GROUP_SIZE, &advancePathsWorkGroupSize);
		cerr << "[PathGPURenderThread::" << threadIndex << "] PathGPU AdvancePaths kernel work group size: " << advancePathsWorkGroupSize << endl;

		advancePathsKernel->getWorkGroupInfo<size_t>(oclDevice, CL_KERNEL_WORK_GROUP_SIZE, &advancePathsWorkGroupSize);
		cerr << "[PathGPURenderThread::" << threadIndex << "] Suggested work group size: " << advancePathsWorkGroupSize << endl;

		if (intersectionDevice->GetForceWorkGroupSize() > 0) {
			advancePathsWorkGroupSize = intersectionDevice->GetForceWorkGroupSize();
			cerr << "[PathGPURenderThread::" << threadIndex << "] Forced work group size: " << advancePathsWorkGroupSize << endl;
		}

		//----------------------------------------------------------------------
		// CollectResults kernel
		//----------------------------------------------------------------------

		delete collectResultsKernel;
		cerr << "[PathGPURenderThread::" << threadIndex << "] Compiling CollectResults Kernel" << endl;
		collectResultsKernel = new cl::Kernel(program, "CollectResults");
		collectResultsKernel->getWorkGroupInfo<size_t>(oclDevice, CL_KERNEL_WORK_GROUP_SIZE, &collectResultWorkGroupSize);
		cerr << "[PathGPURenderThread::" << threadIndex << "] PathGPU CollectResults kernel work group size: " << collectResultWorkGroupSize << endl;

		collectResultsKernel->getWorkGroupInfo<size_t>(oclDevice, CL_KERNEL_WORK_GROUP_SIZE, &collectResultWorkGroupSize);
		cerr << "[PathGPURenderThread::" << threadIndex << "] Suggested work group size: " << collectResultWorkGroupSize << endl;

		if (intersectionDevice->GetForceWorkGroupSize() > 0) {
			collectResultWorkGroupSize = intersectionDevice->GetForceWorkGroupSize();
			cerr << "[PathGPURenderThread::" << threadIndex << "] Forced work group size: " << collectResultWorkGroupSize << endl;
		}

		//----------------------------------------------------------------------

		tEnd = WallClockTime();
		cerr  << "[PathGPU2RenderThread::" << threadIndex << "] Kernels compilation time: " << int((tEnd - tStart) * 1000.0) << "ms" << endl;
	} else
		cerr << "[PathGPU2RenderThread::" << threadIndex << "] Using cached kernels" << endl;

	//--------------------------------------------------------------------------
	// Initialize
	//--------------------------------------------------------------------------

	// Set kernel arguments

	randomSamplerKernel->setArg(0, *tasksBuff);

	unsigned int argIndex = 0;
	generateRaysKernel->setArg(argIndex++, *tasksBuff);
	generateRaysKernel->setArg(argIndex++, *raysBuff);
	generateRaysKernel->setArg(argIndex++, *hitsBuff);
	generateRaysKernel->setArg(argIndex++, *frameBufferBuff);
	generateRaysKernel->setArg(argIndex++, *materialsBuff);
	generateRaysKernel->setArg(argIndex++, *meshMatsBuff);
	generateRaysKernel->setArg(argIndex++, *meshIDBuff);
	generateRaysKernel->setArg(argIndex++, *colorsBuff);
	generateRaysKernel->setArg(argIndex++, *normalsBuff);
	generateRaysKernel->setArg(argIndex++, *trianglesBuff);
	if (cameraBuff)
		generateRaysKernel->setArg(argIndex++, *cameraBuff);
	if (infiniteLight)
		generateRaysKernel->setArg(argIndex++, *infiniteLightBuff);
	if (triLightsBuff)
		generateRaysKernel->setArg(argIndex++, *triLightsBuff);
	if (texMapRGBBuff)
		generateRaysKernel->setArg(argIndex++, *texMapRGBBuff);
	if (texMapAlphaBuff)
		generateRaysKernel->setArg(argIndex++, *texMapAlphaBuff);
	if (texMapRGBBuff || texMapAlphaBuff) {
		generateRaysKernel->setArg(argIndex++, *texMapDescBuff);
		generateRaysKernel->setArg(argIndex++, *meshTexsBuff);
		generateRaysKernel->setArg(argIndex++, *uvsBuff);
	}

	argIndex = 0;
	advancePathsKernel->setArg(argIndex++, *tasksBuff);
	advancePathsKernel->setArg(argIndex++, *raysBuff);
	advancePathsKernel->setArg(argIndex++, *hitsBuff);
	advancePathsKernel->setArg(argIndex++, *frameBufferBuff);
	advancePathsKernel->setArg(argIndex++, *materialsBuff);
	advancePathsKernel->setArg(argIndex++, *meshMatsBuff);
	advancePathsKernel->setArg(argIndex++, *meshIDBuff);
	advancePathsKernel->setArg(argIndex++, *colorsBuff);
	advancePathsKernel->setArg(argIndex++, *normalsBuff);
	advancePathsKernel->setArg(argIndex++, *trianglesBuff);
	if (cameraBuff)
		advancePathsKernel->setArg(argIndex++, *cameraBuff);
	if (infiniteLight)
		advancePathsKernel->setArg(argIndex++, *infiniteLightBuff);
	if (triLightsBuff)
		advancePathsKernel->setArg(argIndex++, *triLightsBuff);
	if (texMapRGBBuff)
		advancePathsKernel->setArg(argIndex++, *texMapRGBBuff);
	if (texMapAlphaBuff)
		advancePathsKernel->setArg(argIndex++, *texMapAlphaBuff);
	if (texMapRGBBuff || texMapAlphaBuff) {
		advancePathsKernel->setArg(argIndex++, *texMapDescBuff);
		advancePathsKernel->setArg(argIndex++, *meshTexsBuff);
		advancePathsKernel->setArg(argIndex++, *uvsBuff);
	}

	collectResultsKernel->setArg(0, *tasksBuff);
	collectResultsKernel->setArg(1, *frameBufferBuff);

	cl::CommandQueue &oclQueue = intersectionDevice->GetOpenCLQueue();

	// Clear the frame buffer
	initFBKernel->setArg(0, *frameBufferBuff);
	oclQueue.enqueueNDRangeKernel(*initFBKernel, cl::NullRange,
			cl::NDRange(RoundUp<unsigned int>(
				renderEngine->film->GetWidth() * renderEngine->film->GetHeight(), initFBWorkGroupSize)),
			cl::NDRange(initFBWorkGroupSize));

	// Initialize the tasks buffer
	initKernel->setArg(0, *tasksBuff);
	oclQueue.enqueueNDRangeKernel(*initKernel, cl::NullRange,
			cl::NDRange(PATHGPU2_TASK_COUNT), cl::NDRange(initWorkGroupSize));

	// Reset statistics to be more accurate
	intersectionDevice->ResetPerformaceStats();
}

void PathGPU2RenderThread::Interrupt() {
	if (renderThread)
		renderThread->interrupt();
}

void PathGPU2RenderThread::Stop() {
	if (renderThread) {
		renderThread->interrupt();
		renderThread->join();
		delete renderThread;
		renderThread = NULL;
	}

	// Transfer of the frame buffer
	cl::CommandQueue &oclQueue = intersectionDevice->GetOpenCLQueue();
	const unsigned int pixelCount = renderEngine->film->GetWidth() * renderEngine->film->GetHeight();
	oclQueue.enqueueReadBuffer(
		*frameBufferBuff,
		CL_TRUE,
		0,
		sizeof(PathGPU2::Pixel) * pixelCount,
		frameBuffer);

	const OpenCLDeviceDescription *deviceDesc = intersectionDevice->GetDeviceDesc();
	deviceDesc->FreeMemory(raysBuff->getInfo<CL_MEM_SIZE>());
	delete raysBuff;
	deviceDesc->FreeMemory(hitsBuff->getInfo<CL_MEM_SIZE>());
	delete hitsBuff;
	deviceDesc->FreeMemory(tasksBuff->getInfo<CL_MEM_SIZE>());
	delete tasksBuff;
	deviceDesc->FreeMemory(frameBufferBuff->getInfo<CL_MEM_SIZE>());
	delete frameBufferBuff;
	deviceDesc->FreeMemory(materialsBuff->getInfo<CL_MEM_SIZE>());
	delete materialsBuff;
	deviceDesc->FreeMemory(meshIDBuff->getInfo<CL_MEM_SIZE>());
	delete meshIDBuff;
	deviceDesc->FreeMemory(meshMatsBuff->getInfo<CL_MEM_SIZE>());
	delete meshMatsBuff;
	deviceDesc->FreeMemory(colorsBuff->getInfo<CL_MEM_SIZE>());
	delete colorsBuff;
	deviceDesc->FreeMemory(normalsBuff->getInfo<CL_MEM_SIZE>());
	delete normalsBuff;
	deviceDesc->FreeMemory(trianglesBuff->getInfo<CL_MEM_SIZE>());
	delete trianglesBuff;
	if (infiniteLightBuff) {
		deviceDesc->FreeMemory(infiniteLightBuff->getInfo<CL_MEM_SIZE>());
		delete infiniteLightBuff;
	}
	if (cameraBuff) {
		deviceDesc->FreeMemory(cameraBuff->getInfo<CL_MEM_SIZE>());
		delete cameraBuff;
	}
	if (triLightsBuff) {
		deviceDesc->FreeMemory(triLightsBuff->getInfo<CL_MEM_SIZE>());
		delete triLightsBuff;
	}
	if (texMapRGBBuff) {
		deviceDesc->FreeMemory(texMapRGBBuff->getInfo<CL_MEM_SIZE>());
		delete texMapRGBBuff;
	}
	if (texMapAlphaBuff) {
		deviceDesc->FreeMemory(texMapAlphaBuff->getInfo<CL_MEM_SIZE>());
		delete texMapAlphaBuff;
	}
	if (texMapRGBBuff || texMapAlphaBuff) {
		deviceDesc->FreeMemory(texMapDescBuff->getInfo<CL_MEM_SIZE>());
		delete texMapDescBuff;
		deviceDesc->FreeMemory(meshTexsBuff->getInfo<CL_MEM_SIZE>());
		delete meshTexsBuff;
		deviceDesc->FreeMemory(uvsBuff->getInfo<CL_MEM_SIZE>());
		delete uvsBuff;
	}

	started = false;

	// frameBuffer is delete on the destructor to allow image saving after
	// the rendering is finished
}

void PathGPU2RenderThread::RenderThreadImpl(PathGPU2RenderThread *renderThread) {
	cerr << "[PathGPU2RenderThread::" << renderThread->threadIndex << "] Rendering thread started" << endl;

	cl::CommandQueue &oclQueue = renderThread->intersectionDevice->GetOpenCLQueue();
	const unsigned int pixelCount = renderThread->renderEngine->film->GetWidth() * renderThread->renderEngine->film->GetHeight();

	try {
		double startTime = WallClockTime();
		while (!boost::this_thread::interruption_requested()) {
			//if(renderThread->threadIndex == 0)
			//	cerr<< "[DEBUG] =================================" << endl;

			// Async. transfer of the frame buffer

			oclQueue.enqueueReadBuffer(
				*(renderThread->frameBufferBuff),
				CL_FALSE,
				0,
				sizeof(PathGPU2::Pixel) * pixelCount,
				renderThread->frameBuffer);

			for (;;) {
				cl::Event event;

				for (unsigned int i = 0; i < 4; ++i) {
					// Generate the samples
					if (i == 0)
						oclQueue.enqueueNDRangeKernel(*(renderThread->randomSamplerKernel), cl::NullRange,
								cl::NDRange(PATHGPU2_TASK_COUNT), cl::NDRange(renderThread->randomSamplerWorkGroupSize),
								NULL, &event);
					else
						oclQueue.enqueueNDRangeKernel(*(renderThread->randomSamplerKernel), cl::NullRange,
								cl::NDRange(PATHGPU2_TASK_COUNT), cl::NDRange(renderThread->randomSamplerWorkGroupSize));

					// Render samples
					for (unsigned int k = 0; k < PATHGPU2_SAMPLE_COUNT; ++k) {
						oclQueue.enqueueNDRangeKernel(*(renderThread->generateRaysKernel), cl::NullRange,
								cl::NDRange(PATHGPU2_TASK_COUNT), cl::NDRange(renderThread->generateRaysWorkGroupSize));

						// Trace rays
						renderThread->intersectionDevice->EnqueueTraceRayBuffer(*(renderThread->raysBuff),
								*(renderThread->hitsBuff), PATHGPU2_TASK_COUNT);

						oclQueue.enqueueNDRangeKernel(*(renderThread->advancePathsKernel), cl::NullRange,
								cl::NDRange(PATHGPU2_TASK_COUNT), cl::NDRange(renderThread->advancePathsWorkGroupSize));
					}

					oclQueue.enqueueNDRangeKernel(*(renderThread->collectResultsKernel), cl::NullRange,
							cl::NDRange(PATHGPU2_TASK_COUNT), cl::NDRange(renderThread->collectResultWorkGroupSize));
				}
				oclQueue.flush();

				event.wait();
				const double elapsedTime = WallClockTime() - startTime;

				//if(renderThread->threadIndex == 0)
				//	cerr<< "[DEBUG] Elapsed time: " << elapsedTime * 1000.0 << "ms" << endl;

				if ((elapsedTime > renderThread->renderEngine->screenRefreshInterval) ||
						boost::this_thread::interruption_requested())
					break;
			}

			startTime = WallClockTime();
		}

		cerr << "[PathGPU2RenderThread::" << renderThread->threadIndex << "] Rendering thread halted" << endl;
	} catch (boost::thread_interrupted) {
		cerr << "[PathGPU2RenderThread::" << renderThread->threadIndex << "] Rendering thread halted" << endl;
	} catch (cl::Error err) {
		cerr << "[PathGPU2RenderThread::" << renderThread->threadIndex << "] Rendering thread ERROR: " << err.what() << "(" << err.err() << ")" << endl;
	}

	oclQueue.enqueueReadBuffer(
			*(renderThread->frameBufferBuff),
			CL_TRUE,
			0,
			sizeof(PathGPU2::Pixel) * pixelCount,
			renderThread->frameBuffer);
}

//------------------------------------------------------------------------------
// PathGPU2RenderEngine
//------------------------------------------------------------------------------

PathGPU2RenderEngine::PathGPU2RenderEngine(SLGScene *scn, Film *flm, boost::mutex *filmMutex,
		vector<IntersectionDevice *> intersectionDevices, const Properties &cfg) :
		RenderEngine(scn, flm, filmMutex) {
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

	cerr << "Found "<< oclIntersectionDevices.size() << " OpenCL intersection devices for PathGPU2 render engine" << endl;

	if (oclIntersectionDevices.size() < 1)
		throw runtime_error("Unable to find an OpenCL intersection device for PathGPU2 render engine");

	// Check if I have to enable OpenCL interoperability and if I can
	bool lowLatency = (cfg.GetInt("opencl.latency.mode", 1) != 0);
	dynamicCamera = (cfg.GetInt("pathgpu.dynamiccamera.enable", lowLatency ? 1 : 0) != 0);
	screenRefreshInterval = cfg.GetInt("screen.refresh.interval", lowLatency ? 100 : 2000) / 1000.0;

	if (lowLatency)
		dynamicCamera = true;

	const unsigned int seedBase = (unsigned int)(WallClockTime() / 1000.0);

	// Create and start render threads
	const size_t renderThreadCount = oclIntersectionDevices.size();
	cerr << "Starting "<< renderThreadCount << " PathGPU2 render threads" << endl;
	for (size_t i = 0; i < renderThreadCount; ++i) {
		PathGPU2RenderThread *t = new PathGPU2RenderThread(
				i, seedBase + i * PATHGPU2_TASK_COUNT, i / (float)renderThreadCount,
				oclIntersectionDevices[i], this);
		renderThreads.push_back(t);
	}
}

PathGPU2RenderEngine::~PathGPU2RenderEngine() {
	if (started) {
		Interrupt();
		Stop();
	}

	for (size_t i = 0; i < renderThreads.size(); ++i)
		delete renderThreads[i];

	film->FreeSampleBuffer(sampleBuffer);
}

void PathGPU2RenderEngine::Start() {
	RenderEngine::Start();

	samplesCount = 0;
	elapsedTime = 0.0f;

	for (size_t i = 0; i < renderThreads.size(); ++i)
		renderThreads[i]->Start();

	startTime = WallClockTime();
}

void PathGPU2RenderEngine::Interrupt() {
	for (size_t i = 0; i < renderThreads.size(); ++i)
		renderThreads[i]->Interrupt();
}

void PathGPU2RenderEngine::Stop() {
	RenderEngine::Stop();

	for (size_t i = 0; i < renderThreads.size(); ++i)
		renderThreads[i]->Stop();

	UpdateFilm();
}

unsigned int PathGPU2RenderEngine::GetThreadCount() const {
	return renderThreads.size();
}

void PathGPU2RenderEngine::UpdateFilm() {
	boost::unique_lock<boost::mutex> lock(*filmMutex);

	elapsedTime = WallClockTime() - startTime;
	const unsigned int imgWidth = film->GetWidth();
	const unsigned int pixelCount = imgWidth * film->GetHeight();

	film->Reset();

	unsigned long long totalCount = 0;
	for (unsigned int p = 0; p < pixelCount; ++p) {
		Spectrum c;
		float count = 0;
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

unsigned int PathGPU2RenderEngine::GetPass() const {
	return samplesCount / (film->GetWidth() * film->GetHeight());
}

#endif
