// This file is part of the reference implementation for the paper 
//   Bayesian Collaborative Denoising for Monte-Carlo Rendering
//   Malik Boughida and Tamy Boubekeur.
//   Computer Graphics Forum (Proc. EGSR 2017), vol. 36, no. 4, p. 137-153, 2017
//
// All rights reserved. Use of this source code is governed by a
// BSD-style license that can be found in the LICENSE.txt file.

#include "Denoiser.h"

#include "DenoisingUnit.h"

#ifdef FOUND_CUDA
#include "cuda_runtime.h"
#endif

#include <iostream>
#include <algorithm>
#include <random>

#include <chrono>

#include <cassert>

#define USE_ATOMIC
#ifndef USE_ATOMIC
#define USE_CRITICAL
#endif


using namespace std;

namespace bcd
{

	void Denoiser::ompTest()
	{
		m_width = m_inputs.m_pColors->getWidth();
		m_height = m_inputs.m_pColors->getHeight();
		m_nbOfPixels = m_width * m_height;
		vector<PixelPosition> pixelSet(m_nbOfPixels);
		{
			int k = 0;
			for (int line = 0; line < m_height; line++)
				for (int col = 0; col < m_width; col++)
					pixelSet[k++] = PixelPosition(line, col);
		}
		reorderPixelSet(pixelSet);

		m_outputs.m_pDenoisedColors->fill(0.f);
		int radius = 2;

#pragma omp parallel for ordered schedule(dynamic)
		for (int pixelIndex = 0; pixelIndex < m_nbOfPixels; pixelIndex++)
		{
			int l0, c0, li, ci, lf, cf, l, c;
			pixelSet[pixelIndex].get(l0, c0);
	//		cout << "(" << l0 << "," << c0 << ")" << endl;
			li = max(0, l0 - radius);
			ci = max(0, c0 - radius);
			lf = min(m_width - 1, l0 + radius);
			cf = min(m_height - 1, c0 + radius);
#ifdef USE_CRITICAL
#pragma omp critical(accumulateOutput)
#endif
			{
				for (l = li; l <= lf; l++)
					for (c = ci; c <= cf; c++)
					{
#ifdef USE_ATOMIC
						float& rValue = m_outputs.m_pDenoisedColors->get(l, c, 0);
#pragma omp atomic
						rValue += 0.04f;
#endif
#ifdef USE_CRITICAL
						m_outputs.m_pDenoisedColors->get(l, c, 0) += 0.04f;
#endif
					}
			}
		}

	}

