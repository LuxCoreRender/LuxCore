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
	float weights[3] = {
		Sqr(Sqr(fabs(hitPoint->shadeN.x))),
		Sqr(Sqr(fabs(hitPoint->shadeN.y))),
		Sqr(Sqr(fabs(hitPoint->shadeN.z)))
	};

    const float sum = weights[0] + weights[1] + weights[2];
    weights[0] = weights[0] / sum;
    weights[1] = weights[1] / sum;
    weights[2] = weights[2] / sum;

	switch (texture->trilinearTex.mapping.type) {
		case GLOBALMAPPING3D:
		case LOCALMAPPING3D: {
			return <<CS_TEXTURE_X_INDEX>> * weights[0] +
					<<CS_TEXTURE_Y_INDEX>> * weights[1] +
					<<CS_TEXTURE_Z_INDEX>> * weights[2];
		}
		case UVMAPPING3D: {
			const float3 p = TextureMapping3D_Map(&texture->trilinearTex.mapping, hitPoint);
			const uint dataIndex = texture->trilinearTex.mapping.uvMapping3D.dataIndex;

			// To workaround the "const" part
			__global HitPoint *hitPointTmp = (__global HitPoint *)hitPoint;

			// Save original UV values
			const float origU = hitPointTmp->uv[dataIndex].u;
			const float origV = hitPointTmp->uv[dataIndex].v;

			hitPointTmp->uv[dataIndex].u = p.y;
			hitPointTmp->uv[dataIndex].v = p.z;
			float3 result = <<CS_TEXTURE_X_INDEX>> * weights[0];

			hitPointTmp->uv[dataIndex].u = p.x;
			hitPointTmp->uv[dataIndex].v = p.z;
			result += <<CS_TEXTURE_Y_INDEX>> * weights[1];

			hitPointTmp->uv[dataIndex].u = p.x;
			hitPointTmp->uv[dataIndex].v = p.y;
			result += <<CS_TEXTURE_Z_INDEX>> * weights[2];

			// Restore original UV values
			hitPointTmp->uv[dataIndex].u = origU;
			hitPointTmp->uv[dataIndex].v = origV;

			return result;
		}
		default:
			return BLACK;
	}
}

OPENCL_FORCE_INLINE float Texture_Index<<CS_TRIPLANAR_TEX_INDEX>>_EvaluateFloat(__global const Texture *texture,
		__global const HitPoint *hitPoint
		TEXTURES_PARAM_DECL) {
	const float3 result = Texture_Index<<CS_TRIPLANAR_TEX_INDEX>>_EvaluateSpectrum(texture, hitPoint
			TEXTURES_PARAM);

	return Spectrum_Y(result);
}

#endif
