// This file is part of the reference implementation for the paper 
//   Bayesian Collaborative Denoising for Monte-Carlo Rendering
//   Malik Boughida and Tamy Boubekeur.
//   Computer Graphics Forum (Proc. EGSR 2017), vol. 36, no. 4, p. 137-153, 2017
//
// All rights reserved. Use of this source code is governed by a
// BSD-style license that can be found in the LICENSE.txt file.

#include "CudaHistogramDistance.h"

#include "CudaUtils.h"

#include <iostream>
#include <cstdlib>

using namespace std;


#define N 10

namespace bcd
{

	__global__ void testCudaPrintKernel()
	{
		printf("(%d, %d): hello\n", blockIdx.x, threadIdx.x);
	}

	void testCudaPrint()
	{
		cout << "Entering testCudaPrint()" << endl;
#if __CUDA_ARCH__ >= 200
		cout << "Cuda arch is >= 200" << endl;
#endif

		dim3 nbOfBlocksPerGrid(3);
		dim3 nbOfThreadsPerBlock(2);
		testCudaPrintKernel<<<nbOfBlocksPerGrid, nbOfThreadsPerBlock>>>();
		cudaDeviceSynchronize();
	}

	/// @brief Parameters that will remain constant for all the program
	struct CudaHistogramDistanceConstantParameters
	{
	public:
		int m_histogramImageColumnOffset;
		int m_histogramImageLineOffset;
		int m_nbOfSamplesImageColumnOffset;
		int m_nbOfSamplesImageLineOffset;
		int m_patchSize;
		int m_searchWindowSize;
		int m_nbOfBins;
		int m_nbOfFloatsInHistogramPatch; // = m_patchSize * m_patchSize * m_nbOfBins
		int m_powerOfTwoBeforeNbOfFloatsInHistogramPatch; // highest power of two that is strictly inferior to m_nbOfFloatsInHistogramPatch
	};

	/* // to be tested
	/// @brief Parameters which will change for each kernel change
	struct CudaHistogramDistanceParameters
	{
	public:
		float* m_dMainPatchTopLeftCornerHist;
		float* m_dSearchWindowTopLeftCornerHist;
		float* m_dMainPatchTopLeftCornerNbOfSamples;
		float* m_dSearchWindowTopLeftCornerNbOfSamples;
	};
	*/

	__constant__ CudaHistogramDistanceConstantParameters g_constantParameters;
	//__constant__ CudaHistogramDistanceParameters g_parameters;

