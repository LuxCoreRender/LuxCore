#line 2 "texture_funcs.cl"

/***************************************************************************
 *   Copyright (C) 1998-2010 by authors (see AUTHORS.txt )                 *
 *                                                                         *
 *   This file is part of LuxRays.                                         *
 *                                                                         *
 *   LuxRays is free software; you can redistribute it and/or modify       *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   LuxRays is distributed in the hope that it will be useful,            *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 *                                                                         *
 *   LuxRays website: http://www.luxrender.net                             *
 ***************************************************************************/

#define TEXTURE_STACK_SIZE 16

//------------------------------------------------------------------------------
// ImageMaps support
//------------------------------------------------------------------------------

#if defined(PARAM_HAS_IMAGEMAPS)

__global float *ImageMap_GetPixelsAddress(
#if defined(PARAM_IMAGEMAPS_PAGE_0)
	__global float *imageMapBuff0,
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_1)
	__global float *imageMapBuff1,
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_2)
	__global float *imageMapBuff2,
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_3)
	__global float *imageMapBuff3,
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_4)
	__global float *imageMapBuff4,
#endif
	const uint page, const uint offset
    ) {
    switch (page) {
#if defined(PARAM_IMAGEMAPS_PAGE_1)
        case 1:
            return &imageMapBuff1[offset];
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_2)
        case 2:
            return &imageMapBuff2[offset];
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_3)
        case 3:
            return &imageMapBuff3[offset];
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_4)
        case 4:
            return &imageMapBuff4[offset];
#endif
        default:
        case 0:
            return &imageMapBuff0[offset];
    }
}

float ImageMap_GetTexel_Grey(__global float *pixels,
		const uint width, const uint height, const uint channelCount,
		const int s, const int t) {
	const uint u = Mod(s, width);
	const uint v = Mod(t, height);

	const uint index = channelCount * (v * width + u);

	return (channelCount == 1) ? pixels[index] : Spectrum_Y(VLOAD3F(&pixels[index]));
}

float3 ImageMap_GetTexel_Color(__global float *pixels,
		const uint width, const uint height, const uint channelCount,
		const int s, const int t) {
	const uint u = Mod(s, width);
	const uint v = Mod(t, height);

	const uint index = channelCount * (v * width + u);

	return (channelCount == 1) ? pixels[index] : VLOAD3F(&pixels[index]);
}

float ImageMap_GetGrey(__global float *pixels,
		const uint width, const uint height, const uint channelCount,
		const float u, const float v) {
	const float s = u * width - 0.5f;
	const float t = v * height - 0.5f;

	const int s0 = (int)floor(s);
	const int t0 = (int)floor(t);

	const float ds = s - s0;
	const float dt = t - t0;

	const float ids = 1.f - ds;
	const float idt = 1.f - dt;

	const float c0 = ImageMap_GetTexel_Grey(pixels, width, height, channelCount, s0, t0);
	const float c1 = ImageMap_GetTexel_Grey(pixels, width, height, channelCount, s0, t0 + 1);
	const float c2 = ImageMap_GetTexel_Grey(pixels, width, height, channelCount, s0 + 1, t0);
	const float c3 = ImageMap_GetTexel_Grey(pixels, width, height, channelCount, s0 + 1, t0 + 1);

	const float k0 = ids * idt;
	const float k1 = ids * dt;
	const float k2 = ds * idt;
	const float k3 = ds * dt;

	return (k0 * c0 + k1 *c1 + k2 * c2 + k3 * c3);
}

float3 ImageMap_GetColor(__global float *pixels,
		const uint width, const uint height, const uint channelCount,
		const float u, const float v) {
	const float s = u * width - 0.5f;
	const float t = v * height - 0.5f;

	const int s0 = (int)floor(s);
	const int t0 = (int)floor(t);

	const float ds = s - s0;
	const float dt = t - t0;

	const float ids = 1.f - ds;
	const float idt = 1.f - dt;

	const float3 c0 = ImageMap_GetTexel_Color(pixels, width, height, channelCount, s0, t0);
	const float3 c1 = ImageMap_GetTexel_Color(pixels, width, height, channelCount, s0, t0 + 1);
	const float3 c2 = ImageMap_GetTexel_Color(pixels, width, height, channelCount, s0 + 1, t0);
	const float3 c3 = ImageMap_GetTexel_Color(pixels, width, height, channelCount, s0 + 1, t0 + 1);

	const float k0 = ids * idt;
	const float k1 = ids * dt;
	const float k2 = ds * idt;
	const float k3 = ds * dt;

	return (k0 * c0 + k1 *c1 + k2 * c2 + k3 * c3);
}

