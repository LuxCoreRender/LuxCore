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

// Base on SamplesAccumulator from:

// This file is part of the reference implementation for the paper
//   Bayesian Collaborative Denoising for Monte-Carlo Rendering
//   Malik Boughida and Tamy Boubekeur.
//   Computer Graphics Forum (Proc. EGSR 2017), vol. 36, no. 4, p. 137-153, 2017
//
// All rights reserved. Use of this source code is governed by a
// BSD-style license that can be found in the LICENSE.txt file.

#include <cassert>

#include <bcd/core/CovarianceMatrix.h>

#include "luxrays/utils/atomic.h"
#include "slg/film/denoiser/samplesaccumulator.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// BCD samples accumulator
//------------------------------------------------------------------------------

BOOST_CLASS_EXPORT_IMPLEMENT(slg::SamplesAccumulator)

SamplesAccumulator::SamplesAccumulator(
		int i_width, int i_height,
		const bcd::HistogramParameters &i_rHistogramParameters) :
		m_width(i_width), m_height(i_height),
		m_histogramParameters(i_rHistogramParameters),
		m_samplesStatisticsImages(i_width, i_height, i_rHistogramParameters.m_nbOfBins),
		m_squaredWeightSumsImage(i_width, i_height, 1),
		m_isValid(true) {
	Clear();
}

SamplesAccumulator::SamplesAccumulator() {
}

void SamplesAccumulator::AddSample(
		int i_line, int i_column,
		float i_sampleR, float i_sampleG, float i_sampleB,
		float i_weight) {
	assert(m_isValid);

	const float sample[3] = {i_sampleR, i_sampleG, i_sampleB};
	const float satureLevelGamma = 2.f; // used for determining the weight to give to the sample in the highest two bins, when the sample is saturated

	m_samplesStatisticsImages.m_nbOfSamplesImage.get(i_line, i_column, 0) += i_weight;
	m_squaredWeightSumsImage.get(i_line, i_column, 0) += i_weight * i_weight;

	bcd::DeepImage<float>& rSum = m_samplesStatisticsImages.m_meanImage;
	rSum.get(i_line, i_column, 0) += i_weight * i_sampleR;
	rSum.get(i_line, i_column, 1) += i_weight * i_sampleG;
	rSum.get(i_line, i_column, 2) += i_weight * i_sampleB;

	bcd::DeepImage<float>& rCovSum = m_samplesStatisticsImages.m_covarImage;
	rCovSum.get(i_line, i_column, int(bcd::ESymMatData::e_xx)) += i_weight * i_sampleR * i_sampleR;
	rCovSum.get(i_line, i_column, int(bcd::ESymMatData::e_yy)) += i_weight * i_sampleG * i_sampleG;
	rCovSum.get(i_line, i_column, int(bcd::ESymMatData::e_zz)) += i_weight * i_sampleB * i_sampleB;
	rCovSum.get(i_line, i_column, int(bcd::ESymMatData::e_yz)) += i_weight * i_sampleG * i_sampleB;
	rCovSum.get(i_line, i_column, int(bcd::ESymMatData::e_xz)) += i_weight * i_sampleR * i_sampleB;
	rCovSum.get(i_line, i_column, int(bcd::ESymMatData::e_xy)) += i_weight * i_sampleR * i_sampleG;

	int floorBinIndex;
	int ceilBinIndex;
	float binFloatIndex;
	float floorBinWeight;
	float ceilBinWeight;

	for (int32_t channelIndex = 0; channelIndex < 3; ++channelIndex) { // fill histogram; code refactored from Ray Histogram Fusion PBRT code
		float value = sample[channelIndex];
		value = (value > 0 ? value : 0);
		if (m_histogramParameters.m_gamma > 1)
			value = pow(value, 1.f / m_histogramParameters.m_gamma); // exponential scaling
		if (m_histogramParameters.m_maxValue > 0)
			value = (value / m_histogramParameters.m_maxValue); // normalize to the maximum value
		value = value > satureLevelGamma ? satureLevelGamma : value;

		binFloatIndex = value * (m_histogramParameters.m_nbOfBins - 2);
		floorBinIndex = int(binFloatIndex);

		if (floorBinIndex < m_histogramParameters.m_nbOfBins - 2) // in bounds
		{
			ceilBinIndex = floorBinIndex + 1;
			ceilBinWeight = binFloatIndex - floorBinIndex;
			floorBinWeight = 1.0f - ceilBinWeight;
		} else { //out of bounds... v >= 1
			floorBinIndex = m_histogramParameters.m_nbOfBins - 2;
			ceilBinIndex = floorBinIndex + 1;
			ceilBinWeight = (value - 1.0f) / (satureLevelGamma - 1.f);
			floorBinWeight = 1.0f - ceilBinWeight;
		}
		m_samplesStatisticsImages.m_histoImage.get(i_line, i_column, channelIndex * m_histogramParameters.m_nbOfBins + floorBinIndex) += i_weight * floorBinWeight;
		m_samplesStatisticsImages.m_histoImage.get(i_line, i_column, channelIndex * m_histogramParameters.m_nbOfBins + ceilBinIndex) += i_weight * ceilBinWeight;
	}
}

