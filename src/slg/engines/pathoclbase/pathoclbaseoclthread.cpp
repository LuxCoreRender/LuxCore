/***************************************************************************
 * Copyright 1998-2020 by authors (see AUTHORS.txt)                        *
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
#include "luxrays/devices/ocldevice.h"
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
		HardwareIntersectionDevice *device, PathOCLBaseRenderEngine *re) {
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
	materialEvalOpsBuff = nullptr;
	materialEvalStackBuff = nullptr;
	texturesBuff = nullptr;
	textureEvalOpsBuff = nullptr;
	textureEvalStackBuff = nullptr;
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
	elvcTileDistributionOffsetsBuff = nullptr;
	elvcBVHNodesBuff = nullptr;
	envLightDistributionsBuff = nullptr;
	vertsBuff = nullptr;
	normalsBuff = nullptr;
	triNormalsBuff = nullptr;
	uvsBuff = nullptr;
	colsBuff = nullptr;
	alphasBuff = nullptr;
	vertexAOVBuff = nullptr;
	triAOVBuff = nullptr;
	trianglesBuff = nullptr;
	interpolatedTransformsBuff = nullptr;
	cameraBuff = nullptr;
	lightIndexOffsetByMeshIndexBuff = nullptr;
	lightIndexByTriIndexBuff = nullptr;
	imageMapDescsBuff = nullptr;
	pgicRadiancePhotonsBuff = nullptr;
	pgicRadiancePhotonsValuesBuff = nullptr;
	pgicRadiancePhotonsBVHNodesBuff = nullptr;
	pgicCausticPhotonsBuff = nullptr;
	pgicCausticPhotonsBVHNodesBuff = nullptr;

	// OpenCL task related buffers
	raysBuff = nullptr;
	hitsBuff = nullptr;
	taskConfigBuff = nullptr;
	tasksBuff = nullptr;
	tasksDirectLightBuff = nullptr;
	tasksStateBuff = nullptr;
	samplerSharedDataBuff = nullptr;
	samplesBuff = nullptr;
	sampleDataBuff = nullptr;
	sampleResultsBuff = nullptr;
	taskStatsBuff = nullptr;
	eyePathInfosBuff = nullptr;
	directLightVolInfosBuff = nullptr;
	pixelFilterBuff = nullptr;

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
	TransferThreadFilms(intersectionDevice);
	FreeThreadFilmsOCLBuffers();

	// Scene buffers
	intersectionDevice->FreeBuffer(&materialsBuff);
	intersectionDevice->FreeBuffer(&materialEvalOpsBuff);
	intersectionDevice->FreeBuffer(&materialEvalStackBuff);
	intersectionDevice->FreeBuffer(&texturesBuff);
	intersectionDevice->FreeBuffer(&textureEvalOpsBuff);
	intersectionDevice->FreeBuffer(&textureEvalStackBuff);
	intersectionDevice->FreeBuffer(&meshDescsBuff);
	intersectionDevice->FreeBuffer(&scnObjsBuff);
	intersectionDevice->FreeBuffer(&normalsBuff);
	intersectionDevice->FreeBuffer(&triNormalsBuff);
	intersectionDevice->FreeBuffer(&uvsBuff);
	intersectionDevice->FreeBuffer(&colsBuff);
	intersectionDevice->FreeBuffer(&alphasBuff);
	intersectionDevice->FreeBuffer(&triAOVBuff);
	intersectionDevice->FreeBuffer(&trianglesBuff);
	intersectionDevice->FreeBuffer(&interpolatedTransformsBuff);
	intersectionDevice->FreeBuffer(&vertsBuff);
	intersectionDevice->FreeBuffer(&lightsBuff);
	intersectionDevice->FreeBuffer(&envLightIndicesBuff);
	intersectionDevice->FreeBuffer(&lightsDistributionBuff);
	intersectionDevice->FreeBuffer(&infiniteLightSourcesDistributionBuff);
	intersectionDevice->FreeBuffer(&dlscAllEntriesBuff);
	intersectionDevice->FreeBuffer(&dlscDistributionsBuff);
	intersectionDevice->FreeBuffer(&dlscBVHNodesBuff);
	intersectionDevice->FreeBuffer(&elvcAllEntriesBuff);
	intersectionDevice->FreeBuffer(&elvcDistributionsBuff);
	intersectionDevice->FreeBuffer(&elvcTileDistributionOffsetsBuff);
	intersectionDevice->FreeBuffer(&elvcBVHNodesBuff);
	intersectionDevice->FreeBuffer(&envLightDistributionsBuff);
	intersectionDevice->FreeBuffer(&cameraBuff);
	intersectionDevice->FreeBuffer(&lightIndexOffsetByMeshIndexBuff);
	intersectionDevice->FreeBuffer(&lightIndexByTriIndexBuff);
	intersectionDevice->FreeBuffer(&imageMapDescsBuff);

	for (u_int i = 0; i < imageMapsBuff.size(); ++i)
		intersectionDevice->FreeBuffer(&imageMapsBuff[i]);
	imageMapsBuff.resize(0);
	intersectionDevice->FreeBuffer(&pgicRadiancePhotonsBuff);
	intersectionDevice->FreeBuffer(&pgicRadiancePhotonsValuesBuff);
	intersectionDevice->FreeBuffer(&pgicRadiancePhotonsBVHNodesBuff);
	intersectionDevice->FreeBuffer(&pgicCausticPhotonsBuff);
	intersectionDevice->FreeBuffer(&pgicCausticPhotonsBVHNodesBuff);

	// OpenCL task related buffers
	intersectionDevice->FreeBuffer(&raysBuff);
	intersectionDevice->FreeBuffer(&hitsBuff);
	intersectionDevice->FreeBuffer(&taskConfigBuff);
	intersectionDevice->FreeBuffer(&tasksBuff);
	intersectionDevice->FreeBuffer(&tasksDirectLightBuff);
	intersectionDevice->FreeBuffer(&tasksStateBuff);
	intersectionDevice->FreeBuffer(&samplerSharedDataBuff);
	intersectionDevice->FreeBuffer(&samplesBuff);
	intersectionDevice->FreeBuffer(&sampleDataBuff);
	intersectionDevice->FreeBuffer(&sampleResultsBuff);
	intersectionDevice->FreeBuffer(&taskStatsBuff);
	intersectionDevice->FreeBuffer(&eyePathInfosBuff);
	intersectionDevice->FreeBuffer(&directLightVolInfosBuff);
	intersectionDevice->FreeBuffer(&pixelFilterBuff);

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

		// Clear the frame buffers
		ClearThreadFilms();
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

void PathOCLBaseOCLRenderThread::ClearThreadFilms() {
	// Clear all thread films
	BOOST_FOREACH(ThreadFilm *threadFilm, threadFilms)
		threadFilm->ClearFilm(intersectionDevice, filmClearKernel, filmClearWorkGroupSize);
}

void PathOCLBaseOCLRenderThread::TransferThreadFilms(HardwareIntersectionDevice *intersectionDevice) {
	// Clear all thread films
	BOOST_FOREACH(ThreadFilm *threadFilm, threadFilms)
		threadFilm->RecvFilm(intersectionDevice);
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