#endif

//------------------------------------------------------------------------------
// ConstFloat texture
//------------------------------------------------------------------------------

#if defined (PARAM_ENABLE_TEX_CONST_FLOAT)

void ConstFloatTexture_EvaluateGrey(__global Texture *texture, __global HitPoint *hitPoint,
		float texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
	texValues[(*texValuesSize)++] = texture->constFloat.value;
}

void ConstFloatTexture_EvaluateColor(__global Texture *texture, __global HitPoint *hitPoint,
		float3 texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
	texValues[(*texValuesSize)++] = texture->constFloat.value;
}

void ConstFloatTexture_EvaluateDuDv(__global Texture *texture, __global HitPoint *hitPoint,
		float2 texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
	texValues[(*texValuesSize)++] = 0.f;
}

#endif

//------------------------------------------------------------------------------
// ConstFloat3 texture
//------------------------------------------------------------------------------

#if defined (PARAM_ENABLE_TEX_CONST_FLOAT3)

void ConstFloat3Texture_EvaluateGrey(__global Texture *texture, __global HitPoint *hitPoint,
		float texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
	texValues[(*texValuesSize)++] = Spectrum_Y(VLOAD3F(&texture->constFloat3.color.r));
}

void ConstFloat3Texture_EvaluateColor(__global Texture *texture, __global HitPoint *hitPoint,
		float3 texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
	texValues[(*texValuesSize)++] = VLOAD3F(&texture->constFloat3.color.r);
}

void ConstFloat3Texture_EvaluateDuDv(__global Texture *texture, __global HitPoint *hitPoint,
		float2 texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
	texValues[(*texValuesSize)++] = 0.f;
}

#endif

//------------------------------------------------------------------------------
// ConstFloat4 texture
//------------------------------------------------------------------------------

#if defined (PARAM_ENABLE_TEX_CONST_FLOAT4)

void ConstFloat4Texture_EvaluateGrey(__global Texture *texture, __global HitPoint *hitPoint,
		float texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
	texValues[(*texValuesSize)++] = Spectrum_Y(VLOAD3F(&texture->constFloat4.color.r));
}

void ConstFloat4Texture_EvaluateColor(__global Texture *texture, __global HitPoint *hitPoint,
		float3 texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
	texValues[(*texValuesSize)++] = VLOAD3F(&texture->constFloat4.color.r);
}

void ConstFloat4Texture_EvaluateDuDv(__global Texture *texture, __global HitPoint *hitPoint,
		float2 texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
	texValues[(*texValuesSize)++] = 0.f;
}

#endif

//------------------------------------------------------------------------------
// ImageMap texture
//------------------------------------------------------------------------------

#if defined (PARAM_ENABLE_TEX_IMAGEMAP)

void ImageMapTexture_EvaluateGrey(__global Texture *texture, __global HitPoint *hitPoint,
		float texValues[TEXTURE_STACK_SIZE], uint *texValuesSize
		IMAGEMAPS_PARAM_DECL) {
	__global ImageMap *imageMap = &imageMapDescs[texture->imageMapTex.imageMapIndex];
	__global float *pixels = ImageMap_GetPixelsAddress(
#if defined(PARAM_IMAGEMAPS_PAGE_0)
		imageMapBuff0,
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_1)
		imageMapBuff1,
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_2)
		imageMapBuff2,
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_3)
		imageMapBuff3,
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_4)
		imageMapBuff4,
#endif
		imageMap->pageIndex, imageMap->pixelsIndex);

	const float2 uv = VLOAD2F(&hitPoint->uv.u);
	const float2 mapUV = Mapping_Map2D(&texture->imageMapTex.mapping, uv);

	texValues[(*texValuesSize)++] = texture->imageMapTex.gain * ImageMap_GetGrey(
			pixels,
			imageMap->width, imageMap->height, imageMap->channelCount,
			mapUV.s0, mapUV.s1);
}