	bool Denoiser::denoise()
	{
		if(!inputsOutputsAreOk())
			return false;

		m_width = m_inputs.m_pColors->getWidth();
		m_height = m_inputs.m_pColors->getHeight();
		m_nbOfPixels = m_width * m_height;
		int widthWithoutBorder = m_width - 2 * m_parameters.m_patchRadius;
		int heightWithoutBorder = m_height - 2 * m_parameters.m_patchRadius;
		int nbOfPixelsWithoutBorder = widthWithoutBorder * heightWithoutBorder;

		computeNbOfSamplesSqrt();
		computePixelCovFromSampleCov();

#ifdef FOUND_CUDA

		if(m_parameters.m_useCuda)
			cout << "Parallelizing computations using Cuda" << endl;
		else
			cout << "Cuda parallelization has been disabled" << endl;
#else
		if(m_parameters.m_useCuda)
			cout << "WARNING: Cuda parallelization has been disabled because the program has not been built with Cuda" << endl;
		else
			cout << "The program has not been built with Cuda" << endl;
#endif

		if(m_parameters.m_nbOfCores > 0)
			omp_set_num_threads(m_parameters.m_nbOfCores);

#pragma omp parallel
#pragma omp master
		{
			m_parameters.m_nbOfCores = omp_get_num_threads();
			// now m_parameters.m_nbOfCores is set to the actual number of threads
			// even if it was set to the default value 0
			if(m_parameters.m_nbOfCores > 1)
				cout << "Parallelizing computations with " << m_parameters.m_nbOfCores << " threads using OpenMP" << endl;
			else
				cout << "No parallelization using OpenMP" << endl;
		}

		vector<PixelPosition> pixelSet(nbOfPixelsWithoutBorder);
		{
			int k = 0;
			int lMin = m_parameters.m_patchRadius;
			int lMax = m_height - m_parameters.m_patchRadius - 1;
			int cMin = m_parameters.m_patchRadius;
			int cMax = m_width - m_parameters.m_patchRadius - 1;
			for (int l = lMin; l <= lMax; l++)
				for (int c = cMin; c <= cMax; c++)
					pixelSet[k++] = PixelPosition(l, c);
		}
		reorderPixelSet(pixelSet);

		m_outputSummedColorImages.resize(m_parameters.m_nbOfCores);
		m_outputSummedColorImages[0].resize(m_width, m_height, m_inputs.m_pColors->getDepth());
		m_outputSummedColorImages[0].fill(0.f);
		for(int i = 1; i < m_parameters.m_nbOfCores; ++i)
			m_outputSummedColorImages[i] = m_outputSummedColorImages[0];

		m_estimatesCountImages.resize(m_parameters.m_nbOfCores);
		m_estimatesCountImages[0].resize(m_width, m_height, 1);
		m_estimatesCountImages[0].fill(0);
		for(int i = 1; i < m_parameters.m_nbOfCores; ++i)
			m_estimatesCountImages[i] = m_estimatesCountImages[0];

		m_isCenterOfAlreadyDenoisedPatchImage.resize(m_width, m_height, 1);
		m_isCenterOfAlreadyDenoisedPatchImage.fill(false);

		int chunkSize; // nb of pixels a thread has to treat before dynamically asking for more work
		chunkSize = widthWithoutBorder * (2 * m_parameters.m_searchWindowRadius);

		int nbOfPixelsComputed = 0;
		int currentPercentage = 0, newPercentage = 0;
#pragma omp parallel
		{
			DenoisingUnit denoisingUnit(*this);
#pragma omp for ordered schedule(dynamic, chunkSize)
			for (int pixelIndex = 0; pixelIndex < nbOfPixelsWithoutBorder; pixelIndex++)
			{
	//			if(pixelIndex != 2222)
	//				continue;
				PixelPosition mainPatchCenter = pixelSet[pixelIndex];
				denoisingUnit.denoisePatchAndSimilarPatches(mainPatchCenter);
#pragma omp atomic
				++nbOfPixelsComputed;
				if(omp_get_thread_num() == 0)
				{
					newPercentage = (nbOfPixelsComputed * 100) / nbOfPixelsWithoutBorder;
					if(newPercentage != currentPercentage)
					{
						currentPercentage = newPercentage;
						cout << "\r" << currentPercentage << " %" << flush;
						m_progressCallback(float(currentPercentage) * 0.01f);
					}
				}

			}
#pragma omp master
			cout << endl << endl;
#pragma omp barrier
#ifdef COMPUTE_DENOISING_STATS
#pragma omp critical
			{
				denoisingUnit.m_uStats->storeElapsedTimes();
				denoisingUnit.m_uStats->print();
			}
#endif
		}

		m_outputs.m_pDenoisedColors->resize(m_width, m_height, 3);
		m_outputs.m_pDenoisedColors->fill(0.f);
		finalAggregation();

		return true;
	}

	void printCudaDevicesProperties()
	{
#ifdef FOUND_CUDA
		int nbOfDevices;
		cudaError_t error = cudaGetDeviceCount(&nbOfDevices);
		if(error != cudaSuccess)
		{
			cout << cudaGetErrorString(error) << endl;
			return;
		}
		for(int deviceIndex = 0; deviceIndex < nbOfDevices; deviceIndex++)
		{
			cudaDeviceProp properties;
			cudaGetDeviceProperties(&properties, deviceIndex);
			cout << "Device Number: " << deviceIndex << endl;
			cout << "  Device name: " << properties.name << endl;
			cout << "  Memory Clock Rate (KHz): " << properties.memoryClockRate << endl;
			cout << "  Memory Bus Width (bits): " << properties.memoryBusWidth << endl;
			cout << "  Peak Memory Bandwidth (GB/s): "
					<< 2.0*properties.memoryClockRate*(properties.memoryBusWidth/8)/1.0e6 << endl << endl;
		}
#endif
	}

