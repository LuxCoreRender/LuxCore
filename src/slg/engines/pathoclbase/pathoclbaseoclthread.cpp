/***************************************************************************
 * Copyright 1998-2018 by authors (see AUTHORS.txt)                        *
 *                                                                         *
 *   This file is part of LuxCoreRender.                                   *
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

#include "luxcore/cfg.h"

#include "slg/slg.h"
#include "slg/kernels/kernels.h"
#include "slg/renderconfig.h"
#include "slg/engines/pathoclbase/pathoclbase.h"
#include "slg/samplers/sobol.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// PathOCLBaseOCLRenderThread
//------------------------------------------------------------------------------

PathOCLBaseOCLRenderThread::PathOCLBaseOCLRenderThread(const u_int index,
		OpenCLIntersectionDevice *device, PathOCLBaseRenderEngine *re) {
	threadIndex = index;
	intersectionDevice = device;
	renderEngine = re;

	renderThread = nullptr;
	started = false;
	editMode = false;
	threadDone = false;

	kernelSrcHash = "";
	filmClearKernel = nullptr;

	// Scene buffers
	materialsBuff = nullptr;
	texturesBuff = nullptr;
	meshDescsBuff = nullptr;
	scnObjsBuff = nullptr;
	lightsBuff = nullptr;
	envLightIndicesBuff = nullptr;
	lightsDistributionBuff = nullptr;
	infiniteLightSourcesDistributionBuff = nullptr;
	dlscAllEntriesBuff = nullptr;
	dlscDistributionsBuff = nullptr;
	dlscBVHNodesBuff = nullptr;
	elvcAllEntriesBuff = nullptr;
	elvcDistributionsBuff = nullptr;
	elvcBVHNodesBuff = nullptr;
	envLightDistributionsBuff = nullptr;
	vertsBuff = nullptr;
	normalsBuff = nullptr;
	triNormalsBuff = nullptr;
	uvsBuff = nullptr;
	colsBuff = nullptr;
	alphasBuff = nullptr;
	trianglesBuff = nullptr;
	interpolatedTransformsBuff = nullptr;
	cameraBuff = nullptr;
	lightIndexOffsetByMeshIndexBuff = nullptr;
	lightIndexByTriIndexBuff = nullptr;
	imageMapDescsBuff = nullptr;
	// OpenCL memory buffers
	raysBuff = nullptr;
	hitsBuff = nullptr;
	tasksBuff = nullptr;
	tasksDirectLightBuff = nullptr;
	tasksStateBuff = nullptr;
	samplerSharedDataBuff = nullptr;
	samplesBuff = nullptr;
	sampleDataBuff = nullptr;
	taskStatsBuff = nullptr;
	eyePathInfosBuff = nullptr;
	directLightVolInfosBuff = nullptr;
	pixelFilterBuff = nullptr;
	pgicRadiancePhotonsBuff = nullptr;
	pgicRadiancePhotonsBVHNodesBuff = nullptr;
	pgicCausticPhotonsBuff = nullptr;
	pgicCausticPhotonsBVHNodesBuff = nullptr;

	// Check the kind of kernel cache to use
	string type = renderEngine->renderConfig->cfg.Get(Property("opencl.kernelcache")("PERSISTENT")).Get<string>();
	if (type == "PERSISTENT")
		kernelCache = new oclKernelPersistentCache("LUXCORE_" LUXCORE_VERSION_MAJOR "." LUXCORE_VERSION_MINOR);
	else if (type == "VOLATILE")
		kernelCache = new oclKernelVolatileCache();
	else if (type == "NONE")
		kernelCache = new oclKernelDummyCache();
	else
		throw runtime_error("Unknown opencl.kernelcache type: " + type);
	
	// OpenCL kernels
	initSeedKernel = nullptr;
	initKernel = nullptr;
	advancePathsKernel_MK_RT_NEXT_VERTEX = nullptr;
	advancePathsKernel_MK_HIT_NOTHING = nullptr;
	advancePathsKernel_MK_HIT_OBJECT = nullptr;
	advancePathsKernel_MK_RT_DL = nullptr;
	advancePathsKernel_MK_DL_ILLUMINATE = nullptr;
	advancePathsKernel_MK_DL_SAMPLE_BSDF = nullptr;
	advancePathsKernel_MK_GENERATE_NEXT_VERTEX_RAY = nullptr;
	advancePathsKernel_MK_SPLAT_SAMPLE = nullptr;
	advancePathsKernel_MK_NEXT_SAMPLE = nullptr;
	advancePathsKernel_MK_GENERATE_CAMERA_RAY = nullptr;

	initKernelArgsCount  = 0;

	gpuTaskStats = nullptr;
}

PathOCLBaseOCLRenderThread::~PathOCLBaseOCLRenderThread() {
	if (editMode)
		EndSceneEdit(EditActionList());
	if (started)
		Stop();

	FreeThreadFilms();

	delete filmClearKernel;
	delete kernelCache;
	delete initSeedKernel;
	delete initKernel;
	delete advancePathsKernel_MK_RT_NEXT_VERTEX;
	delete advancePathsKernel_MK_HIT_NOTHING;
	delete advancePathsKernel_MK_HIT_OBJECT;
	delete advancePathsKernel_MK_RT_DL;
	delete advancePathsKernel_MK_DL_ILLUMINATE;
	delete advancePathsKernel_MK_DL_SAMPLE_BSDF;
	delete advancePathsKernel_MK_GENERATE_NEXT_VERTEX_RAY;
	delete advancePathsKernel_MK_SPLAT_SAMPLE;
	delete advancePathsKernel_MK_NEXT_SAMPLE;
	delete advancePathsKernel_MK_GENERATE_CAMERA_RAY;

	delete[] gpuTaskStats;
}

void PathOCLBaseOCLRenderThread::Start() {
	started = true;

	InitRender();
	StartRenderThread();
}

void PathOCLBaseOCLRenderThread::Interrupt() {
	if (renderThread)
		renderThread->interrupt();
}

void PathOCLBaseOCLRenderThread::Stop() {
	StopRenderThread();

	// Transfer the films
	TransferThreadFilms(intersectionDevice->GetOpenCLQueue());
	FreeThreadFilmsOCLBuffers();

	// Scene buffers
	FreeOCLBuffer(&materialsBuff);
	FreeOCLBuffer(&texturesBuff);
	FreeOCLBuffer(&meshDescsBuff);
	FreeOCLBuffer(&scnObjsBuff);
	FreeOCLBuffer(&normalsBuff);
	FreeOCLBuffer(&triNormalsBuff);
	FreeOCLBuffer(&uvsBuff);
	FreeOCLBuffer(&colsBuff);
	FreeOCLBuffer(&alphasBuff);
	FreeOCLBuffer(&trianglesBuff);
	FreeOCLBuffer(&interpolatedTransformsBuff);
	FreeOCLBuffer(&vertsBuff);
	FreeOCLBuffer(&lightsBuff);
	FreeOCLBuffer(&envLightIndicesBuff);
	FreeOCLBuffer(&lightsDistributionBuff);
	FreeOCLBuffer(&infiniteLightSourcesDistributionBuff);
	FreeOCLBuffer(&dlscAllEntriesBuff);
	FreeOCLBuffer(&dlscDistributionsBuff);
	FreeOCLBuffer(&dlscBVHNodesBuff);
	FreeOCLBuffer(&elvcAllEntriesBuff);
	FreeOCLBuffer(&elvcDistributionsBuff);
	FreeOCLBuffer(&elvcBVHNodesBuff);
	FreeOCLBuffer(&envLightDistributionsBuff);
	FreeOCLBuffer(&cameraBuff);
	FreeOCLBuffer(&lightIndexOffsetByMeshIndexBuff);
	FreeOCLBuffer(&lightIndexByTriIndexBuff);
	FreeOCLBuffer(&imageMapDescsBuff);
	for (u_int i = 0; i < imageMapsBuff.size(); ++i)
		FreeOCLBuffer(&imageMapsBuff[i]);
	imageMapsBuff.resize(0);
	FreeOCLBuffer(&raysBuff);
	FreeOCLBuffer(&hitsBuff);
	FreeOCLBuffer(&tasksBuff);
	FreeOCLBuffer(&tasksDirectLightBuff);
	FreeOCLBuffer(&tasksStateBuff);
	FreeOCLBuffer(&samplerSharedDataBuff);
	FreeOCLBuffer(&samplesBuff);
	FreeOCLBuffer(&sampleDataBuff);
	FreeOCLBuffer(&taskStatsBuff);
	FreeOCLBuffer(&eyePathInfosBuff);
	FreeOCLBuffer(&directLightVolInfosBuff);
	FreeOCLBuffer(&pixelFilterBuff);
	FreeOCLBuffer(&pgicRadiancePhotonsBuff);
	FreeOCLBuffer(&pgicRadiancePhotonsBVHNodesBuff);
	FreeOCLBuffer(&pgicCausticPhotonsBuff);
	FreeOCLBuffer(&pgicCausticPhotonsBVHNodesBuff);

	started = false;

	// Film is deleted in the destructor to allow image saving after
	// the rendering is finished
}

void PathOCLBaseOCLRenderThread::StartRenderThread() {
	threadDone = false;

	// Create the thread for the rendering
	renderThread = new boost::thread(&PathOCLBaseOCLRenderThread::RenderThreadImpl, this);
}

void PathOCLBaseOCLRenderThread::StopRenderThread() {
	if (renderThread) {
		renderThread->interrupt();
		renderThread->join();
		delete renderThread;
		renderThread = nullptr;
	}
}

void PathOCLBaseOCLRenderThread::BeginSceneEdit() {
	StopRenderThread();
}

void PathOCLBaseOCLRenderThread::EndSceneEdit(const EditActionList &editActions) {
	//--------------------------------------------------------------------------
	// Update OpenCL buffers
	//
	// Note: if you edit this, you have probably to edit
	// RTPathOCLRenderThread::UpdateOCLBuffers().
	//--------------------------------------------------------------------------

	CompiledScene *cscene = renderEngine->compiledScene;

	if (cscene->wasCameraCompiled) {
		// Update Camera
		InitCamera();
	}

	if (cscene->wasGeometryCompiled) {
		// Update Scene Geometry
		InitGeometry();
	}

	if (cscene->wasImageMapsCompiled) {
		// Update Image Maps
		InitImageMaps();
	}

	if (cscene->wasMaterialsCompiled) {
		// Update Scene Textures and Materials
		InitTextures();
		InitMaterials();
	}

	if (cscene->wasSceneObjectsCompiled) {
		// Update Mesh <=> Material relation
		InitSceneObjects();
	}

	if  (cscene->wasLightsCompiled) {
		// Update Scene Lights
		InitLights();
	}

	if (cscene->wasPhotonGICompiled) {
		// Update PhotonGI cache
		InitPhotonGI();
	}

	//--------------------------------------------------------------------------
	// Recompile Kernels if required
	//--------------------------------------------------------------------------

	// The following actions can require a kernel re-compilation:
	// - Dynamic code generation of textures and materials;
	// - Material types edit;
	// - Light types edit;
	// - Image types edit;
	// - Geometry type edit;
	// - etc.
	InitKernels();

	if (editActions.HasAnyAction()) {
		SetKernelArgs();

		//----------------------------------------------------------------------
		// Execute initialization kernels
		//----------------------------------------------------------------------

		cl::CommandQueue &oclQueue = intersectionDevice->GetOpenCLQueue();

		// Clear the frame buffers
		ClearThreadFilms(oclQueue);
	}

	// Reset statistics in order to be more accurate
	intersectionDevice->ResetPerformaceStats();

	StartRenderThread();
}

bool PathOCLBaseOCLRenderThread::HasDone() const {
	return (renderThread == nullptr) || threadDone;
}

void PathOCLBaseOCLRenderThread::WaitForDone() const {
	if (renderThread)
		renderThread->join();
}

void PathOCLBaseOCLRenderThread::IncThreadFilms() {
	threadFilms.push_back(new ThreadFilm(this));

	// Initialize the new thread film
	u_int threadFilmWidth, threadFilmHeight, threadFilmSubRegion[4];
	GetThreadFilmSize(&threadFilmWidth, &threadFilmHeight, threadFilmSubRegion);

	threadFilms.back()->Init(renderEngine->film, threadFilmWidth, threadFilmHeight,
			threadFilmSubRegion);
}

void PathOCLBaseOCLRenderThread::ClearThreadFilms(cl::CommandQueue &oclQueue) {
	// Clear all thread films
	BOOST_FOREACH(ThreadFilm *threadFilm, threadFilms)
		threadFilm->ClearFilm(oclQueue, *filmClearKernel, filmClearWorkGroupSize);
}

void PathOCLBaseOCLRenderThread::TransferThreadFilms(cl::CommandQueue &oclQueue) {
	// Clear all thread films
	BOOST_FOREACH(ThreadFilm *threadFilm, threadFilms)
		threadFilm->RecvFilm(oclQueue);
}

void PathOCLBaseOCLRenderThread::FreeThreadFilmsOCLBuffers() {
	BOOST_FOREACH(ThreadFilm *threadFilm, threadFilms)
		threadFilm->FreeAllOCLBuffers();
}

void PathOCLBaseOCLRenderThread::FreeThreadFilms() {
	BOOST_FOREACH(ThreadFilm *threadFilm, threadFilms)
		delete threadFilm;
	threadFilms.clear();
}

#endif