	__global__ void computeDistancesWithCuda(
			float* o_dOutputDistances,
			float* i_dMainPatchTopLeftCornerHist, float* i_dSearchWindowTopLeftCornerHist,
			float* i_dMainPatchTopLeftCornerNbOfSamples, float* i_dSearchWindowTopLeftCornerNbOfSamples)
	{
		// threadIdx.x: bin index
		// threadIdx.y: patch column offset
		// threadIdx.z: patch line offset
		// blockIdx.x: search window column offset
		// blockIdx.y: search window line offset

		extern __shared__ float s_dDynamicSharedMemory[];
		float* s_dSumTerms = s_dDynamicSharedMemory;
		float* s_dNonBoth0SumTerms = reinterpret_cast<float*>(s_dDynamicSharedMemory + g_constantParameters.m_nbOfFloatsInHistogramPatch);

	//	if(threadIdx.x == 0 && threadIdx.y == 0 && threadIdx.z == 0)
	//		printf("(%d, %d): hello\n", blockIdx.x, blockIdx.y);

	//	if(threadIdx.x >= g_constantParameters.m_nbOfBins)
	//		return; // Warning: danger with later __syncthreads()...?!
		float h1 = i_dMainPatchTopLeftCornerHist[
				threadIdx.x +
				threadIdx.y * g_constantParameters.m_histogramImageColumnOffset +
				threadIdx.z * g_constantParameters.m_histogramImageLineOffset];
		float n1 = i_dMainPatchTopLeftCornerNbOfSamples[
				threadIdx.y * g_constantParameters.m_nbOfSamplesImageColumnOffset +
				threadIdx.z * g_constantParameters.m_nbOfSamplesImageLineOffset];
		float h2 = i_dSearchWindowTopLeftCornerHist[
				threadIdx.x +
				(blockIdx.x + threadIdx.y) * g_constantParameters.m_histogramImageColumnOffset +
				(blockIdx.y + threadIdx.z) * g_constantParameters.m_histogramImageLineOffset];
		float n2 = i_dSearchWindowTopLeftCornerNbOfSamples[
				(blockIdx.x + threadIdx.y) * g_constantParameters.m_nbOfSamplesImageColumnOffset +
				(blockIdx.y + threadIdx.z) * g_constantParameters.m_nbOfSamplesImageLineOffset];
		float diff = h1 * n2 - h2 * n1;
		float h1h2Sum = h1 + h2;
		int termIndex = threadIdx.x + g_constantParameters.m_nbOfBins *
				(threadIdx.y + g_constantParameters.m_patchSize * threadIdx.z);
		s_dNonBoth0SumTerms[termIndex] = h1h2Sum > 0 ? 1 : 0;
		s_dSumTerms[termIndex] = h1h2Sum > 0 ? (diff * diff) / (n1 * n2 * h1h2Sum) : 0;

		// now we sum up the terms
		__syncthreads();

		int nbOfTerms = g_constantParameters.m_powerOfTwoBeforeNbOfFloatsInHistogramPatch;
		if(termIndex + nbOfTerms < g_constantParameters.m_nbOfFloatsInHistogramPatch)
		{
			s_dSumTerms[termIndex] += s_dSumTerms[termIndex + nbOfTerms];
			s_dNonBoth0SumTerms[termIndex] += s_dNonBoth0SumTerms[termIndex + nbOfTerms];
		}
		__syncthreads();
		nbOfTerms >>= 1;

	//	for(; nbOfTerms > 32; nbOfTerms >>= 1) // for "optimization" that does not work
		for(; nbOfTerms > 0; nbOfTerms >>= 1)
		{
			if(termIndex < nbOfTerms)
			{
				s_dSumTerms[termIndex] += s_dSumTerms[termIndex + nbOfTerms];
				s_dNonBoth0SumTerms[termIndex] += s_dNonBoth0SumTerms[termIndex + nbOfTerms];
				__syncthreads();
			}
		}

		/* "optimization" that... does not work
		if (termIndex < 32)
		{
			s_dSumTerms[termIndex] += s_dSumTerms[termIndex + 32];
			s_dSumTerms[termIndex] += s_dSumTerms[termIndex + 16];
			s_dSumTerms[termIndex] += s_dSumTerms[termIndex + 8];
			s_dSumTerms[termIndex] += s_dSumTerms[termIndex + 4];
			s_dSumTerms[termIndex] += s_dSumTerms[termIndex + 2];
			s_dSumTerms[termIndex] += s_dSumTerms[termIndex + 1];

			s_dNonBoth0SumTerms[termIndex] += s_dNonBoth0SumTerms[termIndex + 32];
			s_dNonBoth0SumTerms[termIndex] += s_dNonBoth0SumTerms[termIndex + 16];
			s_dNonBoth0SumTerms[termIndex] += s_dNonBoth0SumTerms[termIndex + 8];
			s_dNonBoth0SumTerms[termIndex] += s_dNonBoth0SumTerms[termIndex + 4];
			s_dNonBoth0SumTerms[termIndex] += s_dNonBoth0SumTerms[termIndex + 2];
			s_dNonBoth0SumTerms[termIndex] += s_dNonBoth0SumTerms[termIndex + 1];
		}
		*/

		if(termIndex == 0)
		{
	//		printf("(%d, %d): %f\n", blockIdx.x, blockIdx.y, s_dSumTerms[0]);
			o_dOutputDistances[blockIdx.x + gridDim.x * blockIdx.y] =
					s_dSumTerms[0] / s_dNonBoth0SumTerms[0];
		}
	}


