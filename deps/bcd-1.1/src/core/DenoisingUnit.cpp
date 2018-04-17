// This file is part of the reference implementation for the paper 
//   Bayesian Collaborative Denoising for Monte-Carlo Rendering
//   Malik Boughida and Tamy Boubekeur.
//   Computer Graphics Forum (Proc. EGSR 2017), vol. 36, no. 4, p. 137-153, 2017
//
// All rights reserved. Use of this source code is governed by a
// BSD-style license that can be found in the LICENSE.txt file.

#include "DenoisingUnit.h"

#include "Denoiser.h"

#ifdef FOUND_CUDA
#include "CudaHistogramDistance.h"
#include "cuda_runtime.h"
#endif

#include <iostream>
#include <chrono>

#include <cassert>


using namespace std;
using namespace Eigen;

namespace bcd
{

#ifdef COMPUTE_DENOISING_STATS

	DenoisingStatistics::DenoisingStatistics() :
			m_nbOfManagedPixels(0),
			m_nbOfDenoiseOnlyMainPatch(0),
			m_chronoElapsedTimes(),
			m_chronometers()
	{
		m_chronoElapsedTimes.fill(0.f);
	}

	DenoisingStatistics& DenoisingStatistics::operator+=(const DenoisingStatistics& i_rStats)
	{
		m_nbOfManagedPixels += i_rStats.m_nbOfManagedPixels;
		m_nbOfDenoiseOnlyMainPatch += i_rStats.m_nbOfDenoiseOnlyMainPatch;
		for(int i = 0; i < static_cast<int>(EChronometer::e_nb); ++i)
			m_chronoElapsedTimes[i] += i_rStats.m_chronoElapsedTimes[i];
		return *this;
	}

	void DenoisingStatistics::storeElapsedTimes()
	{
		for(int i = 0; i < static_cast<int>(EChronometer::e_nb); ++i)
			m_chronoElapsedTimes[i] = m_chronometers[i].getElapsedTime();
	}

	void DenoisingStatistics::print()
	{
		cout << "Number of pixels with fall back to simple average: "
			 << m_nbOfDenoiseOnlyMainPatch << " / " << m_nbOfManagedPixels << endl;
		cout << "Chronometers:" << endl;
		cout << "  denoisePatchAndSimilarPatches:      " << Chronometer::getStringFromTime(m_chronoElapsedTimes[static_cast<size_t>(EChronometer::e_denoisePatchAndSimilarPatches     )]) << endl;
		cout << "    selectSimilarPatches              " << Chronometer::getStringFromTime(m_chronoElapsedTimes[static_cast<size_t>(EChronometer::e_selectSimilarPatches              )]) << endl;
		cout << "    denoiseSelectedPatches            " << Chronometer::getStringFromTime(m_chronoElapsedTimes[static_cast<size_t>(EChronometer::e_denoiseSelectedPatches            )]) << endl;
		cout << "      computeNoiseCovPatchesMean      " << Chronometer::getStringFromTime(m_chronoElapsedTimes[static_cast<size_t>(EChronometer::e_computeNoiseCovPatchesMean        )]) << endl;
		cout << "      denoiseSelectedPatchesStep1     " << Chronometer::getStringFromTime(m_chronoElapsedTimes[static_cast<size_t>(EChronometer::e_denoiseSelectedPatchesStep1       )]) << endl;
		cout << "      denoiseSelectedPatchesStep2     " << Chronometer::getStringFromTime(m_chronoElapsedTimes[static_cast<size_t>(EChronometer::e_denoiseSelectedPatchesStep2       )]) << endl;
		cout << "      aggregateOutputPatches          " << Chronometer::getStringFromTime(m_chronoElapsedTimes[static_cast<size_t>(EChronometer::e_aggregateOutputPatches            )]) << endl;
		cout << endl;
	}

#endif



	// to shorten notations
	const size_t g_xx = static_cast<size_t>(ESymmetricMatrix3x3Data::e_xx);
	const size_t g_yy = static_cast<size_t>(ESymmetricMatrix3x3Data::e_yy);
	const size_t g_zz = static_cast<size_t>(ESymmetricMatrix3x3Data::e_zz);
	const size_t g_yz = static_cast<size_t>(ESymmetricMatrix3x3Data::e_yz);
	const size_t g_xz = static_cast<size_t>(ESymmetricMatrix3x3Data::e_xz);
	const size_t g_xy = static_cast<size_t>(ESymmetricMatrix3x3Data::e_xy);

	DenoisingUnit::DenoisingUnit(Denoiser& i_rDenoiser) :
			m_rDenoiser(i_rDenoiser),
			m_width(i_rDenoiser.getImagesWidth()),
			m_height(i_rDenoiser.getImagesHeight()),