void ImageMapTexture_EvaluateColor(__global Texture *texture, __global HitPoint *hitPoint,
		float3 texValues[TEXTURE_STACK_SIZE], uint *texValuesSize
		IMAGEMAPS_PARAM_DECL) {
	__global ImageMap *imageMap = &imageMapDescs[texture->imageMapTex.imageMapIndex];
	__global float *pixels = ImageMap_GetPixelsAddress(
#if defined(PARAM_IMAGEMAPS_PAGE_0)
		imageMapBuff0,
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_1)
		imageMapBuff1,
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_2)
		imageMapBuff2,
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_3)
		imageMapBuff3,
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_4)
		imageMapBuff4,
#endif
		imageMap->pageIndex, imageMap->pixelsIndex);

	const float2 uv = VLOAD2F(&hitPoint->uv.u);
	const float2 mapUV = Mapping_Map2D(&texture->imageMapTex.mapping, uv);

	texValues[(*texValuesSize)++] = texture->imageMapTex.gain * ImageMap_GetColor(
			pixels,
			imageMap->width, imageMap->height, imageMap->channelCount,
			mapUV.s0, mapUV.s1);
}

void ImageMapTexture_EvaluateDuDv(__global Texture *texture, __global HitPoint *hitPoint,
		float2 texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
	texValues[(*texValuesSize)++] = VLOAD2F(&texture->imageMapTex.Du);
}

#endif

//------------------------------------------------------------------------------
// Scale texture
//------------------------------------------------------------------------------

#if defined (PARAM_ENABLE_TEX_SCALE)

void ScaleTexture_EvaluateGrey(__global Texture *texture, __global HitPoint *hitPoint,
		float texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
	const float value = texValues[--(*texValuesSize)] * texValues[--(*texValuesSize)];

	texValues[(*texValuesSize)++] = value;
}

void ScaleTexture_EvaluateColor(__global Texture *texture, __global HitPoint *hitPoint,
		float3 texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
	const float3 value = texValues[--(*texValuesSize)] * texValues[--(*texValuesSize)];

	texValues[(*texValuesSize)++] = value;
}

void ScaleTexture_EvaluateDuDv(__global Texture *texture, __global HitPoint *hitPoint,
		float2 texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
	const float2 dudv1 = texValues[--(*texValuesSize)];
	const float2 dudv2 = texValues[--(*texValuesSize)];

	texValues[(*texValuesSize)++] = (float2)(fmax(dudv1.x, dudv2.x), fmax(dudv1.y, dudv2.y));
}

#endif

//------------------------------------------------------------------------------
// FresnelApproxN & FresnelApproxK texture
//------------------------------------------------------------------------------

#if defined (PARAM_ENABLE_FRESNEL_APPROX_N)

float FresnelApproxN(const float Fr) {
	const float sqrtReflectance = sqrt(clamp(Fr, 0.f, .999f));

	return (1.f + sqrtReflectance) /
		(1.f - sqrtReflectance);
}

float3 FresnelApproxN3(const float3 Fr) {
	const float3 sqrtReflectance = Spectrum_Sqrt(clamp(Fr, 0.f, .999f));

	return (WHITE + sqrtReflectance) /
		(WHITE - sqrtReflectance);
}

void FresnelApproxNTexture_EvaluateGrey(__global Texture *texture, __global HitPoint *hitPoint,
		float texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
	const float value = texValues[--(*texValuesSize)];

	texValues[(*texValuesSize)++] = FresnelApproxN(value);
}

void FresnelApproxN3Texture_EvaluateColor(__global Texture *texture, __global HitPoint *hitPoint,
		float texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
	const float3 value = texValues[--(*texValuesSize)];

	texValues[(*texValuesSize)++] = FresnelApproxN3(value);
}

void FresnelApproxNTexture_EvaluateDuDv(__global Texture *texture, __global HitPoint *hitPoint,
		float2 texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
	texValues[(*texValuesSize)++] = texValues[--(*texValuesSize)];
}

#endif

#if defined (PARAM_ENABLE_FRESNEL_APPROX_K)

float FresnelApproxK(const float Fr) {
	const float reflectance = clamp(Fr, 0.f, .999f);

	return 2.f * sqrt(reflectance /
		(1.f - reflectance));
}

float3 FresnelApproxK3(const float3 Fr) {
	const float3 reflectance = clamp(Fr, 0.f, .999f);

	return 2.f * Spectrum_Sqrt(reflectance /
		(WHITE - reflectance));
}

