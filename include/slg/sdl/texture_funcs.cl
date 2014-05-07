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

#if defined(PARAM_ENABLE_TEX_CONST_FLOAT)

float ConstFloatTexture_DynamicEvaluateFloat(__global Texture *texture, __global HitPoint *hitPoint) {
	return texture->constFloat.value;
}

float3 ConstFloatTexture_DynamicEvaluateSpectrum(__global Texture *texture, __global HitPoint *hitPoint) {
	return texture->constFloat.value;
}

#if defined(PARAM_DIASBLE_TEX_DYNAMIC_EVALUATION)
void ConstFloatTexture_EvaluateFloat(__global Texture *texture, __global HitPoint *hitPoint,
		float texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
	texValues[(*texValuesSize)++] = ConstFloatTexture_DynamicEvaluateFloat(texture, hitPoint);
}

void ConstFloatTexture_EvaluateSpectrum(__global Texture *texture, __global HitPoint *hitPoint,
		float3 texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
	texValues[(*texValuesSize)++] = ConstFloatTexture_DynamicEvaluateSpectrum(texture, hitPoint);
}
#endif

#endif

//------------------------------------------------------------------------------
// ConstFloat3 texture
//------------------------------------------------------------------------------

#if defined(PARAM_ENABLE_TEX_CONST_FLOAT3)

float ConstFloat3Texture_DynamicEvaluateFloat(__global Texture *texture, __global HitPoint *hitPoint) {
	return Spectrum_Y(VLOAD3F(texture->constFloat3.color.c));
}

float3 ConstFloat3Texture_DynamicEvaluateSpectrum(__global Texture *texture, __global HitPoint *hitPoint) {
	return VLOAD3F(texture->constFloat3.color.c);
}

#if defined(PARAM_DIASBLE_TEX_DYNAMIC_EVALUATION)
void ConstFloat3Texture_EvaluateFloat(__global Texture *texture, __global HitPoint *hitPoint,
		float texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
	texValues[(*texValuesSize)++] = ConstFloat3Texture_DynamicEvaluateFloat(texture, hitPoint);
}

void ConstFloat3Texture_EvaluateSpectrum(__global Texture *texture, __global HitPoint *hitPoint,
		float3 texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
	texValues[(*texValuesSize)++] = ConstFloat3Texture_DynamicEvaluateSpectrum(texture, hitPoint);
}
#endif

#endif

//------------------------------------------------------------------------------
// ImageMap texture
//------------------------------------------------------------------------------

#if defined(PARAM_ENABLE_TEX_IMAGEMAP)

float ImageMapTexture_DynamicEvaluateFloat(__global Texture *texture, __global HitPoint *hitPoint
		IMAGEMAPS_PARAM_DECL) {
	__global ImageMap *imageMap = &imageMapDescs[texture->imageMapTex.imageMapIndex];
	__global float *pixels = ImageMap_GetPixelsAddress(
			imageMapBuff, imageMap->pageIndex, imageMap->pixelsIndex);

	const float2 uv = VLOAD2F(&hitPoint->uv.u);
	const float2 mapUV = TextureMapping2D_Map(&texture->imageMapTex.mapping, hitPoint);

	return texture->imageMapTex.gain * ImageMap_GetFloat(
			pixels,
			imageMap->width, imageMap->height, imageMap->channelCount,
			mapUV.s0, mapUV.s1);
}

float3 ImageMapTexture_DynamicEvaluateSpectrum(__global Texture *texture, __global HitPoint *hitPoint
		IMAGEMAPS_PARAM_DECL) {
	__global ImageMap *imageMap = &imageMapDescs[texture->imageMapTex.imageMapIndex];
	__global float *pixels = ImageMap_GetPixelsAddress(
			imageMapBuff, imageMap->pageIndex, imageMap->pixelsIndex);

	const float2 uv = VLOAD2F(&hitPoint->uv.u);
	const float2 mapUV = TextureMapping2D_Map(&texture->imageMapTex.mapping, hitPoint);

	return texture->imageMapTex.gain * ImageMap_GetSpectrum(
			pixels,
			imageMap->width, imageMap->height, imageMap->channelCount,
			mapUV.s0, mapUV.s1);
}

#if defined(PARAM_DIASBLE_TEX_DYNAMIC_EVALUATION)
void ImageMapTexture_EvaluateFloat(__global Texture *texture, __global HitPoint *hitPoint,
		float texValues[TEXTURE_STACK_SIZE], uint *texValuesSize
		IMAGEMAPS_PARAM_DECL) {
	texValues[(*texValuesSize)++] = ImageMapTexture_DynamicEvaluateFloat(texture, hitPoint
		IMAGEMAPS_PARAM);
}

void ImageMapTexture_EvaluateSpectrum(__global Texture *texture, __global HitPoint *hitPoint,
		float3 texValues[TEXTURE_STACK_SIZE], uint *texValuesSize
		IMAGEMAPS_PARAM_DECL) {
	texValues[(*texValuesSize)++] =  ImageMapTexture_DynamicEvaluateSpectrum(texture, hitPoint
		IMAGEMAPS_PARAM);
}
#endif