			m_histogramDistanceThreshold(i_rDenoiser.getParameters().m_histogramDistanceThreshold),
			m_patchRadius(i_rDenoiser.getParameters().m_patchRadius),
			m_searchWindowRadius(i_rDenoiser.getParameters().m_searchWindowRadius),

			m_nbOfPixelsInPatch((2 * i_rDenoiser.getParameters().m_patchRadius + 1)
							  * (2 * i_rDenoiser.getParameters().m_patchRadius + 1)),
			m_maxNbOfSimilarPatches((2 * i_rDenoiser.getParameters().m_searchWindowRadius + 1)
							 * (2 * i_rDenoiser.getParameters().m_searchWindowRadius + 1)),
			m_colorPatchDimension(3 * m_nbOfPixelsInPatch),

			m_pColorImage(i_rDenoiser.getInputs().m_pColors),
			m_pNbOfSamplesImage(i_rDenoiser.getInputs().m_pNbOfSamples),
			m_pHistogramImage(i_rDenoiser.getInputs().m_pHistograms),
			m_pCovarianceImage(&(i_rDenoiser.getPixelCovarianceImage())),

			m_pNbOfSamplesSqrtImage(&(i_rDenoiser.getNbOfSamplesSqrtImage())),
			m_pOutputSummedColorImage(&(i_rDenoiser.getOutputSummedColorImage(omp_get_thread_num()))),
			m_pEstimatesCountImage(&(i_rDenoiser.getEstimatesCountImage(omp_get_thread_num()))),
			m_pIsCenterOfAlreadyDenoisedPatchImage(&(i_rDenoiser.getIsCenterOfAlreadyDenoisedPatchImage())),

			m_nbOfBins(i_rDenoiser.getInputs().m_pHistograms->getDepth()),

			m_mainPatchCenter(),
			m_similarPatchesCenters(m_maxNbOfSimilarPatches),

			m_nbOfSimilarPatches(0),
			m_nbOfSimilarPatchesInv(0.f),

			m_noiseCovPatchesMean(static_cast<size_t>(m_nbOfPixelsInPatch)),

			m_colorPatches(m_maxNbOfSimilarPatches, VectorXf(m_colorPatchDimension)),
			m_colorPatchesMean(m_colorPatchDimension),
			m_centeredColorPatches(m_maxNbOfSimilarPatches, VectorXf(m_colorPatchDimension)),
			m_colorPatchesCovMat(m_colorPatchDimension, m_colorPatchDimension),
			m_clampedCovMat(m_colorPatchDimension, m_colorPatchDimension),
			m_inversedCovMat(m_colorPatchDimension, m_colorPatchDimension),
			m_denoisedColorPatches(m_maxNbOfSimilarPatches, VectorXf(m_colorPatchDimension)),

			m_eigenSolver(m_colorPatchDimension),

			m_tmpNoiseCovPatch(static_cast<size_t>(m_nbOfPixelsInPatch)),
			m_tmpVec(m_colorPatchDimension),
			m_tmpMatrix(m_colorPatchDimension, m_colorPatchDimension)
#ifdef FOUND_CUDA
			, m_uCudaHistogramDistance(nullptr)
			, m_distancesToNeighborPatches(m_maxNbOfSimilarPatches)
#endif
#ifdef COMPUTE_DENOISING_STATS
			, m_uStats(new DenoisingStatistics())
#endif
	{
#ifdef FOUND_CUDA
		if(m_rDenoiser.getParameters().m_useCuda)
			m_uCudaHistogramDistance = unique_ptr<bcd::CudaHistogramDistance>(new bcd::CudaHistogramDistance(
					m_pHistogramImage->getDataPtr(), m_pNbOfSamplesImage->getDataPtr(),
					m_pHistogramImage->getWidth(), m_pHistogramImage->getHeight(), m_pHistogramImage->getDepth(),
					m_patchRadius, m_searchWindowRadius));
#endif
	}

	DenoisingUnit::~DenoisingUnit()
	{
	}