void FresnelApproxKTexture_EvaluateGrey(__global Texture *texture, __global HitPoint *hitPoint,
		float texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
	const float value = texValues[--(*texValuesSize)];

	texValues[(*texValuesSize)++] = FresnelApproxK(value);
}

void FresnelApproxKTexture_EvaluateColor(__global Texture *texture, __global HitPoint *hitPoint,
		float texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
	const float3 value = texValues[--(*texValuesSize)];

	texValues[(*texValuesSize)++] = FresnelApproxK3(value);
}

void FresnelApproxKTexture_EvaluateDuDv(__global Texture *texture, __global HitPoint *hitPoint,
		float2 texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
	texValues[(*texValuesSize)++] = texValues[--(*texValuesSize)];
}

#endif

//------------------------------------------------------------------------------
// CheckerBoard2D texture
//------------------------------------------------------------------------------

#if defined (PARAM_ENABLE_CHECKERBOARD2D)

void CheckerBoard2DTexture_EvaluateGrey(__global Texture *texture, __global HitPoint *hitPoint,
		float texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
	const float value1 = texValues[--(*texValuesSize)];
	const float value2 = texValues[--(*texValuesSize)];

	const float2 uv = VLOAD2F(&hitPoint->uv.u);
	const float2 mapUV = Mapping_Map2D(&texture->checkerBoard2D.mapping, uv);
	texValues[(*texValuesSize)++] = (((int)floor(mapUV.s0) + (int)floor(mapUV.s1)) % 2 == 0) ? value1 : value2;
}

void  CheckerBoard2DTexture_EvaluateColor(__global Texture *texture, __global HitPoint *hitPoint,
		float3 texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
	const float3 value1 = texValues[--(*texValuesSize)];
	const float3 value2 = texValues[--(*texValuesSize)];

	const float2 uv = VLOAD2F(&hitPoint->uv.u);
	const float2 mapUV = Mapping_Map2D(&texture->checkerBoard2D.mapping, uv);
	texValues[(*texValuesSize)++] = (((int)floor(mapUV.s0) + (int)floor(mapUV.s1)) % 2 == 0) ? value1 : value2;
}

void  CheckerBoard2DTexture_EvaluateDuDv(__global Texture *texture, __global HitPoint *hitPoint,
		float2 texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
	const float2 value1 = texValues[--(*texValuesSize)];
	const float2 value2 = texValues[--(*texValuesSize)];

	const float2 uv = VLOAD2F(&hitPoint->uv.u);
	const float2 mapUV = Mapping_Map2D(&texture->checkerBoard2D.mapping, uv);
	texValues[(*texValuesSize)++] = (((int)floor(mapUV.s0) + (int)floor(mapUV.s1)) % 2 == 0) ? value1 : value2;
}

#endif

//------------------------------------------------------------------------------
// Generic texture functions with support for recursive textures
//------------------------------------------------------------------------------

uint Texture_AddSubTexture(__global Texture *texture,
		__global Texture *todoTex[TEXTURE_STACK_SIZE], uint *todoTexSize
		TEXTURES_PARAM_DECL) {
	switch (texture->type) {
#if defined(PARAM_ENABLE_TEX_SCALE)
		case SCALE_TEX:
			todoTex[(*todoTexSize)++] = &texs[texture->scaleTex.tex1Index];
			todoTex[(*todoTexSize)++] = &texs[texture->scaleTex.tex2Index];
			return 2;
#endif
#if defined (PARAM_ENABLE_FRESNEL_APPROX_N)
		case FRESNEL_APPROX_K:
			todoTex[(*todoTexSize)++] = &texs[texture->fresnelApproxN.texIndex];
			return 1;
#endif
#if defined (PARAM_ENABLE_FRESNEL_APPROX_K)
		case FRESNEL_APPROX_K:
			todoTex[(*todoTexSize)++] = &texs[texture->fresnelApproxK.texIndex];
			return 1;
#endif
#if defined(PARAM_ENABLE_CHECKERBOARD2D)
		case CHECKERBOARD2D:
			todoTex[(*todoTexSize)++] = &texs[texture->checkerBoard2D.tex1Index];
			todoTex[(*todoTexSize)++] = &texs[texture->checkerBoard2D.tex2Index];
			return 2;
#endif
#if defined(PARAM_ENABLE_TEX_CONST_FLOAT)
		case CONST_FLOAT:
#endif
#if defined(PARAM_ENABLE_TEX_CONST_FLOAT3)
		case CONST_FLOAT3:
#endif
#if defined(PARAM_ENABLE_TEX_CONST_FLOAT4)
		case CONST_FLOAT4:
#endif
#if defined(PARAM_ENABLE_TEX_IMAGEMAP)
		case IMAGEMAP:
#endif
		default:
			return 0;
	}

}

