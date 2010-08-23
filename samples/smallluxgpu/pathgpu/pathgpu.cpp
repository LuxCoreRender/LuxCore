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
#include <string>
#include <sstream>
#include <stdexcept>

#include <GL/glew.h>

#include "smalllux.h"

#if !defined(LUXRAYS_DISABLE_OPENCL)

#include "pathgpu/pathgpu.h"
#include "pathgpu/kernels/kernels.h"
#include "renderconfig.h"
#include "displayfunc.h"
#include "luxrays/accelerators/mqbvhaccel.h"
#include "luxrays/accelerators/bvhaccel.h"
#include "luxrays/core/pixel/samplebuffer.h"


//------------------------------------------------------------------------------
// PathGPURenderThread
//------------------------------------------------------------------------------

PathGPURenderThread::PathGPURenderThread(const unsigned int index, const unsigned int seedBase,
		const float samplStart, OpenCLIntersectionDevice *device,
		PathGPURenderEngine *re) {
	intersectionDevice = device;
	samplingStart = samplStart;
	seed = seedBase + index;
	reportedPermissionError = false;

	renderThread = NULL;

	initKernel = NULL;
	advancePathKernel = NULL;

	pbo = 0;

	threadIndex = index;
	renderEngine = re;
	started = false;
	frameBuffer = NULL;
}

PathGPURenderThread::~PathGPURenderThread() {
	if (started)
		Stop();

	delete[] frameBuffer;
}

static void AppendMatrixDefinition(stringstream &ss, const char *paramName, const Matrix4x4 &m) {
	for (unsigned int i = 0; i < 4; ++i) {
		for (unsigned int j = 0; j < 4; ++j)
			ss << " -D " << paramName << "_" << i << j << "=" << m.m[i][j] << "f";
	}
}