	CudaHistogramDistance::CudaHistogramDistance(
			const float* i_pHistogramData, const float* i_pNbOfSamplesData,
			int i_width, int i_height, int i_nbOfBins,
			int i_patchRadius, int i_searchWindowRadius) :
		m_dHistogramData(NULL), m_dNbOfSamplesData(NULL),
		m_dOutputDistances(NULL),
		m_width(i_width), m_height(i_height), m_nbOfBins(i_nbOfBins),
		m_patchRadius(i_patchRadius), m_searchWindowRadius(i_searchWindowRadius),
		m_patchSize(1 + 2 * i_patchRadius), m_searchWindowSize(1 + 2 * i_searchWindowRadius),
		m_nbOfFloatsInHistogramPatch(m_patchSize * m_patchSize * m_nbOfBins)
	{
		int nbOfPixels = i_width * i_height;
		int nbOfPixelsInSearchWindow = (2*i_searchWindowRadius + 1)*(2*i_searchWindowRadius + 1);
		HANDLE_ERROR(cudaMalloc(reinterpret_cast<void**>(&m_dOutputDistances), nbOfPixelsInSearchWindow * sizeof(float)));

		int nbOfFloatsInHistogramImage = nbOfPixels * i_nbOfBins;
		HANDLE_ERROR(cudaMalloc(reinterpret_cast<void**>(&m_dHistogramData), nbOfFloatsInHistogramImage * sizeof(float)));
		HANDLE_ERROR(cudaMalloc(reinterpret_cast<void**>(&m_dNbOfSamplesData), nbOfPixels * sizeof(float)));
		HANDLE_ERROR(cudaMemcpy(m_dHistogramData, i_pHistogramData, nbOfFloatsInHistogramImage * sizeof(float), cudaMemcpyHostToDevice));
		HANDLE_ERROR(cudaMemcpy(m_dNbOfSamplesData, i_pNbOfSamplesData, nbOfPixels * sizeof(float), cudaMemcpyHostToDevice));

		CudaHistogramDistanceConstantParameters params;
		params.m_histogramImageColumnOffset = i_nbOfBins;
		params.m_histogramImageLineOffset = i_nbOfBins * i_width;
		params.m_nbOfSamplesImageColumnOffset = 1;
		params.m_nbOfSamplesImageLineOffset = i_width;
		params.m_patchSize = m_patchSize;
		params.m_searchWindowSize = m_searchWindowSize;
		params.m_nbOfBins = i_nbOfBins;
		params.m_nbOfFloatsInHistogramPatch = m_nbOfFloatsInHistogramPatch;
		params.m_powerOfTwoBeforeNbOfFloatsInHistogramPatch = previousPowerOfTwo(params.m_nbOfFloatsInHistogramPatch);
		HANDLE_ERROR(cudaMemcpyToSymbol(g_constantParameters, &params, sizeof(CudaHistogramDistanceConstantParameters)));
	}

	CudaHistogramDistance::~CudaHistogramDistance()
	{
		HANDLE_ERROR(cudaFree(m_dHistogramData));
		HANDLE_ERROR(cudaFree(m_dNbOfSamplesData));
		HANDLE_ERROR(cudaFree(m_dOutputDistances));
	}

	int CudaHistogramDistance::previousPowerOfTwo(int i_number)
	{
		int powerOfTwo = 1;
		while(powerOfTwo < i_number)
			powerOfTwo <<= 1;
		return powerOfTwo >> 1;
	}


	void CudaHistogramDistance::computeDistances(float* o_pDistances, int i_line, int i_column)
	{
		int mainPatchTopLeftCornerOffset = m_width * (i_line - m_patchRadius) + (i_column - m_patchRadius);

		// searchWindowExtended = searchWindow + border of thickness m_patchRadius
		int searchWindowExtendedTopBorderLine = max(0, i_line - m_searchWindowRadius - m_patchRadius);
		int searchWindowExtendedLeftBorderColumn = max(0, i_column - m_searchWindowRadius - m_patchRadius);
		int searchWindowExtendedBottomBorderLine = min(m_height - 1, i_line + m_searchWindowRadius + m_patchRadius);
		int searchWindowExtendedRightBorderColumn = min(m_width - 1, i_column + m_searchWindowRadius + m_patchRadius);
		int searchWindowNbOfLines = searchWindowExtendedBottomBorderLine - searchWindowExtendedTopBorderLine - 1;
		int searchWindowNbOfColumns = searchWindowExtendedRightBorderColumn - searchWindowExtendedLeftBorderColumn - 1;
		int searchWindowExtendedTopLeftCornerOffset = m_width * searchWindowExtendedTopBorderLine + searchWindowExtendedLeftBorderColumn;
		dim3 nbOfBlocksPerGrid(searchWindowNbOfColumns, searchWindowNbOfLines);
		dim3 nbOfThreadsPerBlock(m_nbOfBins, m_patchSize, m_patchSize);

		computeDistancesWithCuda<<< nbOfBlocksPerGrid, nbOfThreadsPerBlock,
									m_nbOfFloatsInHistogramPatch * (sizeof(float) + sizeof(float)) >>>(
				m_dOutputDistances,
				m_dHistogramData + mainPatchTopLeftCornerOffset * m_nbOfBins,
				m_dHistogramData + searchWindowExtendedTopLeftCornerOffset * m_nbOfBins,
				m_dNbOfSamplesData + mainPatchTopLeftCornerOffset,
				m_dNbOfSamplesData + searchWindowExtendedTopLeftCornerOffset);

		cudaDeviceSynchronize();
		HANDLE_ERROR(cudaMemcpy(o_pDistances, m_dOutputDistances, m_searchWindowSize * m_searchWindowSize * sizeof(float), cudaMemcpyDeviceToHost));
	}

} // namespace bcd