void SamplesAccumulator::AddSampleAtomic(
		int i_line, int i_column,
		float i_sampleR, float i_sampleG, float i_sampleB,
		float i_weight) {
	assert(m_isValid);

	const float sample[3] = {i_sampleR, i_sampleG, i_sampleB};
	const float satureLevelGamma = 2.f; // used for determining the weight to give to the sample in the highest two bins, when the sample is saturated

	AtomicAdd(&m_samplesStatisticsImages.m_nbOfSamplesImage.get(i_line, i_column, 0), i_weight);
	AtomicAdd(&m_squaredWeightSumsImage.get(i_line, i_column, 0), i_weight * i_weight);

	bcd::DeepImage<float>& rSum = m_samplesStatisticsImages.m_meanImage;
	AtomicAdd(&rSum.get(i_line, i_column, 0), i_weight * i_sampleR);
	AtomicAdd(&rSum.get(i_line, i_column, 1), i_weight * i_sampleG);
	AtomicAdd(&rSum.get(i_line, i_column, 2), i_weight * i_sampleB);

	bcd::DeepImage<float>& rCovSum = m_samplesStatisticsImages.m_covarImage;
	AtomicAdd(&rCovSum.get(i_line, i_column, int(bcd::ESymMatData::e_xx)), i_weight * i_sampleR * i_sampleR);
	AtomicAdd(&rCovSum.get(i_line, i_column, int(bcd::ESymMatData::e_yy)), i_weight * i_sampleG * i_sampleG);
	AtomicAdd(&rCovSum.get(i_line, i_column, int(bcd::ESymMatData::e_zz)), i_weight * i_sampleB * i_sampleB);
	AtomicAdd(&rCovSum.get(i_line, i_column, int(bcd::ESymMatData::e_yz)), i_weight * i_sampleG * i_sampleB);
	AtomicAdd(&rCovSum.get(i_line, i_column, int(bcd::ESymMatData::e_xz)), i_weight * i_sampleR * i_sampleB);
	AtomicAdd(&rCovSum.get(i_line, i_column, int(bcd::ESymMatData::e_xy)), i_weight * i_sampleR * i_sampleG);

	int floorBinIndex;
	int ceilBinIndex;
	float binFloatIndex;
	float floorBinWeight;
	float ceilBinWeight;

	for (int32_t channelIndex = 0; channelIndex < 3; ++channelIndex) { // fill histogram; code refactored from Ray Histogram Fusion PBRT code
		float value = sample[channelIndex];
		value = (value > 0 ? value : 0);
		if (m_histogramParameters.m_gamma > 1)
			value = pow(value, 1.f / m_histogramParameters.m_gamma); // exponential scaling
		if (m_histogramParameters.m_maxValue > 0)
			value = (value / m_histogramParameters.m_maxValue); // normalize to the maximum value
		value = value > satureLevelGamma ? satureLevelGamma : value;

		binFloatIndex = value * (m_histogramParameters.m_nbOfBins - 2);
		floorBinIndex = int(binFloatIndex);

		if (floorBinIndex < m_histogramParameters.m_nbOfBins - 2) // in bounds
		{
			ceilBinIndex = floorBinIndex + 1;
			ceilBinWeight = binFloatIndex - floorBinIndex;
			floorBinWeight = 1.0f - ceilBinWeight;
		} else { //out of bounds... v >= 1
			floorBinIndex = m_histogramParameters.m_nbOfBins - 2;
			ceilBinIndex = floorBinIndex + 1;
			ceilBinWeight = (value - 1.0f) / (satureLevelGamma - 1.f);
			floorBinWeight = 1.0f - ceilBinWeight;
		}
		AtomicAdd(&m_samplesStatisticsImages.m_histoImage.get(i_line, i_column, channelIndex * m_histogramParameters.m_nbOfBins + floorBinIndex), i_weight * floorBinWeight);
		AtomicAdd(&m_samplesStatisticsImages.m_histoImage.get(i_line, i_column, channelIndex * m_histogramParameters.m_nbOfBins + ceilBinIndex), i_weight * ceilBinWeight);
	}
}

