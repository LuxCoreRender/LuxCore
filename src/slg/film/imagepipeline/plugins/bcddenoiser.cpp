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

#include <boost/format.hpp>
//#include <OpenImageIO/imageio.h>
//#include <OpenImageIO/imagebuf.h>

#include <bcd/core/SamplesAccumulator.h>
#include <bcd/core/Denoiser.h>
#include <bcd/core/MultiscaleDenoiser.h>
#include <bcd/core/IDenoiser.h>
#include <bcd/core/Utils.h>
#include <bcd/core/SpikeRemovalFilter.h>

#include "slg/film/film.h"
#include "slg/film/imagepipeline/plugins/bcddenoiser.h"

using namespace std;
using namespace luxrays;
using namespace slg;

/*OIIO_NAMESPACE_USING

static void WriteEXR(const string &fileName, const bcd::Deepimf &img) {
	int depth = img.getDepth();
	
	ImageSpec spec(img.getWidth(), img.getHeight(), depth, TypeDesc::FLOAT);
	
	if (depth != 3) {
		char channelName[32];
		for (int i = 0; i < depth; ++i) {
			sprintf(channelName, "Bin_%04d", i);
			spec.channelnames[i] = string(channelName);
		}
	}

 	ImageBuf buffer(spec);
	
 	for (ImageBuf::ConstIterator<float> it(buffer); !it.done(); ++it) {
 		const u_int x = it.x();
 		const u_int y = it.y();
 		
		float *pixel = (float *)buffer.pixeladdr(x, y, 0);

		for (int i = 0; i < depth; ++i)
			pixel[i] = img.get(y, x, i);
 	}

	if (!buffer.write(fileName))
		throw runtime_error("Error while writing BCDDenoiserPlugin output: " +
				fileName + " (error = " + geterror() + ")");
}*/

	
//------------------------------------------------------------------------------
// Background image plugin
//------------------------------------------------------------------------------

BOOST_CLASS_EXPORT_IMPLEMENT(slg::BCDDenoiserPlugin)

BCDDenoiserPlugin::BCDDenoiserPlugin(
		const float warmUpSamplesPerPixelVal,
		const float histogramDistanceThresholdVal,
		const int patchRadiusVal,
		const int searchWindowRadiusVal,
		const float minEigenValueVal,
		const bool useRandomPixelOrderVal,
		const float markedPixelsSkippingProbabilityVal,
		const int threadCountVal,
		const int scalesVal,
	    const bool filterSpikesVal,
		const bool applyDenoiseVal,
	    const float prefilterThresholdStDevFactorVal) :
		warmUpSamplesPerPixel(warmUpSamplesPerPixelVal),
		histogramDistanceThreshold(histogramDistanceThresholdVal),
		patchRadius(patchRadiusVal),
		searchWindowRadius(searchWindowRadiusVal),
		minEigenValue(minEigenValueVal),
		useRandomPixelOrder(useRandomPixelOrderVal),
		markedPixelsSkippingProbability(markedPixelsSkippingProbabilityVal),
		threadCount(threadCountVal),
		scales(scalesVal),
		filterSpikes(filterSpikesVal),
		applyDenoise(applyDenoiseVal),
		prefilterThresholdStDevFactor(prefilterThresholdStDevFactorVal) {
}
	
BCDDenoiserPlugin::BCDDenoiserPlugin() {
}

BCDDenoiserPlugin::~BCDDenoiserPlugin() {
}

ImagePipelinePlugin *BCDDenoiserPlugin::Copy() const {
	return new BCDDenoiserPlugin(
			warmUpSamplesPerPixel,
			histogramDistanceThreshold,
			patchRadius,
			searchWindowRadius,
			minEigenValue,
			useRandomPixelOrder,
			markedPixelsSkippingProbability,
			threadCount,
			scales,
			filterSpikes,
			applyDenoise,
			prefilterThresholdStDevFactor);
}

//------------------------------------------------------------------------------
// CPU version
//------------------------------------------------------------------------------

static void ProgressCallBack(const float progress) {
	static double lastPrint = WallClockTime();
	
	const double now = WallClockTime();
	if (now - lastPrint > 1.0) {
		SLG_LOG("BCD progress: " << (boost::format("%.2f") % (100.0 * progress)) << "%");
		lastPrint = now;
	}
}

void BCDDenoiserPlugin::CopyOutputToFilm(const Film &film, const u_int index,
		const bcd::DeepImage<float> &outputImg) const {
	const u_int width = film.GetWidth();
	const u_int height = film.GetHeight();	

	const FilmDenoiser &filmDenoiser = film.GetDenoiser();
	const float sampleScale = filmDenoiser.GetSampleScale();

	// Copy to output pixels
	Spectrum *dstPixels = (Spectrum *)film.channel_IMAGEPIPELINEs[index]->GetPixels();
	const float invSampleScale = 1.f / sampleScale;
	for(u_int y = 0; y < height; ++y) {
		for(u_int x = 0; x < width; ++x) {
			const u_int i = (height - y - 1) * width + x;
			Spectrum *dstPixel = dstPixels + i;
			
			dstPixel->c[0] += outputImg.get(y, x, 0) * invSampleScale;
			dstPixel->c[1] += outputImg.get(y, x, 1) * invSampleScale;
			dstPixel->c[2] += outputImg.get(y, x, 2) * invSampleScale;
		}
	}
}