void PathGPURenderThread::Start() {
	started = true;
	const unsigned int pixelCount = renderEngine->film->GetWidth() * renderEngine->film->GetHeight();

	// Delete previous allocated frameBuffer
	delete[] frameBuffer;
	frameBuffer = new PathGPU::Pixel[pixelCount];

	for (unsigned int i = 0; i < pixelCount; ++i) {
		frameBuffer[i].c.r = 0.f;
		frameBuffer[i].c.g = 0.f;
		frameBuffer[i].c.b = 0.f;
		frameBuffer[i].count = 0;
	}

	SLGScene *scene = renderEngine->scene;

	// Check the used accelerator type
	if (scene->dataSet->GetAcceleratorType() == ACCEL_MQBVH)
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

	cerr << "[PathGPURenderThread::" << threadIndex << "] Ray buffer size: " << (sizeof(Ray) * PATHGPU_PATH_COUNT / 1024) << "Kbytes" << endl;
	raysBuff = new cl::Buffer(oclContext,
			CL_MEM_READ_WRITE,
			sizeof(Ray) * PATHGPU_PATH_COUNT);
	deviceDesc->AllocMemory(raysBuff->getInfo<CL_MEM_SIZE>());

	cerr << "[PathGPURenderThread::" << threadIndex << "] RayHit buffer size: " << (sizeof(RayHit) * PATHGPU_PATH_COUNT / 1024) << "Kbytes" << endl;
	hitsBuff = new cl::Buffer(oclContext,
			CL_MEM_READ_WRITE,
			sizeof(RayHit) * PATHGPU_PATH_COUNT);
	deviceDesc->AllocMemory(hitsBuff->getInfo<CL_MEM_SIZE>());

	cerr << "[PathGPURenderThread::" << threadIndex << "] FrameBuffer buffer size: " << (sizeof(PathGPU::Pixel) * renderEngine->film->GetWidth() * renderEngine->film->GetHeight() / 1024) << "Kbytes" << endl;
	frameBufferBuff = new cl::Buffer(oclContext,
			CL_MEM_READ_WRITE,
			sizeof(PathGPU::Pixel) * renderEngine->film->GetWidth() * renderEngine->film->GetHeight());
	deviceDesc->AllocMemory(frameBufferBuff->getInfo<CL_MEM_SIZE>());

	const unsigned int trianglesCount = scene->dataSet->GetTotalTriangleCount();
	cerr << "[PathGPURenderThread::" << threadIndex << "] MeshIDs buffer size: " << (sizeof(unsigned int) * trianglesCount / 1024) << "Kbytes" << endl;
	meshIDBuff = new cl::Buffer(oclContext,
			CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
			sizeof(unsigned int) * trianglesCount,
			(void *)scene->dataSet->GetMeshIDTable());
	deviceDesc->AllocMemory(meshIDBuff->getInfo<CL_MEM_SIZE>());

	cerr << "[PathGPURenderThread::" << threadIndex << "] TriangleIDs buffer size: " << (sizeof(unsigned int) * trianglesCount / 1024) << "Kbytes" << endl;
	triIDBuff = new cl::Buffer(oclContext,
			CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
			sizeof(unsigned int) * trianglesCount,
			(void *)scene->dataSet->GetMeshTriangleIDTable());
	deviceDesc->AllocMemory(triIDBuff->getInfo<CL_MEM_SIZE>());

	//--------------------------------------------------------------------------
	// Translate material definitions
	//--------------------------------------------------------------------------

	const unsigned int materialsCount = scene->materials.size();
	PathGPU::Material *mats = new PathGPU::Material[materialsCount];
	for (unsigned int i = 0; i < materialsCount; ++i) {
		Material *m = scene->materials[i];
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
			case MIRROR: {
				MirrorMaterial *mm = (MirrorMaterial *)m;

				gpum->type = MAT_MIRROR;
				gpum->mat.mirror.r = mm->GetKr().r;
				gpum->mat.mirror.g = mm->GetKr().g;
				gpum->mat.mirror.b = mm->GetKr().b;
				gpum->mat.mirror.specularBounce = mm->HasSpecularBounceEnabled();
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

	cerr << "[PathGPURenderThread::" << threadIndex << "] Materials buffer size: " << (sizeof(PathGPU::Material) * materialsCount / 1024) << "Kbytes" << endl;
	materialsBuff = new cl::Buffer(oclContext,
			CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
			sizeof(PathGPU::Material) * materialsCount,
			mats);
	deviceDesc->AllocMemory(materialsBuff->getInfo<CL_MEM_SIZE>());

	delete[] mats;

	//--------------------------------------------------------------------------
	// Translate area lights
	//--------------------------------------------------------------------------

	const unsigned int areaLightCount = scene->GetLightCount(true);
	if (areaLightCount > 0) {
		PathGPU::TriangleLight *tals = new PathGPU::TriangleLight[areaLightCount];

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

		cerr << "[PathGPURenderThread::" << threadIndex << "] Triangle lights buffer size: " << (sizeof(PathGPU::TriangleLight) * areaLightCount / 1024) << "Kbytes" << endl;
		triLightsBuff = new cl::Buffer(oclContext,
				CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
				sizeof(PathGPU::TriangleLight) * areaLightCount,
				tals);
		deviceDesc->AllocMemory(triLightsBuff->getInfo<CL_MEM_SIZE>());

		delete[] tals;
	} else
		triLightsBuff = NULL;

	//--------------------------------------------------------------------------
	// Allocate path buffer
	//--------------------------------------------------------------------------

	const size_t PathGPUSize = (renderEngine->hasOpenGLInterop) ?
		(triLightsBuff ? sizeof(PathGPU::PathLowLatencyDL) : sizeof(PathGPU::PathLowLatencyDL)) :
		(triLightsBuff ? sizeof(PathGPU::PathDL) : sizeof(PathGPU::PathDL));
	cerr << "[PathGPURenderThread::" << threadIndex << "] Paths buffer size: " << (PathGPUSize * PATHGPU_PATH_COUNT / 1024) << "Kbytes" << endl;
	pathsBuff = new cl::Buffer(oclContext,
			CL_MEM_READ_WRITE,
			PathGPUSize * PATHGPU_PATH_COUNT);
	deviceDesc->AllocMemory(pathsBuff->getInfo<CL_MEM_SIZE>());

	//--------------------------------------------------------------------------
	// Translate mesh material indices
	//--------------------------------------------------------------------------

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

	cerr << "[PathGPURenderThread::" << threadIndex << "] Mesh material index buffer size: " << (sizeof(unsigned int) * meshCount / 1024) << "Kbytes" << endl;
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
	if (scene->infiniteLight)
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

		cerr << "[PathGPURenderThread::" << threadIndex << "] InfiniteLight buffer size: " << (sizeof(Spectrum) * pixelCount / 1024) << "Kbytes" << endl;
		infiniteLightBuff = new cl::Buffer(oclContext,
				CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
				sizeof(Spectrum) * pixelCount,
				(void *)texMap->GetPixels());
		deviceDesc->AllocMemory(infiniteLightBuff->getInfo<CL_MEM_SIZE>());
	} else
		infiniteLightBuff = NULL;

	//--------------------------------------------------------------------------
	// Translate mesh colors
	//--------------------------------------------------------------------------

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

	cerr << "[PathGPURenderThread::" << threadIndex << "] Colors buffer size: " << (sizeof(Spectrum) * colorsCount / 1024) << "Kbytes" << endl;
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

	cerr << "[PathGPURenderThread::" << threadIndex << "] Normals buffer size: " << (sizeof(Normal) * normalsCount / 1024) << "Kbytes" << endl;
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
			throw runtime_error("ACCEL_MQBVH is not yet supported by PathGPURenderEngine");
	}

	cerr << "[PathGPURenderThread::" << threadIndex << "] Triangles buffer size: " << (sizeof(Triangle) * trianglesCount / 1024) << "Kbytes" << endl;
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
	ss.precision(6);
	ss << scientific <<
			" -D PARAM_STARTLINE=" << startLine <<
			" -D PARAM_PATH_COUNT=" << PATHGPU_PATH_COUNT <<
			" -D PARAM_IMAGE_WIDTH=" << renderEngine->film->GetWidth() <<
			" -D PARAM_IMAGE_HEIGHT=" << renderEngine->film->GetHeight() <<
			" -D PARAM_RAY_EPSILON=" << RAY_EPSILON << "f" <<
			" -D PARAM_CLIP_YON=" << scene->camera->GetClipYon() << "f" <<
			" -D PARAM_CLIP_HITHER=" << scene->camera->GetClipHither() << "f" <<
			" -D PARAM_SEED=" << seed <<
			" -D PARAM_MAX_PATH_DEPTH=" << renderEngine->maxPathDepth <<
			" -D PARAM_RR_DEPTH=" << renderEngine->rrDepth <<
			" -D PARAM_RR_CAP=" << renderEngine->rrImportanceCap << "f" <<
			" -D PARAM_SAMPLE_PER_PIXEL=" << renderEngine->samplePerPixel
			;

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

	if (renderEngine->hasOpenGLInterop) {
		// Low Latency mode uses a buffer to transfer camera position/orientation
		ss << " -D PARAM_LOWLATENCY";

		cerr << "[PathGPURenderThread::" << threadIndex << "] Camera buffer size: " << (sizeof(float) * 4 * 4 * 2 / 1024) << "Kbytes" << endl;
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

	cerr << "[PathGPURenderThread::" << threadIndex << "] Defined symbols: " << ss.str() << endl;

	try {
		VECTOR_CLASS<cl::Device> buildDevice;
		buildDevice.push_back(oclDevice);
		program.build(buildDevice, ss.str().c_str());
	} catch (cl::Error err) {
		cl::STRING_CLASS strError = program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(oclDevice);
		cerr << "[PathGPURenderThread::" << threadIndex << "] PathGPU compilation error:\n" << strError.c_str() << endl;

		throw err;
	}

	//--------------------------------------------------------------------------
	// Init kernel
	//--------------------------------------------------------------------------

	initKernel = new cl::Kernel(program, "Init");
	initKernel->getWorkGroupInfo<size_t>(oclDevice, CL_KERNEL_WORK_GROUP_SIZE, &initWorkGroupSize);
	cerr << "[PathGPURenderThread::" << threadIndex << "] PathGPU Init kernel work group size: " << initWorkGroupSize << endl;
	cl_ulong memSize;
	initKernel->getWorkGroupInfo<cl_ulong>(oclDevice, CL_KERNEL_LOCAL_MEM_SIZE, &memSize);
	cerr << "[PathGPURenderThread::" << threadIndex << "] PathGPU Init kernel memory footprint: " << memSize << endl;

	initKernel->getWorkGroupInfo<size_t>(oclDevice, CL_KERNEL_WORK_GROUP_SIZE, &initWorkGroupSize);
	cerr << "[PathGPURenderThread::" << threadIndex << "] Suggested work group size: " << initWorkGroupSize << endl;

	if (intersectionDevice->GetForceWorkGroupSize() > 0) {
		initWorkGroupSize = intersectionDevice->GetForceWorkGroupSize();
		cerr << "[PathGPURenderThread::" << threadIndex << "] Forced work group size: " << initWorkGroupSize << endl;
	}

	//--------------------------------------------------------------------------
	// InitFB kernel
	//--------------------------------------------------------------------------

	initFBKernel = new cl::Kernel(program, "InitFrameBuffer");
	initFBKernel->getWorkGroupInfo<size_t>(oclDevice, CL_KERNEL_WORK_GROUP_SIZE, &initFBWorkGroupSize);
	cerr << "[PathGPURenderThread::" << threadIndex << "] PathGPU InitFrameBuffer kernel work group size: " << initFBWorkGroupSize << endl;
	initFBKernel->getWorkGroupInfo<cl_ulong>(oclDevice, CL_KERNEL_LOCAL_MEM_SIZE, &memSize);
	cerr << "[PathGPURenderThread::" << threadIndex << "] PathGPU InitFrameBuffer kernel memory footprint: " << memSize << endl;

	initFBKernel->getWorkGroupInfo<size_t>(oclDevice, CL_KERNEL_WORK_GROUP_SIZE, &initFBWorkGroupSize);
	cerr << "[PathGPURenderThread::" << threadIndex << "] Suggested work group size: " << initFBWorkGroupSize << endl;

	if (intersectionDevice->GetForceWorkGroupSize() > 0) {
		initFBWorkGroupSize = intersectionDevice->GetForceWorkGroupSize();
		cerr << "[PathGPURenderThread::" << threadIndex << "] Forced work group size: " << initFBWorkGroupSize << endl;
	}

	//--------------------------------------------------------------------------
	// UpdatePB kernel
	//--------------------------------------------------------------------------

	if (renderEngine->hasOpenGLInterop) {
		updatePBKernel = new cl::Kernel(program, "UpdatePixelBuffer");
		updatePBKernel->getWorkGroupInfo<size_t>(oclDevice, CL_KERNEL_WORK_GROUP_SIZE, &updatePBWorkGroupSize);
		cerr << "[PathGPURenderThread::" << threadIndex << "] PathGPU UpdatePixelBuffer kernel work group size: " << updatePBWorkGroupSize << endl;
		updatePBKernel->getWorkGroupInfo<cl_ulong>(oclDevice, CL_KERNEL_LOCAL_MEM_SIZE, &memSize);
		cerr << "[PathGPURenderThread::" << threadIndex << "] PathGPU UpdatePixelBuffer kernel memory footprint: " << memSize << endl;

		updatePBKernel->getWorkGroupInfo<size_t>(oclDevice, CL_KERNEL_WORK_GROUP_SIZE, &updatePBWorkGroupSize);
		cerr << "[PathGPURenderThread::" << threadIndex << "] Suggested work group size: " << updatePBWorkGroupSize << endl;

		if (intersectionDevice->GetForceWorkGroupSize() > 0) {
			updatePBWorkGroupSize = intersectionDevice->GetForceWorkGroupSize();
			cerr << "[PathGPURenderThread::" << threadIndex << "] Forced work group size: " << updatePBWorkGroupSize << endl;
		}
	} else
		updatePBKernel = NULL;

	//--------------------------------------------------------------------------
	// AdvancePaths kernel
	//--------------------------------------------------------------------------

	advancePathKernel = new cl::Kernel(program, "AdvancePaths");
	advancePathKernel->getWorkGroupInfo<size_t>(oclDevice, CL_KERNEL_WORK_GROUP_SIZE, &advancePathWorkGroupSize);
	cerr << "[PathGPURenderThread::" << threadIndex << "] PathGPU AdvancePaths kernel work group size: " << advancePathWorkGroupSize << endl;
	advancePathKernel->getWorkGroupInfo<cl_ulong>(oclDevice, CL_KERNEL_LOCAL_MEM_SIZE, &memSize);
	cerr << "[PathGPURenderThread::" << threadIndex << "] PathGPU AdvancePaths kernel memory footprint: " << memSize << endl;

	advancePathKernel->getWorkGroupInfo<size_t>(oclDevice, CL_KERNEL_WORK_GROUP_SIZE, &advancePathWorkGroupSize);
	cerr << "[PathGPURenderThread::" << threadIndex << "] Suggested work group size: " << advancePathWorkGroupSize << endl;

	if (intersectionDevice->GetForceWorkGroupSize() > 0) {
		advancePathWorkGroupSize = intersectionDevice->GetForceWorkGroupSize();
		cerr << "[PathGPURenderThread::" << threadIndex << "] Forced work group size: " << advancePathWorkGroupSize << endl;
	}

	//--------------------------------------------------------------------------
	// Initialize
	//--------------------------------------------------------------------------

	if (renderEngine->hasOpenGLInterop)
		InitPixelBuffer();

	// Clear the frame buffer
	cl::CommandQueue &oclQueue = intersectionDevice->GetOpenCLQueue();
	EnqueueInitFrameBufferKernel();

	// Initialize the path buffer
	initKernel->setArg(0, *pathsBuff);
	initKernel->setArg(1, *raysBuff);
	if (renderEngine->hasOpenGLInterop)
		initKernel->setArg(2, *cameraBuff);
	oclQueue.enqueueNDRangeKernel(*initKernel, cl::NullRange,
			cl::NDRange(PATHGPU_PATH_COUNT), cl::NDRange(initWorkGroupSize));

	unsigned int argIndex = 0;
	advancePathKernel->setArg(argIndex++, *pathsBuff);
	advancePathKernel->setArg(argIndex++, *raysBuff);
	advancePathKernel->setArg(argIndex++, *hitsBuff);
	advancePathKernel->setArg(argIndex++, *frameBufferBuff);
	advancePathKernel->setArg(argIndex++, *materialsBuff);
	advancePathKernel->setArg(argIndex++, *meshMatsBuff);
	advancePathKernel->setArg(argIndex++, *meshIDBuff);
	advancePathKernel->setArg(argIndex++, *triIDBuff);
	advancePathKernel->setArg(argIndex++, *colorsBuff);
	advancePathKernel->setArg(argIndex++, *normalsBuff);
	advancePathKernel->setArg(argIndex++, *trianglesBuff);
	if (renderEngine->hasOpenGLInterop)
		advancePathKernel->setArg(argIndex++, *cameraBuff);
	if (infiniteLight)
		advancePathKernel->setArg(argIndex++, *infiniteLightBuff);
	if (triLightsBuff)
		advancePathKernel->setArg(argIndex++, *triLightsBuff);

	if (!renderEngine->hasOpenGLInterop) {
		// Create the thread for the rendering
		renderThread = new boost::thread(boost::bind(PathGPURenderThread::RenderThreadImpl, this));

		// Set renderThread priority
		bool res = SetThreadRRPriority(renderThread);
		if (res && !reportedPermissionError) {
			cerr << "[PathGPURenderThread::" << threadIndex << "] Failed to set ray intersection thread priority (you probably need root/administrator permission to set thread realtime priority)" << endl;
			reportedPermissionError = true;
		}
	}
}

void PathGPURenderThread::Interrupt() {
	if (renderThread)
		renderThread->interrupt();
}

void PathGPURenderThread::Stop() {
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
		sizeof(PathGPU::Pixel) * pixelCount,
		frameBuffer);

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

	delete initKernel;
	delete initFBKernel;
	delete updatePBKernel;
	delete advancePathKernel;

	if (pbo) {
        // Delete old buffer
        delete pboBuff;
        glDeleteBuffersARB(1, &pbo);
		pbo = 0;
    }

	started = false;

	// frameBuffer is delete on the destructor to allow image saving after
	// the rendering is finished
}