#endif

//------------------------------------------------------------------------------
// Scale texture
//------------------------------------------------------------------------------

#if defined(PARAM_ENABLE_TEX_SCALE)

float ScaleTexture_DynamicEvaluateFloat(__global Texture *texture, __global HitPoint *hitPoint,
		const float tex1, const float tex2) {
	return tex1 * tex2;
}

float3 ScaleTexture_DynamicEvaluateSpectrum(__global Texture *texture,  __global HitPoint *hitPoint,
		const float3 tex1, const float3 tex2) {
	return tex1 * tex2;
}

#if defined(PARAM_DIASBLE_TEX_DYNAMIC_EVALUATION)

void ScaleTexture_EvaluateFloat(__global Texture *texture, __global HitPoint *hitPoint,
		float texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
	const float tex1 = texValues[--(*texValuesSize)];
	const float tex2 = texValues[--(*texValuesSize)];
	
	texValues[(*texValuesSize)++] = ScaleTexture_DynamicEvaluateFloat(texture, hitPoint, tex1, tex2);
}

void ScaleTexture_EvaluateSpectrum(__global Texture *texture, __global HitPoint *hitPoint,
		float3 texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
	const float3 tex1 = texValues[--(*texValuesSize)];
	const float3 tex2 = texValues[--(*texValuesSize)];
	
	texValues[(*texValuesSize)++] = ScaleTexture_DynamicEvaluateSpectrum(texture, hitPoint, tex1, tex2);
}

#endif

#endif

//------------------------------------------------------------------------------
// FresnelApproxN & FresnelApproxK texture
//------------------------------------------------------------------------------

#if defined(PARAM_ENABLE_FRESNEL_APPROX_N)

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

float FresnelApproxNTexture_DynamicEvaluateFloat(__global Texture *texture, __global HitPoint *hitPoint,
		const float value) {
	return FresnelApproxN(value);
}

float3 FresnelApproxNTexture_DynamicEvaluateSpectrum(__global Texture *texture, __global HitPoint *hitPoint,
		const float3 value) {
	return FresnelApproxN3(value);
}

#if defined(PARAM_DIASBLE_TEX_DYNAMIC_EVALUATION)
void FresnelApproxNTexture_EvaluateFloat(__global Texture *texture, __global HitPoint *hitPoint,
		float texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
	const float value = texValues[--(*texValuesSize)];

	texValues[(*texValuesSize)++] = FresnelApproxNTexture_DynamicEvaluateFloat(texture, hitPoint, value);
}

void FresnelApproxNTexture_EvaluateSpectrum(__global Texture *texture, __global HitPoint *hitPoint,
		float3 texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
	const float3 value = texValues[--(*texValuesSize)];

	texValues[(*texValuesSize)++] = FresnelApproxNTexture_DynamicEvaluateSpectrum(texture, hitPoint, value);
}
#endif

#endif

#if defined(PARAM_ENABLE_FRESNEL_APPROX_K)

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

float FresnelApproxKTexture_DynamicEvaluateFloat(__global Texture *texture, __global HitPoint *hitPoint,
		const float value) {
	return FresnelApproxK(value);
}

float3 FresnelApproxKTexture_DynamicEvaluateSpectrum(__global Texture *texture, __global HitPoint *hitPoint,
		const float3 value) {
	return FresnelApproxK3(value);
}

#if defined(PARAM_DIASBLE_TEX_DYNAMIC_EVALUATION)
void FresnelApproxKTexture_EvaluateFloat(__global Texture *texture, __global HitPoint *hitPoint,
		float texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
	const float value = texValues[--(*texValuesSize)];

	texValues[(*texValuesSize)++] = FresnelApproxKTexture_DynamicEvaluateFloat(texture, hitPoint, value);
}

void FresnelApproxKTexture_EvaluateSpectrum(__global Texture *texture, __global HitPoint *hitPoint,
		float3 texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
	const float3 value = texValues[--(*texValuesSize)];

	texValues[(*texValuesSize)++] = FresnelApproxKTexture_DynamicEvaluateSpectrum(texture, hitPoint, value);
}
#endif

#endif

//------------------------------------------------------------------------------
// Mix texture
//------------------------------------------------------------------------------

#if defined(PARAM_ENABLE_TEX_MIX)

float MixTexture_DynamicEvaluateFloat(__global Texture *texture, __global HitPoint *hitPoint,
		const float amt, const float value1, const float value2) {
	return Lerp(clamp(amt, 0.f, 1.f), value1, value2);
}

float3 MixTexture_DynamicEvaluateSpectrum(__global Texture *texture, __global HitPoint *hitPoint,
		const float3 amt, const float3 value1, const float3 value2) {
	return mix(value1, value2, clamp(amt, 0.f, 1.f));
}

#if defined(PARAM_DIASBLE_TEX_DYNAMIC_EVALUATION)
void MixTexture_EvaluateFloat(__global Texture *texture, __global HitPoint *hitPoint,
		float texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
	const float amt = texValues[--(*texValuesSize)];
	const float value1 = texValues[--(*texValuesSize)];
	const float value2 = texValues[--(*texValuesSize)];

	texValues[(*texValuesSize)++] = MixTexture_DynamicEvaluateFloat(texture,hitPoint, amt, value1, value2);
}

