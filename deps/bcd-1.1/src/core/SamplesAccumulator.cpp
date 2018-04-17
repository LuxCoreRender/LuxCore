// This file is part of the reference implementation for the paper 
//   Bayesian Collaborative Denoising for Monte-Carlo Rendering
//   Malik Boughida and Tamy Boubekeur.
//   Computer Graphics Forum (Proc. EGSR 2017), vol. 36, no. 4, p. 137-153, 2017
//
// All rights reserved. Use of this source code is governed by a
// BSD-style license that can be found in the LICENSE.txt file.

#include "SamplesAccumulator.h"
#include "CovarianceMatrix.h"


#include <cassert>

using namespace std;

namespace bcd
{

	SamplesStatisticsImages::SamplesStatisticsImages(int i_width, int i_height, int i_nbOfBins) :
			m_nbOfSamplesImage(i_width, i_height, 1),
			m_meanImage(i_width, i_height, 3),
			m_covarImage(i_width, i_height, 6),
			m_histoImage(i_width, i_height, 3 * i_nbOfBins)
	{
	}

	SamplesAccumulator::SamplesAccumulator(
			int i_width, int i_height,
			const HistogramParameters& i_rHistogramParameters) :
			m_width(i_width), m_height(i_height),
			m_histogramParameters(i_rHistogramParameters),
			m_samplesStatisticsImages(i_width, i_height, i_rHistogramParameters.m_nbOfBins),
			m_squaredWeightSumsImage(i_width, i_height, 1),
			m_isValid(true)
	{
		m_samplesStatisticsImages.m_nbOfSamplesImage.fill(0.f);
		m_samplesStatisticsImages.m_meanImage.fill(0.f);
		m_samplesStatisticsImages.m_covarImage.fill(0.f);
		m_samplesStatisticsImages.m_histoImage.fill(0.f);
		m_squaredWeightSumsImage.fill(0.f);
	}

	void SamplesAccumulator::addSample(
			int i_line, int i_column,
			float i_sampleR, float i_sampleG, float i_sampleB,
			float i_weight)
	{
		assert(m_isValid);

		const float sample[3] = { i_sampleR, i_sampleG, i_sampleB };
		const float satureLevelGamma = 2.f; // used for determining the weight to give to the sample in the highest two bins, when the sample is saturated

		m_samplesStatisticsImages.m_nbOfSamplesImage.get(i_line, i_column, 0) += i_weight;
		m_squaredWeightSumsImage.get(i_line, i_column, 0) += i_weight * i_weight;

		PixelPosition p(i_line, i_column);
		DeepImage<float>& rSum = m_samplesStatisticsImages.m_meanImage;
		rSum.get(i_line, i_column, 0) += i_weight * i_sampleR;
		rSum.get(i_line, i_column, 1) += i_weight * i_sampleG;
		rSum.get(i_line, i_column, 2) += i_weight * i_sampleB;

		DeepImage<float>& rCovSum = m_samplesStatisticsImages.m_covarImage;
		rCovSum.get(i_line, i_column, int(ESymMatData::e_xx)) += i_weight * i_sampleR * i_sampleR;
		rCovSum.get(i_line, i_column, int(ESymMatData::e_yy)) += i_weight * i_sampleG * i_sampleG;
		rCovSum.get(i_line, i_column, int(ESymMatData::e_zz)) += i_weight * i_sampleB * i_sampleB;
		rCovSum.get(i_line, i_column, int(ESymMatData::e_yz)) += i_weight * i_sampleG * i_sampleB;
		rCovSum.get(i_line, i_column, int(ESymMatData::e_xz)) += i_weight * i_sampleR * i_sampleB;
		rCovSum.get(i_line, i_column, int(ESymMatData::e_xy)) += i_weight * i_sampleR * i_sampleG;

		int floorBinIndex;
		int ceilBinIndex;
		float binFloatIndex;
		float floorBinWeight;
		float ceilBinWeight;

		for(int32_t channelIndex = 0; channelIndex < 3; ++channelIndex)
		{ // fill histogram; code refactored from Ray Histogram Fusion PBRT code
			float value = sample[channelIndex];
			value = (value > 0 ? value : 0);
			if(m_histogramParameters.m_gamma > 1)
				value = pow(value, 1.f / m_histogramParameters.m_gamma); // exponential scaling
			if(m_histogramParameters.m_maxValue > 0)
				value = (value / m_histogramParameters.m_maxValue); // normalize to the maximum value
			value = value > satureLevelGamma ? satureLevelGamma : value;

			binFloatIndex = value * (m_histogramParameters.m_nbOfBins - 2);
			floorBinIndex = int(binFloatIndex);

			if(floorBinIndex < m_histogramParameters.m_nbOfBins - 2) // in bounds
			{
				ceilBinIndex = floorBinIndex + 1;
				ceilBinWeight = binFloatIndex - floorBinIndex;
				floorBinWeight = 1.0f - ceilBinWeight;
			}
			else
			{ //out of bounds... v >= 1
				floorBinIndex = m_histogramParameters.m_nbOfBins - 2;
				ceilBinIndex = floorBinIndex + 1;
				ceilBinWeight = (value - 1.0f) / (satureLevelGamma - 1.f);
				floorBinWeight = 1.0f - ceilBinWeight;
			}
			m_samplesStatisticsImages.m_histoImage.get(i_line, i_column, channelIndex * m_histogramParameters.m_nbOfBins + floorBinIndex) += i_weight * floorBinWeight;
			m_samplesStatisticsImages.m_histoImage.get(i_line, i_column, channelIndex * m_histogramParameters.m_nbOfBins + ceilBinIndex) += i_weight * ceilBinWeight;
		}

	}
	