	void DenoisingUnit::denoisePatchAndSimilarPatches(const PixelPosition& i_rMainPatchCenter)
	{
		startChrono(EChronometer::e_denoisePatchAndSimilarPatches);
#ifdef COMPUTE_DENOISING_STATS
		++m_uStats->m_nbOfManagedPixels;
#endif
		m_mainPatchCenter = i_rMainPatchCenter;
		{
			float skippingProbability = m_rDenoiser.getParameters().m_markedPixelsSkippingProbability;
			if(skippingProbability != 0)
				if(m_pIsCenterOfAlreadyDenoisedPatchImage->getValue(m_mainPatchCenter, 0))
					if(skippingProbability == 1 || rand() < static_cast<int>(skippingProbability * RAND_MAX))
					{
						stopChrono(EChronometer::e_denoisePatchAndSimilarPatches);
						return;
					}
		}
#ifdef FOUND_CUDA
		if(m_rDenoiser.getParameters().m_useCuda)
			selectSimilarPatchesUsingCuda();
		else
			selectSimilarPatches();
#else
		selectSimilarPatches();
#endif
		if(m_nbOfSimilarPatches < m_colorPatchDimension + 1) // cannot inverse covariance matrix: fallback to simple average ; + 1 for safety
		{
			denoiseOnlyMainPatch();
#ifdef COMPUTE_DENOISING_STATS
			++m_uStats->m_nbOfDenoiseOnlyMainPatch;
#endif
			// m_pIsCenterOfAlreadyDenoisedPatchImage->set(m_mainPatchCenter, 0, true); // useless
			stopChrono(EChronometer::e_denoisePatchAndSimilarPatches);
			return;
		}
		denoiseSelectedPatches();
		stopChrono(EChronometer::e_denoisePatchAndSimilarPatches);
	}

	void DenoisingUnit::selectSimilarPatches()
	{
		startChrono(EChronometer::e_selectSimilarPatches);
		m_nbOfSimilarPatches = 0;
		PixelWindow searchWindow(
				m_width, m_height, m_mainPatchCenter,
				m_searchWindowRadius,
				m_patchRadius);
	//	float normalizedThreshold = m_histogramDistanceThreshold * m_nbOfPixelsInPatch;
		m_similarPatchesCenters.resize(m_maxNbOfSimilarPatches);
		for(PixelPosition neighborPixel : searchWindow)
		{
	//		if(histogramPatchSummedDistanceBad(m_mainPatchCenter, neighborPixel) <= normalizedThreshold)
			if(histogramPatchDistance(m_mainPatchCenter, neighborPixel) <= m_histogramDistanceThreshold)
				m_similarPatchesCenters[m_nbOfSimilarPatches++] = neighborPixel;
		}
		assert(m_nbOfSimilarPatches > 0);
		m_nbOfSimilarPatchesInv = 1.f / m_nbOfSimilarPatches;
		m_similarPatchesCenters.resize(m_nbOfSimilarPatches);
	//	m_colorPatches.resize(m_nbOfSimilarPatches);
	//	m_centeredColorPatches.resize(m_nbOfSimilarPatches);
	//	m_denoisedColorPatches.resize(m_nbOfSimilarPatches);
		stopChrono(EChronometer::e_selectSimilarPatches);
	}

#ifdef FOUND_CUDA
	void DenoisingUnit::selectSimilarPatchesUsingCuda()
	{
		startChrono(EChronometer::e_selectSimilarPatches);
		m_nbOfSimilarPatches = 0;

		m_uCudaHistogramDistance->computeDistances(
				&(*(m_distancesToNeighborPatches.begin())),
				m_mainPatchCenter.m_line, m_mainPatchCenter.m_column);

		PixelWindow searchWindow(
				m_width, m_height, m_mainPatchCenter,
				m_searchWindowRadius,
				m_patchRadius);

		m_similarPatchesCenters.resize(m_maxNbOfSimilarPatches);
		int neighborIndex = 0;
		for(PixelPosition neighborPixel : searchWindow)
		{
			// Begin of temporary test
			/*
			if((rand() % 10000) == 0)
			{
				cout << "(line,column) = (" << neighborPixel.m_line << "," << neighborPixel.m_column << ")" << endl;
				cout << "  distance computed by CUDA: " << m_distancesToNeighborPatches[neighborIndex] << endl;
				cout << "  distance computed on the CPU: " << histogramPatchDistance(m_mainPatchCenter, neighborPixel) << endl << endl;
			}
			*/
			// End of temporary test
			if(m_distancesToNeighborPatches[neighborIndex++] <= m_histogramDistanceThreshold)
				m_similarPatchesCenters[m_nbOfSimilarPatches++] = neighborPixel;
		}
		assert(m_nbOfSimilarPatches > 0);
		m_nbOfSimilarPatchesInv = 1.f / m_nbOfSimilarPatches;
		m_similarPatchesCenters.resize(m_nbOfSimilarPatches);
	//	m_colorPatches.resize(m_nbOfSimilarPatches);
	//	m_centeredColorPatches.resize(m_nbOfSimilarPatches);
	//	m_denoisedColorPatches.resize(m_nbOfSimilarPatches);
		stopChrono(EChronometer::e_selectSimilarPatches);
	}
#endif // FOUND_CUDA

