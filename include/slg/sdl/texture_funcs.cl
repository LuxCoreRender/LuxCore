#line 2 "texture_funcs.cl"

/***************************************************************************
 * Copyright 1998-2013 by authors (see AUTHORS.txt)                        *
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

#ifndef TEXTURE_STACK_SIZE
#define TEXTURE_STACK_SIZE 16
#endif

//------------------------------------------------------------------------------
// ImageMaps support
//------------------------------------------------------------------------------

#if defined(PARAM_HAS_IMAGEMAPS)

__global float *ImageMap_GetPixelsAddress(__global float **imageMapBuff,
		const uint page, const uint offset) {
	return &imageMapBuff[page][offset];
}

float ImageMap_GetTexel_Float(__global float *pixels,
		const uint width, const uint height, const uint channelCount,
		const int s, const int t) {
	const uint u = Mod(s, width);
	const uint v = Mod(t, height);

	const uint index = channelCount * (v * width + u);

	return (channelCount == 1) ? pixels[index] : Spectrum_Y(VLOAD3F(&pixels[index]));
}

float3 ImageMap_GetTexel_Spectrum(__global float *pixels,
		const uint width, const uint height, const uint channelCount,
		const int s, const int t) {
	const uint u = Mod(s, width);
	const uint v = Mod(t, height);

	const uint index = channelCount * (v * width + u);

	return (channelCount == 1) ? pixels[index] : VLOAD3F(&pixels[index]);
}

float ImageMap_GetFloat(__global float *pixels,
		const uint width, const uint height, const uint channelCount,
		const float u, const float v) {
	const float s = u * width - 0.5f;
	const float t = v * height - 0.5f;

	const int s0 = Floor2Int(s);
	const int t0 = Floor2Int(t);

	const float ds = s - s0;
	const float dt = t - t0;

	const float ids = 1.f - ds;
	const float idt = 1.f - dt;

	const float c0 = ImageMap_GetTexel_Float(pixels, width, height, channelCount, s0, t0);
	const float c1 = ImageMap_GetTexel_Float(pixels, width, height, channelCount, s0, t0 + 1);
	const float c2 = ImageMap_GetTexel_Float(pixels, width, height, channelCount, s0 + 1, t0);
	const float c3 = ImageMap_GetTexel_Float(pixels, width, height, channelCount, s0 + 1, t0 + 1);

	const float k0 = ids * idt;
	const float k1 = ids * dt;
	const float k2 = ds * idt;
	const float k3 = ds * dt;

	return (k0 * c0 + k1 *c1 + k2 * c2 + k3 * c3);
}

float3 ImageMap_GetSpectrum(__global float *pixels,
		const uint width, const uint height, const uint channelCount,
		const float u, const float v) {
	const float s = u * width - 0.5f;
	const float t = v * height - 0.5f;

	const int s0 = Floor2Int(s);
	const int t0 = Floor2Int(t);

	const float ds = s - s0;
	const float dt = t - t0;

	const float ids = 1.f - ds;
	const float idt = 1.f - dt;

	const float3 c0 = ImageMap_GetTexel_Spectrum(pixels, width, height, channelCount, s0, t0);
	const float3 c1 = ImageMap_GetTexel_Spectrum(pixels, width, height, channelCount, s0, t0 + 1);
	const float3 c2 = ImageMap_GetTexel_Spectrum(pixels, width, height, channelCount, s0 + 1, t0);
	const float3 c3 = ImageMap_GetTexel_Spectrum(pixels, width, height, channelCount, s0 + 1, t0 + 1);

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

void ConstFloatTexture_EvaluateFloat(__global Texture *texture, __global HitPoint *hitPoint,
		float texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
	texValues[(*texValuesSize)++] = texture->constFloat.value;
}

void ConstFloatTexture_EvaluateSpectrum(__global Texture *texture, __global HitPoint *hitPoint,
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

void ConstFloat3Texture_EvaluateFloat(__global Texture *texture, __global HitPoint *hitPoint,
		float texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
	texValues[(*texValuesSize)++] = Spectrum_Y(VLOAD3F(texture->constFloat3.color.c));
}

void ConstFloat3Texture_EvaluateSpectrum(__global Texture *texture, __global HitPoint *hitPoint,
		float3 texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
	texValues[(*texValuesSize)++] = VLOAD3F(texture->constFloat3.color.c);
}

void ConstFloat3Texture_EvaluateDuDv(__global Texture *texture, __global HitPoint *hitPoint,
		float2 texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
	texValues[(*texValuesSize)++] = 0.f;
}

#endif

//------------------------------------------------------------------------------
// ImageMap texture
//------------------------------------------------------------------------------

#if defined (PARAM_ENABLE_TEX_IMAGEMAP)

void ImageMapTexture_EvaluateFloat(__global Texture *texture, __global HitPoint *hitPoint,
		float texValues[TEXTURE_STACK_SIZE], uint *texValuesSize
		IMAGEMAPS_PARAM_DECL) {
	__global ImageMap *imageMap = &imageMapDescs[texture->imageMapTex.imageMapIndex];
	__global float *pixels = ImageMap_GetPixelsAddress(
			imageMapBuff, imageMap->pageIndex, imageMap->pixelsIndex);

	const float2 uv = VLOAD2F(&hitPoint->uv.u);
	const float2 mapUV = TextureMapping2D_Map(&texture->imageMapTex.mapping, hitPoint);

	texValues[(*texValuesSize)++] = texture->imageMapTex.gain * ImageMap_GetFloat(
			pixels,
			imageMap->width, imageMap->height, imageMap->channelCount,
			mapUV.s0, mapUV.s1);
}

void ImageMapTexture_EvaluateSpectrum(__global Texture *texture, __global HitPoint *hitPoint,
		float3 texValues[TEXTURE_STACK_SIZE], uint *texValuesSize
		IMAGEMAPS_PARAM_DECL) {
	__global ImageMap *imageMap = &imageMapDescs[texture->imageMapTex.imageMapIndex];
	__global float *pixels = ImageMap_GetPixelsAddress(
			imageMapBuff, imageMap->pageIndex, imageMap->pixelsIndex);

	const float2 uv = VLOAD2F(&hitPoint->uv.u);
	const float2 mapUV = TextureMapping2D_Map(&texture->imageMapTex.mapping, hitPoint);

	texValues[(*texValuesSize)++] = texture->imageMapTex.gain * ImageMap_GetSpectrum(
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

void ScaleTexture_EvaluateFloat(__global Texture *texture, __global HitPoint *hitPoint,
		float texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
	const float value = texValues[--(*texValuesSize)] * texValues[--(*texValuesSize)];

	texValues[(*texValuesSize)++] = value;
}

void ScaleTexture_EvaluateSpectrum(__global Texture *texture, __global HitPoint *hitPoint,
		float3 texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
	const float3 value = texValues[--(*texValuesSize)] * texValues[--(*texValuesSize)];

	texValues[(*texValuesSize)++] = value;
}

void ScaleTexture_EvaluateDuDv(__global Texture *texture, __global HitPoint *hitPoint,
		float2 texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
	const float2 dudv1 = texValues[--(*texValuesSize)];
	const float2 dudv2 = texValues[--(*texValuesSize)];

	texValues[(*texValuesSize)++] = fmax(dudv1, dudv2);
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

void FresnelApproxNTexture_EvaluateFloat(__global Texture *texture, __global HitPoint *hitPoint,
		float texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
	const float value = texValues[--(*texValuesSize)];

	texValues[(*texValuesSize)++] = FresnelApproxN(value);
}

void FresnelApproxNTexture_EvaluateSpectrum(__global Texture *texture, __global HitPoint *hitPoint,
		float3 texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
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

void FresnelApproxKTexture_EvaluateFloat(__global Texture *texture, __global HitPoint *hitPoint,
		float texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
	const float value = texValues[--(*texValuesSize)];

	texValues[(*texValuesSize)++] = FresnelApproxK(value);
}

void FresnelApproxKTexture_EvaluateSpectrum(__global Texture *texture, __global HitPoint *hitPoint,
		float3 texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
	const float3 value = texValues[--(*texValuesSize)];

	texValues[(*texValuesSize)++] = FresnelApproxK3(value);
}

void FresnelApproxKTexture_EvaluateDuDv(__global Texture *texture, __global HitPoint *hitPoint,
		float2 texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
	texValues[(*texValuesSize)++] = texValues[--(*texValuesSize)];
}

#endif

//------------------------------------------------------------------------------
// CheckerBoard 2D & 3D texture
//------------------------------------------------------------------------------

#if defined (PARAM_ENABLE_CHECKERBOARD2D)

void CheckerBoard2DTexture_EvaluateFloat(__global Texture *texture, __global HitPoint *hitPoint,
		float texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
	const float value1 = texValues[--(*texValuesSize)];
	const float value2 = texValues[--(*texValuesSize)];

	const float2 uv = VLOAD2F(&hitPoint->uv.u);
	const float2 mapUV = TextureMapping2D_Map(&texture->checkerBoard2D.mapping, hitPoint);

	texValues[(*texValuesSize)++] = ((Floor2Int(mapUV.s0) + Floor2Int(mapUV.s1)) % 2 == 0) ? value1 : value2;
}

void CheckerBoard2DTexture_EvaluateSpectrum(__global Texture *texture, __global HitPoint *hitPoint,
		float3 texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
	const float3 value1 = texValues[--(*texValuesSize)];
	const float3 value2 = texValues[--(*texValuesSize)];

	const float2 uv = VLOAD2F(&hitPoint->uv.u);
	const float2 mapUV = TextureMapping2D_Map(&texture->checkerBoard2D.mapping, hitPoint);

	texValues[(*texValuesSize)++] = ((Floor2Int(mapUV.s0) + Floor2Int(mapUV.s1)) % 2 == 0) ? value1 : value2;
}

void CheckerBoard2DTexture_EvaluateDuDv(__global Texture *texture, __global HitPoint *hitPoint,
		float2 texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
	const float2 dudv1 = texValues[--(*texValuesSize)];
	const float2 dudv2 = texValues[--(*texValuesSize)];

	texValues[(*texValuesSize)++] = fmax(dudv1, dudv2);
}

#endif

#if defined (PARAM_ENABLE_CHECKERBOARD3D)

void CheckerBoard3DTexture_EvaluateFloat(__global Texture *texture, __global HitPoint *hitPoint,
		float texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
	const float value1 = texValues[--(*texValuesSize)];
	const float value2 = texValues[--(*texValuesSize)];

	const float3 mapP = TextureMapping3D_Map(&texture->checkerBoard3D.mapping, hitPoint);

	texValues[(*texValuesSize)++] = ((Floor2Int(mapP.x) + Floor2Int(mapP.y) + Floor2Int(mapP.z)) % 2 == 0) ? value1 : value2;
}

void CheckerBoard3DTexture_EvaluateSpectrum(__global Texture *texture, __global HitPoint *hitPoint,
		float3 texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
	const float3 value1 = texValues[--(*texValuesSize)];
	const float3 value2 = texValues[--(*texValuesSize)];

	const float3 mapP = TextureMapping3D_Map(&texture->checkerBoard3D.mapping, hitPoint);

	texValues[(*texValuesSize)++] = ((Floor2Int(mapP.x) + Floor2Int(mapP.y) + Floor2Int(mapP.z)) % 2 == 0) ? value1 : value2;
}

void CheckerBoard3DTexture_EvaluateDuDv(__global Texture *texture, __global HitPoint *hitPoint,
		float2 texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
	const float2 dudv1 = texValues[--(*texValuesSize)];
	const float2 dudv2 = texValues[--(*texValuesSize)];

	texValues[(*texValuesSize)++] = fmax(dudv1, dudv2);
}

#endif

//------------------------------------------------------------------------------
// Mix texture
//------------------------------------------------------------------------------

#if defined (PARAM_ENABLE_TEX_MIX)

void MixTexture_EvaluateFloat(__global Texture *texture, __global HitPoint *hitPoint,
		float texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
	const float amt = clamp(texValues[--(*texValuesSize)], 0.f, 1.f);;
	const float value1 = texValues[--(*texValuesSize)];
	const float value2 = texValues[--(*texValuesSize)];

	texValues[(*texValuesSize)++] = Lerp(amt, value1, value2);
}

void MixTexture_EvaluateSpectrum(__global Texture *texture, __global HitPoint *hitPoint,
		float3 texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
	const float3 amt = clamp(texValues[--(*texValuesSize)], 0.f, 1.f);
	const float3 value1 = texValues[--(*texValuesSize)];
	const float3 value2 = texValues[--(*texValuesSize)];

	texValues[(*texValuesSize)++] = mix(value1, value2, amt);
}

void MixTexture_EvaluateDuDv(__global Texture *texture, __global HitPoint *hitPoint,
		float2 texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
	const float2 dudv1 = texValues[--(*texValuesSize)];
	const float2 dudv2 = texValues[--(*texValuesSize)];
	const float2 dudv3 = texValues[--(*texValuesSize)];

	texValues[(*texValuesSize)++] = fmax(fmax(dudv1, dudv2), dudv3);
}

#endif

//------------------------------------------------------------------------------
// FBM texture
//------------------------------------------------------------------------------

#if defined (PARAM_ENABLE_FBM_TEX)

void FBMTexture_EvaluateFloat(__global Texture *texture, __global HitPoint *hitPoint,
		float texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
	const float3 mapP = TextureMapping3D_Map(&texture->fbm.mapping, hitPoint);

	texValues[(*texValuesSize)++] = FBm(mapP, texture->fbm.omega, texture->fbm.octaves);
}

void FBMTexture_EvaluateSpectrum(__global Texture *texture, __global HitPoint *hitPoint,
		float3 texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
	const float3 mapP = TextureMapping3D_Map(&texture->fbm.mapping, hitPoint);

	texValues[(*texValuesSize)++] = FBm(mapP, texture->fbm.omega, texture->fbm.octaves);
}

void FBMTexture_EvaluateDuDv(__global Texture *texture, __global HitPoint *hitPoint,
		float2 texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
	texValues[(*texValuesSize)++] = (float2)(DUDV_VALUE, DUDV_VALUE);
}

#endif

//------------------------------------------------------------------------------
// Marble texture
//------------------------------------------------------------------------------

#if defined (PARAM_ENABLE_MARBLE)

// Evaluate marble spline at _t_
__constant float MarbleTexture_c[9][3] = {
	{ .58f, .58f, .6f},
	{ .58f, .58f, .6f},
	{ .58f, .58f, .6f},
	{ .5f, .5f, .5f},
	{ .6f, .59f, .58f},
	{ .58f, .58f, .6f},
	{ .58f, .58f, .6f},
	{.2f, .2f, .33f},
	{ .58f, .58f, .6f}
};

float3 MarbleTexture_Evaluate(__global Texture *texture, __global HitPoint *hitPoint) {
	const float3 P = texture->marble.scale * TextureMapping3D_Map(&texture->marble.mapping, hitPoint);

	float marble = P.y + texture->marble.variation * FBm(P, texture->marble.omega, texture->marble.octaves);
	float t = .5f + .5f * sin(marble);
#define NC  sizeof(MarbleTexture_c) / sizeof(MarbleTexture_c[0])
#define NSEG (NC-3)
	const int first = Floor2Int(t * NSEG);
	t = (t * NSEG - first);
#undef NC
#undef NSEG
#define ASSIGN_CF3(a) (float3)(a[0], a[1], a[2])
	const float3 c0 = ASSIGN_CF3(MarbleTexture_c[first]);
	const float3 c1 = ASSIGN_CF3(MarbleTexture_c[first + 1]);
	const float3 c2 = ASSIGN_CF3(MarbleTexture_c[first + 2]);
	const float3 c3 = ASSIGN_CF3(MarbleTexture_c[first + 3]);
#undef ASSIGN_CF3
	// Bezier spline evaluated with de Castilejau's algorithm	
	float3 s0 = mix(c0, c1, t);
	float3 s1 = mix(c1, c2, t);
	float3 s2 = mix(c2, c3, t);
	s0 = mix(s0, s1, t);
	s1 = mix(s1, s2, t);
	// Extra scale of 1.5 to increase variation among colors
	return 1.5f * mix(s0, s1, t);
}

void MarbleTexture_EvaluateFloat(__global Texture *texture, __global HitPoint *hitPoint,
		float texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
	texValues[(*texValuesSize)++] = Spectrum_Y(MarbleTexture_Evaluate(texture, hitPoint));
}

void MarbleTexture_EvaluateSpectrum(__global Texture *texture, __global HitPoint *hitPoint,
		float3 texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
	texValues[(*texValuesSize)++] = MarbleTexture_Evaluate(texture, hitPoint);
}

void MarbleTexture_EvaluateDuDv(__global Texture *texture, __global HitPoint *hitPoint,
		float2 texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
	texValues[(*texValuesSize)++] = (float2)(DUDV_VALUE, DUDV_VALUE);
}

#endif

//------------------------------------------------------------------------------
// Dots texture
//------------------------------------------------------------------------------

#if defined (PARAM_ENABLE_DOTS)

void DotsTexture_EvaluateFloat(__global Texture *texture, __global HitPoint *hitPoint,
		float texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
	const float value1 = texValues[--(*texValuesSize)];
	const float value2 = texValues[--(*texValuesSize)];

	const float2 uv = TextureMapping2D_Map(&texture->dots.mapping, hitPoint);

	const int sCell = Floor2Int(uv.s0 + .5f);
	const int tCell = Floor2Int(uv.s1 + .5f);
	// Return _insideDot_ result if point is inside dot
	if (Noise(sCell + .5f, tCell + .5f, .5f) > 0.f) {
		const float radius = .35f;
		const float maxShift = 0.5f - radius;
		const float sCenter = sCell + maxShift *
			Noise(sCell + 1.5f, tCell + 2.8f, .5f);
		const float tCenter = tCell + maxShift *
			Noise(sCell + 4.5f, tCell + 9.8f, .5f);
		const float ds = uv.s0 - sCenter, dt = uv.s1 - tCenter;
		if (ds * ds + dt * dt < radius * radius) {
			texValues[(*texValuesSize)++] = value1;
			return;
		}
	}

	texValues[(*texValuesSize)++] = value2;
}

void DotsTexture_EvaluateSpectrum(__global Texture *texture, __global HitPoint *hitPoint,
		float3 texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
	const float3 value1 = texValues[--(*texValuesSize)];
	const float3 value2 = texValues[--(*texValuesSize)];

	const float2 uv = TextureMapping2D_Map(&texture->dots.mapping, hitPoint);

	const int sCell = Floor2Int(uv.s0 + .5f);
	const int tCell = Floor2Int(uv.s1 + .5f);
	// Return _insideDot_ result if point is inside dot
	if (Noise(sCell + .5f, tCell + .5f, .5f) > 0.f) {
		const float radius = .35f;
		const float maxShift = 0.5f - radius;
		const float sCenter = sCell + maxShift *
			Noise(sCell + 1.5f, tCell + 2.8f, .5f);
		const float tCenter = tCell + maxShift *
			Noise(sCell + 4.5f, tCell + 9.8f, .5f);
		const float ds = uv.s0 - sCenter, dt = uv.s1 - tCenter;
		if (ds * ds + dt * dt < radius * radius) {
			texValues[(*texValuesSize)++] = value1;
			return;
		}
	}

	texValues[(*texValuesSize)++] = value2;
}

void DotsTexture_EvaluateDuDv(__global Texture *texture, __global HitPoint *hitPoint,
		float2 texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
	const float2 dudv1 = texValues[--(*texValuesSize)];
	const float2 dudv2 = texValues[--(*texValuesSize)];

	texValues[(*texValuesSize)++] = fmax(dudv1, dudv2);
}

#endif

//------------------------------------------------------------------------------
// Brick texture
//------------------------------------------------------------------------------

#if defined (PARAM_ENABLE_BRICK)

bool BrickTexture_RunningAlternate(__global Texture *texture, const float3 p, float3 *i, float3 *b,
		int nWhole) {
	const float run = texture->brick.run;
	const float mortarwidth = texture->brick.mortarwidth;
	const float mortarheight = texture->brick.mortarheight;
	const float mortardepth = texture->brick.mortardepth;

	const float sub = nWhole + 0.5f;
	const float rsub = ceil(sub);
	(*i).z = floor(p.z);
	(*b).x = (p.x + (*i).z * run) / sub;
	(*b).y = (p.y + (*i).z * run) / sub;
	(*i).x = floor((*b).x);
	(*i).y = floor((*b).y);
	(*b).x = ((*b).x - (*i).x) * sub;
	(*b).y = ((*b).y - (*i).y) * sub;
	(*b).z = (p.z - (*i).z) * sub;
	(*i).x += floor((*b).x) / rsub;
	(*i).y += floor((*b).y) / rsub;
	(*b).x -= floor((*b).x);
	(*b).y -= floor((*b).y);
	return (*b).z > mortarheight && (*b).y > mortardepth &&
		(*b).x > mortarwidth;
}

bool BrickTexture_Basket(__global Texture *texture, const float3 p, float3 *i) {
	const float mortarwidth = texture->brick.mortarwidth;
	const float mortardepth = texture->brick.mortardepth;
	const float proportion = texture->brick.proportion;
	const float invproportion = texture->brick.invproportion;

	(*i).x = floor(p.x);
	(*i).y = floor(p.y);
	float bx = p.x - (*i).x;
	float by = p.y - (*i).y;
	(*i).x += (*i).y - 2.f * floor(0.5f * (*i).y);
	const bool split = ((*i).x - 2.f * floor(0.5f * (*i).x)) < 1.f;
	if (split) {
		bx = fmod(bx, invproportion);
		(*i).x = floor(proportion * p.x) * invproportion;
	} else {
		by = fmod(by, invproportion);
		(*i).y = floor(proportion * p.y) * invproportion;
	}
	return by > mortardepth && bx > mortarwidth;
}

bool BrickTexture_Herringbone(__global Texture *texture, const float3 p, float3 *i) {
	const float mortarwidth = texture->brick.mortarwidth;
	const float mortarheight = texture->brick.mortarheight;
	const float proportion = texture->brick.proportion;
	const float invproportion = texture->brick.invproportion;

	(*i).y = floor(proportion * p.y);
	const float px = p.x + (*i).y * invproportion;
	(*i).x = floor(px);
	float bx = 0.5f * px - floor(px * 0.5f);
	bx *= 2.f;
	float by = proportion * p.y - floor(proportion * p.y);
	by *= invproportion;
	if (bx > 1.f + invproportion) {
		bx = proportion * (bx - 1.f);
		(*i).y -= floor(bx - 1.f);
		bx -= floor(bx);
		bx *= invproportion;
		by = 1.f;
	} else if (bx > 1.f) {
		bx = proportion * (bx - 1.f);
		(*i).y -= floor(bx - 1.f);
		bx -= floor(bx);
		bx *= invproportion;
	}
	return by > mortarheight && bx > mortarwidth;
}

bool BrickTexture_Running(__global Texture *texture, const float3 p, float3 *i, float3 *b) {
	const float run = texture->brick.run;
	const float mortarwidth = texture->brick.mortarwidth;
	const float mortarheight = texture->brick.mortarheight;
	const float mortardepth = texture->brick.mortardepth;

	(*i).z = floor(p.z);
	(*b).x = p.x + (*i).z * run;
	(*b).y = p.y - (*i).z * run;
	(*i).x = floor((*b).x);
	(*i).y = floor((*b).y);
	(*b).z = p.z - (*i).z;
	(*b).x -= (*i).x;
	(*b).y -= (*i).y;
	return (*b).z > mortarheight && (*b).y > mortardepth &&
		(*b).x > mortarwidth;
}

bool BrickTexture_English(__global Texture *texture, const float3 p, float3 *i, float3 *b) {
	const float run = texture->brick.run;
	const float mortarwidth = texture->brick.mortarwidth;
	const float mortarheight = texture->brick.mortarheight;
	const float mortardepth = texture->brick.mortardepth;

	(*i).z = floor(p.z);
	(*b).x = p.x + (*i).z * run;
	(*b).y = p.y - (*i).z * run;
	(*i).x = floor((*b).x);
	(*i).y = floor((*b).y);
	(*b).z = p.z - (*i).z;
	const float divider = floor(fmod(fabs((*i).z), 2.f)) + 1.f;
	(*b).x = (divider * (*b).x - floor(divider * (*b).x)) / divider;
	(*b).y = (divider * (*b).y - floor(divider * (*b).y)) / divider;
	return (*b).z > mortarheight && (*b).y > mortardepth &&
		(*b).x > mortarwidth;
}

bool BrickTexture_Evaluate(__global Texture *texture, __global HitPoint *hitPoint) {
#define BRICK_EPSILON 1e-3f
	const float3 P = TextureMapping3D_Map(&texture->brick.mapping, hitPoint);

	const float offs = BRICK_EPSILON + texture->brick.mortarsize;
	float3 bP = P + (float3)(offs, offs, offs);

	// Normalize coordinates according brick dimensions
	bP.x /= texture->brick.brickwidth;
	bP.y /= texture->brick.brickdepth;
	bP.z /= texture->brick.brickheight;

	bP += VLOAD3F(&texture->brick.offsetx);

	float3 brickIndex;
	float3 bevel;
	bool b;
	switch (texture->brick.bond) {
		case FLEMISH:
			b = BrickTexture_RunningAlternate(texture, bP, &brickIndex, &bevel, 1);
			break;
		case RUNNING:
			b = BrickTexture_Running(texture, bP, &brickIndex, &bevel);
			break;
		case ENGLISH:
			b = BrickTexture_English(texture, bP, &brickIndex, &bevel);
			break;
		case HERRINGBONE:
			b = BrickTexture_Herringbone(texture, bP, &brickIndex);
			break;
		case BASKET:
			b = BrickTexture_Basket(texture, bP, &brickIndex);
			break;
		case KETTING:
			b = BrickTexture_RunningAlternate(texture, bP, &brickIndex, &bevel, 2);
			break; 
		default:
			b = true;
			break;
	}

	return b;
#undef BRICK_EPSILON
}

void BrickTexture_EvaluateFloat(__global Texture *texture, __global HitPoint *hitPoint,
		float texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
	const float value1 = texValues[--(*texValuesSize)];
	const float value2 = texValues[--(*texValuesSize)];
	const float value3 = texValues[--(*texValuesSize)];

	if (BrickTexture_Evaluate(texture, hitPoint))
		texValues[(*texValuesSize)++] = value1 * value3;
	else
		texValues[(*texValuesSize)++] = value2;
}

void BrickTexture_EvaluateSpectrum(__global Texture *texture, __global HitPoint *hitPoint,
		float3 texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
	const float3 value1 = texValues[--(*texValuesSize)];
	const float3 value2 = texValues[--(*texValuesSize)];
	const float3 value3 = texValues[--(*texValuesSize)];

	if (BrickTexture_Evaluate(texture, hitPoint))
		texValues[(*texValuesSize)++] = value1 * value3;
	else
		texValues[(*texValuesSize)++] = value2;
}

void BrickTexture_EvaluateDuDv(__global Texture *texture, __global HitPoint *hitPoint,
		float2 texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
	const float2 dudv1 = texValues[--(*texValuesSize)];
	const float2 dudv2 = texValues[--(*texValuesSize)];
	const float2 dudv3 = texValues[--(*texValuesSize)];

	texValues[(*texValuesSize)++] = fmax(fmax(dudv1, dudv2), dudv3);
}

#endif

//------------------------------------------------------------------------------
// Add texture
//------------------------------------------------------------------------------

#if defined (PARAM_ENABLE_TEX_ADD)

void AddTexture_EvaluateFloat(__global Texture *texture, __global HitPoint *hitPoint,
		float texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
	const float value = texValues[--(*texValuesSize)] + texValues[--(*texValuesSize)];

	texValues[(*texValuesSize)++] = value;
}

void AddTexture_EvaluateSpectrum(__global Texture *texture, __global HitPoint *hitPoint,
		float3 texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
	const float3 value = texValues[--(*texValuesSize)] + texValues[--(*texValuesSize)];

	texValues[(*texValuesSize)++] = value;
}

void AddTexture_EvaluateDuDv(__global Texture *texture, __global HitPoint *hitPoint,
		float2 texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
	const float2 dudv1 = texValues[--(*texValuesSize)];
	const float2 dudv2 = texValues[--(*texValuesSize)];

	texValues[(*texValuesSize)++] = fmax(dudv1, dudv2);
}

#endif

//------------------------------------------------------------------------------
// Windy texture
//------------------------------------------------------------------------------

#if defined (PARAM_ENABLE_WINDY)

void WindyTexture_EvaluateFloat(__global Texture *texture, __global HitPoint *hitPoint,
		float texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
	const float3 mapP = TextureMapping3D_Map(&texture->windy.mapping, hitPoint);

	const float windStrength = FBm(.1f * mapP, .5f, 3);
	const float waveHeight = FBm(mapP, .5f, 6);

	texValues[(*texValuesSize)++] = fabs(windStrength) * waveHeight;
}

void WindyTexture_EvaluateSpectrum(__global Texture *texture, __global HitPoint *hitPoint,
		float3 texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
	const float3 mapP = TextureMapping3D_Map(&texture->windy.mapping, hitPoint);

	const float windStrength = FBm(.1f * mapP, .5f, 3);
	const float waveHeight = FBm(mapP, .5f, 6);

	texValues[(*texValuesSize)++] = fabs(windStrength) * waveHeight;
}

void WindyTexture_EvaluateDuDv(__global Texture *texture, __global HitPoint *hitPoint,
		float2 texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
	texValues[(*texValuesSize)++] = (float2)(DUDV_VALUE, DUDV_VALUE);
}

#endif

//------------------------------------------------------------------------------
// Wrinkled texture
//------------------------------------------------------------------------------

#if defined (PARAM_ENABLE_WRINKLED)

void WrinkledTexture_EvaluateFloat(__global Texture *texture, __global HitPoint *hitPoint,
		float texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
	const float3 mapP = TextureMapping3D_Map(&texture->wrinkled.mapping, hitPoint);

	texValues[(*texValuesSize)++] = Turbulence(mapP, texture->wrinkled.omega, texture->wrinkled.octaves);
}

void WrinkledTexture_EvaluateSpectrum(__global Texture *texture, __global HitPoint *hitPoint,
		float3 texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
	const float3 mapP = TextureMapping3D_Map(&texture->wrinkled.mapping, hitPoint);

	texValues[(*texValuesSize)++] = Turbulence(mapP, texture->wrinkled.omega, texture->wrinkled.octaves);
}

void WrinkledTexture_EvaluateDuDv(__global Texture *texture, __global HitPoint *hitPoint,
		float2 texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
	texValues[(*texValuesSize)++] = (float2)(DUDV_VALUE, DUDV_VALUE);
}

#endif

//------------------------------------------------------------------------------
// UV texture
//------------------------------------------------------------------------------

#if defined (PARAM_ENABLE_TEX_UV)

void UVTexture_EvaluateFloat(__global Texture *texture, __global HitPoint *hitPoint,
		float texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
	const float2 uv = TextureMapping2D_Map(&texture->uvTex.mapping, hitPoint);

	texValues[(*texValuesSize)++] = Spectrum_Y((float3)(uv.s0 - Floor2Int(uv.s0), uv.s1 - Floor2Int(uv.s1), 0.f));
}

void UVTexture_EvaluateSpectrum(__global Texture *texture, __global HitPoint *hitPoint,
		float3 texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
	const float2 uv = TextureMapping2D_Map(&texture->uvTex.mapping, hitPoint);

	texValues[(*texValuesSize)++] = (float3)(uv.s0 - Floor2Int(uv.s0), uv.s1 - Floor2Int(uv.s1), 0.f);
}

void UVTexture_EvaluateDuDv(__global Texture *texture, __global HitPoint *hitPoint,
		float2 texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
	texValues[(*texValuesSize)++] = (float2)(DUDV_VALUE, DUDV_VALUE);
}

#endif

//------------------------------------------------------------------------------
// Band texture
//------------------------------------------------------------------------------

#if defined (PARAM_ENABLE_TEX_BAND)

void BandTexture_EvaluateFloat(__global Texture *texture, __global HitPoint *hitPoint,
		float texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
	const float a = clamp(texValues[--(*texValuesSize)], 0.f, 1.f);

	const uint last = texture->band.size - 1;
	if (a < texture->band.offsets[0])
		texValues[(*texValuesSize)++] = Spectrum_Y(VLOAD3F(texture->band.values[0].c));
	else if (a >= texture->band.offsets[last])
		texValues[(*texValuesSize)++] = Spectrum_Y(VLOAD3F(texture->band.values[last].c));
	else {
		uint p = 0;
		for (; p <= last; ++p) {
			if (a < texture->band.offsets[p])
				break;
		}

		const float p1 = Spectrum_Y(VLOAD3F(texture->band.values[p - 1].c));
		const float p0 = Spectrum_Y(VLOAD3F(texture->band.values[p].c));
		const float o1 = texture->band.offsets[p - 1];
		const float o0 = texture->band.offsets[p];
		texValues[(*texValuesSize)++] = Lerp((a - o1) / (o0 - o1), p1, p0);
	}
}

void BandTexture_EvaluateSpectrum(__global Texture *texture, __global HitPoint *hitPoint,
		float3 texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
	const float a = clamp(Spectrum_Y(texValues[--(*texValuesSize)]), 0.f, 1.f);

	const uint last = texture->band.size - 1;
	if (a < texture->band.offsets[0])
		texValues[(*texValuesSize)++] = VLOAD3F(texture->band.values[0].c);
	else if (a >= texture->band.offsets[last])
		texValues[(*texValuesSize)++] = VLOAD3F(texture->band.values[last].c);
	else {
		uint p = 0;
		for (; p <= last; ++p) {
			if (a < texture->band.offsets[p])
				break;
		}

		const float3 p1 = VLOAD3F(texture->band.values[p - 1].c);
		const float3 p0 = VLOAD3F(texture->band.values[p].c);
		const float o1 = texture->band.offsets[p - 1];
		const float o0 = texture->band.offsets[p];
		texValues[(*texValuesSize)++] = Lerp3((a - o1) / (o0 - o1), p1, p0);
	}
}

void BandTexture_EvaluateDuDv(__global Texture *texture, __global HitPoint *hitPoint,
		float2 texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
	// Nothing to do:
	//const float2 dudv = texValues[--(*texValuesSize)];
	//texValues[(*texValuesSize)++] = dudv;
}

#endif

//------------------------------------------------------------------------------
// HitPointColor texture
//------------------------------------------------------------------------------

#if defined (PARAM_ENABLE_TEX_HITPOINTCOLOR)

void HitPointColorTexture_EvaluateFloat(__global Texture *texture, __global HitPoint *hitPoint,
		float texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
	texValues[(*texValuesSize)++] = Spectrum_Y(VLOAD3F(hitPoint->color.c));
}

void HitPointColorTexture_EvaluateSpectrum(__global Texture *texture, __global HitPoint *hitPoint,
		float3 texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
	texValues[(*texValuesSize)++] = VLOAD3F(hitPoint->color.c);
}

void HitPointColorTexture_EvaluateDuDv(__global Texture *texture, __global HitPoint *hitPoint,
		float2 texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
	texValues[(*texValuesSize)++] = 0.f;
}

#endif

//------------------------------------------------------------------------------
// HitPointAlpha texture
//------------------------------------------------------------------------------

#if defined (PARAM_ENABLE_TEX_HITPOINTALPHA)

void HitPointAlphaTexture_EvaluateFloat(__global Texture *texture, __global HitPoint *hitPoint,
		float texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
	texValues[(*texValuesSize)++] = hitPoint->alpha;
}

void HitPointAlphaTexture_EvaluateSpectrum(__global Texture *texture, __global HitPoint *hitPoint,
		float3 texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
	const float alpha = hitPoint->alpha;
	texValues[(*texValuesSize)++] = (float3)(alpha, alpha, alpha);
}

void HitPointAlphaTexture_EvaluateDuDv(__global Texture *texture, __global HitPoint *hitPoint,
		float2 texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
	texValues[(*texValuesSize)++] = 0.f;
}

#endif

//------------------------------------------------------------------------------
// HitPointGrey texture
//------------------------------------------------------------------------------

#if defined (PARAM_ENABLE_TEX_HITPOINTGREY)

void HitPointGreyTexture_EvaluateFloat(__global Texture *texture, __global HitPoint *hitPoint,
		float texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
	const uint channel = texture->hitPointGrey.channel;
	switch (channel) {
		case 0:
			texValues[*texValuesSize] = hitPoint->color.c[0];
			break;
		case 1:
			texValues[*texValuesSize] = hitPoint->color.c[1];
			break;
		case 2:
			texValues[*texValuesSize] = hitPoint->color.c[2];
			break;
		default:
			texValues[*texValuesSize] = Spectrum_Y(VLOAD3F(hitPoint->color.c));
			break;
	}

	++(*texValuesSize);
}

void HitPointGreyTexture_EvaluateSpectrum(__global Texture *texture, __global HitPoint *hitPoint,
		float3 texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
	const uint channel = texture->hitPointGrey.channel;
	float v;
	switch (channel) {
		case 0:
			v = hitPoint->color.c[0];
			break;
		case 1:
			v = hitPoint->color.c[1];
			break;
		case 2:
			v = hitPoint->color.c[2];
			break;
		default:
			v = Spectrum_Y(VLOAD3F(hitPoint->color.c));
			break;
	}

	texValues[(*texValuesSize)++] = (float3)(v, v, v);
}

void HitPointGreyTexture_EvaluateDuDv(__global Texture *texture, __global HitPoint *hitPoint,
		float2 texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
	texValues[(*texValuesSize)++] = 0.f;
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
		case FRESNEL_APPROX_N:
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
#if defined(PARAM_ENABLE_CHECKERBOARD3D)
		case CHECKERBOARD3D:
			todoTex[(*todoTexSize)++] = &texs[texture->checkerBoard3D.tex1Index];
			todoTex[(*todoTexSize)++] = &texs[texture->checkerBoard3D.tex2Index];
			return 2;
#endif
#if defined (PARAM_ENABLE_TEX_MIX)
		case MIX_TEX:
			todoTex[(*todoTexSize)++] = &texs[texture->mixTex.amountTexIndex];
			todoTex[(*todoTexSize)++] = &texs[texture->mixTex.tex1Index];
			todoTex[(*todoTexSize)++] = &texs[texture->mixTex.tex2Index];
			return 3;
#endif
#if defined(PARAM_ENABLE_DOTS)
		case DOTS:
			todoTex[(*todoTexSize)++] = &texs[texture->dots.insideIndex];
			todoTex[(*todoTexSize)++] = &texs[texture->dots.outsideIndex];
			return 2;
#endif
#if defined(PARAM_ENABLE_BRICK)
		case BRICK:
			todoTex[(*todoTexSize)++] = &texs[texture->brick.tex1Index];
			todoTex[(*todoTexSize)++] = &texs[texture->brick.tex2Index];
			todoTex[(*todoTexSize)++] = &texs[texture->brick.tex3Index];
			return 3;
#endif
#if defined(PARAM_ENABLE_TEX_ADD)
		case ADD_TEX:
			todoTex[(*todoTexSize)++] = &texs[texture->addTex.tex1Index];
			todoTex[(*todoTexSize)++] = &texs[texture->addTex.tex2Index];
			return 2;
#endif
#if defined (PARAM_ENABLE_TEX_BAND)
		case BAND_TEX:
			todoTex[(*todoTexSize)++] = &texs[texture->band.amountTexIndex];
			return 1;
#endif
#if defined (PARAM_ENABLE_TEX_HITPOINTGREY)
		case HITPOINTGREY:
#endif
#if defined (PARAM_ENABLE_TEX_HITPOINTALPHA)
		case HITPOINTALPHA:
#endif
#if defined (PARAM_ENABLE_TEX_HITPOINTCOLOR)
		case HITPOINTCOLOR:
#endif
#if defined (PARAM_ENABLE_TEX_UV)
		case UV_TEX:
#endif
#if defined (PARAM_ENABLE_WRINKLED)
		case WRINKLED:
#endif
#if defined (PARAM_ENABLE_WOOD)
		case WOOD:
#endif
#if defined (PARAM_ENABLE_WINDY)
		case WINDY:
#endif
#if defined (PARAM_ENABLE_MARBLE)
		case MARBLE:
#endif
#if defined (PARAM_ENABLE_FBM_TEX)
		case FBM_TEX:
#endif
#if defined(PARAM_ENABLE_TEX_CONST_FLOAT)
		case CONST_FLOAT:
#endif
#if defined(PARAM_ENABLE_TEX_CONST_FLOAT3)
		case CONST_FLOAT3:
#endif
#if defined(PARAM_ENABLE_TEX_IMAGEMAP)
		case IMAGEMAP:
#endif
		default:
			return 0;
	}
}

//------------------------------------------------------------------------------
// Float texture channel
//------------------------------------------------------------------------------

void Texture_EvaluateFloat(__global Texture *texture, __global HitPoint *hitPoint,
		float texValues[TEXTURE_STACK_SIZE], uint *texValuesSize
		IMAGEMAPS_PARAM_DECL) {
	switch (texture->type) {
#if defined(PARAM_ENABLE_TEX_CONST_FLOAT)
		case CONST_FLOAT:
			ConstFloatTexture_EvaluateFloat(texture, hitPoint, texValues, texValuesSize);
			break;
#endif
#if defined(PARAM_ENABLE_TEX_CONST_FLOAT3)
		case CONST_FLOAT3:
			ConstFloat3Texture_EvaluateFloat(texture, hitPoint, texValues, texValuesSize);
			break;
#endif
#if defined(PARAM_ENABLE_TEX_IMAGEMAP)
		case IMAGEMAP:
			ImageMapTexture_EvaluateFloat(texture, hitPoint, texValues, texValuesSize
					IMAGEMAPS_PARAM);
			break;
#endif
#if defined(PARAM_ENABLE_TEX_SCALE)
		case SCALE_TEX:
			ScaleTexture_EvaluateFloat(texture, hitPoint, texValues, texValuesSize);
			break;
#endif
#if defined (PARAM_ENABLE_FRESNEL_APPROX_N)
		case FRESNEL_APPROX_N:
			FresnelApproxNTexture_EvaluateFloat(texture, hitPoint, texValues, texValuesSize);
			break;
#endif
#if defined (PARAM_ENABLE_FRESNEL_APPROX_K)
		case FRESNEL_APPROX_K:
			FresnelApproxKTexture_EvaluateFloat(texture, hitPoint, texValues, texValuesSize);
			break;
#endif
#if defined (PARAM_ENABLE_CHECKERBOARD2D)
		case CHECKERBOARD2D:
			CheckerBoard2DTexture_EvaluateFloat(texture, hitPoint, texValues, texValuesSize);
			break;
#endif
#if defined (PARAM_ENABLE_CHECKERBOARD3D)
		case CHECKERBOARD3D:
			CheckerBoard3DTexture_EvaluateFloat(texture, hitPoint, texValues, texValuesSize);
			break;
#endif
#if defined (PARAM_ENABLE_TEX_MIX)
		case MIX_TEX:
			MixTexture_EvaluateFloat(texture, hitPoint, texValues, texValuesSize);
			break;
#endif
#if defined (PARAM_ENABLE_FBM_TEX)
		case FBM_TEX:
			FBMTexture_EvaluateFloat(texture, hitPoint, texValues, texValuesSize);
			break;
#endif
#if defined (PARAM_ENABLE_MARBLE)
		case MARBLE:
			MarbleTexture_EvaluateFloat(texture, hitPoint, texValues, texValuesSize);
			break;
#endif
#if defined (PARAM_ENABLE_DOTS)
		case DOTS:
			DotsTexture_EvaluateFloat(texture, hitPoint, texValues, texValuesSize);
			break;
#endif
#if defined (PARAM_ENABLE_BRICK)
		case BRICK:
			BrickTexture_EvaluateFloat(texture, hitPoint, texValues, texValuesSize);
			break;
#endif
#if defined(PARAM_ENABLE_TEX_ADD)
		case ADD_TEX:
			AddTexture_EvaluateFloat(texture, hitPoint, texValues, texValuesSize);
			break;
#endif
#if defined(PARAM_ENABLE_WINDY)
		case WINDY:
			WindyTexture_EvaluateFloat(texture, hitPoint, texValues, texValuesSize);
			break;
#endif
#if defined (PARAM_ENABLE_WOOD)
		case WOOD:
			WoodTexture_EvaluateFloat(texture, hitPoint, texValues, texValuesSize);
			break;
#endif
#if defined(PARAM_ENABLE_WRINKLED)
		case WRINKLED:
			WrinkledTexture_EvaluateFloat(texture, hitPoint, texValues, texValuesSize);
			break;
#endif
#if defined(PARAM_ENABLE_TEX_UV)
		case UV_TEX:
			UVTexture_EvaluateFloat(texture, hitPoint, texValues, texValuesSize);
			break;
#endif
#if defined(PARAM_ENABLE_TEX_BAND)
		case BAND_TEX:
			BandTexture_EvaluateFloat(texture, hitPoint, texValues, texValuesSize);
			break;
#endif
#if defined(PARAM_ENABLE_TEX_HITPOINTCOLOR)
		case HITPOINTCOLOR:
			HitPointColorTexture_EvaluateFloat(texture, hitPoint, texValues, texValuesSize);
			break;
#endif
#if defined(PARAM_ENABLE_TEX_HITPOINTALPHA)
		case HITPOINTALPHA:
			HitPointAlphaTexture_EvaluateFloat(texture, hitPoint, texValues, texValuesSize);
			break;
#endif
#if defined(PARAM_ENABLE_TEX_HITPOINTGREY)
		case HITPOINTGREY:
			HitPointGreyTexture_EvaluateFloat(texture, hitPoint, texValues, texValuesSize);
			break;
#endif
		default:
			// Do nothing
			break;
	}
}

float Texture_GetFloatValue(__global Texture *texture, __global HitPoint *hitPoint
		TEXTURES_PARAM_DECL) {
	__global Texture *todoTex[TEXTURE_STACK_SIZE];
	uint todoTexSize = 0;

	__global Texture *pendingTex[TEXTURE_STACK_SIZE];
	uint targetTexCount[TEXTURE_STACK_SIZE];
	uint pendingTexSize = 0;

	float texValues[TEXTURE_STACK_SIZE];
	uint texValuesSize = 0;

	const uint subTexCount = Texture_AddSubTexture(texture, todoTex, &todoTexSize
			TEXTURES_PARAM);
	if (subTexCount == 0) {
		// A fast path for evaluating non recursive textures
		Texture_EvaluateFloat(texture, hitPoint, texValues, &texValuesSize
			IMAGEMAPS_PARAM);
	} else {
		// Normal complex path for evaluating non recursive textures
		pendingTex[pendingTexSize] = texture;
		targetTexCount[pendingTexSize++] = subTexCount;
		do {
			if ((pendingTexSize > 0) && (texValuesSize == targetTexCount[pendingTexSize - 1])) {
				// Pop the a texture to do
				__global Texture *tex = pendingTex[--pendingTexSize];

				Texture_EvaluateFloat(tex, hitPoint, texValues, &texValuesSize
						IMAGEMAPS_PARAM);
				continue;
			}

			if (todoTexSize > 0) {
				// Pop the a texture to do
				__global Texture *tex = todoTex[--todoTexSize];

				// Add this texture to the list of pending one
				const uint subTexCount = Texture_AddSubTexture(tex, todoTex, &todoTexSize
						TEXTURES_PARAM);
				pendingTex[pendingTexSize] = tex;
				targetTexCount[pendingTexSize++] = subTexCount + texValuesSize;
			}
		} while ((todoTexSize > 0) || (pendingTexSize > 0));
	}

	return texValues[0];
}

//------------------------------------------------------------------------------
// Color texture channel
//------------------------------------------------------------------------------

void Texture_EvaluateSpectrum(__global Texture *texture, __global HitPoint *hitPoint,
		float3 texValues[TEXTURE_STACK_SIZE], uint *texValuesSize
		IMAGEMAPS_PARAM_DECL) {
	switch (texture->type) {
#if defined(PARAM_ENABLE_TEX_CONST_FLOAT)
		case CONST_FLOAT:
			ConstFloatTexture_EvaluateSpectrum(texture, hitPoint, texValues, texValuesSize);
			break;
#endif
#if defined(PARAM_ENABLE_TEX_CONST_FLOAT3)
		case CONST_FLOAT3:
			ConstFloat3Texture_EvaluateSpectrum(texture, hitPoint, texValues, texValuesSize);
			break;
#endif
#if defined(PARAM_ENABLE_TEX_IMAGEMAP)
		case IMAGEMAP:
			ImageMapTexture_EvaluateSpectrum(texture, hitPoint, texValues, texValuesSize
					IMAGEMAPS_PARAM);
			break;
#endif
#if defined(PARAM_ENABLE_TEX_SCALE)
		case SCALE_TEX:
			ScaleTexture_EvaluateSpectrum(texture, hitPoint, texValues, texValuesSize);
			break;
#endif
#if defined (PARAM_ENABLE_FRESNEL_APPROX_N)
		case FRESNEL_APPROX_N:
			FresnelApproxNTexture_EvaluateSpectrum(texture, hitPoint, texValues, texValuesSize);
			break;
#endif
#if defined (PARAM_ENABLE_FRESNEL_APPROX_K)
		case FRESNEL_APPROX_K:
			FresnelApproxKTexture_EvaluateSpectrum(texture, hitPoint, texValues, texValuesSize);
			break;
#endif
#if defined (PARAM_ENABLE_CHECKERBOARD2D)
		case CHECKERBOARD2D:
			CheckerBoard2DTexture_EvaluateSpectrum(texture, hitPoint, texValues, texValuesSize);
			break;
#endif
#if defined (PARAM_ENABLE_CHECKERBOARD3D)
		case CHECKERBOARD3D:
			CheckerBoard3DTexture_EvaluateSpectrum(texture, hitPoint, texValues, texValuesSize);
			break;
#endif
#if defined (PARAM_ENABLE_TEX_MIX)
		case MIX_TEX:
			MixTexture_EvaluateSpectrum(texture, hitPoint, texValues, texValuesSize);
			break;
#endif
#if defined (PARAM_ENABLE_FBM_TEX)
		case FBM_TEX:
			FBMTexture_EvaluateSpectrum(texture, hitPoint, texValues, texValuesSize);
			break;
#endif
#if defined (PARAM_ENABLE_MARBLE)
		case MARBLE:
			MarbleTexture_EvaluateSpectrum(texture, hitPoint, texValues, texValuesSize);
			break;
#endif
#if defined (PARAM_ENABLE_DOTS)
		case DOTS:
			DotsTexture_EvaluateSpectrum(texture, hitPoint, texValues, texValuesSize);
			break;
#endif
#if defined (PARAM_ENABLE_BRICK)
		case BRICK:
			BrickTexture_EvaluateSpectrum(texture, hitPoint, texValues, texValuesSize);
			break;
#endif
#if defined(PARAM_ENABLE_TEX_ADD)
		case ADD_TEX:
			AddTexture_EvaluateSpectrum(texture, hitPoint, texValues, texValuesSize);
			break;
#endif
#if defined(PARAM_ENABLE_WINDY)
		case WINDY:
			WindyTexture_EvaluateSpectrum(texture, hitPoint, texValues, texValuesSize);
			break;
#endif
#if defined (PARAM_ENABLE_WOOD)
		case WOOD:
			WoodTexture_EvaluateSpectrum(texture, hitPoint, texValues, texValuesSize);
			break;
#endif
#if defined(PARAM_ENABLE_WRINKLED)
		case WRINKLED:
			WrinkledTexture_EvaluateSpectrum(texture, hitPoint, texValues, texValuesSize);
			break;
#endif
#if defined(PARAM_ENABLE_TEX_UV)
		case UV_TEX:
			UVTexture_EvaluateSpectrum(texture, hitPoint, texValues, texValuesSize);
			break;
#endif
#if defined(PARAM_ENABLE_TEX_BAND)
		case BAND_TEX:
			BandTexture_EvaluateSpectrum(texture, hitPoint, texValues, texValuesSize);
			break;
#endif
#if defined(PARAM_ENABLE_TEX_HITPOINTCOLOR)
		case HITPOINTCOLOR:
			HitPointColorTexture_EvaluateSpectrum(texture, hitPoint, texValues, texValuesSize);
			break;
#endif
#if defined(PARAM_ENABLE_TEX_HITPOINTALPHA)
		case HITPOINTALPHA:
			HitPointAlphaTexture_EvaluateSpectrum(texture, hitPoint, texValues, texValuesSize);
			break;
#endif
#if defined(PARAM_ENABLE_TEX_HITPOINTGREY)
		case HITPOINTGREY:
			HitPointGreyTexture_EvaluateSpectrum(texture, hitPoint, texValues, texValuesSize);
			break;
#endif
		default:
			// Do nothing
			break;
	}
}

float3 Texture_GetSpectrumValue(__global Texture *texture, __global HitPoint *hitPoint
		TEXTURES_PARAM_DECL) {
	__global Texture *todoTex[TEXTURE_STACK_SIZE];
	uint todoTexSize = 0;

	__global Texture *pendingTex[TEXTURE_STACK_SIZE];
	uint targetTexCount[TEXTURE_STACK_SIZE];
	uint pendingTexSize = 0;

	float3 texValues[TEXTURE_STACK_SIZE];
	uint texValuesSize = 0;

	const uint subTexCount = Texture_AddSubTexture(texture, todoTex, &todoTexSize
			TEXTURES_PARAM);
	if (subTexCount == 0) {
		// A fast path for evaluating non recursive textures
		Texture_EvaluateSpectrum(texture, hitPoint, texValues, &texValuesSize
			IMAGEMAPS_PARAM);
	} else {
		// Normal complex path for evaluating non recursive textures
		pendingTex[pendingTexSize] = texture;
		targetTexCount[pendingTexSize++] = subTexCount;
		do {
			if ((pendingTexSize > 0) && (texValuesSize == targetTexCount[pendingTexSize - 1])) {
				// Pop the a texture to do
				__global Texture *tex = pendingTex[--pendingTexSize];

				Texture_EvaluateSpectrum(tex, hitPoint, texValues, &texValuesSize
						IMAGEMAPS_PARAM);
				continue;
			}

			if (todoTexSize > 0) {
				// Pop the a texture to do
				__global Texture *tex = todoTex[--todoTexSize];

				// Add this texture to the list of pending one
				const uint subTexCount = Texture_AddSubTexture(tex, todoTex, &todoTexSize
						TEXTURES_PARAM);
				pendingTex[pendingTexSize] = tex;
				targetTexCount[pendingTexSize++] = subTexCount + texValuesSize;
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
			break;
#endif
#if defined(PARAM_ENABLE_TEX_CONST_FLOAT3)
		case CONST_FLOAT3:
			ConstFloat3Texture_EvaluateDuDv(texture, hitPoint, texValues, texValuesSize);
			break;
#endif
#if defined(PARAM_ENABLE_TEX_IMAGEMAP)
		case IMAGEMAP:
			ImageMapTexture_EvaluateDuDv(texture, hitPoint, texValues, texValuesSize);
			break;
#endif
#if defined(PARAM_ENABLE_TEX_SCALE)
		case SCALE_TEX:
			ScaleTexture_EvaluateDuDv(texture, hitPoint, texValues, texValuesSize);
			break;
#endif
#if defined (PARAM_ENABLE_FRESNEL_APPROX_N)
		case FRESNEL_APPROX_N:
			FresnelApproxNTexture_EvaluateDuDv(texture, hitPoint, texValues, texValuesSize);
			break;
#endif
#if defined (PARAM_ENABLE_FRESNEL_APPROX_K)
		case FRESNEL_APPROX_K:
			FresnelApproxKTexture_EvaluateDuDv(texture, hitPoint, texValues, texValuesSize);
			break;
#endif
#if defined (PARAM_ENABLE_CHECKERBOARD2D)
		case CHECKERBOARD2D:
			CheckerBoard2DTexture_EvaluateDuDv(texture, hitPoint, texValues, texValuesSize);
			break;
#endif
#if defined (PARAM_ENABLE_CHECKERBOARD3D)
		case CHECKERBOARD3D:
			CheckerBoard3DTexture_EvaluateDuDv(texture, hitPoint, texValues, texValuesSize);
			break;
#endif
#if defined (PARAM_ENABLE_TEX_MIX)
		case MIX_TEX:
			MixTexture_EvaluateDuDv(texture, hitPoint, texValues, texValuesSize);
			break;
#endif
#if defined (PARAM_ENABLE_FBM_TEX)
		case FBM_TEX:
			FBMTexture_EvaluateDuDv(texture, hitPoint, texValues, texValuesSize);
			break;
#endif
#if defined (PARAM_ENABLE_MARBLE)
		case MARBLE:
			MarbleTexture_EvaluateDuDv(texture, hitPoint, texValues, texValuesSize);
			break;
#endif
#if defined (PARAM_ENABLE_DOTS)
		case DOTS:
			DotsTexture_EvaluateDuDv(texture, hitPoint, texValues, texValuesSize);
			break;
#endif
#if defined (PARAM_ENABLE_BRICK)
		case BRICK:
			BrickTexture_EvaluateDuDv(texture, hitPoint, texValues, texValuesSize);
			break;
#endif
#if defined(PARAM_ENABLE_TEX_ADD)
		case ADD_TEX:
			AddTexture_EvaluateDuDv(texture, hitPoint, texValues, texValuesSize);
			break;
#endif
#if defined(PARAM_ENABLE_WINDY)
		case WINDY:
			WindyTexture_EvaluateDuDv(texture, hitPoint, texValues, texValuesSize);
			break;
#endif
#if defined (PARAM_ENABLE_WOOD)
		case WOOD:
			WoodTexture_EvaluateDuDv(texture, hitPoint, texValues, texValuesSize);
			break;
#endif
#if defined(PARAM_ENABLE_WRINKLED)
		case WRINKLED:
			WrinkledTexture_EvaluateDuDv(texture, hitPoint, texValues, texValuesSize);
			break;
#endif
#if defined(PARAM_ENABLE_TEX_UV)
		case UV_TEX:
			UVTexture_EvaluateDuDv(texture, hitPoint, texValues, texValuesSize);
			break;
#endif
#if defined(PARAM_ENABLE_TEX_BAND)
		case BAND_TEX:
			BandTexture_EvaluateDuDv(texture, hitPoint, texValues, texValuesSize);
			break;
#endif
#if defined(PARAM_ENABLE_TEX_HITPOINTCOLOR)
		case HITPOINTCOLOR:
			HitPointColorTexture_EvaluateDuDv(texture, hitPoint, texValues, texValuesSize);
			break;
#endif
#if defined(PARAM_ENABLE_TEX_HITPOINTALPHA)
		case HITPOINTALPHA:
			HitPointAlphaTexture_EvaluateDuDv(texture, hitPoint, texValues, texValuesSize);
			break;
#endif
#if defined(PARAM_ENABLE_TEX_HITPOINTGREY)
		case HITPOINTGREY:
			HitPointGreyTexture_EvaluateDuDv(texture, hitPoint, texValues, texValuesSize);
			break;
#endif
		default:
			// Do nothing
			break;
	}
}

float2 Texture_GetDuDv(__global Texture *texture, __global HitPoint *hitPoint
		TEXTURES_PARAM_DECL) {
	__global Texture *todoTex[TEXTURE_STACK_SIZE];
	uint todoTexSize = 0;

	__global Texture *pendingTex[TEXTURE_STACK_SIZE];
	uint targetTexCount[TEXTURE_STACK_SIZE];
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
		targetTexCount[pendingTexSize++] = subTexCount;
		do {
			if ((pendingTexSize > 0) && (texValuesSize == targetTexCount[pendingTexSize - 1])) {
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
				const uint subTexCount = Texture_AddSubTexture(tex, todoTex, &todoTexSize
						TEXTURES_PARAM);
				pendingTex[pendingTexSize] = tex;
				targetTexCount[pendingTexSize++] = subTexCount + texValuesSize;
			}
		} while ((todoTexSize > 0) || (pendingTexSize > 0));
	}

	return texValues[0];
}
