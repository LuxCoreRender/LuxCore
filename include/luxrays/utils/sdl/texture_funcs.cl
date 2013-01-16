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

//------------------------------------------------------------------------------
// ConstFloat texture
//------------------------------------------------------------------------------

#if defined (PARAM_ENABLE_TEX_CONST_FLOAT)

float ConstFloatTexture_GetGreyValue(__global Texture *texture, const float2 uv) {
	return texture->constFloat.value;
}

float3 ConstFloatTexture_GetColorValue(__global Texture *texture, const float2 uv) {
	return texture->constFloat.value;
}

#endif

//------------------------------------------------------------------------------
// ConstFloat3 texture
//------------------------------------------------------------------------------

#if defined (PARAM_ENABLE_TEX_CONST_FLOAT3)

float ConstFloat3Texture_GetGreyValue(__global Texture *texture, const float2 uv) {
	return Spectrum_Y(VLOAD3F(&texture->constFloat3.color.r));
}

float3 ConstFloat3Texture_GetColorValue(__global Texture *texture, const float2 uv) {
	return VLOAD3F(&texture->constFloat3.color.r);
}

#endif

//------------------------------------------------------------------------------
// ConstFloat4 texture
//------------------------------------------------------------------------------

#if defined (PARAM_ENABLE_TEX_CONST_FLOAT4)

float ConstFloat4Texture_GetGreyValue(__global Texture *texture, const float2 uv) {
	return Spectrum_Y(VLOAD3F(&texture->constFloat4.color.r));
}

float3 ConstFloat4Texture_GetColorValue(__global Texture *texture, const float2 uv) {
	return VLOAD3F(&texture->constFloat4.color.r);
}

#endif

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

float ImageMapInstance_GetGrey(
	__global ImageMapInstanceParam *imageMapInstance, const float2 uv
	IMAGEMAPS_PARAM_DECL) {
	__global ImageMap *imageMap = &imageMapDescs[imageMapInstance->imageMapIndex];
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
			imageMap->pageIndex, imageMap->pixelsIndex
		);

	const float2 scale = VLOAD2F(&imageMapInstance->uScale);
	const float2 delta = VLOAD2F(&imageMapInstance->uDelta);
	const float2 mapUV = uv * scale + delta;

	return imageMapInstance->gain * ImageMap_GetGrey(
			pixels,
			imageMap->width, imageMap->height, imageMap->channelCount,
			mapUV.s0, mapUV.s1);
}

float3 ImageMapInstance_GetColor(
	__global ImageMapInstanceParam *imageMapInstance, const float2 uv
	IMAGEMAPS_PARAM_DECL) {
	__global ImageMap *imageMap = &imageMapDescs[imageMapInstance->imageMapIndex];
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
			imageMap->pageIndex, imageMap->pixelsIndex
		);

	const float2 scale = VLOAD2F(&imageMapInstance->uScale);
	const float2 delta = VLOAD2F(&imageMapInstance->uDelta);
	const float2 mapUV = uv * scale + delta;

	return imageMapInstance->gain * ImageMap_GetColor(
			pixels,
			imageMap->width, imageMap->height, imageMap->channelCount,
			mapUV.s0, mapUV.s1);
}

#endif

//------------------------------------------------------------------------------
// ImageMap texture
//------------------------------------------------------------------------------

#if defined (PARAM_ENABLE_TEX_IMAGEMAP)

float ImageMapTexture_GetGreyValue(__global Texture *texture, const float2 uv
	IMAGEMAPS_PARAM_DECL) {
	return ImageMapInstance_GetGrey(&texture->imageMapInstance, uv
			IMAGEMAPS_PARAM);
}

float3 ImageMapTexture_GetColorValue(__global Texture *texture, const float2 uv
	IMAGEMAPS_PARAM_DECL) {
	return ImageMapInstance_GetColor(&texture->imageMapInstance, uv
			IMAGEMAPS_PARAM);
}

#endif

//------------------------------------------------------------------------------
// Generic texture functions
//
// They include the support for all texture but Scale
// (because OpenCL doesn't support recursion)
//------------------------------------------------------------------------------

float Texture_GetGreyValueNoScale(__global Texture *texture, const float2 uv
		IMAGEMAPS_PARAM_DECL) {
	switch (texture->type) {
#if defined(PARAM_ENABLE_TEX_CONST_FLOAT)
		case CONST_FLOAT:
			return ConstFloatTexture_GetGreyValue(texture, uv);
#endif
#if defined(PARAM_ENABLE_TEX_CONST_FLOAT3)
		case CONST_FLOAT3:
			return ConstFloat3Texture_GetGreyValue(texture, uv);
#endif
#if defined(PARAM_ENABLE_TEX_CONST_FLOAT4)
		case CONST_FLOAT4:
			return ConstFloat4Texture_GetGreyValue(texture, uv);
#endif
#if defined(PARAM_ENABLE_TEX_IMAGEMAP)
		case IMAGEMAP:
			return ImageMapTexture_GetGreyValue(texture, uv
					IMAGEMAPS_PARAM);
#endif
		default:
			return 0.f;
	}
}