void MixTexture_EvaluateSpectrum(__global Texture *texture, __global HitPoint *hitPoint,
		float3 texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
	const float3 amt = texValues[--(*texValuesSize)];
	const float3 value1 = texValues[--(*texValuesSize)];
	const float3 value2 = texValues[--(*texValuesSize)];

	texValues[(*texValuesSize)++] = MixTexture_DynamicEvaluateSpectrum(texture,hitPoint, amt, value1, value2);
}
#endif

#endif

//------------------------------------------------------------------------------
// CheckerBoard 2D & 3D texture
//------------------------------------------------------------------------------

#if defined(PARAM_ENABLE_CHECKERBOARD2D)

float CheckerBoard2DTexture_DynamicEvaluateFloat(__global Texture *texture, __global HitPoint *hitPoint,
		const float value1, const float value2) {
	const float2 uv = VLOAD2F(&hitPoint->uv.u);
	const float2 mapUV = TextureMapping2D_Map(&texture->checkerBoard2D.mapping, hitPoint);

	return ((Floor2Int(mapUV.s0) + Floor2Int(mapUV.s1)) % 2 == 0) ? value1 : value2;
}

float3 CheckerBoard2DTexture_DynamicEvaluateSpectrum(__global Texture *texture, __global HitPoint *hitPoint,
		const float3 value1, const float3 value2) {
	const float2 uv = VLOAD2F(&hitPoint->uv.u);
	const float2 mapUV = TextureMapping2D_Map(&texture->checkerBoard2D.mapping, hitPoint);

	return ((Floor2Int(mapUV.s0) + Floor2Int(mapUV.s1)) % 2 == 0) ? value1 : value2;
}

#if defined(PARAM_DIASBLE_TEX_DYNAMIC_EVALUATION)
void CheckerBoard2DTexture_EvaluateFloat(__global Texture *texture, __global HitPoint *hitPoint,
		float texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
	const float value1 = texValues[--(*texValuesSize)];
	const float value2 = texValues[--(*texValuesSize)];

	texValues[(*texValuesSize)++] = CheckerBoard2DTexture_DynamicEvaluateFloat(texture, hitPoint, value1, value2);
}

void CheckerBoard2DTexture_EvaluateSpectrum(__global Texture *texture, __global HitPoint *hitPoint,
		float3 texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
	const float3 value1 = texValues[--(*texValuesSize)];
	const float3 value2 = texValues[--(*texValuesSize)];

	texValues[(*texValuesSize)++] = CheckerBoard2DTexture_DynamicEvaluateSpectrum(texture, hitPoint, value1, value2);
}
#endif

#endif

#if defined(PARAM_ENABLE_CHECKERBOARD3D)

float CheckerBoard3DTexture_DynamicEvaluateFloat(__global Texture *texture, __global HitPoint *hitPoint,
		const float value1, const float value2) {
	const float3 mapP = TextureMapping3D_Map(&texture->checkerBoard3D.mapping, hitPoint);

	return ((Floor2Int(mapP.x) + Floor2Int(mapP.y) + Floor2Int(mapP.z)) % 2 == 0) ? value1 : value2;
}

float3 CheckerBoard3DTexture_DynamicEvaluateSpectrum(__global Texture *texture, __global HitPoint *hitPoint,
		const float3 value1, const float3 value2) {
	const float3 mapP = TextureMapping3D_Map(&texture->checkerBoard3D.mapping, hitPoint);

	return ((Floor2Int(mapP.x) + Floor2Int(mapP.y) + Floor2Int(mapP.z)) % 2 == 0) ? value1 : value2;
}

#if defined(PARAM_DIASBLE_TEX_DYNAMIC_EVALUATION)
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
#endif

#endif

//------------------------------------------------------------------------------
// FBM texture
//------------------------------------------------------------------------------

#if defined(PARAM_ENABLE_FBM_TEX)

float FBMTexture_DynamicEvaluateFloat(__global Texture *texture, __global HitPoint *hitPoint) {
	const float3 mapP = TextureMapping3D_Map(&texture->fbm.mapping, hitPoint);

	return FBm(mapP, texture->fbm.omega, texture->fbm.octaves);
}

float3 FBMTexture_DynamicEvaluateSpectrum(__global Texture *texture, __global HitPoint *hitPoint) {
	const float3 mapP = TextureMapping3D_Map(&texture->fbm.mapping, hitPoint);

	return FBm(mapP, texture->fbm.omega, texture->fbm.octaves);
}

#if defined(PARAM_DIASBLE_TEX_DYNAMIC_EVALUATION)
void FBMTexture_EvaluateFloat(__global Texture *texture, __global HitPoint *hitPoint,
		float texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
	texValues[(*texValuesSize)++] = FBMTexture_DynamicEvaluateFloat(texture, hitPoint);
}

void FBMTexture_EvaluateSpectrum(__global Texture *texture, __global HitPoint *hitPoint,
		float3 texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
	texValues[(*texValuesSize)++] = FBMTexture_DynamicEvaluateSpectrum(texture, hitPoint);
}
#endif

