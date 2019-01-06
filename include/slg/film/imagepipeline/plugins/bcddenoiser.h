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

#ifndef _SLG_BCD_DENOISER_PLUGIN_H
#define	_SLG_BCD_DENOISER_PLUGIN_H

#include <bcd/core/SamplesAccumulator.h>

#include "luxrays/utils/serializationutils.h"
#include "slg/film/imagepipeline/imagepipeline.h"

namespace slg {

//------------------------------------------------------------------------------
// BCD denoiser image plugin
//------------------------------------------------------------------------------

class BCDDenoiserPlugin : public ImagePipelinePlugin {
public:
	BCDDenoiserPlugin(
			const float warmUpSamplesPerPixel,
			const float histogramDistanceThreshold,
			const int patchRadius,
			const int searchWindowRadius,
			const float minEigenValue,
			const bool useRandomPixelOrder,
			const float markedPixelsSkippingProbability,
			const int threadCount,
			const int scales,
			const bool applyDenoiseVal,
			const bool filterSpikes,
			const float prefilterThresholdStDevFactor);
	virtual ~BCDDenoiserPlugin();

	float GetWarmUpSPP() const { return warmUpSamplesPerPixel; }
	const bcd::HistogramParameters &GetHistogramParameters() const { return histogramParams; }
	
	virtual ImagePipelinePlugin *Copy() const;

	virtual void Apply(Film &film, const u_int index);

	friend class boost::serialization::access;

private:
	// Used by serialization
	BCDDenoiserPlugin();

	void CopyOutputToFilm(const Film &film, const u_int index,
		const bcd::DeepImage<float> &outputImg) const;

	template<class Archive> void serialize(Archive &ar, const u_int version) {
		ar & BOOST_SERIALIZATION_BASE_OBJECT_NVP(ImagePipelinePlugin);

		ar & warmUpSamplesPerPixel;
		ar & histogramDistanceThreshold;
		ar & patchRadius;
		ar & searchWindowRadius;
		ar & minEigenValue;
		ar & minEigenValue;
		ar & useRandomPixelOrder;
		ar & markedPixelsSkippingProbability;
		ar & threadCount;
		ar & scales;
		ar & filterSpikes;
		ar & applyDenoise;
		ar & prefilterThresholdStDevFactor;

		ar & histogramParams.m_gamma;
		ar & histogramParams.m_maxValue;
		ar & histogramParams.m_nbOfBins;
	}

	void Apply(Film &film, const u_int index, const bool pixelNormalizedSampleAccumulator);

	float warmUpSamplesPerPixel;
	float histogramDistanceThreshold;
  	int patchRadius;
	int searchWindowRadius;
	float minEigenValue;
	bool useRandomPixelOrder;
	float markedPixelsSkippingProbability;
	int threadCount;
	int scales;
	bool filterSpikes, applyDenoise;
	float prefilterThresholdStDevFactor;

	bcd::HistogramParameters histogramParams;
};

}

BOOST_CLASS_VERSION(slg::BCDDenoiserPlugin, 5)

BOOST_CLASS_EXPORT_KEY(slg::BCDDenoiserPlugin)

#endif	/*  _SLG_BCD_DENOISER_PLUGIN_H */