	inline
	float DenoisingUnit::histogramPatchSummedDistanceBad(
			const PixelPosition& i_rPatchCenter1,
			const PixelPosition& i_rPatchCenter2)
	{
		float summedDistance = 0;
		PixelPatch pixPatch1(m_width, m_height, i_rPatchCenter1, m_patchRadius);
		PixelPatch pixPatch2(m_width, m_height, i_rPatchCenter2, m_patchRadius);

		assert(pixPatch1.getSize() == pixPatch2.getSize());

		PixPatchIt pixPatch1It = pixPatch1.begin();
		PixPatchIt pixPatch2It = pixPatch2.begin();
		PixPatchIt pixPatch1ItEnd = pixPatch1.end();
		for( ; pixPatch1It != pixPatch1ItEnd; ++pixPatch1It, ++pixPatch2It)
		{
			summedDistance += pixelHistogramDistanceBad(*pixPatch1It, *pixPatch2It);
		}
		return summedDistance;
	}

	inline
	float DenoisingUnit::pixelHistogramDistanceBad(
			const PixelPosition& i_rPixel1,
			const PixelPosition& i_rPixel2)
	{
		int nbOfNonBoth0Bins = 0;
		const float* pHistogram1Val = &(m_pHistogramImage->get(i_rPixel1, 0));
		const float* pHistogram2Val = &(m_pHistogramImage->get(i_rPixel2, 0));
		float binValue1, binValue2;
		float nbOfSamples1 = m_pNbOfSamplesImage->get(i_rPixel1, 0);
		float nbOfSamples2 = m_pNbOfSamplesImage->get(i_rPixel2, 0);;
		float diff;
		float sum = 0.f;
		for(int binIndex = 0; binIndex < m_nbOfBins; ++binIndex)
		{
			binValue1 = *pHistogram1Val++;
			binValue2 = *pHistogram2Val++;
			if(binValue1 == 0.f && binValue2 == 0.f)
				continue;
			++nbOfNonBoth0Bins;
			diff = nbOfSamples2 * binValue1 - nbOfSamples1 * binValue2;
			sum += diff * diff / (nbOfSamples1 * nbOfSamples2 * (binValue1 + binValue2));
		}
		return sum / nbOfNonBoth0Bins;
	}

	float DenoisingUnit::pixelHistogramDistanceBad2(
			const PixelPosition& i_rPixel1,
			const PixelPosition& i_rPixel2)
	{
		int nbOfNonBoth0Bins = 0;
		const float* pHistogram1Val = &(m_pHistogramImage->get(i_rPixel1, 0));
		const float* pHistogram2Val = &(m_pHistogramImage->get(i_rPixel2, 0));
		float binValue1, binValue2;
		float diff;
		float sum = 0.f;
		float nbOfSamplesSqrtQuotient = m_pNbOfSamplesSqrtImage->get(i_rPixel1, 0)
				/ m_pNbOfSamplesSqrtImage->get(i_rPixel2, 0);
		float nbOfSamplesSqrtQuotientInv = 1.f / nbOfSamplesSqrtQuotient;
		for(int binIndex = 0; binIndex < m_nbOfBins; ++binIndex)
		{
			binValue1 = *pHistogram1Val++;
			binValue2 = *pHistogram2Val++;
			if(binValue1 == 0.f && binValue2 == 0.f)
				continue;
			++nbOfNonBoth0Bins;
			diff = binValue1 * nbOfSamplesSqrtQuotientInv - binValue2 * nbOfSamplesSqrtQuotient;
			sum += diff * diff / (binValue1 + binValue2);
		}
		return sum / nbOfNonBoth0Bins;
	}

	inline
	float DenoisingUnit::histogramPatchDistance(
			const PixelPosition& i_rPatchCenter1,
			const PixelPosition& i_rPatchCenter2)
	{
		float summedDistance = 0;
		PixelPatch pixPatch1(m_width, m_height, i_rPatchCenter1, m_patchRadius);
		PixelPatch pixPatch2(m_width, m_height, i_rPatchCenter2, m_patchRadius);

		assert(pixPatch1.getSize() == pixPatch2.getSize());

		PixPatchIt pixPatch1It = pixPatch1.begin();
		PixPatchIt pixPatch2It = pixPatch2.begin();
		PixPatchIt pixPatch1ItEnd = pixPatch1.end();
		int totalNbOfNonBoth0Bins = 0;
		int nbOfNonBoth0Bins = 0;
		for( ; pixPatch1It != pixPatch1ItEnd; ++pixPatch1It, ++pixPatch2It)
		{
			summedDistance += pixelSummedHistogramDistance(nbOfNonBoth0Bins, *pixPatch1It, *pixPatch2It);
			totalNbOfNonBoth0Bins += nbOfNonBoth0Bins;
		}
		return summedDistance / totalNbOfNonBoth0Bins;
	}

