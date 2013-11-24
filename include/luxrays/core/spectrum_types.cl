#line 2 "spectrum_types.cl"

/***************************************************************************
 * Copyright 1998-2013 by authors (see AUTHORS.txt)                        *
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

#if defined(SLG_OPENCL_KERNEL)
#define BLACK ((float3)(0.f, 0.f, 0.f))
#define WHITE ((float3)(1.f, 1.f, 1.f))
#endif

#define ASSIGN_SPECTRUM(c0, c1) { (c0).r = (c1).r; (c0).g = (c1).g; (c0).b = (c1).b; }

typedef struct {
	float r, g, b;
} Spectrum;
