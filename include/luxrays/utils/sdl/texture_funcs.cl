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

float3 ConstFloatTexture_GetColorValue(__global Texture *texture, const float2 uv) {
	return texture->constFloat.value;
}

//------------------------------------------------------------------------------
// ConstFloat3 texture
//------------------------------------------------------------------------------

float3 ConstFloat3Texture_GetColorValue(__global Texture *texture, const float2 uv) {
	return vload3(0, &texture->constFloat3.color.r);
}

//------------------------------------------------------------------------------
// ConstFloat4 texture
//------------------------------------------------------------------------------

float3 ConstFloat4Texture_GetColorValue(__global Texture *texture, const float2 uv) {
	return vload3(0, &texture->constFloat4.color.r);
}

//------------------------------------------------------------------------------
// ImageMap texture
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

float3 ImageMap_GetTexel_Color(__global float *pixels,
		const uint width, const uint height, const uint channelCount,
		const int s, const int t) {
	const uint u = Mod(s, width);
	const uint v = Mod(t, height);

	const uint index = channelCount * (v * width + u);

	return vload3(0, &pixels[index]);
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

float3 ImageMapInstance_GetColor(
	__global ImageMapInstanceParam *imageMapInstance,
	__global ImageMap *imageMapDescs,
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
	const float2 uv) {
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

	const float2 scale = vload2(0, &imageMapInstance->uScale);
	const float2 delta = vload2(0, &imageMapInstance->uDelta);
	const float2 mapUV = uv * scale + delta;

	return imageMapInstance->gain * ImageMap_GetColor(
			pixels,
			imageMap->width, imageMap->height, imageMap->channelCount,
			mapUV.s0, mapUV.s1);
}

#endif

//------------------------------------------------------------------------------

float3 Texture_GetColorValue(__global Texture *texture, const float2 uv) {
	switch (texture->type) {
		case CONST_FLOAT:
			return ConstFloatTexture_GetColorValue(texture, uv);
		case CONST_FLOAT3:
			return ConstFloat3Texture_GetColorValue(texture, uv);
		case CONST_FLOAT4:
			return ConstFloat4Texture_GetColorValue(texture, uv);
		default:
			return 0.f;
	}
}
