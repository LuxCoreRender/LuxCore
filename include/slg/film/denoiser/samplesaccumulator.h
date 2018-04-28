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

// Base on SamplesAccumulator from:

// This file is part of the reference implementation for the paper 
//   Bayesian Collaborative Denoising for Monte-Carlo Rendering
//   Malik Boughida and Tamy Boubekeur.
//   Computer Graphics Forum (Proc. EGSR 2017), vol. 36, no. 4, p. 137-153, 2017
//
// All rights reserved. Use of this source code is governed by a
// BSD-style license that can be found in the LICENSE.txt file.

#ifndef _SLG_SAMPLESACCUMULATOR_H
#define _SLG_SAMPLESACCUMULATOR_H

#include <bcd/core/SamplesAccumulator.h>

#include "luxrays/utils/serializationutils.h"

namespace slg {

class SamplesAccumulator {
public:
	SamplesAccumulator(
			int i_width, int i_height,
			const bcd::HistogramParameters& i_rHistogramParameters);
	void reset();

	const bcd::HistogramParameters &GetHistogramParameters() const;

	void addSample(
			int i_line, int i_column,
			float i_sampleR, float i_sampleG, float i_sampleB,
			float i_weight = 1.f);
	void addSampleAtomic(
			int i_line, int i_column,
			float i_sampleR, float i_sampleG, float i_sampleB,
			float i_weight = 1.f);
	void addAccumulator(const SamplesAccumulator &samplesAccumulator);

	bcd::SamplesStatisticsImages getSamplesStatistics() const;

	bcd::SamplesStatisticsImages extractSamplesStatistics();

	friend class boost::serialization::access;

private:
	// Used by serialization
	SamplesAccumulator();
	
	template<class Archive> void save(Archive &ar, const unsigned int version) const;
	template<class Archive>	void load(Archive &ar, const unsigned int version);
	BOOST_SERIALIZATION_SPLIT_MEMBER()
	
	void computeSampleStatistics(bcd::SamplesStatisticsImages &io_sampleStats) const;

	int m_width;
	int m_height;
	bcd::HistogramParameters m_histogramParameters;
	bcd::SamplesStatisticsImages m_samplesStatisticsImages;
	bcd::DeepImage<float> m_squaredWeightSumsImage;

	bool m_isValid; ///< If you call extractSamplesStatistics, the object becomes invalid and should be destroyed
};

}

BOOST_CLASS_VERSION(slg::SamplesAccumulator, 1)

BOOST_CLASS_EXPORT_KEY(slg::SamplesAccumulator)

#endif // _SLG_SAMPLESACCUMULATOR_H
