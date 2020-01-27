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

//OPENCL_FORCE_NOT_INLINE float3 GenericTexture_Bump(
//		const uint texIndex,
//		__global HitPoint *hitPoint,
//		const float sampleDistance
//		TEXTURES_PARAM_DECL) {
//	const float3 dpdu = VLOAD3F(&hitPoint->dpdu.x);
//	const float3 dpdv = VLOAD3F(&hitPoint->dpdv.x);
//	const float3 dndu = VLOAD3F(&hitPoint->dndu.x);
//	const float3 dndv = VLOAD3F(&hitPoint->dndv.x);
//
//	// Calculate bump map value at intersection point
//	const float base = Texture_GetFloatValue(texIndex, hitPoint
//			TEXTURES_PARAM);
//
//	// Compute offset positions and evaluate displacement texIndex
//	const float3 origP = VLOAD3F(&hitPoint->p.x);
//	const float3 origShadeN = VLOAD3F(&hitPoint->shadeN.x);
//	const float2 origUV = VLOAD2F(&hitPoint->uv[0].u);
//
//	float2 duv;
//
//	// Shift hitPointTmp.du in the u direction and calculate value
//	const float uu = sampleDistance / length(dpdu);
//	VSTORE3F(origP + uu * dpdu, &hitPoint->p.x);
//	hitPoint->uv[0].u += uu;
//	VSTORE3F(normalize(origShadeN + uu * dndu), &hitPoint->shadeN.x);
//	const float duValue = Texture_GetFloatValue(texIndex, hitPoint
//			TEXTURES_PARAM);
//	duv.s0 = (duValue - base) / uu;
//
//	// Shift hitPointTmp.dv in the v direction and calculate value
//	const float vv = sampleDistance / length(dpdv);
//	VSTORE3F(origP + vv * dpdv, &hitPoint->p.x);
//	hitPoint->uv[0].u = origUV.s0;
//	hitPoint->uv[0].v += vv;
//	VSTORE3F(normalize(origShadeN + vv * dndv), &hitPoint->shadeN.x);
//	const float dvValue = Texture_GetFloatValue(texIndex, hitPoint
//			TEXTURES_PARAM);
//	duv.s1 = (dvValue - base) / vv;
//
//	// Restore HitPoint
//	VSTORE3F(origP, &hitPoint->p.x);
//	VSTORE3F(origShadeN, &hitPoint->shadeN.x);
//	VSTORE2F(origUV, &hitPoint->uv[0].u);
//
//	// Compute the new dpdu and dpdv
//	const float3 bumpDpdu = dpdu + duv.s0 * origShadeN;
//	const float3 bumpDpdv = dpdv + duv.s1 * origShadeN;
//	float3 newShadeN = normalize(cross(bumpDpdu, bumpDpdv));
//
//	// The above transform keeps the normal in the original normal
//	// hemisphere. If they are opposed, it means UVN was indirect and
//	// the normal needs to be reversed
//	newShadeN *= (dot(origShadeN, newShadeN) < 0.f) ? -1.f : 1.f;
//
//	return newShadeN;
//}

//------------------------------------------------------------------------------
// ConstFloatTexture
//------------------------------------------------------------------------------

OPENCL_FORCE_INLINE float3 ConstFloatTexture_Bump(__global const HitPoint *hitPoint) {
	return VLOAD3F(&hitPoint->shadeN.x);
}

//------------------------------------------------------------------------------
// ConstFloat3Texture
//------------------------------------------------------------------------------

OPENCL_FORCE_INLINE float3 ConstFloat3Texture_Bump(__global const HitPoint *hitPoint) {
	return VLOAD3F(&hitPoint->shadeN.x);
}

//------------------------------------------------------------------------------
// FresnelConstTexture
//------------------------------------------------------------------------------

#if defined(PARAM_ENABLE_TEX_FRESNELCONST)
OPENCL_FORCE_INLINE float3 FresnelConstTexture_Bump(__global HitPoint *hitPoint) {
	return VLOAD3F(&hitPoint->shadeN.x);
}
#endif

//------------------------------------------------------------------------------
// FresnelColorTexture
//------------------------------------------------------------------------------

#if defined(PARAM_ENABLE_TEX_FRESNELCOLOR)
OPENCL_FORCE_INLINE float3 FresnelColorTexture_Bump(__global HitPoint *hitPoint) {
	return VLOAD3F(&hitPoint->shadeN.x);
}
#endif

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
// NormalMapTexture
//------------------------------------------------------------------------------

#if defined(PARAM_ENABLE_TEX_NORMALMAP)
OPENCL_FORCE_NOT_INLINE float3 NormalMapTexture_Bump(
		__global const Texture* restrict tex,
		__global HitPoint *hitPoint,
		const float sampleDistance
		TEXTURES_PARAM_DECL) {
	// Normal from normal map
	float3 rgb = Texture_GetSpectrumValue(tex->normalMap.texIndex, hitPoint
			TEXTURES_PARAM);
	rgb = clamp(rgb, -1.f, 1.f);

	// Normal from normal map
	float3 n = 2.f * rgb - (float3)(1.f, 1.f, 1.f);
	const float scale = tex->normalMap.scale;
	n.x *= scale;
	n.y *= scale;

	const float3 oldShadeN = VLOAD3F(&hitPoint->shadeN.x);
	float3 dpdu = VLOAD3F(&hitPoint->dpdu.x);
	float3 dpdv = VLOAD3F(&hitPoint->dpdv.x);
	
	Frame frame;
	Frame_Set_Private(&frame, dpdu, dpdv, oldShadeN);

	// Transform n from tangent to object space
	float3 shadeN = normalize(Frame_ToWorld_Private(&frame, n));
	shadeN *= (dot(oldShadeN, shadeN) < 0.f) ? -1.f : 1.f;

	return shadeN;
}
#endif

//------------------------------------------------------------------------------
// TriplanarTexture
//------------------------------------------------------------------------------

//OPENCL_FORCE_INLINE float3 TriplanarTexture_Bump(
//		__global HitPoint *hitPoint,
//		const float evalFloatTexBase,
//		const float evalFloatTexOffsetX,
//		const float evalFloatTexOffsetY,
//		const float evalFloatTexOffsetZ) {
//	__global const Texture* restrict tex = &texs[texIndex];
//
//// TODO
////	if (tex->triplanarTex.enableUVlessBumpMap) {
//		// Calculate bump map value at intersection point
//		const float base = evalFloatTexBase;
//
//		float3 dhdx;
//
//		const float offsetX = evalFloatTexOffsetX;
//		dhdx.x = (offsetX - base) / sampleDistance;
//
//		const float offsetY = evalFloatTexOffsetY;
//		dhdx.y = (offsetY - base) / sampleDistance;
//
//		const float offsetZ = evalFloatTexOffsetZ;
//		dhdx.z = (offsetZ - base) / sampleDistance;
//
//		const float3 shadeN = VLOAD3F(&hitPoint->shadeN.x);
//		float3 newShadeN = normalize(shadeN - dhdx);
//		newShadeN *= (dot(shadeN, newShadeN) < 0.f) ? -1.f : 1.f;
//
//		return newShadeN;
////	} else
////		return GenericTexture_Bump(texIndex, hitPoint, sampleDistance TEXTURES_PARAM);
//}
