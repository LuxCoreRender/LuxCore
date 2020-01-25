#line 2 "texture_eval_funcs.cl"

/***************************************************************************
 * Copyright 1998-2018 by authors (see AUTHORS.txt)                        *
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

//------------------------------------------------------------------------------
// Texture evaluation functions
//------------------------------------------------------------------------------

OPENCL_FORCE_NOT_INLINE float Texture_GetFloatValue(const uint texIndex,
		__global const HitPoint *hitPoint
		TEXTURES_PARAM_DECL) {
	return 0.f;
}

OPENCL_FORCE_NOT_INLINE float3 Texture_GetSpectrumValue(const uint texIndex,
		__global const HitPoint *hitPoint
		TEXTURES_PARAM_DECL) {
	return 0.f;
}

OPENCL_FORCE_NOT_INLINE float3 Texture_Bump(const uint texIndex,
		__global HitPoint *hitPoint,
		const float sampleDistance
		TEXTURES_PARAM_DECL) {
	return VLOAD3F(&hitPoint->shadeN.x);
}