#endif

//------------------------------------------------------------------------------
// Marble texture
//------------------------------------------------------------------------------

#if defined(PARAM_ENABLE_MARBLE)

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

float MarbleTexture_DynamicEvaluateFloat(__global Texture *texture, __global HitPoint *hitPoint) {
	return Spectrum_Y(MarbleTexture_Evaluate(texture, hitPoint));
}

float3 MarbleTexture_DynamicEvaluateSpectrum(__global Texture *texture, __global HitPoint *hitPoint) {
	return MarbleTexture_Evaluate(texture, hitPoint);
}

#if defined(PARAM_DIASBLE_TEX_DYNAMIC_EVALUATION)
void MarbleTexture_EvaluateFloat(__global Texture *texture, __global HitPoint *hitPoint,
		float texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
	texValues[(*texValuesSize)++] = MarbleTexture_DynamicEvaluateFloat(texture, hitPoint);
}

void MarbleTexture_EvaluateSpectrum(__global Texture *texture, __global HitPoint *hitPoint,
		float3 texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
	texValues[(*texValuesSize)++] = MarbleTexture_EvaluateSpectrum(texture, hitPoint);
}
#endif

#endif

//------------------------------------------------------------------------------
// Dots texture
//------------------------------------------------------------------------------

#if defined(PARAM_ENABLE_DOTS)

float DotsTexture_DynamicEvaluateFloat(__global Texture *texture, __global HitPoint *hitPoint,
		const float value1, const float value2) {
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
		if (ds * ds + dt * dt < radius * radius)
			return value1;
	}

	return value2;
}

float3 DotsTexture_DynamicEvaluateSpectrum(__global Texture *texture, __global HitPoint *hitPoint,
		const float3 value1, const float3 value2) {
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
		if (ds * ds + dt * dt < radius * radius)
			return value1;
	}

	return value2;
}

#if defined(PARAM_DIASBLE_TEX_DYNAMIC_EVALUATION)
void DotsTexture_EvaluateFloat(__global Texture *texture, __global HitPoint *hitPoint,
		float texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
	const float value1 = texValues[--(*texValuesSize)];
	const float value2 = texValues[--(*texValuesSize)];

	texValues[(*texValuesSize)++] = DotsTexture_DynamicEvaluateFloat(texture, hitPoint, value1, value2);
}

void DotsTexture_EvaluateSpectrum(__global Texture *texture, __global HitPoint *hitPoint,
		float3 texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
	const float3 value1 = texValues[--(*texValuesSize)];
	const float3 value2 = texValues[--(*texValuesSize)];

	texValues[(*texValuesSize)++] = DotsTexture_DynamicEvaluateSpectrum(texture, hitPoint, value1, value2);
}
#endif

#endif

//------------------------------------------------------------------------------
// Brick texture
//------------------------------------------------------------------------------

#if defined(PARAM_ENABLE_BRICK)

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

float BrickTexture_DynamicEvaluateFloat(__global Texture *texture, __global HitPoint *hitPoint,
		const float value1, const float value2, const float value3) {
	if (BrickTexture_Evaluate(texture, hitPoint))
		return value1 * value3;
	else
		return value2;
}

float3 BrickTexture_DynamicEvaluateSpectrum(__global Texture *texture, __global HitPoint *hitPoint,
		const float3 value1, const float3 value2, const float3 value3) {
	if (BrickTexture_Evaluate(texture, hitPoint))
		return value1 * value3;
	else
		return value2;
}

#if defined(PARAM_DIASBLE_TEX_DYNAMIC_EVALUATION)
void BrickTexture_EvaluateFloat(__global Texture *texture, __global HitPoint *hitPoint,
		float texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
	const float value1 = texValues[--(*texValuesSize)];
	const float value2 = texValues[--(*texValuesSize)];
	const float value3 = texValues[--(*texValuesSize)];

	texValues[(*texValuesSize)++] = BrickTexture_DynamicEvaluateFloat(hitPoint, texture, value1, value2, value3);
}

void BrickTexture_EvaluateSpectrum(__global Texture *texture, __global HitPoint *hitPoint,
		float3 texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
	const float3 value1 = texValues[--(*texValuesSize)];
	const float3 value2 = texValues[--(*texValuesSize)];
	const float3 value3 = texValues[--(*texValuesSize)];

	texValues[(*texValuesSize)++] = BrickTexture_DynamicEvaluateSpectrum(hitPoint, texture, value1, value2, value3);
}
#endif

#endif

//------------------------------------------------------------------------------
// Add texture
//------------------------------------------------------------------------------

#if defined(PARAM_ENABLE_TEX_ADD)

float AddTexture_DynamicEvaluateFloat(__global Texture *texture, __global HitPoint *hitPoint,
		const float value1, const float value2) {
	return value1 + value2;
}

float3 AddTexture_DynamicEvaluateSpectrum(__global Texture *texture, __global HitPoint *hitPoint,
		const float3 value1, const float3 value2) {
	return value1 + value2;
}