	inline
	float DenoisingUnit::pixelSummedHistogramDistance(
			int& i_rNbOfNonBoth0Bins,
			const PixelPosition& i_rPixel1,
			const PixelPosition& i_rPixel2)
	{
		i_rNbOfNonBoth0Bins = 0;
		const float* pHistogram1Val = &(m_pHistogramImage->get(i_rPixel1, 0));
		const float* pHistogram2Val = &(m_pHistogramImage->get(i_rPixel2, 0));
		float binValue1, binValue2;
		float nbOfSamples1 = m_pNbOfSamplesImage->get(i_rPixel1, 0);
		float nbOfSamples2 = m_pNbOfSamplesImage->get(i_rPixel2, 0);;
		float diff;
		float sum = 0.f;
		for(int binIndex = 0; binIndex < m_nbOfBins; ++binIndex)
		{
			binValue1 = *pHistogram1Val++;
			binValue2 = *pHistogram2Val++;
	//		if(binValue1 == 0.f && binValue2 == 0.f) // TEMPORARILY COMMENTED
			if(binValue1 + binValue2 <= 1.f) // TEMPORARY
				continue;
			++i_rNbOfNonBoth0Bins;
			diff = nbOfSamples2 * binValue1 - nbOfSamples1 * binValue2;
			sum += diff * diff / (nbOfSamples1 * nbOfSamples2 * (binValue1 + binValue2));
		}
		return sum;
	}

	void DenoisingUnit::denoiseSelectedPatches()
	{
		startChrono(EChronometer::e_denoiseSelectedPatches);

		computeNoiseCovPatchesMean();
		denoiseSelectedPatchesStep1();
		denoiseSelectedPatchesStep2();
		aggregateOutputPatches();

		stopChrono(EChronometer::e_denoiseSelectedPatches);
	}

	void DenoisingUnit::computeNoiseCovPatchesMean()
	{
		startChrono(EChronometer::e_computeNoiseCovPatchesMean);

		CovMat3x3 zero3x3;
		zero3x3.m_data.fill(0.f);
		fill(m_noiseCovPatchesMean.m_blocks.begin(), m_noiseCovPatchesMean.m_blocks.end(), zero3x3);

		for(PixelPosition similarPatchCenter : m_similarPatchesCenters)
		{
			size_t patchPixelIndex = 0;
			ConstPatch patch(*m_pCovarianceImage, similarPatchCenter, m_patchRadius);
			for(const float* pPixelCovData : patch)
				m_tmpNoiseCovPatch.m_blocks[patchPixelIndex++].copyFrom(pPixelCovData);
			m_noiseCovPatchesMean += m_tmpNoiseCovPatch;
		}
		m_noiseCovPatchesMean *= m_nbOfSimilarPatchesInv;

		stopChrono(EChronometer::e_computeNoiseCovPatchesMean);
	}

	void DenoisingUnit::denoiseSelectedPatchesStep1()
	{
		startChrono(EChronometer::e_denoiseSelectedPatchesStep1);

		pickColorPatchesFromColorImage(m_colorPatches);
		empiricalMean(m_colorPatchesMean, m_colorPatches, m_nbOfSimilarPatches);
		centerPointCloud(m_centeredColorPatches, m_colorPatchesMean, m_colorPatches, m_nbOfSimilarPatches);
		empiricalCovarianceMatrix(m_colorPatchesCovMat, m_centeredColorPatches, m_nbOfSimilarPatches);
		substractCovMatPatchFromMatrix(m_colorPatchesCovMat, m_noiseCovPatchesMean);
		clampNegativeEigenValues(m_clampedCovMat, m_colorPatchesCovMat);
		addCovMatPatchToMatrix(m_clampedCovMat, m_noiseCovPatchesMean);
		inverseSymmetricMatrix(m_inversedCovMat, m_clampedCovMat);
		finalDenoisingMatrixMultiplication(m_denoisedColorPatches, m_colorPatches, m_noiseCovPatchesMean, m_inversedCovMat, m_centeredColorPatches);

		stopChrono(EChronometer::e_denoiseSelectedPatchesStep1);
	}