float3 Texture_GetColorValueNoScale(__global Texture *texture, const float2 uv
		IMAGEMAPS_PARAM_DECL) {
	switch (texture->type) {
#if defined(PARAM_ENABLE_TEX_CONST_FLOAT)
		case CONST_FLOAT:
			return ConstFloatTexture_GetColorValue(texture, uv);
#endif
#if defined(PARAM_ENABLE_TEX_CONST_FLOAT3)
		case CONST_FLOAT3:
			return ConstFloat3Texture_GetColorValue(texture, uv);
#endif
#if defined(PARAM_ENABLE_TEX_CONST_FLOAT4)
		case CONST_FLOAT4:
			return ConstFloat4Texture_GetColorValue(texture, uv);
#endif
#if defined(PARAM_ENABLE_TEX_IMAGEMAP)
		case IMAGEMAP:
			return ImageMapTexture_GetColorValue(texture, uv
					IMAGEMAPS_PARAM);
#endif
		default:
			return BLACK;
	}
}

float2 Texture_GetDuDvNoScale(__global Texture *texture) {
	switch (texture->type) {
#if defined(PARAM_ENABLE_TEX_IMAGEMAP)
		case IMAGEMAP:
			return VLOAD2F(&texture->imageMapInstance.Du);
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
		default:
			return 0.f;
	}
}

//------------------------------------------------------------------------------
// Scale texture
//------------------------------------------------------------------------------

#if defined (PARAM_ENABLE_TEX_SCALE)

float ScaleTexture_GetGreyValue(__global Texture *texture, const float2 uv
		TEXTURES_PARAM_DECL) {
	__global Texture *tex1 = &texs[texture->scaleTex.tex1Index];
	__global Texture *tex2 = &texs[texture->scaleTex.tex2Index];

	return Texture_GetGreyValueNoScale(tex1, uv
				IMAGEMAPS_PARAM) *
			Texture_GetGreyValueNoScale(tex2, uv
				IMAGEMAPS_PARAM);
}

float3 ScaleTexture_GetColorValue(__global Texture *texture, const float2 uv
		TEXTURES_PARAM_DECL) {
	__global Texture *tex1 = &texs[texture->scaleTex.tex1Index];
	__global Texture *tex2 = &texs[texture->scaleTex.tex2Index];

	return Texture_GetColorValueNoScale(tex1, uv
				IMAGEMAPS_PARAM) * 
			Texture_GetColorValueNoScale(tex2, uv
				IMAGEMAPS_PARAM);
}

float2 ScaleTexture_GetDuDv(__global Texture *texture
		TEXTURES_PARAM_DECL) {
	__global Texture *tex1 = &texs[texture->scaleTex.tex1Index];
	__global Texture *tex2 = &texs[texture->scaleTex.tex2Index];

	const float2 dudv1 = Texture_GetDuDvNoScale(tex1);
	const float2 dudv2 = Texture_GetDuDvNoScale(tex2);

	return (float2)(fmax(dudv1.x, dudv2.x), fmax(dudv1.y, dudv2.y));
}

#endif

//------------------------------------------------------------------------------
// Generic texture functions with Scale support
//------------------------------------------------------------------------------

float Texture_GetGreyValue(__global Texture *texture, const float2 uv
		TEXTURES_PARAM_DECL) {
#if defined(PARAM_ENABLE_TEX_SCALE)
	if (texture->type == SCALE_TEX)
		return ScaleTexture_GetGreyValue(texture, uv
				TEXTURES_PARAM);
	else
#endif
		return Texture_GetGreyValueNoScale(texture, uv
				IMAGEMAPS_PARAM);
}

float3 Texture_GetColorValue(__global Texture *texture, const float2 uv
		TEXTURES_PARAM_DECL) {
#if defined(PARAM_ENABLE_TEX_SCALE)
	if (texture->type == SCALE_TEX)
		return ScaleTexture_GetColorValue(texture, uv
				TEXTURES_PARAM);
	else
#endif
		return Texture_GetColorValueNoScale(texture, uv
				IMAGEMAPS_PARAM);
}

float2 Texture_GetDuDv(__global Texture *texture
		TEXTURES_PARAM_DECL) {
#if defined(PARAM_ENABLE_TEX_SCALE)
	if (texture->type == SCALE_TEX) {
		return ScaleTexture_GetDuDv(texture
				TEXTURES_PARAM);
	} else
#endif
		return Texture_GetDuDvNoScale(texture);
}
