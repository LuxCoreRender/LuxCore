// This file is part of the reference implementation for the paper 
//   Bayesian Collaborative Denoising for Monte-Carlo Rendering
//   Malik Boughida and Tamy Boubekeur.
//   Computer Graphics Forum (Proc. EGSR 2017), vol. 36, no. 4, p. 137-153, 2017
//
// All rights reserved. Use of this source code is governed by a
// BSD-style license that can be found in the LICENSE.txt file.

#ifndef DENOISER_H
#define DENOISER_H

#include "IDenoiser.h"

#include "DeepImage.h"

#include <vector>
#include <memory>

namespace bcd
{

	class PixelPosition;

	/// @brief Class to implement the monoscale Bayesian Collaborative Filtering for Monte-Carlo Rendering
	class Denoiser : public IDenoiser
	{
	public:
		Denoiser() : IDenoiser() {}
		virtual ~Denoiser() {}

	public: // public methods
		void ompTest();
		virtual bool denoise();

	public: // getters and setters
		const Deepimf& getNbOfSamplesSqrtImage() const { return m_nbOfSamplesSqrtImage; }
		const Deepimf& getPixelCovarianceImage() const { return m_pixelCovarianceImage; }
		Deepimf& getOutputSummedColorImage(int i_index) { return m_outputSummedColorImages[i_index]; }
		DeepImage<int>& getEstimatesCountImage(int i_index) { return m_estimatesCountImages[i_index]; }
		DeepImage<bool>& getIsCenterOfAlreadyDenoisedPatchImage() { return m_isCenterOfAlreadyDenoisedPatchImage; }
		int getImagesWidth() const { return m_width; }
		int getImagesHeight() const { return m_height; }

		bool inputsOutputsAreOk();

	private:
		int m_width;
		int m_height;
		int m_nbOfPixels; ///< equals to m_width x m_height

		Deepimf m_nbOfSamplesSqrtImage;
		Deepimf m_pixelCovarianceImage;
		std::vector< Deepimf > m_outputSummedColorImages;
		std::vector< DeepImage<int> > m_estimatesCountImages;
		DeepImage<bool> m_isCenterOfAlreadyDenoisedPatchImage; ///< For the "marking strategy"


	private: // private auxilliary methods
		void computeNbOfSamplesSqrt();
		void computePixelCovFromSampleCov();
		void reorderPixelSet(std::vector<PixelPosition>& io_rPixelSet) const; ///< Reorders the pixel set
		/// @brief Reorders the pixel by cutting the image in horizontal strips,
		/// and scanning the strips in the order 1, 3, 5, 7, ... 2, 4, 6, 8, ...
		void reorderPixelSetJumpNextStrip(std::vector<PixelPosition>& io_rPixelSet) const;
		static void reorderPixelSetJumpNextChunk(std::vector<PixelPosition>& io_rPixelSet, int i_chunkSize);
		static void reorderPixelSetShuffle(std::vector<PixelPosition>& io_rPixelSet); ///< Totally random reordering
		void finalAggregation();

	};

} // namespace bcd


#endif // DENOISER_H
