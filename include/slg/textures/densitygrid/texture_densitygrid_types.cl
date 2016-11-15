#line 2 "densitygrid_types.cl"

/***************************************************************************
 * Copyright 1998-2016 by authors (see AUTHORS.txt)                        *
 *                                                                         *
 *   This file is part of LuxRender.                                       *
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

typedef enum {
	WRAP_REPEAT, WRAP_BLACK, WRAP_WHITE, WRAP_CLAMP
} WrapModeType;

typedef struct {
	unsigned int nx, ny, nz;
	WrapModeType wrapMode;
	unsigned int pageIndex, voxelsIndex;
} DensityGrid;

//------------------------------------------------------------------------------
// Some macro trick in order to have more readable code
//------------------------------------------------------------------------------

#if defined(SLG_OPENCL_KERNEL)

#if defined(PARAM_HAS_DENSITYGRIDS)

#define DENSITYGRIDS_PARAM_DECL , __global const DensityGrid* restrict densityGridDescs, __global const float* restrict* restrict densityGridBuff
#define DENSITYGRIDS_PARAM , densityGridDescs, densityGridBuff

#else

#define DENSITYGRIDS_PARAM_DECL
#define DENSITYGRIDS_PARAM

#endif

#endif