	void DenoisingUnit::denoiseSelectedPatchesStep2()
	{
		startChrono(EChronometer::e_denoiseSelectedPatchesStep2);

		empiricalMean(m_colorPatchesMean, m_denoisedColorPatches, m_nbOfSimilarPatches);
		centerPointCloud(m_centeredColorPatches, m_colorPatchesMean, m_denoisedColorPatches, m_nbOfSimilarPatches);
		empiricalCovarianceMatrix(m_colorPatchesCovMat, m_centeredColorPatches, m_nbOfSimilarPatches);
	//	clampNegativeEigenValues(m_clampedCovMat, m_colorPatchesCovMat); // maybe to uncomment to ensure a stricly positive minimum for eigen values... In that case comment next line
		m_clampedCovMat = m_colorPatchesCovMat;
		addCovMatPatchToMatrix(m_clampedCovMat, m_noiseCovPatchesMean);
		inverseSymmetricMatrix(m_inversedCovMat, m_clampedCovMat);
		centerPointCloud(m_centeredColorPatches, m_colorPatchesMean, m_colorPatches, m_nbOfSimilarPatches);
		finalDenoisingMatrixMultiplication(m_denoisedColorPatches, m_colorPatches, m_noiseCovPatchesMean, m_inversedCovMat, m_centeredColorPatches);

		stopChrono(EChronometer::e_denoiseSelectedPatchesStep2);
	}

	void DenoisingUnit::denoiseOnlyMainPatch()
	{
		m_colorPatchesMean.fill(0.f);
		int patchDataIndex = 0;
		for(const PixelPosition& rSimilarPatchCenter : m_similarPatchesCenters)
		{
			patchDataIndex = 0;
			ConstPatch patch(*m_pColorImage, rSimilarPatchCenter, m_patchRadius);
			for(const float* pPixelColorData : patch)
			{
				m_colorPatchesMean(patchDataIndex++) += pPixelColorData[0];
				m_colorPatchesMean(patchDataIndex++) += pPixelColorData[1];
				m_colorPatchesMean(patchDataIndex++) += pPixelColorData[2];
			}
		}
		Patch outputMainPatch(*m_pOutputSummedColorImage, m_mainPatchCenter, m_patchRadius);
		patchDataIndex = 0;
		for(float* pPixelOutputColorData : outputMainPatch)
		{
			pPixelOutputColorData[0] += m_nbOfSimilarPatchesInv * m_colorPatchesMean(patchDataIndex++);
			pPixelOutputColorData[1] += m_nbOfSimilarPatchesInv * m_colorPatchesMean(patchDataIndex++);
			pPixelOutputColorData[2] += m_nbOfSimilarPatchesInv * m_colorPatchesMean(patchDataIndex++);
		}
		ImageWindow<int> estimatesCountPatch(*m_pEstimatesCountImage, m_mainPatchCenter, m_patchRadius);
		for(int* pPixelEstimateCount : estimatesCountPatch)
			++(pPixelEstimateCount[0]);
	}

	void DenoisingUnit::pickColorPatchesFromColorImage(vector<VectorXf>& o_rColorPatches) const
	{
		int patchIndex = 0;
		for(const PixelPosition& rSimilarPatchCenter : m_similarPatchesCenters)
		{
			VectorXf& rColorPatch = o_rColorPatches[patchIndex++];
			int patchDataIndex = 0;
			ConstPatch patch(*m_pColorImage, rSimilarPatchCenter, m_patchRadius);
			for(const float* pPixelColorData : patch)
			{
				rColorPatch(patchDataIndex++) = pPixelColorData[0];
				rColorPatch(patchDataIndex++) = pPixelColorData[1];
				rColorPatch(patchDataIndex++) = pPixelColorData[2];
			}
		}
	}

	void DenoisingUnit::empiricalMean(
			VectorXf& o_rMean,
			const vector<VectorXf>& i_rPointCloud,
			int i_nbOfPoints) const
	{
		o_rMean.fill(0.f);
		for(int i = 0; i < i_nbOfPoints; ++i)
			o_rMean += i_rPointCloud[i];
		o_rMean *= (1.f / i_nbOfPoints);
	}

	void DenoisingUnit::centerPointCloud(
			vector<VectorXf>& o_rCenteredPointCloud,
			VectorXf& o_rMean,
			const vector<VectorXf>& i_rPointCloud,
			int i_nbOfPoints) const
	{
		vector<VectorXf>::iterator it = o_rCenteredPointCloud.begin();
		for(int i = 0; i < i_nbOfPoints; ++i)
			*it++ = i_rPointCloud[i] - o_rMean;
	}

	void DenoisingUnit::empiricalCovarianceMatrix(
			MatrixXf& o_rCovMat,
			const vector<VectorXf>& i_rCenteredPointCloud,
			int i_nbOfPoints) const
	{
		int d = static_cast<int>(o_rCovMat.rows());
		assert(d == o_rCovMat.cols());
		assert(d == i_rCenteredPointCloud[0].rows());
		o_rCovMat.fill(0.f);
		for(int i = 0; i < i_nbOfPoints; ++i)
			for(int c = 0; c < d; ++c)
				for(int r = 0; r < d; ++r)
					o_rCovMat(r, c) += i_rCenteredPointCloud[i](r) * i_rCenteredPointCloud[i](c);
		o_rCovMat *= (1.f / (i_nbOfPoints - 1));
	}

