// This file is part of the reference implementation for the paper 
//   Bayesian Collaborative Denoising for Monte-Carlo Rendering
//   Malik Boughida and Tamy Boubekeur.
//   Computer Graphics Forum (Proc. EGSR 2017), vol. 36, no. 4, p. 137-153, 2017
//
// All rights reserved. Use of this source code is governed by a
// BSD-style license that can be found in the LICENSE.txt file.

#ifndef CUDA_UTILS_H
#define CUDA_UTILS_H

#include <iostream>
#include <cstdio>

namespace bcd
{

	static void HandleError(
			cudaError_t i_error,
			const char* i_pFileName,
			int i_lineInFile)
	{
		if (i_error != cudaSuccess)
		{
			std::cout << cudaGetErrorString(i_error) << " in file '" << i_pFileName
				 << "' at line " << i_lineInFile << std::endl;
			exit( EXIT_FAILURE );
		}
	}
#define HANDLE_ERROR( err ) (HandleError( err, __FILE__, __LINE__ ))

	void printCudaDeviceProperties()
	{
		cudaDeviceProp prop;
		int count;
		HANDLE_ERROR( cudaGetDeviceCount( &count ) );
		for (int i=0; i< count; i++) {
			HANDLE_ERROR( cudaGetDeviceProperties( &prop, i ) );
			printf( " --- General Information for device %d ---\n", i );
			printf( "Name: %s\n", prop.name );
			printf( "Compute capability: %d.%d\n", prop.major, prop.minor );
			printf( "Clock rate: %d\n", prop.clockRate );
			printf( "Device copy overlap: " );
			if (prop.deviceOverlap)
				printf( "Enabled\n" );
			else
				printf( "Disabled\n" );
			printf( "Kernel execition timeout : " );
			if (prop.kernelExecTimeoutEnabled)
				printf( "Enabled\n" );
			else
				printf( "Disabled\n" );
			printf( " --- Memory Information for device %d ---\n", i );
			printf( "Total global mem: %ld\n", prop.totalGlobalMem );
			printf( "Total constant Mem: %ld\n", prop.totalConstMem );
			printf( "Max mem pitch: %ld\n", prop.memPitch );
			printf( "Texture Alignment: %ld\n", prop.textureAlignment );
			printf( " --- MP Information for device %d ---\n", i );
			printf( "Multiprocessor count: %d\n", prop.multiProcessorCount );
			printf( "Shared mem per mp: %ld\n", prop.sharedMemPerBlock );
			printf( "Registers per mp: %d\n", prop.regsPerBlock );
			printf( "Threads in warp: %d\n", prop.warpSize );
			printf( "Max threads per block: %d\n", prop.maxThreadsPerBlock );
			printf( "Max thread dimensions: (%d, %d, %d)\n", prop.maxThreadsDim[0], prop.maxThreadsDim[1], prop.maxThreadsDim[2] );
			printf( "Max grid dimensions: (%d, %d, %d)\n", prop.maxGridSize[0], prop.maxGridSize[1], prop.maxGridSize[2] );
			printf( "\n" );
		}
	}

} // namespace bcd

#endif // CUDA_UTILS_H