	bool Denoiser::inputsOutputsAreOk()
	{
//		printCudaDevicesProperties();
		if(m_parameters.m_useCuda)
		{
			if(m_parameters.m_patchRadius != 1)
			{
				m_parameters.m_useCuda = false;
				cout << "Warning: disabling Cuda, that cannot be used for patch radius " << m_parameters.m_patchRadius << " > 1" << endl;
			}
#ifdef FOUND_CUDA
			{
				int nbOfDevices;
				cudaError_t error = cudaGetDeviceCount(&nbOfDevices);
				if(error != cudaSuccess)
				{
					m_parameters.m_useCuda = false;
					cout << "Warning: disabling Cuda, because the following error was encountered: ";
					cout << cudaGetErrorString(error) << endl;
				}
				else if(nbOfDevices == 0)
				{
					m_parameters.m_useCuda = false;
					cout << "Warning: disabling Cuda, because no CUDA-compatible GPU was found" << endl;
				}
			}
#endif
		}
		{

		}
		{
			bool imageNullptr = false;
			if(!m_inputs.m_pColors)
			{
				imageNullptr = true;
				cerr << "Aborting denoising: nullptr for input color image" << endl;
			}
			if(!m_inputs.m_pNbOfSamples)
			{
				imageNullptr = true;
				cerr << "Aborting denoising: nullptr for input number of samples image" << endl;
			}
			if(!m_inputs.m_pHistograms)
			{
				imageNullptr = true;
				cerr << "Aborting denoising: nullptr for input histogram image" << endl;
			}
			if(!m_inputs.m_pSampleCovariances)
			{
				imageNullptr = true;
				cerr << "Aborting denoising: nullptr for input covariance image" << endl;
			}
			if(imageNullptr)
				return false;
		}
		{
			bool emptyInput = false;
			if(m_inputs.m_pColors->isEmpty())
			{
				emptyInput = true;
				cerr << "Aborting denoising: input color image is empty" << endl;
			}
			if(m_inputs.m_pNbOfSamples->isEmpty())
			{
				emptyInput = true;
				cerr << "Aborting denoising: input number of samples image is empty" << endl;
			}
			if(m_inputs.m_pHistograms->isEmpty())
			{
				emptyInput = true;
				cerr << "Aborting denoising: input histogram image is empty" << endl;
			}
			if(m_inputs.m_pSampleCovariances->isEmpty())
			{
				emptyInput = true;
				cerr << "Aborting denoising: input covariance image is empty" << endl;
			}
			if(emptyInput)
				return false;
		}
		{
			int w = m_inputs.m_pColors->getWidth();
			int h = m_inputs.m_pColors->getHeight();
			bool badImageSize = false;
			if(m_inputs.m_pNbOfSamples->getWidth() != w || m_inputs.m_pNbOfSamples->getHeight() != h)
			{
				badImageSize = true;
				cerr << "Aborting denoising: input number of samples image is "
						<< m_inputs.m_pNbOfSamples->getWidth() << "x" << m_inputs.m_pNbOfSamples->getHeight()
						<< "but input color image is " << w << "x" << h << endl;
			}
			if(m_inputs.m_pHistograms->getWidth() != w || m_inputs.m_pHistograms->getHeight() != h)
			{
				badImageSize = true;
				cerr << "Aborting denoising: input histogram image is "
						<< m_inputs.m_pHistograms->getWidth() << "x" << m_inputs.m_pHistograms->getHeight()
						<< "but input color image is " << w << "x" << h << endl;
			}
			if(m_inputs.m_pSampleCovariances->getWidth() != w || m_inputs.m_pSampleCovariances->getHeight() != h)
			{
				badImageSize = true;
				cerr << "Aborting denoising: input covariance image is "
						<< m_inputs.m_pSampleCovariances->getWidth() << "x" << m_inputs.m_pSampleCovariances->getHeight()
						<< "but input color image is " << w << "x" << h << endl;
			}
			if(badImageSize)
				return false;
		}
		return true;
	}

	void Denoiser::computeNbOfSamplesSqrt()
	{
		m_nbOfSamplesSqrtImage = *(m_inputs.m_pNbOfSamples);
		for(float* pPixelValues : m_nbOfSamplesSqrtImage)
			pPixelValues[0] = sqrt(pPixelValues[0]);
	}

	void Denoiser::computePixelCovFromSampleCov()
	{
		m_pixelCovarianceImage = *(m_inputs.m_pSampleCovariances);
		ImfIt covIt = m_pixelCovarianceImage.begin();
		float nbOfSamplesInv;
		for(const float* pPixelNbOfSamples : *(m_inputs.m_pNbOfSamples))
		{
			nbOfSamplesInv = 1.f / *pPixelNbOfSamples;
			covIt[0] *= nbOfSamplesInv;
			covIt[1] *= nbOfSamplesInv;
			covIt[2] *= nbOfSamplesInv;
			covIt[3] *= nbOfSamplesInv;
			covIt[4] *= nbOfSamplesInv;
			covIt[5] *= nbOfSamplesInv;
			++covIt;
		}
	}