void SamplesAccumulator::Clear() {
	m_samplesStatisticsImages.m_nbOfSamplesImage.fill(0.f);
	m_samplesStatisticsImages.m_meanImage.fill(0.f);
	m_samplesStatisticsImages.m_covarImage.fill(0.f);
	m_samplesStatisticsImages.m_histoImage.fill(0.f);
	m_squaredWeightSumsImage.fill(0.f);
}

const bcd::HistogramParameters &SamplesAccumulator::GetHistogramParameters() const {
	return m_histogramParameters;
}

void SamplesAccumulator::AddAccumulator(const SamplesAccumulator &samplesAccumulator,
		const int srcOffsetX, const int srcOffsetY,
		const int srcWidth, const int srcHeight,
		const int dstOffsetX, const int dstOffsetY) {
	assert(m_isValid);
	assert(m_histogramParameters.m_nbOfBins == samplesAccumulator.m_histogramParameters.m_nbOfBins);
	assert(m_histogramParameters.m_gamma == samplesAccumulator.m_histogramParameters.m_gamma);
	assert(m_histogramParameters.m_maxValue == samplesAccumulator.m_histogramParameters.m_maxValue);

	const int srcTotalHeight = samplesAccumulator.m_samplesStatisticsImages.m_nbOfSamplesImage.getHeight();
	const int dstTotalHeight = m_samplesStatisticsImages.m_nbOfSamplesImage.getHeight();

#pragma omp parallel for
	for (int line = 0; line < srcHeight; ++line) {
		for (int column = 0; column < srcWidth; ++column) {
			const int srcLine = line + (srcTotalHeight - (srcOffsetY + srcHeight));
			const int srcColumn = column + srcOffsetX;
			const int dstLine = line + (dstTotalHeight - (dstOffsetY + srcHeight));
			const int dstColumn = column + dstOffsetX;
			
			m_samplesStatisticsImages.m_nbOfSamplesImage.get(dstLine, dstColumn, 0) +=
					samplesAccumulator.m_samplesStatisticsImages.m_nbOfSamplesImage.get(srcLine, srcColumn, 0);

			m_squaredWeightSumsImage.get(dstLine, dstColumn, 0) +=
					samplesAccumulator.m_squaredWeightSumsImage.get(srcLine, srcColumn, 0);

			bcd::DeepImage<float> &rSumDst = m_samplesStatisticsImages.m_meanImage;
			const bcd::DeepImage<float> &rSumSrc = samplesAccumulator.m_samplesStatisticsImages.m_meanImage;
			rSumDst.get(dstLine, dstColumn, 0) += rSumSrc.get(srcLine, srcColumn, 0);
			rSumDst.get(dstLine, dstColumn, 1) += rSumSrc.get(srcLine, srcColumn, 1);
			rSumDst.get(dstLine, dstColumn, 2) += rSumSrc.get(srcLine, srcColumn, 2);

			bcd::DeepImage<float> &rCovSumDst = m_samplesStatisticsImages.m_covarImage;
			const bcd::DeepImage<float> &rCovSumSrc = samplesAccumulator.m_samplesStatisticsImages.m_covarImage;
			rCovSumDst.get(dstLine, dstColumn, int(bcd::ESymMatData::e_xx)) += rCovSumSrc.get(srcLine, srcColumn, int(bcd::ESymMatData::e_xx));
			rCovSumDst.get(dstLine, dstColumn, int(bcd::ESymMatData::e_yy)) += rCovSumSrc.get(srcLine, srcColumn, int(bcd::ESymMatData::e_yy));
			rCovSumDst.get(dstLine, dstColumn, int(bcd::ESymMatData::e_zz)) += rCovSumSrc.get(srcLine, srcColumn, int(bcd::ESymMatData::e_zz));
			rCovSumDst.get(dstLine, dstColumn, int(bcd::ESymMatData::e_yz)) += rCovSumSrc.get(srcLine, srcColumn, int(bcd::ESymMatData::e_yz));
			rCovSumDst.get(dstLine, dstColumn, int(bcd::ESymMatData::e_xz)) += rCovSumSrc.get(srcLine, srcColumn, int(bcd::ESymMatData::e_xz));
			rCovSumDst.get(dstLine, dstColumn, int(bcd::ESymMatData::e_xy)) += rCovSumSrc.get(srcLine, srcColumn, int(bcd::ESymMatData::e_xy));

			for (int32_t channelIndex = 0; channelIndex < 3; ++channelIndex) {
				for (int32_t binIndex = 0; binIndex < m_histogramParameters.m_nbOfBins; ++binIndex) {
					m_samplesStatisticsImages.m_histoImage.get(dstLine, dstColumn, channelIndex * m_histogramParameters.m_nbOfBins + binIndex) +=
							samplesAccumulator.m_samplesStatisticsImages.m_histoImage.get(srcLine, srcColumn, channelIndex * m_histogramParameters.m_nbOfBins + binIndex);
				}
			}
		}
	}
}

