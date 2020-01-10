#line 2 "texture_triplanar_funcs.cl"

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
// Triplanar mapping texture
//------------------------------------------------------------------------------

#if defined(PARAM_ENABLE_TEX_TRIPLANAR)

OPENCL_FORCE_INLINE float3 Texture_Index<<CS_TRIPLANAR_TEX_INDEX>>_EvaluateSpectrum(__global const Texture *texture,
		__global const HitPoint *hitPoint
		TEXTURES_PARAM_DECL) {
	float3 localShadeN;
	const float3 localPoint = TextureMapping3D_Map(&texture->triplanarTex.mapping, hitPoint, &localShadeN);

	float weights[3] = {
		Sqr(Sqr(fabs(localShadeN.x))),
		Sqr(Sqr(fabs(localShadeN.y))),
		Sqr(Sqr(fabs(localShadeN.z)))
	};

    const float sum = weights[0] + weights[1] + weights[2];
    weights[0] = weights[0] / sum;
    weights[1] = weights[1] / sum;
    weights[2] = weights[2] / sum;

	const uint uvIndex = texture->triplanarTex.uvIndex;

	// To workaround the "const" part
	__global HitPoint *hitPointTmp = (__global HitPoint *)hitPoint;

	// Save original UV values
	const float origU = hitPointTmp->uv[uvIndex].u;
	const float origV = hitPointTmp->uv[uvIndex].v;

	hitPointTmp->uv[uvIndex].u = localPoint.y;
	hitPointTmp->uv[uvIndex].v = localPoint.z;
	float3 result = <<CS_TEXTURE_X_INDEX>> * weights[0];

	hitPointTmp->uv[uvIndex].u = localPoint.x;
	hitPointTmp->uv[uvIndex].v = localPoint.z;
	result += <<CS_TEXTURE_Y_INDEX>> * weights[1];

	hitPointTmp->uv[uvIndex].u = localPoint.x;
	hitPointTmp->uv[uvIndex].v = localPoint.y;
	result += <<CS_TEXTURE_Z_INDEX>> * weights[2];

	// Restore original UV values
	hitPointTmp->uv[uvIndex].u = origU;
	hitPointTmp->uv[uvIndex].v = origV;

	return result;
}

OPENCL_FORCE_INLINE float Texture_Index<<CS_TRIPLANAR_TEX_INDEX>>_EvaluateFloat(__global const Texture *texture,
		__global const HitPoint *hitPoint
		TEXTURES_PARAM_DECL) {
	const float3 result = Texture_Index<<CS_TRIPLANAR_TEX_INDEX>>_EvaluateSpectrum(texture, hitPoint
			TEXTURES_PARAM);

	return Spectrum_Y(result);
}

// Note: TriplanarTexture_Bump() is defined in texture_bump_funcs.cl

#endif