void BCDDenoiserPlugin::Apply(Film &film, const u_int index, const bool pixelNormalizedSampleAccumulator) {
	FilmDenoiser &filmDenoiser = film.GetDenoiser();

	bcd::SamplesStatisticsImages stats = filmDenoiser.GetSamplesStatistics(pixelNormalizedSampleAccumulator);
	if (stats.m_nbOfSamplesImage.isEmpty()
			|| stats.m_histoImage.isEmpty()
			|| stats.m_covarImage.isEmpty()) {
		SLG_LOG("WARNING: not enough samples to run BCDDenoiserPlugin. Warm up samples per pixel: " << warmUpSamplesPerPixel);

		return;
	}
	
	// Init inputs
	
	const u_int width = film.GetWidth();
	const u_int height = film.GetHeight();	
	bcd::DeepImage<float> inputColors(width, height, 3);

	const float sampleScale = filmDenoiser.GetSampleScale();
	SLG_LOG("BCD sample scale: " << sampleScale);
	const float sampleMaxValue = filmDenoiser.GetSampleMaxValue();
	SLG_LOG("BCD sample max. value: " << sampleMaxValue);
	// TODO alpha?
	const bool use_RADIANCE_PER_PIXEL_NORMALIZEDs = pixelNormalizedSampleAccumulator;
	const bool use_RADIANCE_PER_SCREEN_NORMALIZEDs = !pixelNormalizedSampleAccumulator;

	const double RADIANCE_PER_SCREEN_NORMALIZED_SampleCount = film.GetTotalLightSampleCount();
	
	for(u_int y = 0; y < height; ++y) {
		for(u_int x = 0; x < width; ++x) {
			Spectrum color;
			film.GetPixelFromMergedSampleBuffers(use_RADIANCE_PER_PIXEL_NORMALIZEDs,
					use_RADIANCE_PER_SCREEN_NORMALIZEDs,
					&filmDenoiser.GetRadianceChannelScales(),
					RADIANCE_PER_SCREEN_NORMALIZED_SampleCount,
					x, y, color.c);
			
			color = (color *  sampleScale).Clamp(0.f, sampleMaxValue);
			const u_int column = x;
			const u_int line = height - y - 1;
			inputColors.set(line, column, 0, color.c[0]);
			inputColors.set(line, column, 1, color.c[1]);
			inputColors.set(line, column, 2, color.c[2]);
		}
	}
	
	if (filterSpikes)
		bcd::SpikeRemovalFilter::filter(inputColors,
										stats.m_nbOfSamplesImage,
										stats.m_histoImage,
										stats.m_covarImage,
										prefilterThresholdStDevFactor);

	if (applyDenoise) {
		bcd::DenoiserInputs inputs;
		inputs.m_pColors = &inputColors;
		inputs.m_pNbOfSamples = &stats.m_nbOfSamplesImage;
		inputs.m_pHistograms = &stats.m_histoImage;
		inputs.m_pSampleCovariances = &stats.m_covarImage;

		// Some debug output
		/*WriteEXR("input-mean.exr", stats.m_meanImage);
		WriteEXR("input-nsamples.exr", *inputs.m_pNbOfSamples);
		// bcd-cli can be used to process the following files
		WriteEXR("input.exr", *inputs.m_pColors);
		bcd::Deepimf histoAndNbOfSamplesImage = bcd::Utils::mergeHistogramAndNbOfSamples(*inputs.m_pHistograms, *inputs.m_pNbOfSamples);
		WriteEXR("input-hist.exr", histoAndNbOfSamplesImage);
		WriteEXR("input-cov.exr", *inputs.m_pSampleCovariances);*/

		// Init parameters

		bcd::DenoiserParameters parameters;
		parameters.m_histogramDistanceThreshold = histogramDistanceThreshold;
		parameters.m_patchRadius = patchRadius;
		parameters.m_searchWindowRadius = searchWindowRadius;
		parameters.m_minEigenValue = minEigenValue;
		parameters.m_useRandomPixelOrder = useRandomPixelOrder;
		parameters.m_markedPixelsSkippingProbability = markedPixelsSkippingProbability;
		parameters.m_nbOfCores = threadCount;
		parameters.m_useCuda = false;

		// Init outputs

		bcd::DeepImage<float> denoisedImg(width, height, 3);
		bcd::DenoiserOutputs outputs;
		outputs.m_pDenoisedColors = &denoisedImg;

		// Create denoiser and run denoising

		unique_ptr<bcd::IDenoiser> denoiser = nullptr;

		if (scales > 1)
			denoiser.reset(new bcd::MultiscaleDenoiser(scales));
		else
			denoiser.reset(new bcd::Denoiser());
		denoiser->setProgressCallback(ProgressCallBack);

		denoiser->setInputs(inputs);
		denoiser->setOutputs(outputs);
		denoiser->setParameters(parameters);

		denoiser->denoise();
		
		CopyOutputToFilm(film, index, denoisedImg);
	} else
		CopyOutputToFilm(film, index, inputColors);
}

void BCDDenoiserPlugin::Apply(Film &film, const u_int index) {
	const double startTime = WallClockTime();
	
	const FilmDenoiser &filmDenoiser = film.GetDenoiser();
	if (filmDenoiser.HasSamplesStatistics(true) || filmDenoiser.HasSamplesStatistics(false))
		film.channel_IMAGEPIPELINEs[index]->Clear();

	if (filmDenoiser.HasSamplesStatistics(true)) {
		SLG_LOG("BCD pixel normalized pass");
		Apply(film, index, true);
	}
	if (filmDenoiser.HasSamplesStatistics(false)) {
		SLG_LOG("BCD screen normalized pass");
		Apply(film, index, false);
	}

	SLG_LOG("BCD Apply took: " << (boost::format("%.1f") % (WallClockTime() - startTime)) << "secs");
}