void SamplesAccumulator::ComputeSampleStatistics(bcd::SamplesStatisticsImages &io_sampleStats) const {
#pragma omp parallel for
	for (int line = 0; line < m_height; ++line) {
		float mean[3];
		float cov[6];

		for (int column = 0; column < m_width; ++column) {
			float weightSum = io_sampleStats.m_nbOfSamplesImage.get(line, column, 0);
			float squaredWeightSum = m_squaredWeightSumsImage.get(line, column, 0);

			float invWeightSum = 1.f / weightSum;

			for (int i = 0; i < 3; ++i) {
				mean[i] = invWeightSum * io_sampleStats.m_meanImage.get(line, column, i);
				io_sampleStats.m_meanImage.set(line, column, i, mean[i]);
			}

			for (int i = 0; i < 6; ++i)
				cov[i] = io_sampleStats.m_covarImage.get(line, column, i) * invWeightSum;
			cov[int(bcd::ESymMatData::e_xx)] -= mean[0] * mean[0];
			cov[int(bcd::ESymMatData::e_yy)] -= mean[1] * mean[1];
			cov[int(bcd::ESymMatData::e_zz)] -= mean[2] * mean[2];
			cov[int(bcd::ESymMatData::e_yz)] -= mean[1] * mean[2];
			cov[int(bcd::ESymMatData::e_xz)] -= mean[0] * mean[2];
			cov[int(bcd::ESymMatData::e_xy)] -= mean[0] * mean[1];
			float biasCorrectionFactor = 1.f / (1 - squaredWeightSum / (weightSum * weightSum));
			for (int i = 0; i < 6; ++i)
				io_sampleStats.m_covarImage.set(line, column, i, cov[i] * biasCorrectionFactor);
		}
	}
}