//------------------------------------------------------------------------------
// Grey texture channel
//------------------------------------------------------------------------------

void Texture_EvaluateGrey(__global Texture *texture, __global HitPoint *hitPoint,
		float texValues[TEXTURE_STACK_SIZE], uint *texValuesSize
		IMAGEMAPS_PARAM_DECL) {
	switch (texture->type) {
#if defined(PARAM_ENABLE_TEX_CONST_FLOAT)
		case CONST_FLOAT:
			return ConstFloatTexture_EvaluateGrey(texture, hitPoint, texValues, texValuesSize);
#endif
#if defined(PARAM_ENABLE_TEX_CONST_FLOAT3)
		case CONST_FLOAT3:
			return ConstFloat3Texture_EvaluateGrey(texture, hitPoint, texValues, texValuesSize);
#endif
#if defined(PARAM_ENABLE_TEX_CONST_FLOAT4)
		case CONST_FLOAT4:
			return ConstFloat4Texture_EvaluateGrey(texture, hitPoint, texValues, texValuesSize);
#endif
#if defined(PARAM_ENABLE_TEX_IMAGEMAP)
		case IMAGEMAP:
			return ImageMapTexture_EvaluateGrey(texture, hitPoint, texValues, texValuesSize
					IMAGEMAPS_PARAM);
#endif
#if defined(PARAM_ENABLE_TEX_SCALE)
		case SCALE_TEX:
			return ScaleTexture_EvaluateGrey(texture, hitPoint, texValues, texValuesSize);
#endif
#if defined (PARAM_ENABLE_FRESNEL_APPROX_N)
		case FRESNEL_APPROX_K:
			return FresnelApproxNTexture_EvaluateGrey(texture, hitPoint, texValues, texValuesSize);
#endif
#if defined (PARAM_ENABLE_FRESNEL_APPROX_K)
		case FRESNEL_APPROX_K:
			return FresnelApproxKTexture_EvaluateGrey(texture, hitPoint, texValues, texValuesSize);
#endif
#if defined (PARAM_ENABLE_CHECKERBOARD2D)
		case CHECKERBOARD2D:
			return CheckerBoard2DTexture_EvaluateGrey(texture, hitPoint, texValues, texValuesSize);
#endif
		default:
			// Do nothing
			;
	}
}

float Texture_GetGreyValue(__global Texture *texture, __global HitPoint *hitPoint
		TEXTURES_PARAM_DECL) {
	__global Texture *todoTex[TEXTURE_STACK_SIZE];
	uint todoTexSize = 0;

	__global Texture *pendingTex[TEXTURE_STACK_SIZE];
	uint pendingSubTexCount[TEXTURE_STACK_SIZE];
	uint pendingTexSize = 0;

	float texValues[TEXTURE_STACK_SIZE];
	uint texValuesSize = 0;

	const uint subTexCount = Texture_AddSubTexture(texture, todoTex, &todoTexSize
			TEXTURES_PARAM);
	if (subTexCount == 0) {
		// A fast path for evaluating non recursive textures
		Texture_EvaluateGrey(texture, hitPoint, texValues, &texValuesSize
			IMAGEMAPS_PARAM);
	} else {
		// Normal complex path for evaluating non recursive textures
		pendingTex[pendingTexSize] = texture;
		pendingSubTexCount[pendingTexSize++] = subTexCount;
		do {
			if ((pendingTexSize > 0) && (texValuesSize >= pendingSubTexCount[pendingTexSize - 1])) {
				// Pop the a texture to do
				__global Texture *tex = pendingTex[--pendingTexSize];

				Texture_EvaluateGrey(tex, hitPoint, texValues, &texValuesSize
						IMAGEMAPS_PARAM);
				continue;
			}

			if (todoTexSize > 0) {
				// Pop the a texture to do
				__global Texture *tex = todoTex[--todoTexSize];

				// Add this texture to the list of pending one
				pendingTex[pendingTexSize] = tex;
				pendingSubTexCount[pendingTexSize++] = Texture_AddSubTexture(tex, todoTex, &todoTexSize
						TEXTURES_PARAM);
			}
		} while ((todoTexSize > 0) || (pendingTexSize > 0));
	}

	return texValues[0];
}

