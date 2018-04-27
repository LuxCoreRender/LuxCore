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

	void SetEnabled(const bool v) { enabled = v; }
	bool IsEnabled() const { return enabled; }

	bool IsWarmUpDone() const { return warmUpDone; }

	void SetReferenceFilm(const Film *refFilm) { referenceFilm = refFilm; }
	bool HasReferenceFilm() const { return (referenceFilm != NULL); }

	void WarmUpDone();

	void AddSample(const u_int x, const u_int y,
			const SampleResult &sampleResult, const float weight);
	bcd::SamplesStatisticsImages GetDenoiserSamplesStatistics() const;

	bcd::SamplesStatisticsImages GetSamplesStatistics() const;
	float GetSampleScale() const { return sampleScale; }
	float GetSampleMaxValue() const { return samplesAccumulator->GetHistogramParameters().m_maxValue; }

	friend class boost::serialization::access;

private:
	template<class Archive> void save(Archive &ar, const unsigned int version) const {
		// TODO
	}
	template<class Archive>	void load(Archive &ar, const unsigned int version) {
		// TODO
	}
	BOOST_SERIALIZATION_SPLIT_MEMBER()

	const Film *film;

	SamplesAccumulator *samplesAccumulator;
	const std::vector<RadianceChannelScale> *radianceChannelScales;
	float sampleScale;
	bool warmUpDone;
	// The reference film is used by local thread films to share command
	// bcd::SamplesAccumulator parameters
	const Film *referenceFilm;
	
	bool enabled;
};

}

BOOST_CLASS_VERSION(slg::FilmDenoiser, 1)

BOOST_CLASS_EXPORT_KEY(slg::FilmDenoiser)

#endif	/* _SLG_FILMDENOISER_H */