bcd::SamplesStatisticsImages SamplesAccumulator::GetSamplesStatistics() const {
	bcd::SamplesStatisticsImages stats(m_samplesStatisticsImages);
	ComputeSampleStatistics(stats);
	return move(stats);
}

bcd::SamplesStatisticsImages SamplesAccumulator::ExtractSamplesStatistics() {
	ComputeSampleStatistics(m_samplesStatisticsImages);
	return move(m_samplesStatisticsImages);
}

//------------------------------------------------------------------------------
// Serialization methods
//------------------------------------------------------------------------------

template<class Archive> void SamplesAccumulator::save(Archive &ar, const unsigned int version) const {
	ar & m_width;
	ar & m_height;

	ar & m_histogramParameters.m_gamma;
	ar & m_histogramParameters.m_maxValue;
	ar & m_histogramParameters.m_nbOfBins;
	
	ar & boost::serialization::make_array<const float>(m_samplesStatisticsImages.m_covarImage.getDataPtr(),
			m_samplesStatisticsImages.m_covarImage.getSize());
	ar & boost::serialization::make_array<const float>(m_samplesStatisticsImages.m_histoImage.getDataPtr(),
			m_samplesStatisticsImages.m_histoImage.getSize());
	ar & boost::serialization::make_array<const float>(m_samplesStatisticsImages.m_meanImage.getDataPtr(),
			m_samplesStatisticsImages.m_meanImage.getSize());
	ar & boost::serialization::make_array<const float>(m_samplesStatisticsImages.m_nbOfSamplesImage.getDataPtr(),
			m_samplesStatisticsImages.m_nbOfSamplesImage.getSize());

	ar & boost::serialization::make_array<const float>(m_squaredWeightSumsImage.getDataPtr(),
			m_squaredWeightSumsImage.getSize());	
	
	ar & m_isValid;
}

template<class Archive>	void SamplesAccumulator::load(Archive &ar, const unsigned int version) {
	ar & m_width;
	ar & m_height;

	ar & m_histogramParameters.m_gamma;
	ar & m_histogramParameters.m_maxValue;
	ar & m_histogramParameters.m_nbOfBins;
	
	m_samplesStatisticsImages.m_covarImage.resize(m_width, m_height, 6);
	ar & boost::serialization::make_array<float>(m_samplesStatisticsImages.m_covarImage.getDataPtr(),
			m_samplesStatisticsImages.m_covarImage.getSize());

	m_samplesStatisticsImages.m_histoImage.resize(m_width, m_height, 3 * m_histogramParameters.m_nbOfBins);
	ar & boost::serialization::make_array<float>(m_samplesStatisticsImages.m_histoImage.getDataPtr(),
			m_samplesStatisticsImages.m_histoImage.getSize());

	m_samplesStatisticsImages.m_meanImage.resize(m_width, m_height, 3);
	ar & boost::serialization::make_array<float>(m_samplesStatisticsImages.m_meanImage.getDataPtr(),
			m_samplesStatisticsImages.m_meanImage.getSize());

	m_samplesStatisticsImages.m_nbOfSamplesImage.resize(m_width, m_height, 1);
	ar & boost::serialization::make_array<float>(m_samplesStatisticsImages.m_nbOfSamplesImage.getDataPtr(),
			m_samplesStatisticsImages.m_nbOfSamplesImage.getSize());

	m_squaredWeightSumsImage.resize(m_width, m_height, 1);
	ar & boost::serialization::make_array<float>(m_squaredWeightSumsImage.getDataPtr(),
			m_squaredWeightSumsImage.getSize());	
	
	ar & m_isValid;	
}

namespace slg {
// Explicit instantiations for portable archives
template void SamplesAccumulator::save(LuxOutputBinArchive &ar, const u_int version) const;
template void SamplesAccumulator::load(LuxInputBinArchive &ar, const u_int version);
template void SamplesAccumulator::save(LuxOutputTextArchive &ar, const u_int version) const;
template void SamplesAccumulator::load(LuxInputTextArchive &ar, const u_int version);
}
