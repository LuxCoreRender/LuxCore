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
#include "slg/engines/rtbiaspathocl/rtbiaspathocl.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// BiasPathOCLRenderEngine
//------------------------------------------------------------------------------

BiasPathOCLRenderEngine::BiasPathOCLRenderEngine(const RenderConfig *rcfg, Film *flm,
		boost::mutex *flmMutex) : PathOCLBaseRenderEngine(rcfg, flm, flmMutex) {
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

	Properties defaultProps = (GetType() == BIASPATHOCL) ?
		BiasPathOCLRenderEngine::GetDefaultProps() :
		RTBiasPathOCLRenderEngine::GetDefaultProps();

	// Path depth settings
	maxPathDepth.depth = Max(0, cfg.Get(defaultProps.Get("biaspath.pathdepth.total")).Get<int>());
	maxPathDepth.diffuseDepth = Max(0, cfg.Get(defaultProps.Get("biaspath.pathdepth.diffuse")).Get<int>());
	maxPathDepth.glossyDepth = Max(0, cfg.Get(defaultProps.Get("biaspath.pathdepth.glossy")).Get<int>());
	maxPathDepth.specularDepth = Max(0, cfg.Get(defaultProps.Get("biaspath.pathdepth.specular")).Get<int>());

	// Samples settings
	aaSamples = (GetType() == BIASPATHOCL) ?
		Max(1, cfg.Get(defaultProps.Get("biaspath.sampling.aa.size")).Get<int>()) :
		1;
	diffuseSamples = Max(0, cfg.Get(defaultProps.Get("biaspath.sampling.diffuse.size")).Get<int>());
	glossySamples = Max(0, cfg.Get(defaultProps.Get("biaspath.sampling.glossy.size")).Get<int>());
	specularSamples = Max(0, cfg.Get(defaultProps.Get("biaspath.sampling.specular.size")).Get<int>());
	directLightSamples = Max(1, cfg.Get(defaultProps.Get("biaspath.sampling.directlight.size")).Get<int>());

	// Clamping settings
	// clamping.radiance.maxvalue is the old radiance clamping, now converted in variance clamping
	sqrtVarianceClampMaxValue = cfg.Get(Property("biaspath.clamping.radiance.maxvalue")(0.f)).Get<float>();
	if (cfg.IsDefined("biaspath.clamping.variance.maxvalue"))
		sqrtVarianceClampMaxValue = cfg.Get(defaultProps.Get("biaspath.clamping.variance.maxvalue")).Get<float>();
	sqrtVarianceClampMaxValue = Max(0.f, sqrtVarianceClampMaxValue);
	pdfClampValue = Max(0.f, cfg.Get(defaultProps.Get("biaspath.clamping.pdf.value")).Get<float>());

	// Light settings
	lowLightThreashold = Max(0.f, cfg.Get(defaultProps.Get("biaspath.lights.lowthreshold")).Get<float>());
	nearStartLight = Max(0.f, cfg.Get(defaultProps.Get("biaspath.lights.nearstart")).Get<float>());
	firstVertexLightSampleCount = Max(1, cfg.Get(defaultProps.Get("biaspath.lights.firstvertexsamples")).Get<int>());

	maxTilePerDevice = cfg.Get(Property("biaspathocl.devices.maxtiles")(16)).Get<u_int>();

	PrintSamplesInfo();

	InitPixelFilterDistribution();

	//--------------------------------------------------------------------------
	// Tile related parameters
	//--------------------------------------------------------------------------

	film->Reset();

	Properties tileProps = renderConfig->cfg.GetAllProperties("tile");
	if (GetType() == RTBIASPATHOCL) {
		tileProps.Delete("tile.size");

		// Check if I'm going to use a single device
		u_int tileWidth, tileHeight;
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

		// Tile width must be a multiple of RESOLUTION_REDUCTION to support RT variable resolution rendering
		RTBiasPathOCLRenderEngine *rtengine = (RTBiasPathOCLRenderEngine *)this;
		tileWidth = RoundUp(tileWidth, Max(rtengine->previewResolutionReduction, rtengine->resolutionReduction));

		tileProps <<
				Property("tile.size.x")(tileWidth) <<
				Property("tile.size.y")(tileHeight);
	}

	tileRepository = TileRepository::FromProperties(tileProps);
	if (GetType() == RTBIASPATHOCL)
		tileRepository->enableMultipassRendering = false;
	tileRepository->varianceClamping = VarianceClamping(sqrtVarianceClampMaxValue);
	tileRepository->InitTiles(*film);

	// I don't know yet the workgroup size of each device so I can not
	// round up task count to be a multiple of workgroups size of all devices
	// used. Rounding to 8192 is a simple trick based on the assumption that
	// workgroup size is a power of 2 and <= 8192.
	taskCount = RoundUp<u_int>(tileRepository->tileWidth * tileRepository->tileHeight * aaSamples * aaSamples, 8192);
	//SLG_LOG("[BiasPathOCLRenderEngine] OpenCL task count: " << taskCount);
	
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
	if (GetType() != RTBIASPATHOCL) {
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

//------------------------------------------------------------------------------
// Static methods used by RenderEngineRegistry
//------------------------------------------------------------------------------

Properties BiasPathOCLRenderEngine::ToProperties(const Properties &cfg) {
	return OCLRenderEngine::ToProperties(cfg) <<
			cfg.Get(GetDefaultProps().Get("renderengine.type")) <<
			cfg.Get(GetDefaultProps().Get("biaspath.pathdepth.total")) <<
			cfg.Get(GetDefaultProps().Get("biaspath.pathdepth.diffuse")) <<
			cfg.Get(GetDefaultProps().Get("biaspath.pathdepth.glossy")) <<
			cfg.Get(GetDefaultProps().Get("biaspath.pathdepth.specular")) <<
			cfg.Get(GetDefaultProps().Get("biaspath.sampling.aa.size")) <<
			cfg.Get(GetDefaultProps().Get("biaspath.sampling.diffuse.size")) <<
			cfg.Get(GetDefaultProps().Get("biaspath.sampling.glossy.size")) <<
			cfg.Get(GetDefaultProps().Get("biaspath.sampling.specular.size")) <<
			cfg.Get(GetDefaultProps().Get("biaspath.sampling.directlight.size")) <<
			cfg.Get(GetDefaultProps().Get("biaspath.clamping.variance.maxvalue")) <<
			cfg.Get(GetDefaultProps().Get("biaspath.clamping.pdf.value")) <<
			cfg.Get(GetDefaultProps().Get("biaspath.lights.lowthreshold")) <<
			cfg.Get(GetDefaultProps().Get("biaspath.lights.nearstart")) <<
			cfg.Get(GetDefaultProps().Get("biaspath.lights.firstvertexsamples")) <<
			cfg.Get(GetDefaultProps().Get("biaspathocl.devices.maxtiles")) <<
			TileRepository::ToProperties(cfg);
}

RenderEngine *BiasPathOCLRenderEngine::FromProperties(const RenderConfig *rcfg, Film *flm, boost::mutex *flmMutex) {
	return new BiasPathOCLRenderEngine(rcfg, flm, flmMutex);
}

const Properties &BiasPathOCLRenderEngine::GetDefaultProps() {
	static Properties props = Properties() <<
			OCLRenderEngine::GetDefaultProps() <<
			Property("renderengine.type")(GetObjectTag()) <<
			Property("biaspath.pathdepth.total")(10) <<
			Property("biaspath.pathdepth.diffuse")(4) <<
			Property("biaspath.pathdepth.glossy")(3) <<
			Property("biaspath.pathdepth.specular")(3) <<
			Property("biaspath.sampling.aa.size")(3) <<
			Property("biaspath.sampling.diffuse.size")(2) <<
			Property("biaspath.sampling.glossy.size")(2) <<
			Property("biaspath.sampling.specular.size")(2) <<
			Property("biaspath.sampling.directlight.size")(1) <<
			Property("biaspath.clamping.variance.maxvalue")(0.f) <<
			Property("biaspath.clamping.pdf.value")(0.f) <<
			Property("biaspath.lights.lowthreshold")(0.f) <<
			Property("biaspath.lights.nearstart")(.001f) <<
			Property("biaspath.lights.firstvertexsamples")(4) <<
			Property("biaspathocl.devices.maxtiles")(16) <<
			TileRepository::GetDefaultProps();

	return props;
}

#endif
