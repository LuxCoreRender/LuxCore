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
void checkAndPutToZeroNegativeInfNaNValues(bcd::DeepImage<float>& io_rImage, bool i_verbose = false)
{
	using std::isnan;
	using std::isinf;
	int width = io_rImage.getWidth();
	int height = io_rImage.getHeight();
	int depth = io_rImage.getDepth();
	bool hasBadValue = false;
	vector<float> values(depth);
	float val = 0.f;
	for (int line = 0; line < height; line++)
		for (int col = 0; col < width; col++)
		{
			hasBadValue = false;
			for(int z = 0; z < depth; ++z)
			{
				val = values[z] = io_rImage.get(line, col, z);
				if(val < 0 || isnan(val) || isinf(val))
				{
					io_rImage.set(line, col, z, 0.f);
					hasBadValue = true;
				}
			}
			if(hasBadValue && i_verbose)
			{
				cout << "Warning: strange value for pixel (line,column)=(" << line << "," << col << "): (" << values[0];
				for(int i = 1; i < depth; ++i)
					cout << ", " << values[i];
				cout << ")\n";
			}
		}
}

void reorderDataForWritingEXR(std::vector<float>& o_rData, const bcd::Deepimf& i_rImage)
{
	int width = i_rImage.getWidth();
	int height = i_rImage.getHeight();
	int depth = i_rImage.getDepth();
	o_rData.resize(width*height*depth);
	for(int l = 0; l < height; l++)
		for(int c = 0; c < width; c++)
			for (int z = 0; z < depth; z++)
				o_rData[(z*height + l)*width + c] = i_rImage.get(l, c, z);
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
		
		// For BCD denoiser
		const int line = sampleResult.pixelY;
		const int column = sampleResult.pixelX;
		const float sampleR = sampleResult.radiance[0].c[0];
		const float sampleG = sampleResult.radiance[0].c[1];
		const float sampleB = sampleResult.radiance[0].c[2];
		const float weight = 1.f;
		engine->samplesAccumulator.addSample(line, column,
											 sampleR, sampleG, sampleB,
											 weight);

		//cout << "sampleR: " << sampleR << " sampleG: " << sampleG << " sampleB: " << sampleB << endl;
		
		// Variance clamping
		if (varianceClamping.hasClamping())
			varianceClamping.Clamp(*threadFilm, sampleResult);

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
	
	SLG_LOG("Collecting BCD stats");
	bcd::SamplesStatisticsImages bcdStats = engine->samplesAccumulator.getSamplesStatistics();
	
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
			frameBuffer->GetWeightedPixel(x, y, weightedPixel);

			inputColors.set(y, x, 0, weightedPixel[0]);
			inputColors.set(y, x, 1, weightedPixel[1]);
			inputColors.set(y, x, 2, weightedPixel[2]);
		}
	}
	
	SLG_LOG("Initializing inputs");
	inputs.m_pColors = &inputColors;
	inputs.m_pNbOfSamples = &bcdStats.m_nbOfSamplesImage;
	inputs.m_pHistograms = &bcdStats.m_histoImage;
	inputs.m_pSampleCovariances = &bcdStats.m_covarImage;
	
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
	
	SLG_LOG("Ireating space for denoised image");
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
	const bool verbose = false;
	checkAndPutToZeroNegativeInfNaNValues(denoisedImg, verbose);
	
	SLG_LOG("Writing denoised image");
	
	//std::vector<float> data;
	//reorderDataForWritingEXR(data, denoisedImg);
	//const float *dataPtr = &data[0];
	
	ImageSpec spec(threadFilm->GetWidth(), threadFilm->GetHeight(), 3, TypeDesc::FLOAT);
 	ImageBuf buffer(spec);
 	for (ImageBuf::ConstIterator<float> it(buffer); !it.done(); ++it) {
 		u_int x = it.x();
 		u_int y = it.y();
 		float *pixel = (float *)buffer.pixeladdr(x, y, 0);
 		// Note that x and y are not in the usual order in DeepImage get/set methods
 		pixel[0] = denoisedImg.get(y, x, 0);
 		pixel[1] = denoisedImg.get(y, x, 1);
 		pixel[2] = denoisedImg.get(y, x, 2);
 		
 		// This does not work either (but looks wrong in a different way...)
 		//pixel[0] = *dataPtr++;
 		//pixel[1] = *dataPtr++;
 		//pixel[2] = *dataPtr++;
 	}
 	buffer.write("denoised.exr");
	SLG_LOG("Denoiser done.");

	//SLG_LOG("[PathCPURenderEngine::" << threadIndex << "] Rendering thread halted");
}
