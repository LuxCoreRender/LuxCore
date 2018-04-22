// This file is part of the reference implementation for the paper 
//   Bayesian Collaborative Denoising for Monte-Carlo Rendering
//   Malik Boughida and Tamy Boubekeur.
//   Computer Graphics Forum (Proc. EGSR 2017), vol. 36, no. 4, p. 137-153, 2017
//
// All rights reserved. Use of this source code is governed by a
// BSD-style license that can be found in the LICENSE.txt file.

#ifndef SAMPLES_ACCUMULATOR_H
#define SAMPLES_ACCUMULATOR_H

#include <memory>
#include <atomic>

#include "DeepImage.h"

namespace bcd
{

	struct HistogramParameters
	{
		HistogramParameters() :
				m_nbOfBins(20),
				m_gamma(2.2f),
				m_maxValue(2.5f) {}

		int m_nbOfBins;
		float m_gamma; ///< exponent for the exponential size increase of histogram bins
		float m_maxValue;
	};

	struct SamplesStatisticsImages
	{
		SamplesStatisticsImages() = default;
		SamplesStatisticsImages(int i_width, int i_height, int i_nbOfBins);
		SamplesStatisticsImages(const SamplesStatisticsImages&) = default;
		SamplesStatisticsImages(SamplesStatisticsImages&&) = default;
		SamplesStatisticsImages& operator=(const SamplesStatisticsImages&) = default;
		SamplesStatisticsImages& operator=(SamplesStatisticsImages&&) = default;
		~SamplesStatisticsImages() = default;

		DeepImage<float> m_nbOfSamplesImage;
		DeepImage<float> m_meanImage;
		DeepImage<float> m_covarImage;
		DeepImage<float> m_histoImage;
	};

	class SamplesAccumulator
	{
	public:
		SamplesAccumulator(
				int i_width, int i_height,
				const HistogramParameters& i_rHistogramParameters);

		void addSample(
				int i_line, int i_column,
				float i_sampleR, float i_sampleG, float i_sampleB,
				float i_weight = 1.f);

		SamplesStatisticsImages getSamplesStatistics() const;

		/// @brief Returns the accumulated sample statistics
		///
		/// This function performs a std::move on the internal arrays of the class,
		/// contrary to SamplesStatisticsImages::getSamplesStatistics which performs a copy.
		/// After this call, the SamplesAccumulator cannot be used to accumulate more samples, and should be destroyed.
		SamplesStatisticsImages extractSamplesStatistics();

	private:
		void computeSampleStatistics(SamplesStatisticsImages& io_sampleStats) const;

	private:
		int m_width;
		int m_height;
		HistogramParameters m_histogramParameters;
		SamplesStatisticsImages m_samplesStatisticsImages;
		DeepImage<float> m_squaredWeightSumsImage;

		bool m_isValid; ///< If you call extractSamplesStatistics, the object becomes invalid and should be destroyed
	};

	class SamplesAccumulatorThreadSafe : public SamplesAccumulator
	{
	public:
		SamplesAccumulatorThreadSafe(
				int i_width, int i_height,
				const HistogramParameters& i_rHistogramParameters);

		void addSampleThreadSafely(
				int i_line, int i_column,
				float i_sampleR, float i_sampleG, float i_sampleB,
				float i_weight = 1.f);

	private:
		mutable DeepImage< std::atomic<bool> > m_mutexImage;

	};

}

#endif // SAMPLES_ACCUMULATOR_H
