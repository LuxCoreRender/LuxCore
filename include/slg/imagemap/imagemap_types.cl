#line 2 "imagemap_types.cl"

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

typedef struct {
	ImageMapDescription desc;

	struct {
		unsigned int pageIndex;
		// The following field must be 64bit aligned (for OpenCL)
#if defined(SLG_OPENCL_KERNEL)
		// CUDA unsigned long has a 32bit size so we need different definitions
		// between CUDA and OpenCL
#if defined(LUXRAYS_CUDA_DEVICE)
		unsigned long long pixelsIndex;
#elif defined(LUXRAYS_OPENCL_DEVICE)
		unsigned long pixelsIndex;
#else
#error "Unsupported device in ImageMap struct"
#endif
#else
#if defined(LUXRAYS_ENABLE_OPENCL)
		cl_ulong pixelsIndex;
#else
		// In this, case cl_ulong is not defined. The type, in this case, doesn't
		// really matter because this structure is not used at all.
		unsigned long long pixelsIndex;
#endif
#endif
	} genericAddr;

	struct {
		// This is the same type of CUtexObject but so it can be compiled everywhere
		ImageMapObj imgMapObj;
	} imageMapAddr;
} ImageMap;

//------------------------------------------------------------------------------
// Some macro trick in order to have more readable code
//------------------------------------------------------------------------------

#if defined(SLG_OPENCL_KERNEL)

#define IMAGEMAPS_PARAM_DECL , __global const ImageMap* restrict imageMapDescs, __global const float* restrict* restrict imageMapBuff
#define IMAGEMAPS_PARAM , imageMapDescs, imageMapBuff

#endif
