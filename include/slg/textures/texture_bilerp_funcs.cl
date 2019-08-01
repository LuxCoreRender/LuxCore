#line 2 "texture_bilerp_funcs.cl"

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
// Bilerp texture
//------------------------------------------------------------------------------

#if defined(PARAM_ENABLE_TEX_BILERP)

OPENCL_FORCE_INLINE float BilerpTexture_ConstEvaluateFloat(__global const HitPoint *hitPoint,
		const float v00, const float v01, const float v10, const float v11) {
	float2 uv = VLOAD2F(&hitPoint->uv.u);
	uv.x -= Floor2Int(uv.x);
	uv.y -= Floor2Int(uv.y);
	return lerp(uv.x, lerp(uv.y, v00, v01), lerp(uv.y, v10, v11));
}

OPENCL_FORCE_INLINE float3 BilerpTexture_ConstEvaluateSpectrum(__global const HitPoint *hitPoint,
		const float3 v00, const float3 v01, const float3 v10, const float3 v11) {
	float2 uv = VLOAD2F(&hitPoint->uv.u);
	uv.x -= Floor2Int(uv.x);
	uv.y -= Floor2Int(uv.y);
	return lerp(uv.x, lerp(uv.y, v00, v01), lerp(uv.y, v10, v11));
}

#endif

