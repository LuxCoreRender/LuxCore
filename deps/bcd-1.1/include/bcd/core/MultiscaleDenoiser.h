// This file is part of the reference implementation for the paper 
//   Bayesian Collaborative Denoising for Monte-Carlo Rendering
//   Malik Boughida and Tamy Boubekeur.
//   Computer Graphics Forum (Proc. EGSR 2017), vol. 36, no. 4, p. 137-153, 2017
//
// All rights reserved. Use of this source code is governed by a
// BSD-style license that can be found in the LICENSE.txt file.

#ifndef MULTISCALE_DENOISER_H
#define MULTISCALE_DENOISER_H

#include "IDenoiser.h"

#include <vector>
#include <memory>

namespace bcd
{

	template<class T> class DeepImage;

	/// @brief Class to implement the multiscale Bayesian Collaborative Filtering for Monte-Carlo Rendering
	class MultiscaleDenoiser : public IDenoiser
	{
	public:
		MultiscaleDenoiser(int i_nbOfScales) : IDenoiser(), m_nbOfScales(i_nbOfScales) {}
		virtual ~MultiscaleDenoiser() {}

	public: // public methods
		virtual bool denoise();

	private: // private methods
		static std::vector< std::unique_ptr< DeepImage<float> > > generateDownscaledEmptyImages(
				const DeepImage<float>& i_rScale0Image,
				int i_nbOfScalesToGenerate);
		static std::vector< std::unique_ptr< DeepImage<float> > > generateDownscaledSumImages(
				const DeepImage<float>& i_rScale0Image,
				int i_nbOfScalesToGenerate);
		static std::vector< std::unique_ptr< DeepImage<float> > > generateDownscaledAverageImages(
				const DeepImage<float>& i_rScale0Image,
				int i_nbOfScalesToGenerate);
		static std::vector< std::unique_ptr< DeepImage<float> > > generateDownscaledSampleCovarianceSumImages(
				const DeepImage<float>& i_rScale0SampleCovarianceImage,
				const DeepImage<float>& i_rScale0NbOfSamplesImage,
				const std::vector< std::unique_ptr< DeepImage<float> > >& i_rDownscaledNbOfSamplesImages,
				int i_nbOfScalesToGenerate);

#if 0
		std::vector< std::unique_ptr< DeepImage<float> > > generateDownscaledWeightedSumImages(
				const DeepImage<float>& i_rScale0Image,
				const DeepImage<float>& i_rScale0WeightImage,
				const std::vector< std::unique_ptr< DeepImage<float> > >& i_rDownscaledWeightImages,
				int i_nbOfScalesToGenerate);
		std::vector< std::unique_ptr< DeepImage<float> > > generateDownscaledSquaredWeightedSumImages(
				const DeepImage<float>& i_rScale0Image,
				const DeepImage<float>& i_rScale0WeightImage,
				const std::vector< std::unique_ptr< DeepImage<float> > >& i_rDownscaledWeightImages,
				int i_nbOfScalesToGenerate);
#endif
		static std::unique_ptr< DeepImage<float> > downscaleSum(const DeepImage<float>& i_rImage);
		static std::unique_ptr< DeepImage<float> > downscaleAverage(const DeepImage<float>& i_rImage);
		static std::unique_ptr< DeepImage<float> > downscaleSampleCovarianceSum(const DeepImage<float>& i_rSampleCovarianceImage, const DeepImage<float>& i_rNbOfSamplesImage);
#if 0
		static std::unique_ptr< DeepImage<float> > downscaleWeightedSum(const DeepImage<float>& i_rImage, const DeepImage<float>& i_rWeightImage);
		static std::unique_ptr< DeepImage<float> > downscaleSquaredWeightedSum(const DeepImage<float>& i_rImage, const DeepImage<float>& i_rWeightImage);
#endif
		/// @brief Merges two buffers of two successive scales
		///
		/// o_rMergedImage and i_rHighFrequencyImage can point to the same buffer
		static void mergeOutputsNoInterpolation(
				DeepImage<float>& o_rMergedImage,
				const DeepImage<float>& i_rLowFrequencyImage,
				const DeepImage<float>& i_rHighFrequencyImage);

		/// @brief Merges two buffers of two successive scales
		///
		/// o_rMergedImage and i_rHighFrequencyImage can point to the same buffer
		static void mergeOutputs(
				DeepImage<float>& o_rMergedHighResImage,
				DeepImage<float>& o_rTmpHighResImage,
				DeepImage<float>& o_rTmpLowResImage,
				const DeepImage<float>& i_rLowResImage,
				const DeepImage<float>& i_rHighResImage);

		static void interpolate(DeepImage<float>& o_rInterpolatedImage, const DeepImage<float>& i_rImage);

		static void downscale(DeepImage<float>& o_rDownscaledImage, const DeepImage<float>& i_rImage);

		/// @brief equivalent to downscale followed by interpolate
		static void lowPass(
				DeepImage<float>& o_rFilteredImage,
				DeepImage<float>& o_rTmpLowResImage,
				const DeepImage<float>& i_rImage);

		/// @brief equivalent to interpolate followed by downscale
		static void interpolateThenDownscale(DeepImage<float>& o_rFilteredImage, const DeepImage<float>& i_rImage);

	private: // private attributes
		int m_nbOfScales;

	};

} // namespace bcd

#endif // MULTISCALE_DENOISER_H