	void DenoisingUnit::addCovMatPatchToMatrix(MatrixXf& io_rMatrix, const CovMatPatch& i_rCovMatPatch) const
	{
		int blockXIndex = 0, blockYIndex = 1, blockZIndex = 2;
		for(const CovMat3x3& rCovMat3x3 : i_rCovMatPatch.m_blocks)
		{
			io_rMatrix(blockXIndex, blockXIndex) += rCovMat3x3.m_data[g_xx];
			io_rMatrix(blockYIndex, blockYIndex) += rCovMat3x3.m_data[g_yy];
			io_rMatrix(blockZIndex, blockZIndex) += rCovMat3x3.m_data[g_zz];
			io_rMatrix(blockYIndex, blockZIndex) += rCovMat3x3.m_data[g_yz];
			io_rMatrix(blockZIndex, blockYIndex) += rCovMat3x3.m_data[g_yz];
			io_rMatrix(blockXIndex, blockZIndex) += rCovMat3x3.m_data[g_xz];
			io_rMatrix(blockZIndex, blockXIndex) += rCovMat3x3.m_data[g_xz];
			io_rMatrix(blockXIndex, blockYIndex) += rCovMat3x3.m_data[g_xy];
			io_rMatrix(blockYIndex, blockXIndex) += rCovMat3x3.m_data[g_xy];
			blockXIndex += 3;
			blockYIndex += 3;
			blockZIndex += 3;
		}
	}

	void DenoisingUnit::substractCovMatPatchFromMatrix(MatrixXf& io_rMatrix, const CovMatPatch& i_rCovMatPatch) const
	{
		int blockXIndex = 0, blockYIndex = 1, blockZIndex = 2;
		for(const CovMat3x3& rCovMat3x3 : i_rCovMatPatch.m_blocks)
		{
			io_rMatrix(blockXIndex, blockXIndex) -= rCovMat3x3.m_data[g_xx];
			io_rMatrix(blockYIndex, blockYIndex) -= rCovMat3x3.m_data[g_yy];
			io_rMatrix(blockZIndex, blockZIndex) -= rCovMat3x3.m_data[g_zz];
			io_rMatrix(blockYIndex, blockZIndex) -= rCovMat3x3.m_data[g_yz];
			io_rMatrix(blockZIndex, blockYIndex) -= rCovMat3x3.m_data[g_yz];
			io_rMatrix(blockXIndex, blockZIndex) -= rCovMat3x3.m_data[g_xz];
			io_rMatrix(blockZIndex, blockXIndex) -= rCovMat3x3.m_data[g_xz];
			io_rMatrix(blockXIndex, blockYIndex) -= rCovMat3x3.m_data[g_xy];
			io_rMatrix(blockYIndex, blockXIndex) -= rCovMat3x3.m_data[g_xy];
			blockXIndex += 3;
			blockYIndex += 3;
			blockZIndex += 3;
		}
	}

	void DenoisingUnit::inverseSymmetricMatrix(MatrixXf& o_rInversedMatrix, const MatrixXf& i_rSymmetricMatrix)
	{
		float minEigenVal = m_rDenoiser.getParameters().m_minEigenValue;

		int d = static_cast<int>(i_rSymmetricMatrix.rows());
		assert(d == i_rSymmetricMatrix.cols());
		assert(d == o_rInversedMatrix.rows());
		assert(d == o_rInversedMatrix.cols());
		assert(d == m_tmpMatrix.rows());
		assert(d == m_tmpMatrix.cols());

		m_eigenSolver.compute(i_rSymmetricMatrix); // Decomposes i_rSymmetricMatrix into V D V^T
		const MatrixXf& rEigenVectors = m_eigenSolver.eigenvectors(); // Matrix V
		const VectorXf& rEigenValues = m_eigenSolver.eigenvalues(); // Matrix D is rEigenValues.asDiagonal()

		float diagValue;
		for(int r = 0; r < d; ++r)
		{
	//		if(rEigenValues(r) <= minEigenVal)
	//			cout << "Warning: eigen value below minimum during inversion:" << endl;
			diagValue = 1.f / max(minEigenVal, rEigenValues(r));
			for(int c = 0; c < d; ++c)
				m_tmpMatrix(r, c) = diagValue * rEigenVectors(c, r);
		}
		// now m_tmpMatrix equals (D^-1) V^T
		o_rInversedMatrix = rEigenVectors * m_tmpMatrix;
	}