	void Denoiser::reorderPixelSet(vector<PixelPosition>& io_rPixelSet) const
	{
		if(m_parameters.m_useRandomPixelOrder)
			reorderPixelSetShuffle(io_rPixelSet);
		else if(m_parameters.m_nbOfCores > 1)
			reorderPixelSetJumpNextStrip(io_rPixelSet);
	}
	void Denoiser::reorderPixelSetJumpNextStrip(vector<PixelPosition>& io_rPixelSet) const
	{
		int widthWithoutBorder = m_width - 2 * m_parameters.m_patchRadius;
		int heightWithoutBorder = m_height - 2 * m_parameters.m_patchRadius;
		int nbOfPixelsWithoutBorder = widthWithoutBorder * heightWithoutBorder;
		assert(nbOfPixelsWithoutBorder == io_rPixelSet.size());
		int chunkSize = widthWithoutBorder * (2 * m_parameters.m_searchWindowRadius);
		// chunkSize is the number of pixels of a strip of 2*searchWindowRadius lines
		reorderPixelSetJumpNextChunk(io_rPixelSet, chunkSize);
	}

	void Denoiser::reorderPixelSetJumpNextChunk(vector<PixelPosition>& io_rPixelSet, int i_chunkSize)
	{
		int doubleChunkSize = 2 * i_chunkSize;
		int nbOfFullChunks = io_rPixelSet.size() / i_chunkSize;

		vector<PixelPosition> pixelSetCopy(io_rPixelSet);
		vector<PixelPosition>::iterator inputIt = pixelSetCopy.begin();
		vector<PixelPosition>::iterator outputIt = io_rPixelSet.begin();

		for(int chunkIndexStart = 0; chunkIndexStart < 2; ++chunkIndexStart)
		{
			inputIt = pixelSetCopy.begin() + chunkIndexStart * i_chunkSize;
			for(int chunkIndex = chunkIndexStart; chunkIndex < nbOfFullChunks; )
			{
				copy(inputIt, inputIt + i_chunkSize, outputIt);
				outputIt += i_chunkSize;
				chunkIndex += 2;
				if(chunkIndex < nbOfFullChunks)
					inputIt += doubleChunkSize;
			}
		}
	}

	void reorderPixelSetShuffleCPP11(vector<PixelPosition>& io_rPixelSet)
	{
		unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();
		shuffle (io_rPixelSet.begin(), io_rPixelSet.end(), std::default_random_engine(seed));
	}

	void Denoiser::reorderPixelSetShuffle(vector<PixelPosition>& io_rPixelSet)
	{
		reorderPixelSetShuffleCPP11(io_rPixelSet);
		/*
		PixelPosition* pPixelSetStart = io_rPixelSet.data();
		for(int i = io_rPixelSet.size(); i > 0; --i)
			swap(pPixelSetStart[rand() % i], pPixelSetStart[i-1]);
		*/
	}



	void Denoiser::finalAggregation()
	{
		int nbOfImages = static_cast<int>(m_outputSummedColorImages.size());

		for(int bufferIndex = 1; bufferIndex < nbOfImages; ++bufferIndex)
		{
			ImfIt it = m_outputSummedColorImages[bufferIndex].begin();
			for(float* pSum : m_outputSummedColorImages[0])
			{
				pSum[0] += it[0];
				pSum[1] += it[1];
				pSum[2] += it[2];
				++it;
			}
		}
		for(int bufferIndex = 1; bufferIndex < nbOfImages; ++bufferIndex)
		{
			DeepImage<int>::iterator it = m_estimatesCountImages[bufferIndex].begin();
			for(int* pSum : m_estimatesCountImages[0])
			{
				pSum[0] += it[0];
				++it;
			}
		}
		ImfIt sumIt = m_outputSummedColorImages[0].begin();
		DeepImage<int>::iterator countIt = m_estimatesCountImages[0].begin();
		float countInv;
		for(float* pFinalColor : *(m_outputs.m_pDenoisedColors))
		{
			countInv = 1.f / countIt[0];
			pFinalColor[0] = countInv * sumIt[0];
			pFinalColor[1] = countInv * sumIt[1];
			pFinalColor[2] = countInv * sumIt[2];
			++sumIt;
			++countIt;
		}
	}

} // namespace bcd