//------------------------------------------------------------------------------
// Color texture channel
//------------------------------------------------------------------------------

void Texture_EvaluateColor(__global Texture *texture, __global HitPoint *hitPoint,
		float3 texValues[TEXTURE_STACK_SIZE], uint *texValuesSize
		IMAGEMAPS_PARAM_DECL) {
	switch (texture->type) {
#if defined(PARAM_ENABLE_TEX_CONST_FLOAT)
		case CONST_FLOAT:
			return ConstFloatTexture_EvaluateColor(texture, hitPoint, texValues, texValuesSize);
#endif
#if defined(PARAM_ENABLE_TEX_CONST_FLOAT3)
		case CONST_FLOAT3:
			return ConstFloat3Texture_EvaluateColor(texture, hitPoint, texValues, texValuesSize);
#endif
#if defined(PARAM_ENABLE_TEX_CONST_FLOAT4)
		case CONST_FLOAT4:
			return ConstFloat4Texture_EvaluateColor(texture, hitPoint, texValues, texValuesSize);
#endif
#if defined(PARAM_ENABLE_TEX_IMAGEMAP)
		case IMAGEMAP:
			return ImageMapTexture_EvaluateColor(texture, hitPoint, texValues, texValuesSize
					IMAGEMAPS_PARAM);
#endif
#if defined(PARAM_ENABLE_TEX_SCALE)
		case SCALE_TEX:
			return ScaleTexture_EvaluateColor(texture, hitPoint, texValues, texValuesSize);
#endif
#if defined (PARAM_ENABLE_FRESNEL_APPROX_N)
		case FRESNEL_APPROX_K:
			return FresnelApproxNTexture_EvaluateColor(texture, hitPoint, texValues, texValuesSize);
#endif
#if defined (PARAM_ENABLE_FRESNEL_APPROX_K)
		case FRESNEL_APPROX_K:
			return FresnelApproxKTexture_EvaluateColor(texture, hitPoint, texValues, texValuesSize);
#endif
#if defined (PARAM_ENABLE_CHECKERBOARD2D)
		case CHECKERBOARD2D:
			return CheckerBoard2DTexture_EvaluateColor(texture, hitPoint, texValues, texValuesSize);
#endif
		default:
			// Do nothing
			;
	}
}

float3 Texture_GetColorValue(__global Texture *texture, __global HitPoint *hitPoint
		TEXTURES_PARAM_DECL) {
	__global Texture *todoTex[TEXTURE_STACK_SIZE];
	uint todoTexSize = 0;

	__global Texture *pendingTex[TEXTURE_STACK_SIZE];
	uint pendingSubTexCount[TEXTURE_STACK_SIZE];
	uint pendingTexSize = 0;

	float3 texValues[TEXTURE_STACK_SIZE];
	uint texValuesSize = 0;

	const uint subTexCount = Texture_AddSubTexture(texture, todoTex, &todoTexSize
			TEXTURES_PARAM);
	if (subTexCount == 0) {
		// A fast path for evaluating non recursive textures
		Texture_EvaluateColor(texture, hitPoint, texValues, &texValuesSize
			IMAGEMAPS_PARAM);
	} else {
		// Normal complex path for evaluating non recursive textures
		pendingTex[pendingTexSize] = texture;
		pendingSubTexCount[pendingTexSize++] = subTexCount;
		do {
			if ((pendingTexSize > 0) && (texValuesSize >= pendingSubTexCount[pendingTexSize - 1])) {
				// Pop the a texture to do
				__global Texture *tex = pendingTex[--pendingTexSize];

				Texture_EvaluateColor(tex, hitPoint, texValues, &texValuesSize
						IMAGEMAPS_PARAM);
				continue;
			}

			if (todoTexSize > 0) {
				// Pop the a texture to do
				__global Texture *tex = todoTex[--todoTexSize];

				// Add this texture to the list of pending one
				pendingTex[pendingTexSize] = tex;
				pendingSubTexCount[pendingTexSize++] = Texture_AddSubTexture(tex, todoTex, &todoTexSize
						TEXTURES_PARAM);
			}
		} while ((todoTexSize > 0) || (pendingTexSize > 0));
	}

	return texValues[0];
}