	void DenoisingUnit::clampNegativeEigenValues(MatrixXf& o_rClampedMatrix, const MatrixXf& i_rSymmetricMatrix)
	{
		float minEigenVal = 0;

		int d = static_cast<int>(i_rSymmetricMatrix.rows());
		assert(d == i_rSymmetricMatrix.cols());
		assert(d == o_rClampedMatrix.rows());
		assert(d == o_rClampedMatrix.cols());
		assert(d == m_tmpMatrix.rows());
		assert(d == m_tmpMatrix.cols());

		m_eigenSolver.compute(i_rSymmetricMatrix); // Decomposes i_rSymmetricMatrix into V D V^T
		const MatrixXf& rEigenVectors = m_eigenSolver.eigenvectors(); // Matrix V
		const VectorXf& rEigenValues = m_eigenSolver.eigenvalues(); // Matrix D is rEigenValues.asDiagonal()

		float diagValue;
		for(int r = 0; r < d; ++r)
		{
			diagValue = max(minEigenVal, rEigenValues(r));
			for(int c = 0; c < d; ++c)
				m_tmpMatrix(r, c) = diagValue * rEigenVectors(c, r);
		}
		// now m_tmpMatrix equals (D^-1) V^T
		o_rClampedMatrix = rEigenVectors * m_tmpMatrix;
	}

	/// @brief o_rVector and i_rVector might be the same
	void DenoisingUnit::multiplyCovMatPatchByVector(VectorXf& o_rVector, const CovMatPatch& i_rCovMatPatch, const VectorXf& i_rVector) const
	{
		int blockXIndex = 0, blockYIndex = 1, blockZIndex = 2;
		for(const CovMat3x3& rCovMat3x3 : i_rCovMatPatch.m_blocks)
		{
			o_rVector(blockXIndex) =
					rCovMat3x3.m_data[g_xx] * i_rVector(blockXIndex) +
					rCovMat3x3.m_data[g_xy] * i_rVector(blockYIndex) +
					rCovMat3x3.m_data[g_xz] * i_rVector(blockZIndex);
			o_rVector(blockYIndex) =
					rCovMat3x3.m_data[g_xy] * i_rVector(blockXIndex) +
					rCovMat3x3.m_data[g_yy] * i_rVector(blockYIndex) +
					rCovMat3x3.m_data[g_yz] * i_rVector(blockZIndex);
			o_rVector(blockZIndex) =
					rCovMat3x3.m_data[g_xz] * i_rVector(blockXIndex) +
					rCovMat3x3.m_data[g_yz] * i_rVector(blockYIndex) +
					rCovMat3x3.m_data[g_zz] * i_rVector(blockZIndex);
			blockXIndex += 3;
			blockYIndex += 3;
			blockZIndex += 3;
		}
	}

	void DenoisingUnit::finalDenoisingMatrixMultiplication(
			std::vector<Eigen::VectorXf>& o_rDenoisedColorPatches,
			const std::vector<Eigen::VectorXf>& i_rNoisyColorPatches,
			const CovMatPatch& i_rNoiseCovMatPatch,
			const Eigen::MatrixXf& i_rInversedCovMat,
			const std::vector<Eigen::VectorXf>& i_rCenteredNoisyColorPatches)
	{
		for(int i = 0; i < m_nbOfSimilarPatches; ++i)
		{
			m_tmpVec = i_rInversedCovMat * i_rCenteredNoisyColorPatches[i];
			m_tmpVec *= -1.f;
			multiplyCovMatPatchByVector(o_rDenoisedColorPatches[i], i_rNoiseCovMatPatch, m_tmpVec);
			o_rDenoisedColorPatches[i] += i_rNoisyColorPatches[i];
		}
	}

	void DenoisingUnit::aggregateOutputPatches()
	{
		startChrono(EChronometer::e_aggregateOutputPatches);
		int patchIndex = 0;
		for(const PixelPosition& rSimilarPatchCenter : m_similarPatchesCenters)
		{
			VectorXf& rColorPatch = m_denoisedColorPatches[patchIndex++];
			int patchDataIndex = 0;
			Patch outputPatch(*m_pOutputSummedColorImage, rSimilarPatchCenter, m_patchRadius);
			for(float* pPixelColorData : outputPatch)
			{
				pPixelColorData[0] += rColorPatch(patchDataIndex++);
				pPixelColorData[1] += rColorPatch(patchDataIndex++);
				pPixelColorData[2] += rColorPatch(patchDataIndex++);
			}
			ImageWindow<int> estimatesCountPatch(*m_pEstimatesCountImage, rSimilarPatchCenter, m_patchRadius);
			for(int* pPixelEstimateCount : estimatesCountPatch)
				++(pPixelEstimateCount[0]);
			m_pIsCenterOfAlreadyDenoisedPatchImage->set(rSimilarPatchCenter, 0, true);
		}
		stopChrono(EChronometer::e_aggregateOutputPatches);
	}

} // namespace bcd