#if defined(PARAM_DIASBLE_TEX_DYNAMIC_EVALUATION)
void AddTexture_EvaluateFloat(__global Texture *texture, __global HitPoint *hitPoint,
		float texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
	const float value1 = texValues[--(*texValuesSize)];
	const float value2 = texValues[--(*texValuesSize)];

	texValues[(*texValuesSize)++] = AddTexture_DynamicEvaluateFloat(texture, hitPoint, value1, value2);
}

void AddTexture_EvaluateSpectrum(__global Texture *texture, __global HitPoint *hitPoint,
		float3 texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
	const float3 value1 = texValues[--(*texValuesSize)];
	const float3 value2 = texValues[--(*texValuesSize)];

	texValues[(*texValuesSize)++] = AddTexture_DynamicEvaluateSpectrum(texture, hitPoint, value1, value2);
}
#endif

#endif

//------------------------------------------------------------------------------
// Windy texture
//------------------------------------------------------------------------------

#if defined(PARAM_ENABLE_WINDY)

float WindyTexture_DynamicEvaluateFloat(__global Texture *texture, __global HitPoint *hitPoint) {
	const float3 mapP = TextureMapping3D_Map(&texture->windy.mapping, hitPoint);

	const float windStrength = FBm(.1f * mapP, .5f, 3);
	const float waveHeight = FBm(mapP, .5f, 6);

	return fabs(windStrength) * waveHeight;
}

float3 WindyTexture_DynamicEvaluateSpectrum(__global Texture *texture, __global HitPoint *hitPoint) {
	const float3 mapP = TextureMapping3D_Map(&texture->windy.mapping, hitPoint);

	const float windStrength = FBm(.1f * mapP, .5f, 3);
	const float waveHeight = FBm(mapP, .5f, 6);

	return fabs(windStrength) * waveHeight;
}

#if defined(PARAM_DIASBLE_TEX_DYNAMIC_EVALUATION)
void WindyTexture_EvaluateFloat(__global Texture *texture, __global HitPoint *hitPoint,
		float texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
	texValues[(*texValuesSize)++] = WindyTexture_DynamicEvaluateFloat(texture, hitPoint);
}

void WindyTexture_EvaluateSpectrum(__global Texture *texture, __global HitPoint *hitPoint,
		float3 texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
	texValues[(*texValuesSize)++] = WindyTexture_DynamicEvaluateSpectrum(texture, hitPoint);
}
#endif

#endif

//------------------------------------------------------------------------------
// Wrinkled texture
//------------------------------------------------------------------------------

#if defined(PARAM_ENABLE_WRINKLED)

float WrinkledTexture_DynamicEvaluateFloat(__global Texture *texture, __global HitPoint *hitPoint) {
	const float3 mapP = TextureMapping3D_Map(&texture->wrinkled.mapping, hitPoint);

	return Turbulence(mapP, texture->wrinkled.omega, texture->wrinkled.octaves);
}

float3 WrinkledTexture_DynamicEvaluateSpectrum(__global Texture *texture, __global HitPoint *hitPoint) {
	const float3 mapP = TextureMapping3D_Map(&texture->wrinkled.mapping, hitPoint);

	return Turbulence(mapP, texture->wrinkled.omega, texture->wrinkled.octaves);
}

#if defined(PARAM_DIASBLE_TEX_DYNAMIC_EVALUATION)
void WrinkledTexture_EvaluateFloat(__global Texture *texture, __global HitPoint *hitPoint,
		float texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
	texValues[(*texValuesSize)++] = WrinkledTexture_DynamicEvaluateFloat(texture, hitPoint);
}

void WrinkledTexture_EvaluateSpectrum(__global Texture *texture, __global HitPoint *hitPoint,
		float3 texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
	texValues[(*texValuesSize)++] = WrinkledTexture_DynamicEvaluateSpectrum(texture, hitPoint);
}
#endif

#endif

//------------------------------------------------------------------------------
// UV texture
//------------------------------------------------------------------------------

#if defined(PARAM_ENABLE_TEX_UV)

float UVTexture_DynamicEvaluateFloat(__global Texture *texture, __global HitPoint *hitPoint) {
	const float2 uv = TextureMapping2D_Map(&texture->uvTex.mapping, hitPoint);

	return Spectrum_Y((float3)(uv.s0 - Floor2Int(uv.s0), uv.s1 - Floor2Int(uv.s1), 0.f));
}

float3 UVTexture_DynamicEvaluateSpectrum(__global Texture *texture, __global HitPoint *hitPoint) {
	const float2 uv = TextureMapping2D_Map(&texture->uvTex.mapping, hitPoint);

	return (float3)(uv.s0 - Floor2Int(uv.s0), uv.s1 - Floor2Int(uv.s1), 0.f);
}

#if defined(PARAM_DIASBLE_TEX_DYNAMIC_EVALUATION)
void UVTexture_EvaluateFloat(__global Texture *texture, __global HitPoint *hitPoint,
		float texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
	texValues[(*texValuesSize)++] = UVTexture_DynamicEvaluateFloat(texture, hitPoint);
}

