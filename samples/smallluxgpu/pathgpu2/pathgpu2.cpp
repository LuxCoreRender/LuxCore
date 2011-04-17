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

#include <GL/glew.h>

#include "smalllux.h"

#if !defined(LUXRAYS_DISABLE_OPENCL)

#include "pathgpu2/pathgpu2.h"
#include "pathgpu2/kernels/kernels.h"
#include "renderconfig.h"
#include "luxrays/core/geometry/transform.h"
#include "luxrays/accelerators/mqbvhaccel.h"
#include "luxrays/accelerators/bvhaccel.h"
#include "luxrays/core/pixel/samplebuffer.h"

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
	samplerKernel = NULL;
	advancePathsKernel = NULL;

	gpuTaskStats = new PathGPU2::GPUTaskStats[renderEngine->taskCount];
}

PathGPU2RenderThread::~PathGPU2RenderThread() {
	if (started)
		Stop();

	delete initKernel;
	delete initFBKernel;
	delete samplerKernel;
	delete advancePathsKernel;

	delete[] frameBuffer;
	delete[] gpuTaskStats;
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

static bool MeshPtrCompare(Mesh *p0, Mesh *p1) {
	return p0 < p1;
}

void PathGPU2RenderThread::InitRenderGeometry() {
	SLGScene *scene = renderEngine->scene;
	cl::Context &oclContext = intersectionDevice->GetOpenCLContext();
	const OpenCLDeviceDescription *deviceDesc = intersectionDevice->GetDeviceDesc();

	const unsigned int verticesCount = scene->dataSet->GetTotalVertexCount();
	const unsigned int trianglesCount = scene->dataSet->GetTotalTriangleCount();

	//--------------------------------------------------------------------------
	// Translate mesh IDs
	//--------------------------------------------------------------------------

	const TriangleMeshID *meshIDs = scene->dataSet->GetMeshIDTable();
	cerr << "[PathGPU2RenderThread::" << threadIndex << "] MeshIDs buffer size: " << (sizeof(unsigned int) * trianglesCount / 1024) << "Kbytes" << endl;
	meshIDBuff = new cl::Buffer(oclContext,
			CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
			sizeof(unsigned int) * trianglesCount,
			(void *)meshIDs);
	deviceDesc->AllocMemory(meshIDBuff->getInfo<CL_MEM_SIZE>());

	const double tStart = WallClockTime();

	// Check the used accelerator type
	if (scene->dataSet->GetAcceleratorType() == ACCEL_MQBVH) {
		// MQBVH geometry must be defined in a specific way.

		//----------------------------------------------------------------------
		// Translate mesh IDs
		//----------------------------------------------------------------------

		const TriangleID *triangleIDs = scene->dataSet->GetMeshTriangleIDTable();
		cerr << "[PathGPU2RenderThread::" << threadIndex << "] TriangleIDs buffer size: " << (sizeof(unsigned int) * trianglesCount / 1024) << "Kbytes" << endl;
		triangleIDBuff = new cl::Buffer(oclContext,
				CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
				sizeof(unsigned int) * trianglesCount,
				(void *)triangleIDs);
		deviceDesc->AllocMemory(triangleIDBuff->getInfo<CL_MEM_SIZE>());

		vector<Point> verts;
		vector<Spectrum> colors;
		vector<Normal> normals;
		vector<UV> uvs;
		vector<Triangle> tris;
		vector<PathGPU2::Mesh> meshDescs;
		std::map<ExtMesh *, unsigned int, bool (*)(Mesh *, Mesh *)> definedMeshs(MeshPtrCompare);

		PathGPU2::Mesh newMeshDesc;
		newMeshDesc.vertsOffset = 0;
		newMeshDesc.trisOffset = 0;

		PathGPU2::Mesh currentMeshDesc;

		for (unsigned int i = 0; i < scene->objects.size(); ++i) {
			ExtMesh *mesh = scene->objects[i];

			bool isExistingInstance;
			if (mesh->GetType() == TYPE_EXT_TRIANGLE_INSTANCE) {
				ExtInstanceTriangleMesh *imesh = (ExtInstanceTriangleMesh *)mesh;

				// Check if is one of the already defined meshes
				std::map<ExtMesh *, unsigned int, bool (*)(Mesh *, Mesh *)>::iterator it = definedMeshs.find(imesh->GetExtTriangleMesh());
				if (it == definedMeshs.end()) {
					// It is a new one
					currentMeshDesc = newMeshDesc;

					newMeshDesc.vertsOffset += imesh->GetTotalVertexCount();
					newMeshDesc.trisOffset += imesh->GetTotalTriangleCount();

					isExistingInstance = false;

					definedMeshs[mesh] = definedMeshs.size();
				} else {
					currentMeshDesc = meshDescs[it->second];

					isExistingInstance = true;
				}

				memcpy(currentMeshDesc.trans, imesh->GetTransformation().GetMatrix().m, sizeof(float[4][4]));
				memcpy(currentMeshDesc.invTrans, imesh->GetInvTransformation().GetMatrix().m, sizeof(float[4][4]));
				mesh = imesh->GetExtTriangleMesh();
			} else {
				memcpy(newMeshDesc.invTrans, Matrix4x4().m, sizeof(float[4][4]));
				currentMeshDesc = newMeshDesc;

				newMeshDesc.vertsOffset += mesh->GetTotalVertexCount();
				newMeshDesc.trisOffset += mesh->GetTotalTriangleCount();

				isExistingInstance = false;
			}

			meshDescs.push_back(currentMeshDesc);

			if (!isExistingInstance) {
				assert (mesh->GetType() == TYPE_EXT_TRIANGLE);

				//--------------------------------------------------------------
				// Translate mesh colors
				//--------------------------------------------------------------

				for (unsigned int j = 0; j < mesh->GetTotalVertexCount(); ++j) {
					if (mesh->HasColors())
						colors.push_back(mesh->GetColor(j));
					else
						colors.push_back(Spectrum(1.f, 1.f, 1.f));
				}

				//--------------------------------------------------------------
				// Translate mesh normals
				//--------------------------------------------------------------

				for (unsigned int j = 0; j < mesh->GetTotalVertexCount(); ++j)
					normals.push_back(mesh->GetNormal(j));

				//----------------------------------------------------------------------
				// Translate vertex uvs
				//----------------------------------------------------------------------

				if (scene->texMapCache->GetSize()) {
					// TODO: I should check if the only texture map is used for infinitelight

					for (unsigned int j = 0; j < mesh->GetTotalVertexCount(); ++j) {
						if (mesh->HasUVs())
							uvs.push_back(mesh->GetUV(j));
						else
							uvs.push_back(UV(0.f, 0.f));
					}
				}

				//--------------------------------------------------------------
				// Translate mesh vertices
				//--------------------------------------------------------------

				for (unsigned int j = 0; j < mesh->GetTotalVertexCount(); ++j)
					verts.push_back(mesh->GetVertex(j));

				//--------------------------------------------------------------
				// Translate mesh indices
				//--------------------------------------------------------------

				Triangle *mtris = mesh->GetTriangles();
				for (unsigned int j = 0; j < mesh->GetTotalTriangleCount(); ++j)
					tris.push_back(mtris[j]);
			}
		}

		cerr << "[PathGPU2RenderThread::" << threadIndex << "] Colors buffer size: " << (sizeof(Spectrum) * colors.size() / 1024) << "Kbytes" << endl;
		colorsBuff = new cl::Buffer(oclContext,
				CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
				sizeof(Spectrum) * colors.size(),
				(void *)&colors[0]);
		deviceDesc->AllocMemory(colorsBuff->getInfo<CL_MEM_SIZE>());

		cerr << "[PathGPU2RenderThread::" << threadIndex << "] Normals buffer size: " << (sizeof(Normal) * normals.size() / 1024) << "Kbytes" << endl;
		normalsBuff = new cl::Buffer(oclContext,
				CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
				sizeof(Normal) * normals.size(),
				(void *)&normals[0]);
		deviceDesc->AllocMemory(normalsBuff->getInfo<CL_MEM_SIZE>());

		if (uvs.size() > 0) {
			cerr << "[PathGPU2RenderThread::" << threadIndex << "] UVs buffer size: " << (sizeof(UV) * verticesCount / 1024) << "Kbytes" << endl;
			uvsBuff = new cl::Buffer(oclContext,
					CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
					sizeof(UV) * verticesCount,
					(void *)&uvs[0]);
			deviceDesc->AllocMemory(uvsBuff->getInfo<CL_MEM_SIZE>());
		} else
			uvsBuff = NULL;

		cerr << "[PathGPU2RenderThread::" << threadIndex << "] Vertices buffer size: " << (sizeof(Point) * verts.size() / 1024) << "Kbytes" << endl;
		vertsBuff = new cl::Buffer(oclContext,
				CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
				sizeof(Point) * verts.size(),
				(void *)&verts[0]);
		deviceDesc->AllocMemory(vertsBuff->getInfo<CL_MEM_SIZE>());

		cerr << "[PathGPU2RenderThread::" << threadIndex << "] Triangles buffer size: " << (sizeof(Triangle) * tris.size() / 1024) << "Kbytes" << endl;
		trianglesBuff = new cl::Buffer(oclContext,
				CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
				sizeof(Triangle) * tris.size(),
				(void *)&tris[0]);
		deviceDesc->AllocMemory(trianglesBuff->getInfo<CL_MEM_SIZE>());

		cerr << "[PathGPU2RenderThread::" << threadIndex << "] Mesh description buffer size: " << (sizeof(PathGPU2::Mesh) * meshDescs.size() / 1024) << "Kbytes" << endl;
		meshDescsBuff = new cl::Buffer(oclContext,
				CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
				sizeof(PathGPU2::Mesh) * meshDescs.size(),
				(void *)&meshDescs[0]);
		deviceDesc->AllocMemory(meshDescsBuff->getInfo<CL_MEM_SIZE>());
	} else {
		triangleIDBuff = NULL;
		meshDescsBuff = NULL;

		//----------------------------------------------------------------------
		// Translate mesh colors
		//----------------------------------------------------------------------

		Spectrum *colors = new Spectrum[verticesCount];
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

		cerr << "[PathGPU2RenderThread::" << threadIndex << "] Colors buffer size: " << (sizeof(Spectrum) * verticesCount / 1024) << "Kbytes" << endl;
		colorsBuff = new cl::Buffer(oclContext,
				CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
				sizeof(Spectrum) * verticesCount,
				colors);
		deviceDesc->AllocMemory(colorsBuff->getInfo<CL_MEM_SIZE>());
		delete[] colors;

		//----------------------------------------------------------------------
		// Translate mesh normals
		//----------------------------------------------------------------------

		Normal *normals = new Normal[verticesCount];
		unsigned int nIndex = 0;
		for (unsigned int i = 0; i < scene->objects.size(); ++i) {
			ExtMesh *mesh = scene->objects[i];

			for (unsigned int j = 0; j < mesh->GetTotalVertexCount(); ++j)
				normals[nIndex++] = mesh->GetNormal(j);
		}

		cerr << "[PathGPU2RenderThread::" << threadIndex << "] Normals buffer size: " << (sizeof(Normal) * verticesCount / 1024) << "Kbytes" << endl;
		normalsBuff = new cl::Buffer(oclContext,
				CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
				sizeof(Normal) * verticesCount,
				normals);
		deviceDesc->AllocMemory(normalsBuff->getInfo<CL_MEM_SIZE>());
		delete[] normals;

		//----------------------------------------------------------------------
		// Translate vertex uvs
		//----------------------------------------------------------------------

		if (scene->texMapCache->GetSize()) {
			// TODO: I should check if the only texture map is used for infinitelight

			UV *uvs = new UV[verticesCount];
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

			cerr << "[PathGPU2RenderThread::" << threadIndex << "] UVs buffer size: " << (sizeof(UV) * verticesCount / 1024) << "Kbytes" << endl;
			uvsBuff = new cl::Buffer(oclContext,
					CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
					sizeof(UV) * verticesCount,
					uvs);
			deviceDesc->AllocMemory(uvsBuff->getInfo<CL_MEM_SIZE>());
			delete[] uvs;
		} else
			uvsBuff = NULL;

		//----------------------------------------------------------------------
		// Translate mesh vertices
		//----------------------------------------------------------------------

		unsigned int *meshOffsets = new unsigned int[scene->objects.size()];
		Point *verts = new Point[verticesCount];
		unsigned int vIndex = 0;
		for (unsigned int i = 0; i < scene->objects.size(); ++i) {
			ExtMesh *mesh = scene->objects[i];

			meshOffsets[i] = vIndex;
			for (unsigned int j = 0; j < mesh->GetTotalVertexCount(); ++j)
				verts[vIndex++] = mesh->GetVertex(j);
		}

		cerr << "[PathGPU2RenderThread::" << threadIndex << "] Vertices buffer size: " << (sizeof(Point) * verticesCount / 1024) << "Kbytes" << endl;
		vertsBuff = new cl::Buffer(oclContext,
				CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
				sizeof(Point) * verticesCount,
				(void *)verts);
		deviceDesc->AllocMemory(vertsBuff->getInfo<CL_MEM_SIZE>());
		delete[] verts;

		//----------------------------------------------------------------------
		// Translate mesh indices
		//----------------------------------------------------------------------

		Triangle *tris = new Triangle[trianglesCount];
		unsigned int tIndex = 0;
		for (unsigned int i = 0; i < scene->objects.size(); ++i) {
			ExtMesh *mesh = scene->objects[i];

			Triangle *mtris = mesh->GetTriangles();
			const unsigned int moffset = meshOffsets[i];
			for (unsigned int j = 0; j < mesh->GetTotalTriangleCount(); ++j) {
				tris[tIndex].v[0] = mtris[j].v[0] + moffset;
				tris[tIndex].v[1] = mtris[j].v[1] + moffset;
				tris[tIndex].v[2] = mtris[j].v[2] + moffset;
				++tIndex;
			}
		}

		cerr << "[PathGPU2RenderThread::" << threadIndex << "] Triangles buffer size: " << (sizeof(Triangle) * trianglesCount / 1024) << "Kbytes" << endl;
		trianglesBuff = new cl::Buffer(oclContext,
				CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
				sizeof(Triangle) * trianglesCount,
				(void *)tris);
		deviceDesc->AllocMemory(trianglesBuff->getInfo<CL_MEM_SIZE>());
		delete[] tris;

		delete[] meshOffsets;
	}

	const double tEnd = WallClockTime();
	cerr << "[PathGPU2RenderThread::" << threadIndex << "] Mesh information translation time: " << int((tEnd - tStart) * 1000.0) << "ms" << endl;
}

void PathGPU2RenderThread::InitRender() {
	SLGScene *scene = renderEngine->scene;
	AcceleratorType accelType = scene->dataSet->GetAcceleratorType();

	const unsigned int frameBufferPixelCount =
		renderEngine->film->GetWidth() * renderEngine->film->GetHeight();

	// Delete previous allocated frameBuffer
	delete[] frameBuffer;
	frameBuffer = new PathGPU2::Pixel[frameBufferPixelCount];

	for (unsigned int i = 0; i < frameBufferPixelCount; ++i) {
		frameBuffer[i].c.r = 0.f;
		frameBuffer[i].c.g = 0.f;
		frameBuffer[i].c.b = 0.f;
		frameBuffer[i].count = 0.f;
	}

	const unsigned int startLine = Clamp<unsigned int>(
			renderEngine->film->GetHeight() * samplingStart,
			0, renderEngine->film->GetHeight() - 1);

	cl::Context &oclContext = intersectionDevice->GetOpenCLContext();
	cl::Device &oclDevice = intersectionDevice->GetOpenCLDevice();
	const OpenCLDeviceDescription *deviceDesc = intersectionDevice->GetDeviceDesc();

	double tStart, tEnd;

	//--------------------------------------------------------------------------
	// Allocate buffers
	//--------------------------------------------------------------------------

	const unsigned int taskCount = renderEngine->taskCount;

	tStart = WallClockTime();

	cerr << "[PathGPU2RenderThread::" << threadIndex << "] Ray buffer size: " << (sizeof(Ray) * taskCount / 1024) << "Kbytes" << endl;
	raysBuff = new cl::Buffer(oclContext,
			CL_MEM_READ_WRITE,
			sizeof(Ray) * taskCount);
	deviceDesc->AllocMemory(raysBuff->getInfo<CL_MEM_SIZE>());

	cerr << "[PathGPU2RenderThread::" << threadIndex << "] RayHit buffer size: " << (sizeof(RayHit) * taskCount / 1024) << "Kbytes" << endl;
	hitsBuff = new cl::Buffer(oclContext,
			CL_MEM_READ_WRITE,
			sizeof(RayHit) * taskCount);
	deviceDesc->AllocMemory(hitsBuff->getInfo<CL_MEM_SIZE>());

	cerr << "[PathGPU2RenderThread::" << threadIndex << "] FrameBuffer buffer size: " << (sizeof(PathGPU2::Pixel) * frameBufferPixelCount / 1024) << "Kbytes" << endl;
	frameBufferBuff = new cl::Buffer(oclContext,
			CL_MEM_READ_WRITE,
			sizeof(PathGPU2::Pixel) * frameBufferPixelCount);
	deviceDesc->AllocMemory(frameBufferBuff->getInfo<CL_MEM_SIZE>());

	tEnd = WallClockTime();
	cerr << "[PathGPU2RenderThread::" << threadIndex << "] OpenCL buffer creation time: " << int((tEnd - tStart) * 1000.0) << "ms" << endl;

	//--------------------------------------------------------------------------
	// Camera definition
	//--------------------------------------------------------------------------

	// Dynamic camera mode uses a buffer to transfer camera position/orientation
	PathGPU2::Camera camera;
	camera.yon = scene->camera->clipYon;
	camera.hither = scene->camera->clipHither;
	camera.lensRadius = scene->camera->lensRadius;
	camera.focalDistance = scene->camera->focalDistance;
	memcpy(&camera.RasterToCameraMatrix[0][0], scene->camera->GetRasterToCameraMatrix().m, 4 * 4 * sizeof(float));
	memcpy(&camera.CameraToWorldMatrix[0][0], scene->camera->GetCameraToWorldMatrix().m, 4 * 4 * sizeof(float));

	cerr << "[PathGPU2RenderThread::" << threadIndex << "] Camera buffer size: " << (sizeof(PathGPU2::Camera) / 1024) << "Kbytes" << endl;
	cameraBuff = new cl::Buffer(oclContext,
			CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
			sizeof(PathGPU2::Camera),
			(void *)&camera);
	deviceDesc->AllocMemory(cameraBuff->getInfo<CL_MEM_SIZE>());

	//--------------------------------------------------------------------------
	// Translate mesh geometry
	//--------------------------------------------------------------------------

	InitRenderGeometry();

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
		PathGPU2::InfiniteLight il;

		il.gain = infiniteLight->GetGain();
		il.shiftU = infiniteLight->GetShiftU();
		il.shiftV = infiniteLight->GetShiftV();
		const TextureMap *texMap = infiniteLight->GetTexture()->GetTexMap();
		il.width = texMap->GetWidth();
		il.height = texMap->GetHeight();

		cerr << "[PathGPU2RenderThread::" << threadIndex << "] InfiniteLight buffer size: " << (sizeof(PathGPU2::InfiniteLight) / 1024) << "Kbytes" << endl;
		infiniteLightBuff = new cl::Buffer(oclContext,
				CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
				sizeof(PathGPU2::InfiniteLight),
				(void *)&il);
		deviceDesc->AllocMemory(infiniteLightBuff->getInfo<CL_MEM_SIZE>());

		const unsigned int pixelCount = il.width * il.height;
		cerr << "[PathGPU2RenderThread::" << threadIndex << "] InfiniteLight Map buffer size: " << (sizeof(Spectrum) * pixelCount / 1024) << "Kbytes" << endl;
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
	cerr << "[PathGPU2RenderThread::" << threadIndex << "] Infinitelight translation time: " << int((tEnd - tStart) * 1000.0) << "ms" << endl;

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
		PathGPU2::SunLight sl;

		sl.sundir = sunLight->GetDir();
		sl.gain = sunLight->GetGain();
		sl.turbidity = sunLight->GetTubidity();
		sl.relSize= sunLight->GetRelSize();
		float tmp;
		sunLight->GetInitData(&sl.x, &sl.y, &tmp, &tmp, &tmp,
				&sl.cosThetaMax, &tmp, &sl.suncolor);

		cerr << "[PathGPU2RenderThread::" << threadIndex << "] SunLight buffer size: " << (sizeof(PathGPU2::SunLight) / 1024) << "Kbytes" << endl;
		sunLightBuff = new cl::Buffer(oclContext,
				CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
				sizeof(PathGPU2::SunLight),
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
		PathGPU2::SkyLight sl;

		sl.gain = skyLight->GetGain();
		skyLight->GetInitData(&sl.thetaS, &sl.phiS,
				&sl.zenith_Y, &sl.zenith_x, &sl.zenith_y,
				sl.perez_Y, sl.perez_x, sl.perez_y);

		cerr << "[PathGPU2RenderThread::" << threadIndex << "] SkyLight buffer size: " << (sizeof(PathGPU2::SkyLight) / 1024) << "Kbytes" << endl;
		skyLightBuff = new cl::Buffer(oclContext,
				CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
				sizeof(PathGPU2::SkyLight),
				(void *)&sl);
		deviceDesc->AllocMemory(skyLightBuff->getInfo<CL_MEM_SIZE>());
	} else
		skyLightBuff = NULL;

	if (!skyLight && !sunLight && !infiniteLight && (areaLightCount == 0))
		throw runtime_error("There are no light sources supported by PathGPU2 in the scene");

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

		//----------------------------------------------------------------------

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

		//-----------------------------------------------

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
			cerr << "[PathGPU2RenderThread::" << threadIndex << "] Mesh bump maps index buffer size: " << (sizeof(unsigned int) * meshCount / 1024) << "Kbytes" << endl;
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

			cerr << "[PathGPU2RenderThread::" << threadIndex << "] Mesh bump maps scale buffer size: " << (sizeof(float) * meshCount / 1024) << "Kbytes" << endl;
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
		cerr << "[PathGPU2RenderThread::" << threadIndex << "] Texture maps translation time: " << int((tEnd - tStart) * 1000.0) << "ms" << endl;
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
		sizeof(PathGPU2::Seed);

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
	const size_t uDataSize = (renderEngine->sampler->type == PathGPU2::INLINED_RANDOM) ?
		// Only IDX_SCREEN_X, IDX_SCREEN_Y
		(sizeof(float) * 2) :
		((renderEngine->sampler->type == PathGPU2::METROPOLIS) ?
			(sizeof(float) * 2 + sizeof(unsigned int) * 5 + sizeof(Spectrum) + 2 * (uDataEyePathVertexSize + uDataPerPathVertexSize * renderEngine->maxPathDepth)) :
			(uDataEyePathVertexSize + uDataPerPathVertexSize * renderEngine->maxPathDepth));

	size_t sampleSize =
		// uint pixelIndex;
		((renderEngine->sampler->type == PathGPU2::METROPOLIS) ? 0 : sizeof(unsigned int)) +
		uDataSize +
		// Spectrum radiance;
		sizeof(Spectrum);

	size_t stratifiedDataSize = 0;
	if (renderEngine->sampler->type == PathGPU2::STRATIFIED) {
		PathGPU2::StratifiedSampler *s = (PathGPU2::StratifiedSampler *)renderEngine->sampler;
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
		(((areaLightCount > 0) || sunLight) ? sizeof(PathGPU2::PathStateDL) : sizeof(PathGPU2::PathState));

	const size_t gpuTaksSize = gpuTaksSizePart1 + gpuTaksSizePart2 + gpuTaksSizePart3;
	cerr << "[PathGPU2RenderThread::" << threadIndex << "] Size of a GPUTask: " << gpuTaksSize <<
			"bytes (" << gpuTaksSizePart1 << " + " << gpuTaksSizePart2 << " + " << gpuTaksSizePart3 << ")" << endl;
	cerr << "[PathGPU2RenderThread::" << threadIndex << "] Tasks buffer size: " << (gpuTaksSize * taskCount / 1024) << "Kbytes" << endl;

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

	cerr << "[PathGPU2RenderThread::" << threadIndex << "] Task Stats buffer size: " << (sizeof(PathGPU2::GPUTaskStats) * taskCount / 1024) << "Kbytes" << endl;
	taskStatsBuff = new cl::Buffer(oclContext,
			CL_MEM_READ_WRITE,
			sizeof(PathGPU2::GPUTaskStats) * taskCount);
	deviceDesc->AllocMemory(taskStatsBuff->getInfo<CL_MEM_SIZE>());

	//--------------------------------------------------------------------------
	// Compile kernels
	//--------------------------------------------------------------------------

	// Set #define symbols
	stringstream ss;
	ss.precision(6);
	ss << scientific <<
			" -D PARAM_STARTLINE=" << startLine <<
			" -D PARAM_TASK_COUNT=" << taskCount <<
			" -D PARAM_IMAGE_WIDTH=" << renderEngine->film->GetWidth() <<
			" -D PARAM_IMAGE_HEIGHT=" << renderEngine->film->GetHeight() <<
			" -D PARAM_RAY_EPSILON=" << RAY_EPSILON << "f" <<
			" -D PARAM_SEED=" << seed <<
			" -D PARAM_MAX_PATH_DEPTH=" << renderEngine->maxPathDepth <<
			" -D PARAM_RR_DEPTH=" << renderEngine->rrDepth <<
			" -D PARAM_RR_CAP=" << renderEngine->rrImportanceCap << "f" <<
			" -D PARAM_WORLD_RADIUS=" << (scene->dataSet->GetBSphere().rad * 1.01f) << "f"
			;

	switch (accelType) {
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

	if (scene->camera->lensRadius > 0.f)
		ss << " -D PARAM_CAMERA_HAS_DOF";


	if (infiniteLight)
		ss << " -D PARAM_HAS_INFINITELIGHT";

	if (skyLight)
		ss << " -D PARAM_HAS_SKYLIGHT";

	if (sunLight) {
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

	const PathGPU2::Filter *filter = renderEngine->filter;
	switch (filter->type) {
		case PathGPU2::NONE:
			ss << " -D PARAM_IMAGE_FILTER_TYPE=0";
			break;
		case PathGPU2::BOX:
			ss << " -D PARAM_IMAGE_FILTER_TYPE=1" <<
					" -D PARAM_IMAGE_FILTER_WIDTH_X=" << filter->widthX << "f" <<
					" -D PARAM_IMAGE_FILTER_WIDTH_Y=" << filter->widthY << "f";
			break;
		case PathGPU2::GAUSSIAN:
			ss << " -D PARAM_IMAGE_FILTER_TYPE=2" <<
					" -D PARAM_IMAGE_FILTER_WIDTH_X=" << filter->widthX << "f" <<
					" -D PARAM_IMAGE_FILTER_WIDTH_Y=" << filter->widthY << "f" <<
					" -D PARAM_IMAGE_FILTER_GAUSSIAN_ALPHA=" << ((PathGPU2::GaussianFilter *)filter)->alpha << "f";
			break;
		case PathGPU2::MITCHELL:
			ss << " -D PARAM_IMAGE_FILTER_TYPE=3" <<
					" -D PARAM_IMAGE_FILTER_WIDTH_X=" << filter->widthX << "f" <<
					" -D PARAM_IMAGE_FILTER_WIDTH_Y=" << filter->widthY << "f" <<
					" -D PARAM_IMAGE_FILTER_MITCHELL_B=" << ((PathGPU2::MitchellFilter *)filter)->B << "f" <<
					" -D PARAM_IMAGE_FILTER_MITCHELL_C=" << ((PathGPU2::MitchellFilter *)filter)->C << "f";
			break;
		default:
			assert (false);
	}

	if (renderEngine->usePixelAtomics)
		ss << " -D PARAM_USE_PIXEL_ATOMICS";

	const PathGPU2::Sampler *sampler = renderEngine->sampler;
	switch (sampler->type) {
		case PathGPU2::INLINED_RANDOM:
			ss << " -D PARAM_SAMPLER_TYPE=0";
			break;
		case PathGPU2::RANDOM:
			ss << " -D PARAM_SAMPLER_TYPE=1";
			break;
		case PathGPU2::METROPOLIS:
			ss << " -D PARAM_SAMPLER_TYPE=2" <<
					" -D PARAM_SAMPLER_METROPOLIS_LARGE_STEP_RATE=" << ((PathGPU2::MetropolisSampler *)sampler)->largeStepRate << "f" <<
					" -D PARAM_SAMPLER_METROPOLIS_MAX_CONSECUTIVE_REJECT=" << ((PathGPU2::MetropolisSampler *)sampler)->maxConsecutiveReject;
			break;
		case PathGPU2::STRATIFIED:
			ss << " -D PARAM_SAMPLER_TYPE=3" <<
					" -D PARAM_SAMPLER_STRATIFIED_X_SAMPLES=" << ((PathGPU2::StratifiedSampler *)sampler)->xSamples <<
					" -D PARAM_SAMPLER_STRATIFIED_Y_SAMPLES=" << ((PathGPU2::StratifiedSampler *)sampler)->ySamples;
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

	tStart = WallClockTime();

	// Check if I have to recompile the kernels
	string newKernelParameters = ss.str();
	if (kernelsParameters != newKernelParameters) {
		kernelsParameters = newKernelParameters;

		// Compile sources
		stringstream ssKernel;
		ssKernel << KernelSource_PathGPU2_datatypes << KernelSource_PathGPU2_core <<
				 KernelSource_PathGPU2_filters << KernelSource_PathGPU2_scene <<
				KernelSource_PathGPU2_samplers << KernelSource_PathGPU2_kernels;
		string kernelSource = ssKernel.str();

		cl::Program::Sources source(1, std::make_pair(kernelSource.c_str(), kernelSource.length()));
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
		cerr << "[PathGPU2RenderThread::" << threadIndex << "] Compiling Init Kernel" << endl;
		initKernel = new cl::Kernel(program, "Init");
		initKernel->getWorkGroupInfo<size_t>(oclDevice, CL_KERNEL_WORK_GROUP_SIZE, &initWorkGroupSize);
		cerr << "[PathGPU2RenderThread::" << threadIndex << "] PathGPU Init kernel work group size: " << initWorkGroupSize << endl;

		initKernel->getWorkGroupInfo<size_t>(oclDevice, CL_KERNEL_WORK_GROUP_SIZE, &initWorkGroupSize);
		cerr << "[PathGPU2RenderThread::" << threadIndex << "] Suggested work group size: " << initWorkGroupSize << endl;

		if (intersectionDevice->GetForceWorkGroupSize() > 0) {
			initWorkGroupSize = intersectionDevice->GetForceWorkGroupSize();
			cerr << "[PathGPU2RenderThread::" << threadIndex << "] Forced work group size: " << initWorkGroupSize << endl;
		} else if (renderEngine->sampler->type == PathGPU2::STRATIFIED) {
			// Resize the workgroup to have enough local memory
			size_t localMem = oclDevice.getInfo<CL_DEVICE_LOCAL_MEM_SIZE>();

			while ((initWorkGroupSize > 64) && (stratifiedDataSize * initWorkGroupSize > localMem))
				initWorkGroupSize /= 2;

			if (stratifiedDataSize * initWorkGroupSize > localMem)
				throw std::runtime_error("Not enough local memory to run, try to reduce path.sampler.xsamples and path.sampler.xsamples values");

			cerr << "[PathGPU2RenderThread::" << threadIndex << "] Cap work group size to: " << initWorkGroupSize << endl;
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
		// Sampler kernel
		//----------------------------------------------------------------------

		delete samplerKernel;
		cerr << "[PathGPU2RenderThread::" << threadIndex << "] Compiling Sampler Kernel" << endl;
		samplerKernel = new cl::Kernel(program, "Sampler");
		samplerKernel->getWorkGroupInfo<size_t>(oclDevice, CL_KERNEL_WORK_GROUP_SIZE, &samplerWorkGroupSize);
		cerr << "[PathGPU2RenderThread::" << threadIndex << "] PathGPU Sampler kernel work group size: " << samplerWorkGroupSize << endl;

		samplerKernel->getWorkGroupInfo<size_t>(oclDevice, CL_KERNEL_WORK_GROUP_SIZE, &samplerWorkGroupSize);
		cerr << "[PathGPU2RenderThread::" << threadIndex << "] Suggested work group size: " << samplerWorkGroupSize << endl;

		if (intersectionDevice->GetForceWorkGroupSize() > 0) {
			samplerWorkGroupSize = intersectionDevice->GetForceWorkGroupSize();
			cerr << "[PathGPU2RenderThread::" << threadIndex << "] Forced work group size: " << samplerWorkGroupSize << endl;
		} else if (renderEngine->sampler->type == PathGPU2::STRATIFIED) {
			// Resize the workgroup to have enough local memory
			size_t localMem = oclDevice.getInfo<CL_DEVICE_LOCAL_MEM_SIZE>();

			while ((samplerWorkGroupSize > 64) && (stratifiedDataSize * samplerWorkGroupSize > localMem))
				samplerWorkGroupSize /= 2;

			if (stratifiedDataSize * samplerWorkGroupSize > localMem)
				throw std::runtime_error("Not enough local memory to run, try to reduce path.sampler.xsamples and path.sampler.xsamples values");

			cerr << "[PathGPU2RenderThread::" << threadIndex << "] Cap work group size to: " << samplerWorkGroupSize << endl;
		}

		//----------------------------------------------------------------------
		// AdvancePaths kernel
		//----------------------------------------------------------------------

		delete advancePathsKernel;
		cerr << "[PathGPU2RenderThread::" << threadIndex << "] Compiling AdvancePaths Kernel" << endl;
		advancePathsKernel = new cl::Kernel(program, "AdvancePaths");
		advancePathsKernel->getWorkGroupInfo<size_t>(oclDevice, CL_KERNEL_WORK_GROUP_SIZE, &advancePathsWorkGroupSize);
		cerr << "[PathGPU2RenderThread::" << threadIndex << "] PathGPU AdvancePaths kernel work group size: " << advancePathsWorkGroupSize << endl;

		advancePathsKernel->getWorkGroupInfo<size_t>(oclDevice, CL_KERNEL_WORK_GROUP_SIZE, &advancePathsWorkGroupSize);
		cerr << "[PathGPU2RenderThread::" << threadIndex << "] Suggested work group size: " << advancePathsWorkGroupSize << endl;

		if (intersectionDevice->GetForceWorkGroupSize() > 0) {
			advancePathsWorkGroupSize = intersectionDevice->GetForceWorkGroupSize();
			cerr << "[PathGPU2RenderThread::" << threadIndex << "] Forced work group size: " << advancePathsWorkGroupSize << endl;
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

	unsigned int argIndex = 0;
	samplerKernel->setArg(argIndex++, *tasksBuff);
	samplerKernel->setArg(argIndex++, *taskStatsBuff);
	samplerKernel->setArg(argIndex++, *raysBuff);
	if (cameraBuff)
		samplerKernel->setArg(argIndex++, *cameraBuff);
	if (renderEngine->sampler->type == PathGPU2::STRATIFIED)
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
	if (infiniteLight) {
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

	cl::CommandQueue &oclQueue = intersectionDevice->GetOpenCLQueue();

	// Clear the frame buffer
	initFBKernel->setArg(0, *frameBufferBuff);
	oclQueue.enqueueNDRangeKernel(*initFBKernel, cl::NullRange,
			cl::NDRange(RoundUp<unsigned int>(
				renderEngine->film->GetWidth() * renderEngine->film->GetHeight(), initFBWorkGroupSize)),
			cl::NDRange(initFBWorkGroupSize));

	// Initialize the tasks buffer
	argIndex = 0;
	initKernel->setArg(argIndex++, *tasksBuff);
	initKernel->setArg(argIndex++, *taskStatsBuff);
	initKernel->setArg(argIndex++, *raysBuff);
	if (cameraBuff)
		initKernel->setArg(argIndex++, *cameraBuff);
	if (renderEngine->sampler->type == PathGPU2::STRATIFIED)
		initKernel->setArg(argIndex++, initWorkGroupSize * stratifiedDataSize, NULL);

	oclQueue.enqueueNDRangeKernel(*initKernel, cl::NullRange,
			cl::NDRange(taskCount), cl::NDRange(initWorkGroupSize));
	oclQueue.finish();

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
	oclQueue.enqueueReadBuffer(
		*frameBufferBuff,
		CL_TRUE,
		0,
		frameBufferBuff->getInfo<CL_MEM_SIZE>(),
		frameBuffer);

	const OpenCLDeviceDescription *deviceDesc = intersectionDevice->GetDeviceDesc();
	deviceDesc->FreeMemory(raysBuff->getInfo<CL_MEM_SIZE>());
	delete raysBuff;
	deviceDesc->FreeMemory(hitsBuff->getInfo<CL_MEM_SIZE>());
	delete hitsBuff;
	deviceDesc->FreeMemory(tasksBuff->getInfo<CL_MEM_SIZE>());
	delete tasksBuff;
	deviceDesc->FreeMemory(taskStatsBuff->getInfo<CL_MEM_SIZE>());
	delete taskStatsBuff;
	deviceDesc->FreeMemory(frameBufferBuff->getInfo<CL_MEM_SIZE>());
	delete frameBufferBuff;
	deviceDesc->FreeMemory(materialsBuff->getInfo<CL_MEM_SIZE>());
	delete materialsBuff;
	deviceDesc->FreeMemory(meshIDBuff->getInfo<CL_MEM_SIZE>());
	delete meshIDBuff;
	if (triangleIDBuff) {
		deviceDesc->FreeMemory(triangleIDBuff->getInfo<CL_MEM_SIZE>());
		delete triangleIDBuff;
	}
	if (meshDescsBuff) {
		deviceDesc->FreeMemory(meshDescsBuff->getInfo<CL_MEM_SIZE>());
		delete meshDescsBuff;
	}
	deviceDesc->FreeMemory(meshMatsBuff->getInfo<CL_MEM_SIZE>());
	delete meshMatsBuff;
	deviceDesc->FreeMemory(colorsBuff->getInfo<CL_MEM_SIZE>());
	delete colorsBuff;
	deviceDesc->FreeMemory(normalsBuff->getInfo<CL_MEM_SIZE>());
	delete normalsBuff;
	deviceDesc->FreeMemory(trianglesBuff->getInfo<CL_MEM_SIZE>());
	delete trianglesBuff;
	deviceDesc->FreeMemory(vertsBuff->getInfo<CL_MEM_SIZE>());
	delete vertsBuff;
	if (infiniteLightBuff) {
		deviceDesc->FreeMemory(infiniteLightBuff->getInfo<CL_MEM_SIZE>());
		delete infiniteLightBuff;
		deviceDesc->FreeMemory(infiniteLightMapBuff->getInfo<CL_MEM_SIZE>());
		delete infiniteLightMapBuff;
	}
	if (sunLightBuff) {
		deviceDesc->FreeMemory(sunLightBuff->getInfo<CL_MEM_SIZE>());
		delete sunLightBuff;
	}
	if (skyLightBuff) {
		deviceDesc->FreeMemory(skyLightBuff->getInfo<CL_MEM_SIZE>());
		delete skyLightBuff;
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
		if (meshBumpsBuff) {
			deviceDesc->FreeMemory(meshBumpsBuff->getInfo<CL_MEM_SIZE>());
			delete meshBumpsBuff;
			deviceDesc->FreeMemory(meshBumpsScaleBuff->getInfo<CL_MEM_SIZE>());
			delete meshBumpsScaleBuff;
		}
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
				sizeof(PathGPU2::GPUTaskStats) * taskCount,
				renderThread->gpuTaskStats);

			for (;;) {
				cl::Event event;

				for (unsigned int i = 0; i < 8; ++i) {
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

				if ((elapsedTime * 1000.0 > (double)renderThread->renderEngine->screenRefreshInterval) ||
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
			renderThread->frameBufferBuff->getInfo<CL_MEM_SIZE>(),
			renderThread->frameBuffer);
}

//------------------------------------------------------------------------------
// PathGPU2RenderEngine
//------------------------------------------------------------------------------

PathGPU2RenderEngine::PathGPU2RenderEngine(SLGScene *scn, Film *flm, boost::mutex *filmMutex,
		vector<IntersectionDevice *> intersectionDevices, const Properties &cfg) :
		RenderEngine(scn, flm, filmMutex) {
	// Rendering parameters

	taskCount = RoundUpPow2(cfg.GetInt("opencl.task.count", 65536));
	cerr << "[PathGPU2RenderThread] OpenCL task count: " << taskCount << endl;

	maxPathDepth = cfg.GetInt("path.maxdepth", 5);
	rrDepth = cfg.GetInt("path.russianroulette.depth", 3);
	rrImportanceCap = cfg.GetFloat("path.russianroulette.cap", 0.125f);

	//--------------------------------------------------------------------------
	// Sampler
	//--------------------------------------------------------------------------

	 const string samplerTypeName = cfg.GetString("path.sampler.type", "METROPOLIS");
	 if (samplerTypeName.compare("INLINED_RANDOM") == 0)
		 sampler = new PathGPU2::InlinedRandomSampler();
	 else if (samplerTypeName.compare("RANDOM") == 0)
		 sampler = new PathGPU2::RandomSampler();
	 else if (samplerTypeName.compare("STRATIFIED") == 0) {
		 const unsigned int xSamples = cfg.GetInt("path.sampler.xsamples", 3);
		 const unsigned int ySamples = cfg.GetInt("path.sampler.ysamples", 3);

		 sampler = new PathGPU2::StratifiedSampler(xSamples, ySamples);
	 } else if (samplerTypeName.compare("METROPOLIS") == 0) {
		 const float rate = cfg.GetFloat("path.sampler.largesteprate", 0.4f);
		 const float reject = cfg.GetFloat("path.sampler.maxconsecutivereject", 512);

		 sampler = new PathGPU2::MetropolisSampler(rate, reject);
	 } else
		throw std::runtime_error("Unknown path.sampler.type");

	//--------------------------------------------------------------------------
	// Filter
	//--------------------------------------------------------------------------

	const string filterType = cfg.GetString("path.filter.type", "NONE");
	const float filterWidthX = cfg.GetFloat("path.filter.width.x", 1.5f);
	const float filterWidthY = cfg.GetFloat("path.filter.width.y", 1.5f);
	if ((filterWidthX <= 0.f) || (filterWidthX > 1.5f))
		throw std::runtime_error("path.filter.width.x must be between 0.0 and 1.5");
	if ((filterWidthY <= 0.f) || (filterWidthY > 1.5f))
		throw std::runtime_error("path.filter.width.y must be between 0.0 and 1.5");

	if (filterType.compare("NONE") == 0)
		filter = new PathGPU2::NoneFilter();
	else if (filterType.compare("BOX") == 0)
		filter = new PathGPU2::BoxFilter(filterWidthX, filterWidthY);
	else if (filterType.compare("GAUSSIAN") == 0) {
		const float alpha = cfg.GetFloat("path.filter.alpha", 2.f);
		filter = new PathGPU2::GaussianFilter(filterWidthX, filterWidthY, alpha);
	} else if (filterType.compare("MITCHELL") == 0) {
		const float B = cfg.GetFloat("path.filter.B", 1.f / 3.f);
		const float C = cfg.GetFloat("path.filter.C", 1.f / 3.f);
		filter = new PathGPU2::MitchellFilter(filterWidthX, filterWidthY, B, C);
	} else
		throw std::runtime_error("Unknown path.filter.type");

	usePixelAtomics = (cfg.GetInt("path.pixelatomics.enable", 0) != 0);

	//--------------------------------------------------------------------------

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

	const unsigned int seedBase = (unsigned int)(WallClockTime() / 1000.0);

	// Create and start render threads
	const size_t renderThreadCount = oclIntersectionDevices.size();
	cerr << "Starting "<< renderThreadCount << " PathGPU2 render threads" << endl;
	for (size_t i = 0; i < renderThreadCount; ++i) {
		PathGPU2RenderThread *t = new PathGPU2RenderThread(
				i, seedBase + i * taskCount, i / (float)renderThreadCount,
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

	delete sampler;
	delete filter;
}

void PathGPU2RenderEngine::Start() {
	boost::unique_lock<boost::mutex> lock(engineMutex);

	RenderEngine::Start();

	samplesCount = 0;
	elapsedTime = 0.0f;

	for (size_t i = 0; i < renderThreads.size(); ++i)
		renderThreads[i]->Start();

	startTime = WallClockTime();
}

void PathGPU2RenderEngine::Interrupt() {
	boost::unique_lock<boost::mutex> lock(engineMutex);

	for (size_t i = 0; i < renderThreads.size(); ++i)
		renderThreads[i]->Interrupt();
}

void PathGPU2RenderEngine::Stop() {
	boost::unique_lock<boost::mutex> lock(engineMutex);

	RenderEngine::Stop();

	for (size_t i = 0; i < renderThreads.size(); ++i)
		renderThreads[i]->Stop();

	UpdateFilmLockLess();
}

unsigned int PathGPU2RenderEngine::GetThreadCount() const {
	boost::unique_lock<boost::mutex> lock(engineMutex);

	return renderThreads.size();
}

void PathGPU2RenderEngine::UpdateFilm() {
	boost::unique_lock<boost::mutex> lock(engineMutex);

	if (started)
		UpdateFilmLockLess();
}

void PathGPU2RenderEngine::UpdateFilmLockLess() {
	boost::unique_lock<boost::mutex> lock(*filmMutex);

	elapsedTime = WallClockTime() - startTime;
	const unsigned int imgWidth = film->GetWidth();
	const unsigned int imgHeight = film->GetHeight();
	const unsigned int pixelCount = imgWidth * imgHeight;

	film->Reset();

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
		}

		if (sampleBuffer->GetSampleCount() > 0) {
			// Splat all samples on the film
			film->SplatSampleBuffer(true, sampleBuffer);
			sampleBuffer = film->GetFreeSampleBuffer();
		}
	}

	// Update the sample count statistic
	unsigned long long totalCount = 0;
	for (size_t i = 0; i < renderThreads.size(); ++i) {
		PathGPU2::GPUTaskStats *stats = renderThreads[i]->gpuTaskStats;

		for (size_t i = 0; i < taskCount; ++i)
			totalCount += stats[i].sampleCount;
	}

	samplesCount = totalCount;
}

unsigned int PathGPU2RenderEngine::GetPass() const {
	return samplesCount / (film->GetWidth() * film->GetHeight());
}

#endif