	void SamplesAccumulator::reset() {
		m_samplesStatisticsImages.m_nbOfSamplesImage.fill(0.f);
		m_samplesStatisticsImages.m_meanImage.fill(0.f);
		m_samplesStatisticsImages.m_covarImage.fill(0.f);
		m_samplesStatisticsImages.m_histoImage.fill(0.f);
		m_squaredWeightSumsImage.fill(0.f);
	}

	void SamplesAccumulator::addAccumulator(const SamplesAccumulator &samplesAccumulator) {
		assert(m_isValid);
		assert(m_width == samplesAccumulator.m_width);
		assert(m_height == samplesAccumulator.m_height);
		assert(m_histogramParameters.m_nbOfBins == samplesAccumulator.m_histogramParameters.m_nbOfBins);
		assert(m_histogramParameters.m_gamma == samplesAccumulator.m_histogramParameters.m_gamma);
		assert(m_histogramParameters.m_maxValue == samplesAccumulator.m_histogramParameters.m_maxValue);
		
		for(int line = 0; line < m_height; ++line) {
			for(int column = 0; column < m_width; ++column) {
				m_samplesStatisticsImages.m_nbOfSamplesImage.get(line, column, 0) +=
						samplesAccumulator.m_samplesStatisticsImages.m_nbOfSamplesImage.get(line, column, 0);

				m_squaredWeightSumsImage.get(line, column, 0) +=
						samplesAccumulator.m_squaredWeightSumsImage.get(line, column, 0);

				DeepImage<float> &rSumDst = m_samplesStatisticsImages.m_meanImage;
				const DeepImage<float> &rSumSrc = samplesAccumulator.m_samplesStatisticsImages.m_meanImage;
				rSumDst.get(line, column, 0) += rSumSrc.get(line, column, 0);
				rSumDst.get(line, column, 1) += rSumSrc.get(line, column, 1);
				rSumDst.get(line, column, 2) += rSumSrc.get(line, column, 2);
				
				DeepImage<float> &rCovSumDst = m_samplesStatisticsImages.m_covarImage;
				const DeepImage<float> &rCovSumSrc = samplesAccumulator.m_samplesStatisticsImages.m_covarImage;
				rCovSumDst.get(line, column, int(ESymMatData::e_xx)) += rCovSumSrc.get(line, column, int(ESymMatData::e_xx));
				rCovSumDst.get(line, column, int(ESymMatData::e_yy)) += rCovSumSrc.get(line, column, int(ESymMatData::e_yy));
				rCovSumDst.get(line, column, int(ESymMatData::e_zz)) += rCovSumSrc.get(line, column, int(ESymMatData::e_zz));
				rCovSumDst.get(line, column, int(ESymMatData::e_yz)) += rCovSumSrc.get(line, column, int(ESymMatData::e_yz));
				rCovSumDst.get(line, column, int(ESymMatData::e_xz)) += rCovSumSrc.get(line, column, int(ESymMatData::e_xz));
				rCovSumDst.get(line, column, int(ESymMatData::e_xy)) += rCovSumSrc.get(line, column, int(ESymMatData::e_xy));

				for(int32_t channelIndex = 0; channelIndex < 3; ++channelIndex) {
					for(int32_t binIndex = 0; binIndex < m_histogramParameters.m_nbOfBins; ++binIndex) {
						m_samplesStatisticsImages.m_histoImage.get(line, column, channelIndex * m_histogramParameters.m_nbOfBins + binIndex) +=
								samplesAccumulator.m_samplesStatisticsImages.m_histoImage.get(line, column, channelIndex * m_histogramParameters.m_nbOfBins + binIndex);
					}
				}
			}
		}
	}


