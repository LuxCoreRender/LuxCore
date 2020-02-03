#line 2 "texture_bump_funcs.cl"

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

// Texture bump/normal mapping

//------------------------------------------------------------------------------
// Generic texture bump mapping
//------------------------------------------------------------------------------

OPENCL_FORCE_INLINE float3 GenericTexture_Bump(
		__global const HitPoint *hitPoint,
		const float sampleDistance,
		const float evalFloatTexBase,
		const float evalFloatTexOffsetU,
		const float evalFloatTexOffsetV) {
	const float3 dpdu = VLOAD3F(&hitPoint->dpdu.x);
	const float3 dpdv = VLOAD3F(&hitPoint->dpdv.x);

	float2 duv;

	const float base = evalFloatTexBase;

	const float uu = sampleDistance / length(dpdu);
	const float duValue = evalFloatTexOffsetU;
	duv.s0 = (duValue - base) / uu;

	const float vv = sampleDistance / length(dpdv);
	const float dvValue = evalFloatTexOffsetV;
	duv.s1 = (dvValue - base) / vv;

	// Compute the new dpdu and dpdv
	const float3 shadeN = VLOAD3F(&hitPoint->shadeN.x);
	const float3 bumpDpdu = dpdu + duv.s0 * shadeN;
	const float3 bumpDpdv = dpdv + duv.s1 * shadeN;
	float3 newShadeN = normalize(cross(bumpDpdu, bumpDpdv));

	// The above transform keeps the normal in the original normal
	// hemisphere. If they are opposed, it means UVN was indirect and
	// the normal needs to be reversed
	newShadeN *= (dot(shadeN, newShadeN) < 0.f) ? -1.f : 1.f;

	return newShadeN;
}

//------------------------------------------------------------------------------
// ConstTexture_Bump() for constant textures
//------------------------------------------------------------------------------

OPENCL_FORCE_INLINE float3 ConstTexture_Bump(__global const HitPoint *hitPoint) {
	return VLOAD3F(&hitPoint->shadeN.x);
}

//------------------------------------------------------------------------------
// ImageMapTexture
//------------------------------------------------------------------------------

OPENCL_FORCE_NOT_INLINE float3 ImageMapTexture_Bump(__global const Texture* restrict tex,
		__global const HitPoint *hitPoint
		IMAGEMAPS_PARAM_DECL) {
	float2 du, dv;
	const float2 uv = TextureMapping2D_MapDuv(&tex->imageMapTex.mapping, hitPoint, &du, &dv);
	__global const ImageMap *imageMap = &imageMapDescs[tex->imageMapTex.imageMapIndex];
	const float2 dst = ImageMap_GetDuv(imageMap, uv.x, uv.y IMAGEMAPS_PARAM);

	const float2 duv = tex->imageMapTex.gain * (float2)(dot(dst, du), dot(dst, dv));

	const float3 shadeN = VLOAD3F(&hitPoint->shadeN.x);
	const float3 dpdu = VLOAD3F(&hitPoint->dpdu.x) + duv.x * shadeN;
	const float3 dpdv = VLOAD3F(&hitPoint->dpdv.x) + duv.y * shadeN;

	const float3 n = normalize(cross(dpdu, dpdv));

	return ((dot(n, shadeN) < 0.f) ? -1.f : 1.f) * n;
}

//------------------------------------------------------------------------------
// ScaleTexture
//------------------------------------------------------------------------------

OPENCL_FORCE_INLINE float3 ScaleTexture_Bump(__global const HitPoint *hitPoint,
		const float3 bumbNTex1, const float3 bumbNTex2,
		const float evalFloatTex1, const float evalFloatTex2) {
	const float3 shadeN = VLOAD3F(&hitPoint->shadeN.x);
	const float3 dpdu = VLOAD3F(&hitPoint->dpdu.x);

	const float3 u = normalize(dpdu);
	const float3 v = normalize(cross(shadeN, dpdu));

	const float3 n1 = bumbNTex1;
	const float nn1 = dot(n1, shadeN);
	float du1, dv1;
	if (nn1 != 0.f) {
		du1 = dot(n1, u) / nn1;
		dv1 = dot(n1, v) / nn1;
	} else {
		du1 = 0.f;
		dv1 = 0.f;
	}

	const float3 n2 = bumbNTex2;
	const float nn2 = dot(n2, shadeN);
	float du2, dv2;
	if (nn2 != 0.f) {
		du2 = dot(n2, u) / nn2;
		dv2 = dot(n2, v) / nn2;
	} else {
		du1 = 0.f;
		dv1 = 0.f;		
	}

	const float t1 = evalFloatTex1;
	const float t2 = evalFloatTex2;

	const float du = du1 * t2 + t1 * du2;
	const float dv = dv1 * t2 + t1 * dv2;

	return normalize(shadeN + du * u + dv * v);
}

