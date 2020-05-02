#line 2 "ocldevice_funcs.cl"

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

// This is a workaround to long compilation time
#define OPENCL_FORCE_NOT_INLINE __attribute__((noinline))
#define OPENCL_FORCE_INLINE __attribute__((always_inline))
// For fast compilation with Intel OpenCL CPU device (i.e. development, debugging, etc.)
//#define OPENCL_FORCE_NOT_INLINE __attribute__((noinline))
//#define OPENCL_FORCE_INLINE __attribute__((noinline))

//------------------------------------------------------------------------------
// MAKE_FLOATn()
//------------------------------------------------------------------------------

#define MAKE_FLOAT2(x, y) ((float2)(x, y))
#define MAKE_FLOAT3(x, y, z) ((float3)(x, y, z))
#define MAKE_FLOAT4(x, y, z, w) ((float4)(x, y, z, w))

#define TO_FLOAT2(x) ((float2)x)
#define TO_FLOAT3(x) ((float3)x)
#define TO_FLOAT4(x) ((float4)x)
