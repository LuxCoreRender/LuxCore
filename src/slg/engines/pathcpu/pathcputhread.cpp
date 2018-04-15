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

#include "slg/engines/pathcpu/pathcpu.h"
#include "slg/volumes/volume.h"
#include "slg/utils/varianceclamping.h"

#include "bcd/core/Denoiser.h"
#include "bcd/core/MultiscaleDenoiser.h"
#include "bcd/core/IDenoiser.h"
#include "bcd/core/DeepImage.h"
// for debug output
#include <OpenImageIO/imageio.h>
#include <OpenImageIO/imagebuf.h>

using namespace std;
using namespace luxrays;
using namespace slg;

OIIO_NAMESPACE_USING


// Copied from bcd/src/cli/main.cpp
static void checkAndPutToZeroNegativeInfNaNValues(bcd::DeepImage<float>& io_rImage, bool i_verbose = false)
{
	using std::isnan;
	using std::isinf;
	int width = io_rImage.getWidth();
	int height = io_rImage.getHeight();
	int depth = io_rImage.getDepth();
	vector<float> values(depth);
	float val = 0.f;
	
	int countNegative = 0;
	int countNan = 0;
	int countInf = 0;
	
	for (int line = 0; line < height; line++)
		for (int col = 0; col < width; col++)
		{
			for(int z = 0; z < depth; ++z)
			{
				val = values[z] = io_rImage.get(line, col, z);
				if(val < 0 || isnan(val) || isinf(val))
				{
					io_rImage.set(line, col, z, 0.f);
					
					if (val < 0) 
						countNegative++;
					if (isnan(val))
						countNan++;
					if (isinf(val))
						countInf++;
				}
			}
		}
		
	if (i_verbose) 
		cout << "Negative: " << countNegative << " | NaN: " << countNan << " | Inf: " << countInf << endl;
}

/*static void reorderDataForWritingEXR(std::vector<float>& o_rData, const bcd::Deepimf& i_rImage)
{
	int width = i_rImage.getWidth();
	int height = i_rImage.getHeight();
	int depth = i_rImage.getDepth();
	o_rData.resize(width*height*depth);
	for(int l = 0; l < height; l++)
		for(int c = 0; c < width; c++)
			for (int z = 0; z < depth; z++)
				o_rData[(z*height + l)*width + c] = i_rImage.get(l, c, z);
}*/

static void WriteEXR(const bcd::Deepimf &img, const string &fileName) {
	ImageSpec spec(img.getWidth(), img.getHeight(), img.getDepth(), TypeDesc::FLOAT);
 	ImageBuf buffer(spec);
 	for (ImageBuf::ConstIterator<float> it(buffer); !it.done(); ++it) {
 		u_int x = it.x();
 		u_int y = it.y();
 		float *pixel = (float *)buffer.pixeladdr(x, y, 0);
 		// Note that x and y are not in the usual order in DeepImage get/set methods
 		pixel[0] = img.get(y, x, 0);
 		pixel[1] = img.get(y, x, 1);
 		pixel[2] = img.get(y, x, 2);
 	}
 	buffer.write(fileName);
}

//------------------------------------------------------------------------------
// PathCPU RenderThread
//------------------------------------------------------------------------------

PathCPURenderThread::PathCPURenderThread(PathCPURenderEngine *engine,
		const u_int index, IntersectionDevice *device) :
		CPUNoTileRenderThread(engine, index, device)
{}