//------------------------------------------------------------------------------
// MixTexture
//------------------------------------------------------------------------------

OPENCL_FORCE_INLINE float3 MixTexture_Bump(__global const HitPoint *hitPoint,
		const float3 bumbNTex1, const float3 bumbNTex2, const float3 bumbNAmount,
		const float evalFloatTex1, const float evalFloatTex2, const float evalFloatAmount) {
	const float3 shadeN = VLOAD3F(&hitPoint->shadeN.x);
	const float3 dpdu = VLOAD3F(&hitPoint->dpdu.x);
	const float3 u = normalize(dpdu);
	const float3 v = normalize(cross(shadeN, dpdu));

	float3 n = bumbNTex1;
	float nn = dot(n, shadeN);
	const float du1 = dot(n, u) / nn;
	const float dv1 = dot(n, v) / nn;

	n = bumbNTex2;
	nn = dot(n, shadeN);
	const float du2 = dot(n, u) / nn;
	const float dv2 = dot(n, v) / nn;

	n = bumbNTex2;
	nn = dot(n, shadeN);
	const float dua = dot(n, u) / nn;
	const float dva = dot(n, v) / nn;

	const float t1 = evalFloatTex1;
	const float t2 = evalFloatTex2;
	const float amt = clamp(evalFloatAmount, 0.f, 1.f);

	const float du = Lerp(amt, du1, du2) + dua * (t2 - t1);
	const float dv = Lerp(amt, dv1, dv2) + dva * (t2 - t1);

	return normalize(shadeN + du * u + dv * v);
};

//------------------------------------------------------------------------------
// AddTexture
//------------------------------------------------------------------------------

 OPENCL_FORCE_INLINE float3 AddTexture_Bump(__global const HitPoint *hitPoint,
		 const float3 bumbNTex1, const float3 bumbNTex2) {
	return normalize(bumbNTex1 + bumbNTex2 - VLOAD3F(&hitPoint->shadeN.x));
}

//------------------------------------------------------------------------------
// SubtractTexture
//------------------------------------------------------------------------------

 OPENCL_FORCE_INLINE float3 SubtractTexture_Bump(__global const HitPoint *hitPoint,
		 const float3 bumbNTex1, const float3 bumbNTex2) {
	return normalize(bumbNTex1 - bumbNTex2 + VLOAD3F(&hitPoint->shadeN.x));
}

//------------------------------------------------------------------------------
// NormalMapTexture
//------------------------------------------------------------------------------

OPENCL_FORCE_INLINE float3 NormalMapTexture_Bump(
		__global const Texture* restrict tex,
		__global const HitPoint *hitPoint,
		const float3 evalSpectrumTex) {
	// Normal from normal map
	const float3 rgb = clamp(evalSpectrumTex, 0.f, 1.f);

	// Normal from normal map
	float3 n = 2.f * rgb - (float3)(1.f, 1.f, 1.f);
	const float scale = tex->normalMap.scale;
	n.x *= scale;
	n.y *= scale;

	const float3 oldShadeN = VLOAD3F(&hitPoint->shadeN.x);
	const float3 dpdu = VLOAD3F(&hitPoint->dpdu.x);
	const float3 dpdv = VLOAD3F(&hitPoint->dpdv.x);
	
	Frame frame;
	Frame_Set_Private(&frame, dpdu, dpdv, oldShadeN);

	// Transform n from tangent to object space
	float3 shadeN = normalize(Frame_ToWorld_Private(&frame, n));
	shadeN *= (dot(oldShadeN, shadeN) < 0.f) ? -1.f : 1.f;

	return shadeN;
}

//------------------------------------------------------------------------------
// TriplanarTexture
//------------------------------------------------------------------------------

OPENCL_FORCE_INLINE float3 TriplanarTexture_BumpUVLess(
		__global const HitPoint *hitPoint,
		const float sampleDistance,
		const float evalFloatTexBase,
		const float evalFloatTexOffsetX,
		const float evalFloatTexOffsetY,
		const float evalFloatTexOffsetZ) {
	// Calculate bump map value at intersection point
	const float base = evalFloatTexBase;

	float3 dhdx;

	const float offsetX = evalFloatTexOffsetX;
	dhdx.x = (offsetX - base) / sampleDistance;

	const float offsetY = evalFloatTexOffsetY;
	dhdx.y = (offsetY - base) / sampleDistance;

	const float offsetZ = evalFloatTexOffsetZ;
	dhdx.z = (offsetZ - base) / sampleDistance;

	const float3 shadeN = VLOAD3F(&hitPoint->shadeN.x);
	float3 newShadeN = normalize(shadeN - dhdx);
	newShadeN *= (dot(shadeN, newShadeN) < 0.f) ? -1.f : 1.f;

	return newShadeN;
}