void UVTexture_EvaluateSpectrum(__global Texture *texture, __global HitPoint *hitPoint,
		float3 texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
	const float2 uv = TextureMapping2D_Map(&texture->uvTex.mapping, hitPoint);

	texValues[(*texValuesSize)++] = UVTexture_DynamicEvaluateSpectrum(texture, hitPoint);
}
#endif

#endif

//------------------------------------------------------------------------------
// Band texture
//------------------------------------------------------------------------------

#if defined(PARAM_ENABLE_TEX_BAND)

float BandTexture_DynamicEvaluateFloat(__global Texture *texture, __global HitPoint *hitPoint,
		const float amt) {
	const float a = clamp(amt, 0.f, 1.f);

	const uint last = texture->band.size - 1;
	if (a < texture->band.offsets[0])
		return Spectrum_Y(VLOAD3F(texture->band.values[0].c));
	else if (a >= texture->band.offsets[last])
		return Spectrum_Y(VLOAD3F(texture->band.values[last].c));
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
		return Lerp((a - o1) / (o0 - o1), p1, p0);
	}
}

float3 BandTexture_DynamicEvaluateSpectrum(__global Texture *texture, __global HitPoint *hitPoint,
		const float3 amt) {
	const float a = clamp(Spectrum_Y(amt), 0.f, 1.f);

	const uint last = texture->band.size - 1;
	if (a < texture->band.offsets[0])
		return VLOAD3F(texture->band.values[0].c);
	else if (a >= texture->band.offsets[last])
		return VLOAD3F(texture->band.values[last].c);
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
		return Lerp3((a - o1) / (o0 - o1), p1, p0);
	}
}

#if defined(PARAM_DIASBLE_TEX_DYNAMIC_EVALUATION)
void BandTexture_EvaluateFloat(__global Texture *texture, __global HitPoint *hitPoint,
		float texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
	const float a = texValues[--(*texValuesSize)];

	texValues[(*texValuesSize)++] = BandTexture_DynamicEvaluateFloat(texture, hitPoint, a);
}

void BandTexture_EvaluateSpectrum(__global Texture *texture, __global HitPoint *hitPoint,
		float3 texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
	const float3 a = texValues[--(*texValuesSize)];

	texValues[(*texValuesSize)++] = BandTexture_EvaluateSpectrum(texture, hitPoint, a);
}
#endif

#endif

//------------------------------------------------------------------------------
// HitPointColor texture
//------------------------------------------------------------------------------

#if defined(PARAM_ENABLE_TEX_HITPOINTCOLOR)

float HitPointColorTexture_DynamicEvaluateFloat(__global Texture *texture, __global HitPoint *hitPoint) {
	return Spectrum_Y(VLOAD3F(hitPoint->color.c));
}

float3 HitPointColorTexture_DynamicEvaluateSpectrum(__global Texture *texture, __global HitPoint *hitPoint) {
	return VLOAD3F(hitPoint->color.c);
}

#if defined(PARAM_DIASBLE_TEX_DYNAMIC_EVALUATION)
void HitPointColorTexture_EvaluateFloat(__global Texture *texture, __global HitPoint *hitPoint,
		float texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
	texValues[(*texValuesSize)++] = HitPointColorTexture_DynamicEvaluateFloat(texture, hitPoint);
}

void HitPointColorTexture_EvaluateSpectrum(__global Texture *texture, __global HitPoint *hitPoint,
		float3 texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
	texValues[(*texValuesSize)++] = HitPointColorTexture_DynamicEvaluateSpectrum(texture, hitPoint);
}
#endif

#endif

//------------------------------------------------------------------------------
// HitPointAlpha texture
//------------------------------------------------------------------------------

#if defined(PARAM_ENABLE_TEX_HITPOINTALPHA)

float HitPointAlphaTexture_DynamicEvaluateFloat(__global Texture *texture, __global HitPoint *hitPoint) {
	return hitPoint->alpha;
}

float3 HitPointAlphaTexture_DynamicEvaluateSpectrum(__global Texture *texture, __global HitPoint *hitPoint) {
	const float alpha = hitPoint->alpha;
	return (float3)(alpha, alpha, alpha);
}

#if defined(PARAM_DIASBLE_TEX_DYNAMIC_EVALUATION)
void HitPointAlphaTexture_EvaluateFloat(__global Texture *texture, __global HitPoint *hitPoint,
		float texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
	texValues[(*texValuesSize)++] = HitPointAlphaTexture_DynamicEvaluateFloat(texture, hitPoint);
}

void HitPointAlphaTexture_EvaluateSpectrum(__global Texture *texture, __global HitPoint *hitPoint,
		float3 texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
	texValues[(*texValuesSize)++] = HitPointAlphaTexture_DynamicEvaluateSpectrum(texture, hitPoint);
}
#endif

#endif

//------------------------------------------------------------------------------
// HitPointGrey texture
//------------------------------------------------------------------------------

#if defined(PARAM_ENABLE_TEX_HITPOINTGREY)

