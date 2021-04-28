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

#ifndef _LUXRAYS_CUDACPP_H
#define	_LUXRAYS_CUDACPP_H

#if !defined(LUXCORE_DISABLE_CUDA_CPLUSPLUS)

#include <stdexcept>

#include "luxrays/utils/strutils.h"

namespace luxrays {

#define CUDACPP_CHECKERROR() luxrays::CudaCPPCheckError(cudaGetLastError(), __FILE__, __LINE__)

inline void CudaCPPCheckError(const cudaError_t error, const char *file, const int line) {
	if (error != cudaSuccess)
		throw std::runtime_error("CUDA C++ error: " + std::string(cudaGetErrorString(error)) + " "
				"(code: " + std::string(cudaGetErrorName(error)) + ", file: " + std::string(file) +
				", line: " + ToString(line) + ")\n");
}

//------------------------------------------------------------------------------
// CudaCPPHostNew()/CudaCPPHostDelete()
//------------------------------------------------------------------------------

template<class T>
inline T *CudaCPPHostNewArray(const size_t n) {
	T *p;
	cudaMallocManaged(&p, n * sizeof(T));
	CUDACPP_CHECKERROR();

	return new (p) T[n];
}

template<class T>
inline void CudaCPPHostDeleteArray(T *p, const size_t n) {
	if (p) {
		for (u_int i = 0; i < n; ++i)
			p[i].~T();

		cudaFree(p);
		CUDACPP_CHECKERROR();
	}
}

//------------------------------------------------------------------------------
// CudaCPPHostAlloc()/CudaCPPHostFree()
//------------------------------------------------------------------------------

template<class T, class... Args>
inline T *CudaCPPHostNew(Args... args) {
	T *p;
	cudaMallocManaged(&p, sizeof(T));
	CUDACPP_CHECKERROR();

	return new (p) T(args...);
}

template<class T>
inline void CudaCPPHostDelete(void *ptr) {
	if (ptr) {
		T *p = (T *)ptr;
		p->~T();

		cudaFree(p);
		CUDACPP_CHECKERROR();
	}
}

//------------------------------------------------------------------------------
// CudaCPPHostAlloc()/CudaCPPHostFree()
//------------------------------------------------------------------------------

//class CudaCPPManaged {
//public:
//  void *operator new(size_t len) {
//    void *ptr;
//
//    cudaMallocManaged(&ptr, len);
//	CUDACPP_CHECKERROR();
//
//    return ptr;
//  }
//
//  void operator delete(void *ptr) {
//    cudaFree(ptr);
//	CUDACPP_CHECKERROR();
//  }
//};

}

#endif

#endif	/* _LUXRAYS_CUDACPP_H */
