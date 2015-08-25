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

void Texture_Bump(
		const uint texIndex,
		__global HitPoint *hitPoint,
		const float sampleDistance
		TEXTURES_PARAM_DECL) {
	const float3 dpdu = VLOAD3F(&hitPoint->dpdu.x);
	const float3 dpdv = VLOAD3F(&hitPoint->dpdv.x);
	const float3 dndu = VLOAD3F(&hitPoint->dndu.x);
	const float3 dndv = VLOAD3F(&hitPoint->dndv.x);

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

	// Compute the new dpdu and dpdv
	const float3 bumpDpdu = dpdu + duv.s0 * origShadeN;
	const float3 bumpDpdv = dpdv + duv.s1 * origShadeN;
	float3 newShadeN = normalize(cross(bumpDpdu, bumpDpdv));

	// The above transform keeps the normal in the original normal
	// hemisphere. If they are opposed, it means UVN was indirect and
	// the normal needs to be reversed
	newShadeN *= (dot(origShadeN, newShadeN) < 0.f) ? -1.f : 1.f;

	// Update HitPoint structure
	VSTORE3F(newShadeN, &hitPoint->shadeN.x);
	VSTORE3F(bumpDpdu, &hitPoint->dpdu.x);
	VSTORE3F(bumpDpdu, &hitPoint->dpdv.x);
}

#if defined(PARAM_ENABLE_TEX_NORMALMAP)
void NormalMapTexture_Bump(
		const uint texIndex,
		__global HitPoint *hitPoint,
		const float sampleDistance
		TEXTURES_PARAM_DECL) {
	// Normal from normal map
	const __global Texture *texture = &texs[texIndex];
	float3 rgb = Texture_GetSpectrumValue(texture->normalMap.texIndex, hitPoint
			TEXTURES_PARAM);
	rgb = clamp(rgb, -1.f, 1.f);

	// Normal from normal map
	float3 n = 2.f * rgb - (float3)(1.f, 1.f, 1.f);

	const float3 oldShadeN = VLOAD3F(&hitPoint->shadeN.x);
	float3 dpdu = VLOAD3F(&hitPoint->dpdu.x);
	float3 dpdv = VLOAD3F(&hitPoint->dpdv.x);
	
	Frame frame;
	Frame_Set_Private(&frame, dpdu, dpdv, oldShadeN);

	// Transform n from tangent to object space
	float3 shadeN = normalize(Frame_ToWorld_Private(&frame, n));
	shadeN *= (dot(oldShadeN, shadeN) < 0.f) ? -1.f : 1.f;

	// Update dpdu and dpdv so they are still orthogonal to shadeN 
	dpdu = cross(shadeN, cross(dpdu, shadeN));
	dpdv = cross(shadeN, cross(dpdv, shadeN));

	// Update HitPoint structure
	VSTORE3F(shadeN, &hitPoint->shadeN.x);
	VSTORE3F(dpdu, &hitPoint->dpdu.x);
	VSTORE3F(dpdv, &hitPoint->dpdv.x);
}
#endif

#endif