void PathGPURenderThread::InitPixelBuffer() {
    if (pbo) {
        // Delete old buffer
        delete pboBuff;
        glDeleteBuffersARB(1, &pbo);
    }

	// Create pixel buffer object for display
    glGenBuffersARB(1, &pbo);
	glBindBufferARB(GL_PIXEL_UNPACK_BUFFER_ARB, pbo);
	glBufferDataARB(GL_PIXEL_UNPACK_BUFFER_ARB, renderEngine->film->GetWidth() * renderEngine->film->GetHeight() *
			sizeof(GLubyte) * 4, 0, GL_STREAM_DRAW_ARB);
	glBindBufferARB(GL_PIXEL_UNPACK_BUFFER_ARB, 0);

	cl::Context ctx = intersectionDevice->GetDeviceDesc()->GetOCLContext();

	pboBuff = new cl::BufferGL(ctx, CL_MEM_READ_WRITE, pbo);
}

void PathGPURenderThread::UpdatePixelBuffer(const unsigned int screenRefreshInterval) {
	cl::CommandQueue &oclQueue = intersectionDevice->GetOpenCLQueue();

	const double tStart = WallClockTime();
	const double tStop = tStart + screenRefreshInterval / 1000.0;
	for (;;) {
		for (unsigned int i = 0; i < 4; ++i) {
			// Trace rays
			intersectionDevice->EnqueueTraceRayBuffer(*raysBuff, *hitsBuff, PATHGPU_PATH_COUNT);

			// Advance paths
			oclQueue.enqueueNDRangeKernel(*advancePathKernel, cl::NullRange,
				cl::NDRange(PATHGPU_PATH_COUNT), cl::NDRange(advancePathWorkGroupSize));
		}

		// Update PixelBuffer
		VECTOR_CLASS<cl::Memory> buffs;
		buffs.push_back(*pboBuff);
		oclQueue.enqueueAcquireGLObjects(&buffs);

		updatePBKernel->setArg(0, *frameBufferBuff);
		updatePBKernel->setArg(1, *pboBuff);
		oclQueue.enqueueNDRangeKernel(*updatePBKernel, cl::NullRange,
				cl::NDRange(RoundUp<unsigned int>(
					renderEngine->film->GetWidth() * renderEngine->film->GetHeight(), updatePBWorkGroupSize)),
				cl::NDRange(updatePBWorkGroupSize));

		oclQueue.enqueueReleaseGLObjects(&buffs);
		oclQueue.finish();

		if (WallClockTime() > tStop)
			break;
	}

	// Draw the image on the screen
	glBindBufferARB(GL_PIXEL_UNPACK_BUFFER_ARB, pbo);
    glDrawPixels(renderEngine->film->GetWidth(), renderEngine->film->GetHeight(), GL_RGBA, GL_UNSIGNED_BYTE, 0);
    glBindBufferARB(GL_PIXEL_UNPACK_BUFFER_ARB, 0);
}

