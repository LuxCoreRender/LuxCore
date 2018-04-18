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

#include "slg/film/film.h"
#include "slg/film/imagepipeline/plugins/bcddenoiser.h"

#include <bcd/core/SamplesAccumulator.h>
#include "bcd/core/Denoiser.h"
#include "bcd/core/MultiscaleDenoiser.h"
#include "bcd/core/IDenoiser.h"
#include "bcd/core/DeepImage.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// Background image plugin
//------------------------------------------------------------------------------

BOOST_CLASS_EXPORT_IMPLEMENT(slg::BCDDenoiserPlugin)

BCDDenoiserPlugin::BCDDenoiserPlugin() {
}

BCDDenoiserPlugin::~BCDDenoiserPlugin() {
}

ImagePipelinePlugin *BCDDenoiserPlugin::Copy() const {
	return new BCDDenoiserPlugin();
}

//------------------------------------------------------------------------------
// CPU version
//------------------------------------------------------------------------------

// TODO Do we need a check for negative/inf values? Can they still appear?

void BCDDenoiserPlugin::Apply(Film &film, const u_int index) {
	const double start = WallClockTime();
	
	Spectrum *pixels = (Spectrum *)film.channel_IMAGEPIPELINEs[index]->GetPixels();
	const u_int width = film.GetWidth();
	const u_int height = film.GetHeight();
	
	const bcd::SamplesStatisticsImages stats = film.GetBCDSamplesStatistics();
	if (stats.m_nbOfSamplesImage.isEmpty()
			|| stats.m_histoImage.isEmpty()
			|| stats.m_covarImage.isEmpty()) {
		return;
	}
	
	// Init inputs
	
	bcd::DeepImage<float> inputColors(width, height, 3);
	const float maxValue = film.GetBCDMaxValue();
	// TODO alpha?
	const double startCopy1 = WallClockTime();
	for(u_int y = 0; y < height; ++y) {
		for(u_int x = 0; x < width; ++x) {
			const u_int i = (height - y - 1) * width + x;
			
			if (maxValue > 0.f) {
				const Spectrum color = pixels[i].Clamp(0.f, maxValue);
				inputColors.set(y, x, 0, color.c[0]);
				inputColors.set(y, x, 1, color.c[1]);
				inputColors.set(y, x, 2, color.c[2]);
			} else {
				const Spectrum color = pixels[i];
				inputColors.set(y, x, 0, color.c[0]);
				inputColors.set(y, x, 1, color.c[1]);
				inputColors.set(y, x, 2, color.c[2]);
			}
		}
	}
	cout << "inputColors copy took: " << WallClockTime() - startCopy1 << endl;
	
	bcd::DenoiserInputs inputs;
	inputs.m_pColors = &inputColors;
	inputs.m_pNbOfSamples = &stats.m_nbOfSamplesImage;
	inputs.m_pHistograms = &stats.m_histoImage;
	inputs.m_pSampleCovariances = &stats.m_covarImage;
	
	// Init parameters
	
	// TODO get from properties
	bcd::DenoiserParameters parameters;
	parameters.m_histogramDistanceThreshold = 1.f;
	parameters.m_patchRadius = 1;
	parameters.m_searchWindowRadius = 6;
	parameters.m_minEigenValue = 1.e-8f;
	parameters.m_useRandomPixelOrder = true;
	parameters.m_markedPixelsSkippingProbability = 1.f;
	parameters.m_nbOfCores = boost::thread::hardware_concurrency();
	parameters.m_useCuda = false;
	// The number of downsampling levels to compute.
	// If greater than 1, the multiscale denoiser is used.
	const int scales = 3;
	
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
		
	denoiser->setInputs(inputs);
	denoiser->setOutputs(outputs);
	denoiser->setParameters(parameters);
	
	denoiser->denoise();
	
	// Copy to output pixels
	const double startCopy2 = WallClockTime();
	for(u_int y = 0; y < height; ++y) {
		for(u_int x = 0; x < width; ++x) {
			const u_int i = (height - y - 1) * width + x;
			Spectrum *pixel = pixels + i;
			
			pixel->c[0] = denoisedImg.get(y, x, 0);
			pixel->c[1] = denoisedImg.get(y, x, 1);
			pixel->c[2] = denoisedImg.get(y, x, 2);
		}
	}
	cout << "denoisedImg copy took: " << WallClockTime() - startCopy2 << endl;
	
	cout << "BCDDenoiserPlugin::Apply took: " << WallClockTime() - start << endl;
}
