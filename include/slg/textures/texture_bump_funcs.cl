#line 2 "texture_bump_funcs.cl"

/***************************************************************************
 * Copyright 1998-2015 by authors (see AUTHORS.txt)                        *
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

//------------------------------------------------------------------------------
// Duv texture information
//------------------------------------------------------------------------------

#if defined(PARAM_HAS_BUMPMAPS)

float2 Texture_GetDuv(
        const uint texIndex,
        __global HitPoint *hitPoint,
        const float3 dpdu, const float3 dpdv,
        const float3 dndu, const float3 dndv,
        const float sampleDistance
        TEXTURES_PARAM_DECL) {
    // Calculate bump map value at intersection point
    const float base = Texture_GetFloatValue(texIndex, hitPoint
			TEXTURES_PARAM);

    // Compute offset positions and evaluate displacement texIndex
    const float3 origP = VLOAD3F(&hitPoint->p.x);
    const float3 origShadeN = VLOAD3F(&hitPoint->shadeN.x);
    const float2 origUV = VLOAD2F(&hitPoint->uv.u);

    float2 duv;

    // Shift hitPointTmp.du in the u direction and calculate value
    const float uu = sampleDistance / length(dpdu);
    VSTORE3F(origP + uu * dpdu, &hitPoint->p.x);
    hitPoint->uv.u += uu;
    VSTORE3F(normalize(origShadeN + uu * dndu), &hitPoint->shadeN.x);
    const float duValue = Texture_GetFloatValue(texIndex, hitPoint
			TEXTURES_PARAM);
    duv.s0 = (duValue - base) / uu;

    // Shift hitPointTmp.dv in the v direction and calculate value
    const float vv = sampleDistance / length(dpdv);
    VSTORE3F(origP + vv * dpdv, &hitPoint->p.x);
    hitPoint->uv.u = origUV.s0;
    hitPoint->uv.v += vv;
    VSTORE3F(normalize(origShadeN + vv * dndv), &hitPoint->shadeN.x);
    const float dvValue = Texture_GetFloatValue(texIndex, hitPoint
			TEXTURES_PARAM);
    duv.s1 = (dvValue - base) / vv;

    // Restore HitPoint
    VSTORE3F(origP, &hitPoint->p.x);
    VSTORE2F(origUV, &hitPoint->uv.u);
    VSTORE3F(origShadeN, &hitPoint->shadeN.x);

    return duv;
}

#if defined(PARAM_ENABLE_TEX_NORMALMAP)
float2 NormalMapTexture_GetDuv(
		const uint texIndex,
        __global HitPoint *hitPoint,
        const float3 dpdu, const float3 dpdv,
        const float3 dndu, const float3 dndv,
        const float sampleDistance
        TEXTURES_PARAM_DECL) {
	__global Texture *texture = &texs[texIndex];
    float3 rgb = Texture_GetSpectrumValue(texture->normalMap.texIndex, hitPoint
			TEXTURES_PARAM);
    rgb = clamp(rgb, -1.f, 1.f);

	// Normal from normal map
	float3 n = 2.f * rgb - (float3)(1.f, 1.f, 1.f);

	const float3 k = VLOAD3F(&hitPoint->shadeN.x);

	// Transform n from tangent to object space
    const float btsign = (dot(dpdv, k) > 0.f) ? 1.f : -1.f;

	// Magnitude of btsign is the magnitude of the interpolated normal
	const float3 kk = k * fabs(btsign);

	// tangent -> object
	n = normalize(n.x * dpdu + n.y * btsign * dpdv + n.z * kk);	

	// Since n is stored normalized in the normal map
	// we need to recover the original length (lambda).
	// We do this by solving 
	//   lambda*n = dp/du x dp/dv
	// where 
	//   p(u,v) = base(u,v) + h(u,v) * k
	// and
	//   k = dbase/du x dbase/dv
	//
	// We recover lambda by dotting the above with k
	//   k . lambda*n = k . (dp/du x dp/dv)
	//   lambda = (k . k) / (k . n)
	// 
	// We then recover dh/du by dotting the first eq by dp/du
	//   dp/du . lambda*n = dp/du . (dp/du x dp/dv)
	//   dp/du . lambda*n = dh/du * [dbase/du . (k x dbase/dv)]
	//
	// The term "dbase/du . (k x dbase/dv)" reduces to "-(k . k)", so we get
	//   dp/du . lambda*n = dh/du * -(k . k)
	//   dp/du . [(k . k) / (k . n)*n] = dh/du * -(k . k)
	//   dp/du . [-n / (k . n)] = dh/du
	// and similar for dh/dv
	// 
	// Since the recovered dh/du will be in units of ||k||, we must divide
	// by ||k|| to get normalized results. Using dg.nn as k in the last eq 
	// yields the same result.
	const float3 nn = (-1.f / dot(k, n)) * n;

	return (float2)(dot(dpdu, nn), dot(dpdv, nn));
}
#endif

#endif