void PathCPURenderThread::RenderFunc() {
	//SLG_LOG("[PathCPURenderEngine::" << threadIndex << "] Rendering thread started");

	//--------------------------------------------------------------------------
	// Initialization
	//--------------------------------------------------------------------------

	PathCPURenderEngine *engine = (PathCPURenderEngine *)renderEngine;
	const PathTracer &pathTracer = engine->pathTracer;
	// (engine->seedBase + 1) seed is used for sharedRndGen
	RandomGenerator *rndGen = new RandomGenerator(engine->seedBase + 1 + threadIndex);

	// Setup the sampler
	Sampler *sampler = engine->renderConfig->AllocSampler(rndGen, threadFilm, NULL,
			engine->samplerSharedData);
	sampler->RequestSamples(pathTracer.sampleSize);
	
	VarianceClamping varianceClamping(pathTracer.sqrtVarianceClampMaxValue);

	// Initialize SampleResult
	vector<SampleResult> sampleResults(1);
	SampleResult &sampleResult = sampleResults[0];
	pathTracer.InitSampleResults(engine->film, sampleResults);

	//--------------------------------------------------------------------------
	// Trace paths
	//--------------------------------------------------------------------------

	bcd::SamplesAccumulator *samplesAccumulator = NULL;
	float maxSampleValue = 0.f;
	float samplesAccumulatorScale = 1.f;

	// I can not use engine->renderConfig->GetProperty() here because the
	// RenderConfig properties cache is not thread safe
	const u_int filmWidth = threadFilm->GetWidth();
	const u_int filmHeight = threadFilm->GetHeight();
	const u_int haltDebug = engine->renderConfig->cfg.Get(Property("batch.haltdebug")(0u)).Get<u_int>() *
		filmWidth * filmHeight;

	for (u_int steps = 0; !boost::this_thread::interruption_requested(); ++steps) {
		// Check if we are in pause mode
		if (engine->pauseMode) {
			// Check every 100ms if I have to continue the rendering
			while (!boost::this_thread::interruption_requested() && engine->pauseMode)
				boost::this_thread::sleep(boost::posix_time::millisec(100));

			if (boost::this_thread::interruption_requested())
				break;
		}

		pathTracer.RenderSample(device, engine->renderConfig->scene, threadFilm, sampler, sampleResults);
		
		// Variance clamping
		if (varianceClamping.hasClamping())
			varianceClamping.Clamp(*threadFilm, sampleResult);

		if (samplesAccumulator) {
			// For BCD denoiser (note: we now add a clamped sample - the input colors are clamped, too)
			const int line = filmHeight - sampleResult.pixelY - 1;
			const int column = sampleResult.pixelX;
			const float sampleR = sampleResult.radiance[0].c[0] * samplesAccumulatorScale;
			const float sampleG = sampleResult.radiance[0].c[1] * samplesAccumulatorScale;
			const float sampleB = sampleResult.radiance[0].c[2] * samplesAccumulatorScale;
			const float weight = 1.f;
			samplesAccumulator->addSample(line, column,
												 sampleR, sampleG, sampleB,
												 weight);
		} else {
			// Warm up period to evaluate maxSampleValue
			
			maxSampleValue = Max(sampleResult.radiance[0].c[0], Max(sampleResult.radiance[0].c[1], Max(maxSampleValue, sampleResult.radiance[0].c[2])));

			// Check if the warm up period is over
			if (threadFilm->GetTotalSampleCount() / (threadFilm->GetWidth() * threadFilm->GetHeight()) > 8) {
				SLG_LOG("Max. sample value: " << maxSampleValue);

				samplesAccumulatorScale = (maxSampleValue == 0.f) ? 1.f : 1.f / maxSampleValue;

				bcd::HistogramParameters histogramParameters;
				histogramParameters.m_maxValue = 0.f;

				samplesAccumulator = new bcd::SamplesAccumulator(threadFilm->GetWidth(), threadFilm->GetHeight(),
						histogramParameters);
			}
		}

		sampler->NextSample(sampleResults);

#ifdef WIN32
		// Work around Windows bad scheduling
		renderThread->yield();
#endif

		// Check halt conditions
		if ((haltDebug > 0u) && (steps >= haltDebug))
			break;
		if (engine->film->GetConvergence() == 1.f)
			break;
	}

	delete sampler;
	delete rndGen;

	threadDone = true;
	
	if (samplesAccumulator) {
		SLG_LOG("Collecting BCD stats");
		bcd::SamplesStatisticsImages bcdStats = samplesAccumulator->getSamplesStatistics();

		bcd::DenoiserInputs inputs;
		bcd::DenoiserOutputs outputs;
		bcd::DenoiserParameters parameters;

		SLG_LOG("Getting film pixels");
		GenericFrameBuffer<4, 1, float> *frameBuffer = threadFilm->channel_RADIANCE_PER_PIXEL_NORMALIZEDs[0];
		bcd::Deepimf inputColors(threadFilm->GetWidth(), threadFilm->GetHeight(), 3);
		SLG_LOG("Copying film to inputColors");
		for(unsigned int y = 0; y < threadFilm->GetHeight(); ++y) {
			for(unsigned int x = 0; x < threadFilm->GetWidth(); ++x) {
				float weightedPixel[3];
				frameBuffer->GetWeightedPixel(x, filmHeight - y - 1, weightedPixel);

				inputColors.set(y, x, 0, weightedPixel[0] * samplesAccumulatorScale);
				inputColors.set(y, x, 1, weightedPixel[1] * samplesAccumulatorScale);
				inputColors.set(y, x, 2, weightedPixel[2] * samplesAccumulatorScale);
			}
		}

		SLG_LOG("Initializing inputs");
		inputs.m_pColors = &inputColors;
		WriteEXR(*inputs.m_pColors, "inputs-colors.exr");

		checkAndPutToZeroNegativeInfNaNValues(bcdStats.m_nbOfSamplesImage, true);
		inputs.m_pNbOfSamples = &bcdStats.m_nbOfSamplesImage;
		WriteEXR(*inputs.m_pNbOfSamples, "inputs-nsamples.exr");

		checkAndPutToZeroNegativeInfNaNValues(bcdStats.m_histoImage, true);
		inputs.m_pHistograms = &bcdStats.m_histoImage;
		//WriteEXR(*inputs.m_pHistograms, "inputs-histo.exr");

		checkAndPutToZeroNegativeInfNaNValues(bcdStats.m_covarImage, true);
		inputs.m_pSampleCovariances = &bcdStats.m_covarImage;
		//WriteEXR(*inputs.m_pSampleCovariances, "inputs-covar.exr");

		SLG_LOG("Initializing parameters");
		// For now, these are just the default values
		parameters.m_histogramDistanceThreshold = 1.f;
		parameters.m_patchRadius = 1;
		parameters.m_searchWindowRadius = 6;
		parameters.m_minEigenValue = 1.e-8f;
		parameters.m_useRandomPixelOrder = true;
		parameters.m_markedPixelsSkippingProbability = 1.f;
		parameters.m_nbOfCores = 8;
		parameters.m_useCuda = false;

		SLG_LOG("Creating space for denoised image");
		bcd::Deepimf denoisedImg(threadFilm->GetWidth(), threadFilm->GetHeight(), 3);
		outputs.m_pDenoisedColors = &denoisedImg;

		SLG_LOG("Initializing denoiser");
		const int nbOfScales = 3;
		bcd::MultiscaleDenoiser denoiser(nbOfScales);
		// This is another denoiser class
		//bcd::Denoiser denoiser;

		denoiser.setInputs(inputs);
		denoiser.setOutputs(outputs);
		denoiser.setParameters(parameters);

		SLG_LOG("DENOISING");
		denoiser.denoise();

		SLG_LOG("Checking results");
		const bool verbose = true;
		checkAndPutToZeroNegativeInfNaNValues(denoisedImg, verbose);

		SLG_LOG("Writing denoised image");

		WriteEXR(denoisedImg, "denoised.exr");

		delete samplesAccumulator;
		SLG_LOG("Denoiser done.");
	}

	//SLG_LOG("[PathCPURenderEngine::" << threadIndex << "] Rendering thread halted");
}