//------------------------------------------------------------------------------
// DuDv texture information
//------------------------------------------------------------------------------

void Texture_EvaluateDuDv(__global Texture *texture, __global HitPoint *hitPoint,
		float2 texValues[TEXTURE_STACK_SIZE], uint *texValuesSize
		IMAGEMAPS_PARAM_DECL) {
	switch (texture->type) {
#if defined(PARAM_ENABLE_TEX_CONST_FLOAT)
		case CONST_FLOAT:
			ConstFloatTexture_EvaluateDuDv(texture, hitPoint, texValues, texValuesSize);
#endif
#if defined(PARAM_ENABLE_TEX_CONST_FLOAT3)
		case CONST_FLOAT3:
			return ConstFloat3Texture_EvaluateDuDv(texture, hitPoint, texValues, texValuesSize);
#endif
#if defined(PARAM_ENABLE_TEX_CONST_FLOAT4)
		case CONST_FLOAT4:
			return ConstFloat4Texture_EvaluateDuDv(texture, hitPoint, texValues, texValuesSize);
#endif
#if defined(PARAM_ENABLE_TEX_IMAGEMAP)
		case IMAGEMAP:
			return ImageMapTexture_EvaluateDuDv(texture, hitPoint, texValues, texValuesSize);
#endif
#if defined(PARAM_ENABLE_TEX_SCALE)
		case SCALE_TEX:
			return ScaleTexture_EvaluateDuDv(texture, hitPoint, texValues, texValuesSize);
#endif
#if defined (PARAM_ENABLE_FRESNEL_APPROX_N)
		case FRESNEL_APPROX_K:
			return FresnelApproxNTexture_EvaluateDuDv(texture, hitPoint, texValues, texValuesSize);
#endif
#if defined (PARAM_ENABLE_FRESNEL_APPROX_K)
		case FRESNEL_APPROX_K:
			return FresnelApproxKTexture_EvaluateDuDv(texture, hitPoint, texValues, texValuesSize);
#endif
#if defined (PARAM_ENABLE_CHECKERBOARD2D)
		case CHECKERBOARD2D:
			return CheckerBoard2DTexture_EvaluateDuDv(texture, hitPoint, texValues, texValuesSize);
#endif
		default:
			// Do nothing
			;
	}
}

float2 Texture_GetDuDv(__global Texture *texture, __global HitPoint *hitPoint
		TEXTURES_PARAM_DECL) {
	__global Texture *todoTex[TEXTURE_STACK_SIZE];
	uint todoTexSize = 0;

	__global Texture *pendingTex[TEXTURE_STACK_SIZE];
	uint pendingSubTexCount[TEXTURE_STACK_SIZE];
	uint pendingTexSize = 0;

	float2 texValues[TEXTURE_STACK_SIZE];
	uint texValuesSize = 0;

	const uint subTexCount = Texture_AddSubTexture(texture, todoTex, &todoTexSize
			TEXTURES_PARAM);
	if (subTexCount == 0) {
		// A fast path for evaluating non recursive textures
		Texture_EvaluateDuDv(texture, hitPoint, texValues, &texValuesSize
			IMAGEMAPS_PARAM);
	} else {
		// Normal complex path for evaluating non recursive textures
		pendingTex[pendingTexSize] = texture;
		pendingSubTexCount[pendingTexSize++] = subTexCount;
		do {
			if ((pendingTexSize > 0) && (texValuesSize >= pendingSubTexCount[pendingTexSize - 1])) {
				// Pop the a texture to do
				__global Texture *tex = pendingTex[--pendingTexSize];

			Texture_EvaluateDuDv(tex, hitPoint, texValues, &texValuesSize
						IMAGEMAPS_PARAM);
				continue;
			}

			if (todoTexSize > 0) {
				// Pop the a texture to do
				__global Texture *tex = todoTex[--todoTexSize];

				// Add this texture to the list of pending one
				pendingTex[pendingTexSize] = tex;
				pendingSubTexCount[pendingTexSize++] = Texture_AddSubTexture(tex, todoTex, &todoTexSize
						TEXTURES_PARAM);
			}
		} while ((todoTexSize > 0) || (pendingTexSize > 0));
	}

	return texValues[0];
}