void PathGPURenderThread::UpdateCamera() {
	cl::CommandQueue &oclQueue = intersectionDevice->GetOpenCLQueue();

	float data[4 * 4 * 2];
	memcpy(&data[0], renderEngine->scene->camera->GetRasterToCameraMatrix().m, 4 * 4 * sizeof(float));
	memcpy(&data[4 * 4], renderEngine->scene->camera->GetCameraToWorldMatrix().m, 4 * 4 * sizeof(float));

	// Clear the frame buffer
	EnqueueInitFrameBufferKernel();

	// Transfer camera data
	oclQueue.enqueueWriteBuffer(
				*cameraBuff,
				CL_TRUE,
				0,
				sizeof(float) * 4 * 4 *2,
				data);

	// Initialize the path buffer
	initKernel->setArg(0, *pathsBuff);
	initKernel->setArg(1, *raysBuff);
	if (renderEngine->hasOpenGLInterop)
		initKernel->setArg(2, *cameraBuff);
	oclQueue.enqueueNDRangeKernel(*initKernel, cl::NullRange,
			cl::NDRange(PATHGPU_PATH_COUNT), cl::NDRange(initWorkGroupSize));
}

void PathGPURenderThread::EnqueueInitFrameBufferKernel() {
	cl::CommandQueue &oclQueue = intersectionDevice->GetOpenCLQueue();

	// Clear the frame buffer
	if (renderEngine->hasOpenGLInterop) {
		VECTOR_CLASS<cl::Memory> buffs;
		buffs.push_back(*pboBuff);
		oclQueue.enqueueAcquireGLObjects(&buffs);


		initFBKernel->setArg(0, *frameBufferBuff);
		initFBKernel->setArg(1, *pboBuff);
		oclQueue.enqueueNDRangeKernel(*initFBKernel, cl::NullRange,
				cl::NDRange(RoundUp<unsigned int>(
					renderEngine->film->GetWidth() * renderEngine->film->GetHeight(), initFBWorkGroupSize)),
				cl::NDRange(initFBWorkGroupSize));

		oclQueue.enqueueReleaseGLObjects(&buffs);
	} else {
		initFBKernel->setArg(0, *frameBufferBuff);
		oclQueue.enqueueNDRangeKernel(*initFBKernel, cl::NullRange,
				cl::NDRange(RoundUp<unsigned int>(
					renderEngine->film->GetWidth() * renderEngine->film->GetHeight(), initFBWorkGroupSize)),
				cl::NDRange(initFBWorkGroupSize));
	}
}