	void SamplesAccumulator::computeSampleStatistics(SamplesStatisticsImages& io_sampleStats) const
	{
		float mean[3];
		float cov[6];

		for(int line = 0; line < m_height; ++line)
			for(int column = 0; column < m_width; ++column)
			{
				float weightSum = io_sampleStats.m_nbOfSamplesImage.get(line, column, 0);
				float squaredWeightSum = m_squaredWeightSumsImage.get(line, column, 0);

				float invWeightSum = 1.f / weightSum;

				for(int i = 0; i < 3; ++i)
				{
					mean[i] = invWeightSum * io_sampleStats.m_meanImage.get(line, column, i);
					io_sampleStats.m_meanImage.set(line, column, i, mean[i]);
				}

				for(int i = 0; i < 6; ++i)
					cov[i] = io_sampleStats.m_covarImage.get(line, column, i) * invWeightSum;
				cov[int(ESymMatData::e_xx)] -= mean[0] * mean[0];
				cov[int(ESymMatData::e_yy)] -= mean[1] * mean[1];
				cov[int(ESymMatData::e_zz)] -= mean[2] * mean[2];
				cov[int(ESymMatData::e_yz)] -= mean[1] * mean[2];
				cov[int(ESymMatData::e_xz)] -= mean[0] * mean[2];
				cov[int(ESymMatData::e_xy)] -= mean[0] * mean[1];
				float biasCorrectionFactor = 1.f / (1 - squaredWeightSum / (weightSum * weightSum));
				for(int i = 0; i < 6; ++i)
					io_sampleStats.m_covarImage.set(line, column, i, cov[i] * biasCorrectionFactor);
			}
	}

	SamplesStatisticsImages SamplesAccumulator::getSamplesStatistics() const
	{
		SamplesStatisticsImages stats(m_samplesStatisticsImages);
		computeSampleStatistics(stats);
		return move(stats);
	}

	SamplesStatisticsImages SamplesAccumulator::extractSamplesStatistics()
	{
		computeSampleStatistics(m_samplesStatisticsImages);
		return move(m_samplesStatisticsImages);
	}

	void SamplesAccumulatorThreadSafe::addSampleThreadSafely(
			int i_line, int i_column,
			float i_sampleR, float i_sampleG, float i_sampleB,
			float i_weight)
	{
		// lock

		addSample(i_line, i_column, i_sampleR, i_sampleG, i_sampleB, i_weight);

	}

} // namespace bcd
