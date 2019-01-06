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

#ifndef _SLG_FILMDENOISER_H
#define	_SLG_FILMDENOISER_H

#include <bcd/core/SamplesAccumulator.h>

#include "luxrays/utils/serializationutils.h"
#include "slg/slg.h"
#include "slg/film/imagepipeline/radiancechannelscale.h"
#include "slg/film/denoiser/samplesaccumulator.h"

namespace slg {

//------------------------------------------------------------------------------
// FilmDenoiser
//------------------------------------------------------------------------------

class Film;
class SampleResult;

class FilmDenoiser {
public:
	FilmDenoiser(const Film *film);
	~FilmDenoiser();

	void Reset();
	void Clear();

	void SetEnabled(const bool v) { enabled = v; }
	bool IsEnabled() const { return enabled; }

	void CheckIfWarmUpDone();
	bool IsWarmUpDone() const { return warmUpDone; }
	void WarmUpDone();

	void SetReferenceFilm(const Film *refFilm,
			const u_int offsetX = 0, const u_int offsetY = 0);
	void CopyReferenceFilm(const Film *refFilm);

	bool HasReferenceFilm() const { return (referenceFilm != NULL); }

	void AddDenoiser(const FilmDenoiser &filmDenoiser,
		const u_int srcOffsetX, const u_int srcOffsetY,
		const u_int srcWidth, const u_int srcHeight,
		const u_int dstOffsetX, const u_int dstOffsetY);
	void AddDenoiser(const FilmDenoiser &filmDenoiser);

	void AddSample(const u_int x, const u_int y,
			const SampleResult &sampleResult, const float weight);

	bool HasSamplesStatistics(const bool pixelNormalizedSampleAccumulator) const;
	bcd::SamplesStatisticsImages GetSamplesStatistics(const bool pixelNormalizedSampleAccumulator) const;
	float GetSampleScale() const { return sampleScale; }
	float GetSampleGamma() const { return  bcd::HistogramParameters().m_gamma; }
	float GetSampleMaxValue() const { return  bcd::HistogramParameters().m_maxValue; }
	int GetHistogramBinsCount() const { return bcd::HistogramParameters().m_nbOfBins; }
	const std::vector<RadianceChannelScale> &GetRadianceChannelScales() const { return radianceChannelScales; }
	
	// Used by OpenCL related code. Return samplesAccumulatorPixelNormalized images.
	float *GetNbOfSamplesImage();
	float *GetSquaredWeightSumsImage();
	float *GetMeanImage();
	float *GetCovarImage();
	float *GetHistoImage();
	
	friend class boost::serialization::access;

private:
	// Used by serialization
	FilmDenoiser();

	void Init();
	void CheckReferenceFilm();

	template<class Archive> void serialize(Archive &ar, const u_int version) {
		ar & film;
		ar & samplesAccumulatorPixelNormalized;
		ar & samplesAccumulatorScreenNormalized;
		ar & radianceChannelScales;
		ar & sampleScale;
		ar & warmUpSPP;
		ar & warmUpDone;
		ar & referenceFilm;
		ar & referenceFilmWidth;
		ar & referenceFilmHeight;
		ar & referenceFilmOffsetX;
		ar & referenceFilmOffsetY;
		ar & enabled;
	}

	const Film *film;

	SamplesAccumulator *samplesAccumulatorPixelNormalized;
	SamplesAccumulator *samplesAccumulatorScreenNormalized;

	// This is a copy of the image pipeline radianceChannelScales where the
	// BCD denoiser plugin is. I need to use a copy and not a pointer because the
	// Image pipeline can be edited, delteted, etc.
	std::vector<RadianceChannelScale> radianceChannelScales;
	float sampleScale;
	boost::mutex warmUpDoneMutex;
	float warmUpSPP;
	bool warmUpDone;
	// The reference film is used by local thread films to share command
	// bcd::SamplesAccumulator parameters
	const Film *referenceFilm;
	u_int referenceFilmWidth, referenceFilmHeight;
	u_int referenceFilmOffsetX, referenceFilmOffsetY;
	
	bool enabled;
};

}

BOOST_CLASS_VERSION(slg::FilmDenoiser, 5)

BOOST_CLASS_EXPORT_KEY(slg::FilmDenoiser)

#endif	/* _SLG_FILMDENOISER_H */