float HitPointGreyTexture_DynamicEvaluateFloat(__global Texture *texture, __global HitPoint *hitPoint) {
	const uint channel = texture->hitPointGrey.channel;
	switch (channel) {
		case 0:
			return hitPoint->color.c[0];
		case 1:
			return hitPoint->color.c[1];
		case 2:
			return hitPoint->color.c[2];
		default:
			return Spectrum_Y(VLOAD3F(hitPoint->color.c));
	}
}

float3 HitPointGreyTexture_DynamicEvaluateSpectrum(__global Texture *texture, __global HitPoint *hitPoint) {
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

	return (float3)(v, v, v);
}

#if defined(PARAM_DIASBLE_TEX_DYNAMIC_EVALUATION)
void HitPointGreyTexture_EvaluateFloat(__global Texture *texture, __global HitPoint *hitPoint,
		float texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
	texValues[(*texValuesSize)++] = HitPointGreyTexture_DynamicEvaluateFloat(texture, hitPoint);
}

void HitPointGreyTexture_EvaluateSpectrum(__global Texture *texture, __global HitPoint *hitPoint,
		float3 texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
	texValues[(*texValuesSize)++] = HitPointGreyTexture_DynamicEvaluateSpectrum(texture, hitPoint);
}
#endif

#endif

//------------------------------------------------------------------------------
// NormalMap texture
//------------------------------------------------------------------------------

#if defined(PARAM_ENABLE_TEX_NORMALMAP)

float NormalMapTexture_DynamicEvaluateFloat(__global Texture *texture, __global HitPoint *hitPoint) {
    return 0.f;
}

float3 NormalMapTexture_DynamicEvaluateSpectrum(__global Texture *texture, __global HitPoint *hitPoint) {
	return (float3)(0.f, 0.f, 0.f);
}

#if defined(PARAM_DIASBLE_TEX_DYNAMIC_EVALUATION)
void NormalMapTexture_EvaluateFloat(__global Texture *texture, __global HitPoint *hitPoint,
		float texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
    texValues[(*texValuesSize)++] = NormalMapTexture_DynamicEvaluateFloat(texture, hitPoint);
}

void NormalMapTexture_EvaluateSpectrum(__global Texture *texture, __global HitPoint *hitPoint,
		float3 texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
	texValues[(*texValuesSize)++] = NormalMapTexture_DynamicEvaluateSpectrum(texture, hitPoint);
}
#endif

#endif

//------------------------------------------------------------------------------
// Generic texture functions with support for recursive textures
//------------------------------------------------------------------------------

#if defined(PARAM_DIASBLE_TEX_DYNAMIC_EVALUATION)

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
#if defined(PARAM_ENABLE_FRESNEL_APPROX_N)
		case FRESNEL_APPROX_N:
			todoTex[(*todoTexSize)++] = &texs[texture->fresnelApproxN.texIndex];
			return 1;
#endif
#if defined(PARAM_ENABLE_FRESNEL_APPROX_K)
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
#if defined(PARAM_ENABLE_TEX_MIX)
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
#if defined(PARAM_ENABLE_TEX_BAND)
		case BAND_TEX:
			todoTex[(*todoTexSize)++] = &texs[texture->band.amountTexIndex];
			return 1;
#endif
#if defined(PARAM_ENABLE_TEX_NORMALMAP)
		case NORMALMAP_TEX:
#endif
#if defined(PARAM_ENABLE_TEX_HITPOINTGREY)
		case HITPOINTGREY:
#endif
#if defined(PARAM_ENABLE_TEX_HITPOINTALPHA)
		case HITPOINTALPHA:
#endif
#if defined(PARAM_ENABLE_TEX_HITPOINTCOLOR)
		case HITPOINTCOLOR:
#endif
#if defined(PARAM_ENABLE_TEX_UV)
		case UV_TEX:
#endif
#if defined(PARAM_ENABLE_WRINKLED)
		case WRINKLED:
#endif
#if defined(PARAM_ENABLE_BLENDER_WOOD)
		case BLENDER_WOOD:
#endif
#if defined(PARAM_ENABLE_BLENDER_CLOUDS)
		case BLENDER_CLOUDS:
#endif
#if defined(PARAM_ENABLE_WINDY)
		case WINDY:
#endif
#if defined(PARAM_ENABLE_MARBLE)
		case MARBLE:
#endif
#if defined(PARAM_ENABLE_FBM_TEX)
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

#endif

//------------------------------------------------------------------------------
// Float texture channel
//------------------------------------------------------------------------------

#if defined(PARAM_DIASBLE_TEX_DYNAMIC_EVALUATION)

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
#if defined(PARAM_ENABLE_FRESNEL_APPROX_N)
		case FRESNEL_APPROX_N:
			FresnelApproxNTexture_EvaluateFloat(texture, hitPoint, texValues, texValuesSize);
			break;
#endif
#if defined(PARAM_ENABLE_FRESNEL_APPROX_K)
		case FRESNEL_APPROX_K:
			FresnelApproxKTexture_EvaluateFloat(texture, hitPoint, texValues, texValuesSize);
			break;
#endif
#if defined(PARAM_ENABLE_CHECKERBOARD2D)
		case CHECKERBOARD2D:
			CheckerBoard2DTexture_EvaluateFloat(texture, hitPoint, texValues, texValuesSize);
			break;
