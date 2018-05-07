// This file is part of the reference implementation for the paper 
//   Bayesian Collaborative Denoising for Monte-Carlo Rendering
//   Malik Boughida and Tamy Boubekeur.
//   Computer Graphics Forum (Proc. EGSR 2017), vol. 36, no. 4, p. 137-153, 2017
//
// All rights reserved. Use of this source code is governed by a
// BSD-style license that can be found in the LICENSE.txt file.

#ifndef CUDA_HISTOGRAM_DISTANCE_H
#define CUDA_HISTOGRAM_DISTANCE_H

namespace bcd
{

	void testCudaPrint();

	class CudaHistogramDistance
	{
	public:
		CudaHistogramDistance(const float *i_pHistogramData, const float *i_pNbOfSamplesData,
				int i_width, int i_height, int i_nbOfBins,
				int i_patchRadius, int i_searchWindowRadius);
		~CudaHistogramDistance();

		static int previousPowerOfTwo(int i_number);

		/// @brief Computes distance of patch centered on the input coordinates with all neighbor patches
		///
		/// @param[out] o_pDistances Array of size (2*m_searchWindowRadius + 1)^2 to store all the computed distances
		/// @param[in] i_line Line of the center of the main patch
		/// @param[in] i_column Column of the center of the main patch
		void computeDistances(float* o_pDistances, int i_line, int i_column);

	private:
		float* m_dHistogramData; // device pointer to histogram data
		float* m_dNbOfSamplesData; // device pointer to number of samples data
		float* m_dOutputDistances; // device pointer to the computed distances for all neighbor patches
		int m_width;
		int m_height;
		int m_nbOfBins;
		int m_patchRadius;
		int m_searchWindowRadius;
		int m_patchSize;
		int m_searchWindowSize;
		int m_nbOfFloatsInHistogramPatch;
	};

} // namespace bcd

#endif // CUDA_HISTOGRAM_DISTANCE_H