void PathGPURenderThread::RenderThreadImpl(PathGPURenderThread *renderThread) {
	cerr << "[PathGPURenderThread::" << renderThread->threadIndex << "] Rendering thread started" << endl;

	cl::CommandQueue &oclQueue = renderThread->intersectionDevice->GetOpenCLQueue();
	const unsigned int pixelCount = renderThread->renderEngine->film->GetWidth() * renderThread->renderEngine->film->GetHeight();

	try {
		//const double refreshInterval = 1.0;
		const double refreshInterval = 3.0;

		// -refreshInterval is a trick to set the first frame buffer refresh after 1 sec
		double startTime = WallClockTime() - (refreshInterval / 2.0);
		while (!boost::this_thread::interruption_requested()) {
			//if(renderThread->threadIndex == 0)
			//	cerr<< "[DEBUG] =================================" << endl;

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

				//if(renderThread->threadIndex == 0)
				//	cerr<< "[DEBUG] =" << j << "="<< elapsedTime * 1000.0 << "ms" << endl;

				if ((elapsedTime > refreshInterval) || boost::this_thread::interruption_requested())
					break;
			}

			startTime = WallClockTime();
		}

		cerr << "[PathGPURenderThread::" << renderThread->threadIndex << "] Rendering thread halted" << endl;
	} catch (boost::thread_interrupted) {
		cerr << "[PathGPURenderThread::" << renderThread->threadIndex << "] Rendering thread halted" << endl;
	} catch (cl::Error err) {
		cerr << "[PathGPURenderThread::" << renderThread->threadIndex << "] Rendering thread ERROR: " << err.what() << "(" << err.err() << ")" << endl;
	}

	oclQueue.enqueueReadBuffer(
			*(renderThread->frameBufferBuff),
			CL_TRUE,
			0,
			sizeof(PathGPU::Pixel) * pixelCount,
			renderThread->frameBuffer);
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

	// Check if I have to enable OpenCL interoperability and if I can
	if (cfg.GetInt("opencl.latency.mode", 1)) {
		if (!oclIntersectionDevices[0]->GetDeviceDesc()->HasOCLContext() || oclIntersectionDevices[0]->GetDeviceDesc()->HasOGLInterop()) {
			oclIntersectionDevices[0]->GetDeviceDesc()->EnableOGLInterop();
			oclIntersectionDevices.resize(1);

			hasOpenGLInterop = true;
			cerr << "PathGPU low latency mode enabled" << endl;
		} else {
			hasOpenGLInterop = false;
			cerr << "It is not possible to enable PathGPU low latency mode" << endl;
			cerr << "PathGPU high bandwidth mode enabled" << endl;
		}
	} else {
		hasOpenGLInterop = false;
		cerr << "PathGPU high bandwidth mode enabled" << endl;
	}

	const unsigned int seedBase = (unsigned long)(WallClockTime() / 1000.0);

	// Create and start render threads
	const size_t renderThreadCount = oclIntersectionDevices.size();
	cerr << "Starting "<< renderThreadCount << " PathGPU render threads" << endl;
	for (size_t i = 0; i < renderThreadCount; ++i) {
		PathGPURenderThread *t = new PathGPURenderThread(
				i, seedBase, i / (float)renderThreadCount,
				oclIntersectionDevices[i], this);
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
	elapsedTime = 0.0f;

	for (size_t i = 0; i < renderThreads.size(); ++i)
		renderThreads[i]->Start();

	startTime = WallClockTime();
}

void PathGPURenderEngine::Interrupt() {
	for (size_t i = 0; i < renderThreads.size(); ++i)
		renderThreads[i]->Interrupt();
}

void PathGPURenderEngine::Stop() {
	RenderEngine::Stop();

	for (size_t i = 0; i < renderThreads.size(); ++i)
		renderThreads[i]->Stop();

	UpdateFilm();
}

unsigned int PathGPURenderEngine::GetThreadCount() const {
	return renderThreads.size();
}

void PathGPURenderEngine::UpdateFilm() {
	boost::unique_lock<boost::mutex> lock(*filmMutex);

	elapsedTime = WallClockTime() - startTime;
	const unsigned int imgWidth = film->GetWidth();
	const unsigned int pixelCount = imgWidth * film->GetHeight();

	if (hasOpenGLInterop && started) {
		// Transfer of the frame buffer
		cl::CommandQueue &oclQueue = renderThreads[0]->intersectionDevice->GetOpenCLQueue();
		oclQueue.enqueueReadBuffer(
			*(renderThreads[0]->frameBufferBuff),
			CL_TRUE,
			0,
			sizeof(PathGPU::Pixel) * pixelCount,
			renderThreads[0]->frameBuffer);
	}

	film->Reset();

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