#endif
#if defined(PARAM_ENABLE_CHECKERBOARD3D)
		case CHECKERBOARD3D:
			CheckerBoard3DTexture_EvaluateFloat(texture, hitPoint, texValues, texValuesSize);
			break;
#endif
#if defined(PARAM_ENABLE_TEX_MIX)
		case MIX_TEX:
			MixTexture_EvaluateFloat(texture, hitPoint, texValues, texValuesSize);
			break;
#endif
#if defined(PARAM_ENABLE_FBM_TEX)
		case FBM_TEX:
			FBMTexture_EvaluateFloat(texture, hitPoint, texValues, texValuesSize);
			break;
#endif
#if defined(PARAM_ENABLE_MARBLE)
		case MARBLE:
			MarbleTexture_EvaluateFloat(texture, hitPoint, texValues, texValuesSize);
			break;
#endif
#if defined(PARAM_ENABLE_DOTS)
		case DOTS:
			DotsTexture_EvaluateFloat(texture, hitPoint, texValues, texValuesSize);
			break;
#endif
#if defined(PARAM_ENABLE_BRICK)
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
#if defined(PARAM_ENABLE_BLENDER_CLOUDS)
		case BLENDER_CLOUDS:
			BlenderCloudsTexture_EvaluateFloat(texture, hitPoint, texValues, texValuesSize);
			break;
#endif
#if defined(PARAM_ENABLE_BLENDER_WOOD)
		case BLENDER_WOOD:
			BlenderWoodTexture_EvaluateFloat(texture, hitPoint, texValues, texValuesSize);
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
#if defined(PARAM_ENABLE_TEX_NORMALMAP)
		case NORMALMAP_TEX:
			NormalMapTexture_EvaluateFloat(texture, hitPoint, texValues, texValuesSize);
			break;
#endif
		default:
			// Do nothing
			break;
	}
}

float Texture_GetFloatValue(const uint texIndex, __global HitPoint *hitPoint
		TEXTURES_PARAM_DECL) {
	__global Texture *texture = &texs[texIndex];
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

#endif

//------------------------------------------------------------------------------
// Color texture channel
//------------------------------------------------------------------------------

#if defined(PARAM_DIASBLE_TEX_DYNAMIC_EVALUATION)

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
#if defined(PARAM_ENABLE_FRESNEL_APPROX_N)
		case FRESNEL_APPROX_N:
			FresnelApproxNTexture_EvaluateSpectrum(texture, hitPoint, texValues, texValuesSize);
			break;
#endif
#if defined(PARAM_ENABLE_FRESNEL_APPROX_K)
		case FRESNEL_APPROX_K:
			FresnelApproxKTexture_EvaluateSpectrum(texture, hitPoint, texValues, texValuesSize);
			break;
#endif
#if defined(PARAM_ENABLE_CHECKERBOARD2D)
		case CHECKERBOARD2D:
			CheckerBoard2DTexture_EvaluateSpectrum(texture, hitPoint, texValues, texValuesSize);
			break;
#endif
#if defined(PARAM_ENABLE_CHECKERBOARD3D)
		case CHECKERBOARD3D:
			CheckerBoard3DTexture_EvaluateSpectrum(texture, hitPoint, texValues, texValuesSize);
			break;
#endif
#if defined(PARAM_ENABLE_TEX_MIX)
		case MIX_TEX:
			MixTexture_EvaluateSpectrum(texture, hitPoint, texValues, texValuesSize);
			break;
#endif
#if defined(PARAM_ENABLE_FBM_TEX)
		case FBM_TEX:
			FBMTexture_EvaluateSpectrum(texture, hitPoint, texValues, texValuesSize);
			break;
#endif
#if defined(PARAM_ENABLE_MARBLE)
		case MARBLE:
			MarbleTexture_EvaluateSpectrum(texture, hitPoint, texValues, texValuesSize);
			break;
#endif
#if defined(PARAM_ENABLE_DOTS)
		case DOTS:
			DotsTexture_EvaluateSpectrum(texture, hitPoint, texValues, texValuesSize);
			break;
#endif
#if defined(PARAM_ENABLE_BRICK)
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
#if defined(PARAM_ENABLE_BLENDER_CLOUDS)
		case BLENDER_CLOUDS:
			BlenderCloudsTexture_EvaluateSpectrum(texture, hitPoint, texValues, texValuesSize);
			break;
#endif
#if defined(PARAM_ENABLE_BLENDER_WOOD)
		case BLENDER_WOOD:
			BlenderWoodTexture_EvaluateSpectrum(texture, hitPoint, texValues, texValuesSize);
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
#if defined(PARAM_ENABLE_TEX_NORMALMAP)
		case NORMALMAP_TEX:
			NormalMapTexture_EvaluateSpectrum(texture, hitPoint, texValues, texValuesSize);
			break;
#endif
		default:
			// Do nothing
			break;
	}
}

float3 Texture_GetSpectrumValue(const uint texIndex, __global HitPoint *hitPoint
		TEXTURES_PARAM_DECL) {
	__global Texture *texture = &texs[texIndex];

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

#endif
