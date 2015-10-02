/***************************************************************************
 * Copyright 1998-2015 by authors (see AUTHORS.txt)                        *
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

#include <boost/format.hpp>

#include "luxrays/core/oclintersectiondevice.h"

#include "slg/slg.h"
#include "slg/engines/biaspathocl/biaspathocl.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// BiasPathOCLRenderEngine
//------------------------------------------------------------------------------

BiasPathOCLRenderEngine::BiasPathOCLRenderEngine(const RenderConfig *rcfg, Film *flm, boost::mutex *flmMutex,
			const bool realTime) :
		PathOCLBaseRenderEngine(rcfg, flm, flmMutex, realTime) {
	pixelFilterDistribution = NULL;
	tileRepository = NULL;
}

BiasPathOCLRenderEngine::~BiasPathOCLRenderEngine() {
	delete[] pixelFilterDistribution;
	delete tileRepository;
}

void BiasPathOCLRenderEngine::PrintSamplesInfo() const {
	// There is pretty much the same method in BiasPathCPURenderEngine

	// Pixel samples
	const u_int aaSamplesCount = aaSamples * aaSamples;
	SLG_LOG("[BiasPathOCLRenderEngine] Pixel samples: " << aaSamplesCount);

	// Diffuse samples
	const int maxDiffusePathDepth = Max<int>(0, Min<int>(maxPathDepth.depth, maxPathDepth.diffuseDepth - 1));
	const u_int diffuseSamplesCount = aaSamplesCount * (diffuseSamples * diffuseSamples);
	const u_int maxDiffuseSamplesCount = diffuseSamplesCount * maxDiffusePathDepth;
	SLG_LOG("[BiasPathOCLRenderEngine] Diffuse samples: " << diffuseSamplesCount <<
			" (with max. bounces " << maxDiffusePathDepth <<": " << maxDiffuseSamplesCount << ")");

	// Glossy samples
	const int maxGlossyPathDepth = Max<int>(0, Min<int>(maxPathDepth.depth, maxPathDepth.glossyDepth - 1));
	const u_int glossySamplesCount = aaSamplesCount * (glossySamples * glossySamples);
	const u_int maxGlossySamplesCount = glossySamplesCount * maxGlossyPathDepth;
	SLG_LOG("[BiasPathOCLRenderEngine] Glossy samples: " << glossySamplesCount <<
			" (with max. bounces " << maxGlossyPathDepth <<": " << maxGlossySamplesCount << ")");

	// Specular samples
	const int maxSpecularPathDepth = Max<int>(0, Min<int>(maxPathDepth.depth, maxPathDepth.specularDepth - 1));
	const u_int specularSamplesCount = aaSamplesCount * (specularSamples * specularSamples);
	const u_int maxSpecularSamplesCount = specularSamplesCount * maxSpecularPathDepth;
	SLG_LOG("[BiasPathOCLRenderEngine] Specular samples: " << specularSamplesCount <<
			" (with max. bounces " << maxSpecularPathDepth <<": " << maxSpecularSamplesCount << ")");

	// Direct light samples
	const u_int directLightSamplesCount = aaSamplesCount * firstVertexLightSampleCount *
			(directLightSamples * directLightSamples) * renderConfig->scene->lightDefs.GetSize();
	SLG_LOG("[BiasPathOCLRenderEngine] Direct light samples on first hit: " << directLightSamplesCount);

	// Total samples for a pixel with hit on diffuse surfaces
	SLG_LOG("[BiasPathOCLRenderEngine] Total samples for a pixel with hit on diffuse surfaces: " <<
			// Direct light sampling on first hit
			directLightSamplesCount +
			// Diffuse samples
			maxDiffuseSamplesCount +
			// Direct light sampling for diffuse samples
			diffuseSamplesCount * Max<int>(0, maxDiffusePathDepth - 1));
}

PathOCLBaseRenderThread *BiasPathOCLRenderEngine::CreateOCLThread(const u_int index,
	OpenCLIntersectionDevice *device) {
	return new BiasPathOCLRenderThread(index, device, this);
}

void BiasPathOCLRenderEngine::InitPixelFilterDistribution() {
	// Compile sample distribution
	delete[] pixelFilterDistribution;
	const FilterDistribution filterDistribution(pixelFilter, 64);
	pixelFilterDistribution = CompiledScene::CompileDistribution2D(
			filterDistribution.GetDistribution2D(), &pixelFilterDistributionSize);
}

void BiasPathOCLRenderEngine::StartLockLess() {
	const Properties &cfg = renderConfig->cfg;

	printedRenderingTime = false;
	film->Reset();

	//--------------------------------------------------------------------------
	// Rendering parameters
	//--------------------------------------------------------------------------

	// Path depth settings
	maxPathDepth.depth = Max(0, cfg.Get(Property("biaspath.pathdepth.total")(10)).Get<int>());
	maxPathDepth.diffuseDepth = Max(0, cfg.Get(Property("biaspath.pathdepth.diffuse")(4)).Get<int>());
	maxPathDepth.glossyDepth = Max(0, cfg.Get(Property("biaspath.pathdepth.glossy")(3)).Get<int>());
	maxPathDepth.specularDepth = Max(0, cfg.Get(Property("biaspath.pathdepth.specular")(3)).Get<int>());

	// Samples settings
	aaSamples = Max(1, cfg.Get(Property("biaspath.sampling.aa.size")(
			(GetEngineType() == RTBIASPATHOCL) ? 1 : 3)).Get<int>());
	diffuseSamples = Max(0, cfg.Get(Property("biaspath.sampling.diffuse.size")(
			(GetEngineType() == RTBIASPATHOCL) ? 1 : 2)).Get<int>());
	glossySamples = Max(0, cfg.Get(Property("biaspath.sampling.glossy.size")(
			(GetEngineType() == RTBIASPATHOCL) ? 1 : 2)).Get<int>());
	specularSamples = Max(0, cfg.Get(Property("biaspath.sampling.specular.size")(1)).Get<int>());
	directLightSamples = Max(1, cfg.Get(Property("biaspath.sampling.directlight.size")(1)).Get<int>());

	// Clamping settings
	// clamping.radiance.maxvalue is the old radiance clamping, now converted in variance clamping
	sqrtVarianceClampMaxValue = Max(0.f,
			cfg.Get(Property("biaspath.clamping.variance.maxvalue")(
				cfg.Get(Property("biaspath.clamping.radiance.maxvalue")(0.f)).Get<float>())
			).Get<float>());
	pdfClampValue = Max(0.f, cfg.Get(Property("biaspath.clamping.pdf.value")(0.f)).Get<float>());

	// Light settings
	lowLightThreashold = Max(0.f, cfg.Get(Property("biaspath.lights.lowthreshold")(0.f)).Get<float>());
	nearStartLight = Max(0.f, cfg.Get(Property("biaspath.lights.nearstart")(.001f)).Get<float>());
	firstVertexLightSampleCount = Max(1, cfg.Get(Property("biaspath.lights.firstvertexsamples")(
			(GetEngineType() == RTBIASPATHOCL) ? 1 : 4)).Get<int>());

	PrintSamplesInfo();

	// Tile related parameters
	u_int tileWidth = 32;
	u_int tileHeight = 32;
	if (GetEngineType() == RTBIASPATHOCL) {
		// Check if I'm going to use a single device
		if (intersectionDevices.size() == 1) {
			// The best configuration, with a single device, is to use a tile
			// as large as the complete image
			tileWidth = film->GetWidth();
			tileHeight = film->GetHeight();
		} else {
			// One slice for each device
			tileWidth = (film->GetWidth() + 1) / intersectionDevices.size();
			tileHeight = film->GetHeight();
		}
	} else {
		if (renderConfig->cfg.IsDefined("tile.size"))
			tileWidth = tileHeight = Max(renderConfig->cfg.Get(Property("tile.size")(32)).Get<u_int>(), 8u);
		tileWidth = Max(renderConfig->cfg.Get(Property("tile.size.x")(tileWidth)).Get<u_int>(), 8u);
		tileHeight = Max(renderConfig->cfg.Get(Property("tile.size.y")(tileHeight)).Get<u_int>(), 8u);
	}
	tileRepository = new TileRepository(tileWidth, tileHeight);

	tileRepository->maxPassCount = renderConfig->GetProperty("batch.haltdebug").Get<u_int>();
	if (GetEngineType() == RTBIASPATHOCL)
		tileRepository->enableMultipassRendering = false;
	else
		tileRepository->enableMultipassRendering = cfg.Get(Property("tile.multipass.enable")(true)).Get<bool>();
	tileRepository->convergenceTestThreshold = cfg.Get(Property("tile.multipass.convergencetest.threshold")(
			cfg.Get(Property("tile.multipass.convergencetest.threshold256")(6.f)).Get<float>() / 256.f)).Get<float>();
	tileRepository->convergenceTestThresholdReduction = cfg.Get(Property("tile.multipass.convergencetest.threshold.reduction")(0.f)).Get<float>();
	tileRepository->convergenceTestWarmUpSamples = cfg.Get(Property("tile.multipass.convergencetest.warmup.count")(32)).Get<u_int>();
	tileRepository->totalSamplesPerPixel = aaSamples * aaSamples;
	tileRepository->varianceClamping = VarianceClamping(sqrtVarianceClampMaxValue);

	tileRepository->InitTiles(*film);

	maxTilePerDevice = cfg.Get(Property("biaspath.devices.maxtiles")(16)).Get<u_int>();

	taskCount = tileRepository->tileWidth * tileRepository->tileHeight * tileRepository->totalSamplesPerPixel;

	InitPixelFilterDistribution();
	
	PathOCLBaseRenderEngine::StartLockLess();
}

void BiasPathOCLRenderEngine::StopLockLess() {
	PathOCLBaseRenderEngine::StopLockLess();

	delete[] pixelFilterDistribution;
	pixelFilterDistribution = NULL;
	delete tileRepository;
	tileRepository = NULL;
}

void BiasPathOCLRenderEngine::EndSceneEditLockLess(const EditActionList &editActions) {
	if (GetEngineType() != RTBIASPATHOCL) {
		// RTBIASPATHOCL will InitTiles() on next frame
		tileRepository->Clear();
		tileRepository->InitTiles(*film);
		printedRenderingTime = false;
	}

	PathOCLBaseRenderEngine::EndSceneEditLockLess(editActions);
}

void BiasPathOCLRenderEngine::UpdateCounters() {
	// Update the sample count statistic
	samplesCount = film->GetTotalSampleCount();

	// Update the ray count statistic
	double totalCount = 0.0;
	for (size_t i = 0; i < renderThreads.size(); ++i) {
		const BiasPathOCLRenderThread *thread = (BiasPathOCLRenderThread *)renderThreads[i];
		totalCount += thread->intersectionDevice->GetTotalRaysCount();
	}
	raysCount = totalCount;

	if (!tileRepository->done) {
		// Update the time only while rendering is not finished
		elapsedTime = WallClockTime() - startTime;
	} else
		convergence = 1.f;
}

#endif
