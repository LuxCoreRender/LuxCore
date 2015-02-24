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

#if defined(PARAM_DISABLE_TEX_DYNAMIC_EVALUATION)
#ifndef TEXTURE_STACK_SIZE
#define TEXTURE_STACK_SIZE 16
#endif
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

float ConstFloatTexture_ConstEvaluateFloat(__global HitPoint *hitPoint,
		const float value) {
	return value;
}

float3 ConstFloatTexture_ConstEvaluateSpectrum(__global HitPoint *hitPoint,
		const float value) {
	return value;
}

#if defined(PARAM_DISABLE_TEX_DYNAMIC_EVALUATION)
void ConstFloatTexture_EvaluateFloat(__global Texture *texture, __global HitPoint *hitPoint,
		float texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
	texValues[(*texValuesSize)++] = ConstFloatTexture_ConstEvaluateFloat(hitPoint,
			texture->constFloat.value);
}

void ConstFloatTexture_EvaluateSpectrum(__global Texture *texture, __global HitPoint *hitPoint,
		float3 texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
	texValues[(*texValuesSize)++] = ConstFloatTexture_ConstEvaluateSpectrum(hitPoint,
			texture->constFloat.value);
}
#endif

#endif

//------------------------------------------------------------------------------
// ConstFloat3 texture
//------------------------------------------------------------------------------

#if defined(PARAM_ENABLE_TEX_CONST_FLOAT3)

float ConstFloat3Texture_ConstEvaluateFloat(__global HitPoint *hitPoint,
		const float3 value) {
	return Spectrum_Y(value);
}

float3 ConstFloat3Texture_ConstEvaluateSpectrum(__global HitPoint *hitPoint,
		const float3 value) {
	return value;
}

#if defined(PARAM_DISABLE_TEX_DYNAMIC_EVALUATION)
void ConstFloat3Texture_EvaluateFloat(__global Texture *texture, __global HitPoint *hitPoint,
		float texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
	texValues[(*texValuesSize)++] = ConstFloat3Texture_ConstEvaluateFloat(hitPoint,
			VLOAD3F(texture->constFloat3.color.c));
}

void ConstFloat3Texture_EvaluateSpectrum(__global Texture *texture, __global HitPoint *hitPoint,
		float3 texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
	texValues[(*texValuesSize)++] = ConstFloat3Texture_ConstEvaluateSpectrum(hitPoint,
			VLOAD3F(texture->constFloat3.color.c));
}
#endif

#endif

//------------------------------------------------------------------------------
// ImageMap texture
//------------------------------------------------------------------------------

#if defined(PARAM_ENABLE_TEX_IMAGEMAP)

float ImageMapTexture_ConstEvaluateFloat(__global HitPoint *hitPoint,
		const float gain, const uint imageMapIndex, __global TextureMapping2D *mapping
		IMAGEMAPS_PARAM_DECL) {
	__global ImageMap *imageMap = &imageMapDescs[imageMapIndex];
	__global float *pixels = ImageMap_GetPixelsAddress(
			imageMapBuff, imageMap->pageIndex, imageMap->pixelsIndex);

	const float2 uv = VLOAD2F(&hitPoint->uv.u);
	const float2 mapUV = TextureMapping2D_Map(mapping, hitPoint);

	return gain * ImageMap_GetFloat(
			pixels,
			imageMap->width, imageMap->height, imageMap->channelCount,
			mapUV.s0, mapUV.s1);
}

float3 ImageMapTexture_ConstEvaluateSpectrum(__global HitPoint *hitPoint,
		const float gain, const uint imageMapIndex, __global TextureMapping2D *mapping
		IMAGEMAPS_PARAM_DECL) {
	__global ImageMap *imageMap = &imageMapDescs[imageMapIndex];
	__global float *pixels = ImageMap_GetPixelsAddress(
			imageMapBuff, imageMap->pageIndex, imageMap->pixelsIndex);

	const float2 uv = VLOAD2F(&hitPoint->uv.u);
	const float2 mapUV = TextureMapping2D_Map(mapping, hitPoint);

	return gain * ImageMap_GetSpectrum(
			pixels,
			imageMap->width, imageMap->height, imageMap->channelCount,
			mapUV.s0, mapUV.s1);
}

#if defined(PARAM_DISABLE_TEX_DYNAMIC_EVALUATION)
void ImageMapTexture_EvaluateFloat(__global Texture *texture, __global HitPoint *hitPoint,
		float texValues[TEXTURE_STACK_SIZE], uint *texValuesSize
		IMAGEMAPS_PARAM_DECL) {
	texValues[(*texValuesSize)++] = ImageMapTexture_ConstEvaluateFloat(hitPoint,
			texture->imageMapTex.gain, texture->imageMapTex.imageMapIndex,
			&texture->imageMapTex.mapping
			IMAGEMAPS_PARAM);
}

void ImageMapTexture_EvaluateSpectrum(__global Texture *texture, __global HitPoint *hitPoint,
		float3 texValues[TEXTURE_STACK_SIZE], uint *texValuesSize
		IMAGEMAPS_PARAM_DECL) {
	texValues[(*texValuesSize)++] =  ImageMapTexture_ConstEvaluateSpectrum(hitPoint,
			texture->imageMapTex.gain, texture->imageMapTex.imageMapIndex,
			&texture->imageMapTex.mapping
			IMAGEMAPS_PARAM);
}
#endif

#endif

//------------------------------------------------------------------------------
// Scale texture
//------------------------------------------------------------------------------

#if defined(PARAM_ENABLE_TEX_SCALE)

float ScaleTexture_ConstEvaluateFloat(__global HitPoint *hitPoint,
		const float tex1, const float tex2) {
	return tex1 * tex2;
}

float3 ScaleTexture_ConstEvaluateSpectrum(__global HitPoint *hitPoint,
		const float3 tex1, const float3 tex2) {
	return tex1 * tex2;
}

#if defined(PARAM_DISABLE_TEX_DYNAMIC_EVALUATION)

void ScaleTexture_EvaluateFloat(__global Texture *texture, __global HitPoint *hitPoint,
		float texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
	const float tex1 = texValues[--(*texValuesSize)];
	const float tex2 = texValues[--(*texValuesSize)];
	
	texValues[(*texValuesSize)++] = ScaleTexture_ConstEvaluateFloat(hitPoint, tex1, tex2);
}

void ScaleTexture_EvaluateSpectrum(__global Texture *texture, __global HitPoint *hitPoint,
		float3 texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
	const float3 tex1 = texValues[--(*texValuesSize)];
	const float3 tex2 = texValues[--(*texValuesSize)];
	
	texValues[(*texValuesSize)++] = ScaleTexture_ConstEvaluateSpectrum(hitPoint, tex1, tex2);
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

float FresnelApproxNTexture_ConstEvaluateFloat(__global HitPoint *hitPoint,
		const float value) {
	return FresnelApproxN(value);
}

float3 FresnelApproxNTexture_ConstEvaluateSpectrum(__global HitPoint *hitPoint,
		const float3 value) {
	return FresnelApproxN3(value);
}

#if defined(PARAM_DISABLE_TEX_DYNAMIC_EVALUATION)
void FresnelApproxNTexture_EvaluateFloat(__global Texture *texture, __global HitPoint *hitPoint,
		float texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
	const float value = texValues[--(*texValuesSize)];

	texValues[(*texValuesSize)++] = FresnelApproxNTexture_ConstEvaluateFloat(hitPoint, value);
}

void FresnelApproxNTexture_EvaluateSpectrum(__global Texture *texture, __global HitPoint *hitPoint,
		float3 texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
	const float3 value = texValues[--(*texValuesSize)];

	texValues[(*texValuesSize)++] = FresnelApproxNTexture_ConstEvaluateSpectrum(hitPoint, value);
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

float FresnelApproxKTexture_ConstEvaluateFloat( __global HitPoint *hitPoint,
		const float value) {
	return FresnelApproxK(value);
}

float3 FresnelApproxKTexture_ConstEvaluateSpectrum(__global HitPoint *hitPoint,
		const float3 value) {
	return FresnelApproxK3(value);
}

#if defined(PARAM_DISABLE_TEX_DYNAMIC_EVALUATION)
void FresnelApproxKTexture_EvaluateFloat(__global Texture *texture, __global HitPoint *hitPoint,
		float texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
	const float value = texValues[--(*texValuesSize)];

	texValues[(*texValuesSize)++] = FresnelApproxKTexture_ConstEvaluateFloat(hitPoint, value);
}

void FresnelApproxKTexture_EvaluateSpectrum(__global Texture *texture, __global HitPoint *hitPoint,
		float3 texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
	const float3 value = texValues[--(*texValuesSize)];

	texValues[(*texValuesSize)++] = FresnelApproxKTexture_ConstEvaluateSpectrum(hitPoint, value);
}
#endif

#endif

//------------------------------------------------------------------------------
// Mix texture
//------------------------------------------------------------------------------

#if defined(PARAM_ENABLE_TEX_MIX)

float MixTexture_ConstEvaluateFloat(__global HitPoint *hitPoint,
		const float amt, const float value1, const float value2) {
	return Lerp(clamp(amt, 0.f, 1.f), value1, value2);
}

float3 MixTexture_ConstEvaluateSpectrum(__global HitPoint *hitPoint,
		const float3 amt, const float3 value1, const float3 value2) {
	return mix(value1, value2, clamp(amt, 0.f, 1.f));
}

#if defined(PARAM_DISABLE_TEX_DYNAMIC_EVALUATION)
void MixTexture_EvaluateFloat(__global Texture *texture, __global HitPoint *hitPoint,
		float texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
	const float amt = texValues[--(*texValuesSize)];
	const float value1 = texValues[--(*texValuesSize)];
	const float value2 = texValues[--(*texValuesSize)];

	texValues[(*texValuesSize)++] = MixTexture_ConstEvaluateFloat(hitPoint, amt, value1, value2);
}

void MixTexture_EvaluateSpectrum(__global Texture *texture, __global HitPoint *hitPoint,
		float3 texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
	const float3 amt = texValues[--(*texValuesSize)];
	const float3 value1 = texValues[--(*texValuesSize)];
	const float3 value2 = texValues[--(*texValuesSize)];

	texValues[(*texValuesSize)++] = MixTexture_ConstEvaluateSpectrum(hitPoint, amt, value1, value2);
}
#endif

#endif

//------------------------------------------------------------------------------
// CheckerBoard 2D & 3D texture
//------------------------------------------------------------------------------

#if defined(PARAM_ENABLE_CHECKERBOARD2D)

float CheckerBoard2DTexture_ConstEvaluateFloat(__global HitPoint *hitPoint,
		const float value1, const float value2, __global TextureMapping2D *mapping) {
	const float2 uv = VLOAD2F(&hitPoint->uv.u);
	const float2 mapUV = TextureMapping2D_Map(mapping, hitPoint);

	return ((Floor2Int(mapUV.s0) + Floor2Int(mapUV.s1)) % 2 == 0) ? value1 : value2;
}

float3 CheckerBoard2DTexture_ConstEvaluateSpectrum(__global HitPoint *hitPoint,
		const float3 value1, const float3 value2, __global TextureMapping2D *mapping) {
	const float2 uv = VLOAD2F(&hitPoint->uv.u);
	const float2 mapUV = TextureMapping2D_Map(mapping, hitPoint);

	return ((Floor2Int(mapUV.s0) + Floor2Int(mapUV.s1)) % 2 == 0) ? value1 : value2;
}

#if defined(PARAM_DISABLE_TEX_DYNAMIC_EVALUATION)
void CheckerBoard2DTexture_EvaluateFloat(__global Texture *texture, __global HitPoint *hitPoint,
		float texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
	const float value1 = texValues[--(*texValuesSize)];
	const float value2 = texValues[--(*texValuesSize)];

	texValues[(*texValuesSize)++] = CheckerBoard2DTexture_ConstEvaluateFloat(
			hitPoint, value1, value2, &texture->checkerBoard2D.mapping);
}

void CheckerBoard2DTexture_EvaluateSpectrum(__global Texture *texture, __global HitPoint *hitPoint,
		float3 texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
	const float3 value1 = texValues[--(*texValuesSize)];
	const float3 value2 = texValues[--(*texValuesSize)];

	texValues[(*texValuesSize)++] = CheckerBoard2DTexture_ConstEvaluateSpectrum(
			hitPoint, value1, value2, &texture->checkerBoard2D.mapping);
}
#endif

#endif

#if defined(PARAM_ENABLE_CHECKERBOARD3D)

float CheckerBoard3DTexture_ConstEvaluateFloat(__global HitPoint *hitPoint,
		const float value1, const float value2, __global TextureMapping3D *mapping) {
	const float3 mapP = TextureMapping3D_Map(mapping, hitPoint);

	return ((Floor2Int(mapP.x) + Floor2Int(mapP.y) + Floor2Int(mapP.z)) % 2 == 0) ? value1 : value2;
}

float3 CheckerBoard3DTexture_ConstEvaluateSpectrum(__global HitPoint *hitPoint,
		const float3 value1, const float3 value2, __global TextureMapping3D *mapping) {
	const float3 mapP = TextureMapping3D_Map(mapping, hitPoint);

	return ((Floor2Int(mapP.x) + Floor2Int(mapP.y) + Floor2Int(mapP.z)) % 2 == 0) ? value1 : value2;
}

#if defined(PARAM_DISABLE_TEX_DYNAMIC_EVALUATION)
void CheckerBoard3DTexture_EvaluateFloat(__global Texture *texture, __global HitPoint *hitPoint,
		float texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
	const float value1 = texValues[--(*texValuesSize)];
	const float value2 = texValues[--(*texValuesSize)];

	texValues[(*texValuesSize)++] = CheckerBoard3DTexture_ConstEvaluateFloat(
			hitPoint, value1, value2, &texture->checkerBoard3D.mapping);
}

void CheckerBoard3DTexture_EvaluateSpectrum(__global Texture *texture, __global HitPoint *hitPoint,
		float3 texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
	const float3 value1 = texValues[--(*texValuesSize)];
	const float3 value2 = texValues[--(*texValuesSize)];

	texValues[(*texValuesSize)++] = CheckerBoard3DTexture_ConstEvaluateSpectrum(
			hitPoint, value1, value2, &texture->checkerBoard3D.mapping);
}
#endif

#endif

//------------------------------------------------------------------------------
// FBM texture
//------------------------------------------------------------------------------

#if defined(PARAM_ENABLE_FBM_TEX)

float FBMTexture_ConstEvaluateFloat(__global HitPoint *hitPoint,
		const float omega, const int octaves, __global TextureMapping3D *mapping) {
	const float3 mapP = TextureMapping3D_Map(mapping, hitPoint);

	return FBm(mapP, omega, octaves);
}

float3 FBMTexture_ConstEvaluateSpectrum(__global HitPoint *hitPoint,
		const float omega, const int octaves, __global TextureMapping3D *mapping) {
	const float3 mapP = TextureMapping3D_Map(mapping, hitPoint);

	return FBm(mapP, omega, octaves);
}

#if defined(PARAM_DISABLE_TEX_DYNAMIC_EVALUATION)
void FBMTexture_EvaluateFloat(__global Texture *texture, __global HitPoint *hitPoint,
		float texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
	texValues[(*texValuesSize)++] = FBMTexture_ConstEvaluateFloat(hitPoint,
			texture->fbm.omega, texture->fbm.octaves, &texture->fbm.mapping);
}

void FBMTexture_EvaluateSpectrum(__global Texture *texture, __global HitPoint *hitPoint,
		float3 texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
	texValues[(*texValuesSize)++] = FBMTexture_ConstEvaluateSpectrum(hitPoint,
			texture->fbm.omega, texture->fbm.octaves, &texture->fbm.mapping);
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

float3 MarbleTexture_Evaluate(__global HitPoint *hitPoint, const float scale,
		const float omega, const int octaves, const float variation,
		__global TextureMapping3D *mapping) {
	const float3 P = scale * TextureMapping3D_Map(mapping, hitPoint);

	float marble = P.y + variation * FBm(P, omega, octaves);
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

float MarbleTexture_ConstEvaluateFloat(__global HitPoint *hitPoint, const float scale,
		const float omega, const int octaves, const float variation,
		__global TextureMapping3D *mapping) {
	return Spectrum_Y(MarbleTexture_Evaluate(hitPoint, scale, omega, octaves,
			variation, mapping));
}

float3 MarbleTexture_ConstEvaluateSpectrum(__global HitPoint *hitPoint, const float scale,
		const float omega, const int octaves, const float variation,
		__global TextureMapping3D *mapping) {
	return MarbleTexture_Evaluate(hitPoint, scale, omega, octaves,
			variation, mapping);
}

#if defined(PARAM_DISABLE_TEX_DYNAMIC_EVALUATION)
void MarbleTexture_EvaluateFloat(__global Texture *texture, __global HitPoint *hitPoint,
		float texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
	texValues[(*texValuesSize)++] = MarbleTexture_ConstEvaluateFloat(hitPoint,
			texture->marble.scale, texture->marble.omega, texture->marble.octaves,
			texture->marble.variation, &texture->marble.mapping);
}

void MarbleTexture_EvaluateSpectrum(__global Texture *texture, __global HitPoint *hitPoint,
		float3 texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
	texValues[(*texValuesSize)++] = MarbleTexture_ConstEvaluateSpectrum(hitPoint,
			texture->marble.scale, texture->marble.omega, texture->marble.octaves,
			texture->marble.variation, &texture->marble.mapping);
}
#endif

#endif

//------------------------------------------------------------------------------
// Dots texture
//------------------------------------------------------------------------------

#if defined(PARAM_ENABLE_DOTS)

bool DotsTexture_Evaluate(__global HitPoint *hitPoint, __global TextureMapping2D *mapping) {
	const float2 uv = TextureMapping2D_Map(mapping, hitPoint);

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
			return true;
	}

	return false;
}

float DotsTexture_ConstEvaluateFloat(__global HitPoint *hitPoint,
		const float value1, const float value2, __global TextureMapping2D *mapping) {
	return DotsTexture_Evaluate(hitPoint, mapping) ? value1 : value2;
}

float3 DotsTexture_ConstEvaluateSpectrum(__global HitPoint *hitPoint,
		const float3 value1, const float3 value2, __global TextureMapping2D *mapping) {
	return DotsTexture_Evaluate(hitPoint, mapping) ? value1 : value2;
}

#if defined(PARAM_DISABLE_TEX_DYNAMIC_EVALUATION)
void DotsTexture_EvaluateFloat(__global Texture *texture, __global HitPoint *hitPoint,
		float texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
	const float value1 = texValues[--(*texValuesSize)];
	const float value2 = texValues[--(*texValuesSize)];

	texValues[(*texValuesSize)++] = DotsTexture_ConstEvaluateFloat(hitPoint, value1, value2,
			&texture->dots.mapping);
}

void DotsTexture_EvaluateSpectrum(__global Texture *texture, __global HitPoint *hitPoint,
		float3 texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
	const float3 value1 = texValues[--(*texValuesSize)];
	const float3 value2 = texValues[--(*texValuesSize)];

	texValues[(*texValuesSize)++] = DotsTexture_ConstEvaluateSpectrum(hitPoint, value1, value2,
			&texture->dots.mapping);
}
#endif

#endif

//------------------------------------------------------------------------------
// Brick texture
//------------------------------------------------------------------------------

#if defined(PARAM_ENABLE_BRICK)

bool BrickTexture_RunningAlternate(const float3 p, float3 *i, float3 *b,
		const float run , const float mortarwidth,
		const float mortarheight, const float mortardepth,
		int nWhole) {
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

bool BrickTexture_Basket(const float3 p, float3 *i,
		const float mortarwidth, const float mortardepth,
		const float proportion, const float invproportion) {
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

bool BrickTexture_Herringbone(const float3 p, float3 *i,
		const float mortarwidth, const float mortarheight,
		const float proportion, const float invproportion) {
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

bool BrickTexture_Running(const float3 p, float3 *i, float3 *b,
		const float run , const float mortarwidth,
		const float mortarheight, const float mortardepth) {
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

bool BrickTexture_English(const float3 p, float3 *i, float3 *b,
		const float run , const float mortarwidth,
		const float mortarheight, const float mortardepth) {
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

bool BrickTexture_Evaluate(__global HitPoint *hitPoint,
		const MasonryBond bond,
		const float brickwidth, const float brickheight,
		const float brickdepth, const float mortarsize,
		const float3 offset,
		const float run , const float mortarwidth,
		const float mortarheight, const float mortardepth,
		const float proportion, const float invproportion,
		__global TextureMapping3D *mapping) {
#define BRICK_EPSILON 1e-3f
	const float3 P = TextureMapping3D_Map(mapping, hitPoint);

	const float offs = BRICK_EPSILON + mortarsize;
	float3 bP = P + (float3)(offs, offs, offs);

	// Normalize coordinates according brick dimensions
	bP.x /= brickwidth;
	bP.y /= brickdepth;
	bP.z /= brickheight;

	bP += offset;

	float3 brickIndex;
	float3 bevel;
	bool b;
	switch (bond) {
		case FLEMISH:
			b = BrickTexture_RunningAlternate(bP, &brickIndex, &bevel,
					run , mortarwidth, mortarheight, mortardepth, 1);
			break;
		case RUNNING:
			b = BrickTexture_Running(bP, &brickIndex, &bevel,
					run, mortarwidth, mortarheight, mortardepth);
			break;
		case ENGLISH:
			b = BrickTexture_English(bP, &brickIndex, &bevel,
					run, mortarwidth, mortarheight, mortardepth);
			break;
		case HERRINGBONE:
			b = BrickTexture_Herringbone(bP, &brickIndex,
					mortarwidth, mortarheight, proportion, invproportion);
			break;
		case BASKET:
			b = BrickTexture_Basket(bP, &brickIndex,
					mortarwidth, mortardepth, proportion, invproportion);
			break;
		case KETTING:
			b = BrickTexture_RunningAlternate(bP, &brickIndex, &bevel,
					run, mortarwidth, mortarheight, mortardepth, 2);
			break; 
		default:
			b = true;
			break;
	}

	return b;
#undef BRICK_EPSILON
}

float BrickTexture_ConstEvaluateFloat(__global HitPoint *hitPoint,
		const float value1, const float value2, const float value3,
		const MasonryBond bond,
		const float brickwidth, const float brickheight,
		const float brickdepth, const float mortarsize,
		const float3 offset,
		const float run , const float mortarwidth,
		const float mortarheight, const float mortardepth,
		const float proportion, const float invproportion,
		__global TextureMapping3D *mapping) {
	return BrickTexture_Evaluate(hitPoint,
			bond,
			brickwidth, brickheight,
			brickdepth, mortarsize,
			offset,
			run , mortarwidth,
			mortarheight, mortardepth,
			proportion, invproportion,
			mapping) ? (value1 * value3) : value2;
}

float3 BrickTexture_ConstEvaluateSpectrum(__global HitPoint *hitPoint,
		const float3 value1, const float3 value2, const float3 value3,
		const MasonryBond bond,
		const float brickwidth, const float brickheight,
		const float brickdepth, const float mortarsize,
		const float3 offset,
		const float run , const float mortarwidth,
		const float mortarheight, const float mortardepth,
		const float proportion, const float invproportion,
		__global TextureMapping3D *mapping) {
	return BrickTexture_Evaluate(hitPoint,
			bond,
			brickwidth, brickheight,
			brickdepth, mortarsize,
			offset,
			run , mortarwidth,
			mortarheight, mortardepth,
			proportion, invproportion,
			mapping) ? (value1 * value3) : value2;
}

#if defined(PARAM_DISABLE_TEX_DYNAMIC_EVALUATION)
void BrickTexture_EvaluateFloat(__global Texture *texture, __global HitPoint *hitPoint,
		float texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
	const float value1 = texValues[--(*texValuesSize)];
	const float value2 = texValues[--(*texValuesSize)];
	const float value3 = texValues[--(*texValuesSize)];

	texValues[(*texValuesSize)++] = BrickTexture_ConstEvaluateFloat(hitPoint,
			value1, value2, value3,
			texture->brick.bond,
			texture->brick.brickwidth, texture->brick.brickheight,
			texture->brick.brickdepth, texture->brick.mortarsize,
			VLOAD3F(&texture->brick.offsetx),
			texture->brick.run , texture->brick.mortarwidth,
			texture->brick.mortarheight, texture->brick.mortardepth,
			texture->brick.proportion, texture->brick.invproportion,
			&texture->brick.mapping);
}

void BrickTexture_EvaluateSpectrum(__global Texture *texture, __global HitPoint *hitPoint,
		float3 texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
	const float3 value1 = texValues[--(*texValuesSize)];
	const float3 value2 = texValues[--(*texValuesSize)];
	const float3 value3 = texValues[--(*texValuesSize)];

	texValues[(*texValuesSize)++] = BrickTexture_ConstEvaluateSpectrum(hitPoint,
			value1, value2, value3,
			texture->brick.bond,
			texture->brick.brickwidth, texture->brick.brickheight,
			texture->brick.brickdepth, texture->brick.mortarsize,
			VLOAD3F(&texture->brick.offsetx),
			texture->brick.run , texture->brick.mortarwidth,
			texture->brick.mortarheight, texture->brick.mortardepth,
			texture->brick.proportion, texture->brick.invproportion,
			&texture->brick.mapping);
}
#endif

#endif

//------------------------------------------------------------------------------
// Add texture
//------------------------------------------------------------------------------

#if defined(PARAM_ENABLE_TEX_ADD)

float AddTexture_ConstEvaluateFloat(__global HitPoint *hitPoint,
		const float value1, const float value2) {
	return value1 + value2;
}

float3 AddTexture_ConstEvaluateSpectrum(__global HitPoint *hitPoint,
		const float3 value1, const float3 value2) {
	return value1 + value2;
}

#if defined(PARAM_DISABLE_TEX_DYNAMIC_EVALUATION)
void AddTexture_EvaluateFloat(__global Texture *texture, __global HitPoint *hitPoint,
		float texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
	const float value1 = texValues[--(*texValuesSize)];
	const float value2 = texValues[--(*texValuesSize)];

	texValues[(*texValuesSize)++] = AddTexture_ConstEvaluateFloat(hitPoint, value1, value2);
}

void AddTexture_EvaluateSpectrum(__global Texture *texture, __global HitPoint *hitPoint,
		float3 texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
	const float3 value1 = texValues[--(*texValuesSize)];
	const float3 value2 = texValues[--(*texValuesSize)];

	texValues[(*texValuesSize)++] = AddTexture_ConstEvaluateSpectrum(hitPoint, value1, value2);
}
#endif

#endif

//------------------------------------------------------------------------------
// Subtract texture
//------------------------------------------------------------------------------

#if defined(PARAM_ENABLE_TEX_SUBTRACT)

float SubtractTexture_ConstEvaluateFloat(__global HitPoint *hitPoint,
const float value1, const float value2) {
	return value1 - value2;
}

float3 SubtractTexture_ConstEvaluateSpectrum(__global HitPoint *hitPoint,
const float3 value1, const float3 value2) {
	return value1 - value2;
}

#if defined(PARAM_DISABLE_TEX_DYNAMIC_EVALUATION)
void SubtractTexture_EvaluateFloat(__global Texture *texture, __global HitPoint *hitPoint,
float texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
	const float value1 = texValues[--(*texValuesSize)];
	const float value2 = texValues[--(*texValuesSize)];
	
	texValues[(*texValuesSize)++] = SubtractTexture_ConstEvaluateFloat(hitPoint, value1, value2);
}

void SubtractTexture_EvaluateSpectrum(__global Texture *texture, __global HitPoint *hitPoint,
float3 texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
	const float3 value1 = texValues[--(*texValuesSize)];
	const float3 value2 = texValues[--(*texValuesSize)];
	
	texValues[(*texValuesSize)++] = SubtractTexture_ConstEvaluateSpectrum(hitPoint, value1, value2);
}
#endif

#endif

//------------------------------------------------------------------------------
// Windy texture
//------------------------------------------------------------------------------

#if defined(PARAM_ENABLE_WINDY)

float WindyTexture_ConstEvaluateFloat(__global HitPoint *hitPoint,
		__global TextureMapping3D *mapping) {
	const float3 mapP = TextureMapping3D_Map(mapping, hitPoint);

	const float windStrength = FBm(.1f * mapP, .5f, 3);
	const float waveHeight = FBm(mapP, .5f, 6);

	return fabs(windStrength) * waveHeight;
}

float3 WindyTexture_ConstEvaluateSpectrum(__global HitPoint *hitPoint,
		__global TextureMapping3D *mapping) {
	return WindyTexture_ConstEvaluateFloat(hitPoint, mapping);
}

#if defined(PARAM_DISABLE_TEX_DYNAMIC_EVALUATION)
void WindyTexture_EvaluateFloat(__global Texture *texture, __global HitPoint *hitPoint,
		float texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
	texValues[(*texValuesSize)++] = WindyTexture_ConstEvaluateFloat(hitPoint,
			&texture->windy.mapping);
}

void WindyTexture_EvaluateSpectrum(__global Texture *texture, __global HitPoint *hitPoint,
		float3 texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
	texValues[(*texValuesSize)++] = WindyTexture_ConstEvaluateSpectrum(hitPoint,
			&texture->windy.mapping);
}
#endif

#endif

//------------------------------------------------------------------------------
// Wrinkled texture
//------------------------------------------------------------------------------

#if defined(PARAM_ENABLE_WRINKLED)

float WrinkledTexture_ConstEvaluateFloat(__global HitPoint *hitPoint,
		const float omega, const int octaves,
		__global TextureMapping3D *mapping) {
	const float3 mapP = TextureMapping3D_Map(mapping, hitPoint);

	return Turbulence(mapP, omega, octaves);
}

float3 WrinkledTexture_ConstEvaluateSpectrum(__global HitPoint *hitPoint,
		const float omega, const int octaves,
		__global TextureMapping3D *mapping) {
	return WrinkledTexture_ConstEvaluateFloat(hitPoint, omega, octaves, mapping);
}

#if defined(PARAM_DISABLE_TEX_DYNAMIC_EVALUATION)
void WrinkledTexture_EvaluateFloat(__global Texture *texture, __global HitPoint *hitPoint,
		float texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
	texValues[(*texValuesSize)++] = WrinkledTexture_ConstEvaluateFloat(hitPoint,
			texture->wrinkled.omega, texture->wrinkled.octaves,
			&texture->wrinkled.mapping);
}

void WrinkledTexture_EvaluateSpectrum(__global Texture *texture, __global HitPoint *hitPoint,
		float3 texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
	texValues[(*texValuesSize)++] = WrinkledTexture_ConstEvaluateSpectrum(hitPoint,
			texture->wrinkled.omega, texture->wrinkled.octaves,
			&texture->wrinkled.mapping);
}
#endif

#endif

//------------------------------------------------------------------------------
// UV texture
//------------------------------------------------------------------------------

#if defined(PARAM_ENABLE_TEX_UV)

float UVTexture_ConstEvaluateFloat(__global HitPoint *hitPoint,
		__global TextureMapping2D *mapping) {
	const float2 uv = TextureMapping2D_Map(mapping, hitPoint);

	return Spectrum_Y((float3)(uv.s0 - Floor2Int(uv.s0), uv.s1 - Floor2Int(uv.s1), 0.f));
}

float3 UVTexture_ConstEvaluateSpectrum(__global HitPoint *hitPoint,
		__global TextureMapping2D *mapping) {
	const float2 uv = TextureMapping2D_Map(mapping, hitPoint);

	return (float3)(uv.s0 - Floor2Int(uv.s0), uv.s1 - Floor2Int(uv.s1), 0.f);
}

#if defined(PARAM_DISABLE_TEX_DYNAMIC_EVALUATION)
void UVTexture_EvaluateFloat(__global Texture *texture, __global HitPoint *hitPoint,
		float texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
	texValues[(*texValuesSize)++] = UVTexture_ConstEvaluateFloat(hitPoint,
			&texture->uvTex.mapping);
}

void UVTexture_EvaluateSpectrum(__global Texture *texture, __global HitPoint *hitPoint,
		float3 texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
	texValues[(*texValuesSize)++] = UVTexture_ConstEvaluateSpectrum(hitPoint,
			&texture->uvTex.mapping);
}
#endif

#endif

//------------------------------------------------------------------------------
// Band texture
//------------------------------------------------------------------------------

#if defined(PARAM_ENABLE_TEX_BAND)

float BandTexture_ConstEvaluateFloat(__global HitPoint *hitPoint,
		const uint size, __global float *offsets,
		__global Spectrum *values, const float amt) {
	const float a = clamp(amt, 0.f, 1.f);

	const uint last = size - 1;
	if (a < offsets[0])
		return Spectrum_Y(VLOAD3F(values[0].c));
	else if (a >= offsets[last])
		return Spectrum_Y(VLOAD3F(values[last].c));
	else {
		uint p = 0;
		for (; p <= last; ++p) {
			if (a < offsets[p])
				break;
		}

		const float p1 = Spectrum_Y(VLOAD3F(values[p - 1].c));
		const float p0 = Spectrum_Y(VLOAD3F(values[p].c));
		const float o1 = offsets[p - 1];
		const float o0 = offsets[p];
		return Lerp((a - o1) / (o0 - o1), p1, p0);
	}
}

float3 BandTexture_ConstEvaluateSpectrum(__global HitPoint *hitPoint,
		const uint size, __global float *offsets,
		__global Spectrum *values, const float3 amt) {
	const float a = clamp(Spectrum_Y(amt), 0.f, 1.f);

	const uint last = size - 1;
	if (a < offsets[0])
		return VLOAD3F(values[0].c);
	else if (a >= offsets[last])
		return VLOAD3F(values[last].c);
	else {
		uint p = 0;
		for (; p <= last; ++p) {
			if (a < offsets[p])
				break;
		}

		const float3 p1 = VLOAD3F(values[p - 1].c);
		const float3 p0 = VLOAD3F(values[p].c);
		const float o1 = offsets[p - 1];
		const float o0 = offsets[p];
		return Lerp3((a - o1) / (o0 - o1), p1, p0);
	}
}

#if defined(PARAM_DISABLE_TEX_DYNAMIC_EVALUATION)
void BandTexture_EvaluateFloat(__global Texture *texture, __global HitPoint *hitPoint,
		float texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
	const float a = texValues[--(*texValuesSize)];

	texValues[(*texValuesSize)++] = BandTexture_ConstEvaluateFloat(hitPoint, texture->band.size,
			texture->band.offsets, texture->band.values, a);
}

void BandTexture_EvaluateSpectrum(__global Texture *texture, __global HitPoint *hitPoint,
		float3 texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
	const float3 a = texValues[--(*texValuesSize)];

	texValues[(*texValuesSize)++] = BandTexture_ConstEvaluateSpectrum(hitPoint, texture->band.size,
			texture->band.offsets, texture->band.values, a);
}
#endif

#endif

//------------------------------------------------------------------------------
// HitPointColor texture
//------------------------------------------------------------------------------

#if defined(PARAM_ENABLE_TEX_HITPOINTCOLOR)

float HitPointColorTexture_ConstEvaluateFloat(__global HitPoint *hitPoint) {
	return Spectrum_Y(VLOAD3F(hitPoint->color.c));
}

float3 HitPointColorTexture_ConstEvaluateSpectrum(__global HitPoint *hitPoint) {
	return VLOAD3F(hitPoint->color.c);
}

#if defined(PARAM_DISABLE_TEX_DYNAMIC_EVALUATION)
void HitPointColorTexture_EvaluateFloat(__global Texture *texture, __global HitPoint *hitPoint,
		float texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
	texValues[(*texValuesSize)++] = HitPointColorTexture_ConstEvaluateFloat(hitPoint);
}

void HitPointColorTexture_EvaluateSpectrum(__global Texture *texture, __global HitPoint *hitPoint,
		float3 texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
	texValues[(*texValuesSize)++] = HitPointColorTexture_ConstEvaluateSpectrum(hitPoint);
}
#endif

#endif

//------------------------------------------------------------------------------
// HitPointAlpha texture
//------------------------------------------------------------------------------

#if defined(PARAM_ENABLE_TEX_HITPOINTALPHA)

float HitPointAlphaTexture_ConstEvaluateFloat(__global HitPoint *hitPoint) {
	return hitPoint->alpha;
}

float3 HitPointAlphaTexture_ConstEvaluateSpectrum(__global HitPoint *hitPoint) {
	const float alpha = hitPoint->alpha;
	return (float3)(alpha, alpha, alpha);
}

#if defined(PARAM_DISABLE_TEX_DYNAMIC_EVALUATION)
void HitPointAlphaTexture_EvaluateFloat(__global Texture *texture, __global HitPoint *hitPoint,
		float texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
	texValues[(*texValuesSize)++] = HitPointAlphaTexture_ConstEvaluateFloat(hitPoint);
}

void HitPointAlphaTexture_EvaluateSpectrum(__global Texture *texture, __global HitPoint *hitPoint,
		float3 texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
	texValues[(*texValuesSize)++] = HitPointAlphaTexture_ConstEvaluateSpectrum(hitPoint);
}
#endif

#endif

//------------------------------------------------------------------------------
// HitPointGrey texture
//------------------------------------------------------------------------------

#if defined(PARAM_ENABLE_TEX_HITPOINTGREY)

float HitPointGreyTexture_ConstEvaluateFloat(__global HitPoint *hitPoint, const uint channel) {
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

float3 HitPointGreyTexture_ConstEvaluateSpectrum(__global HitPoint *hitPoint, const uint channel) {
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

#if defined(PARAM_DISABLE_TEX_DYNAMIC_EVALUATION)
void HitPointGreyTexture_EvaluateFloat(__global Texture *texture, __global HitPoint *hitPoint,
		float texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
	texValues[(*texValuesSize)++] = HitPointGreyTexture_ConstEvaluateFloat(hitPoint,
			texture->hitPointGrey.channel);
}

void HitPointGreyTexture_EvaluateSpectrum(__global Texture *texture, __global HitPoint *hitPoint,
		float3 texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
	texValues[(*texValuesSize)++] = HitPointGreyTexture_ConstEvaluateSpectrum(hitPoint,
			texture->hitPointGrey.channel);
}
#endif

#endif

//------------------------------------------------------------------------------
// NormalMap texture
//------------------------------------------------------------------------------

#if defined(PARAM_ENABLE_TEX_NORMALMAP)

float NormalMapTexture_ConstEvaluateFloat(__global HitPoint *hitPoint) {
    return 0.f;
}

float3 NormalMapTexture_ConstEvaluateSpectrum(__global HitPoint *hitPoint) {
	return (float3)(0.f, 0.f, 0.f);
}

#if defined(PARAM_DISABLE_TEX_DYNAMIC_EVALUATION)
void NormalMapTexture_EvaluateFloat(__global Texture *texture, __global HitPoint *hitPoint,
		float texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
    texValues[(*texValuesSize)++] = NormalMapTexture_ConstEvaluateFloat(hitPoint);
}

void NormalMapTexture_EvaluateSpectrum(__global Texture *texture, __global HitPoint *hitPoint,
		float3 texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
	texValues[(*texValuesSize)++] = NormalMapTexture_ConstEvaluateSpectrum(hitPoint);
}
#endif

#endif

//------------------------------------------------------------------------------
// Generic texture functions with support for recursive textures
//------------------------------------------------------------------------------

#if defined(PARAM_DISABLE_TEX_DYNAMIC_EVALUATION)

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
#if defined(PARAM_ENABLE_TEX_SUBTRACT)
		case SUBTRACT_TEX:
			todoTex[(*todoTexSize)++] = &texs[texture->subtractTex.tex1Index];
			todoTex[(*todoTexSize)++] = &texs[texture->subtractTex.tex2Index];
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

#if defined(PARAM_DISABLE_TEX_DYNAMIC_EVALUATION)

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
#if defined(PARAM_ENABLE_TEX_SUBTRACT)
		case SUBTRACT_TEX:
			SubtractTexture_EvaluateFloat(texture, hitPoint, texValues, texValuesSize);
			break;
			#endif
#if defined(PARAM_ENABLE_WINDY)
		case WINDY:
			WindyTexture_EvaluateFloat(texture, hitPoint, texValues, texValuesSize);
			break;
#endif
#if defined(PARAM_ENABLE_BLENDER_BLEND)
		case BLENDER_BLEND:
			BlenderBlendTexture_EvaluateFloat(texture, hitPoint, texValues, texValuesSize);
			break;
+#endif#if defined(PARAM_ENABLE_BLENDER_CLOUDS)
		case BLENDER_CLOUDS:
			BlenderCloudsTexture_EvaluateFloat(texture, hitPoint, texValues, texValuesSize);
			break;
#endif
#if defined(PARAM_ENABLE_BLENDER_DISTORTED_NOISE)
		case BLENDER_DISTORTED_NOISE:		    
			BlenderDistortedNoiseTexture_EvaluateFloat(texture, hitPoint, texValues, texValuesSize);
			break;
#endif
#if defined(PARAM_ENABLE_BLENDER_MAGIC)
		case BLENDER_MAGIC:
			BlenderMagicTexture_EvaluateFloat(texture, hitPoint, texValues, texValuesSize);
			break;
#endif
#if defined(PARAM_ENABLE_BLENDER_MARBLE)
		case BLENDER_MARBLE:
			BlenderMarbleTexture_EvaluateFloat(texture, hitPoint, texValues, texValuesSize);
			break;
#endif
#if defined(PARAM_ENABLE_BLENDER_STUCCI)
		case BLENDER_STUCCI:
			BlenderStucciTexture_EvaluateFloat(texture, hitPoint, texValues, texValuesSize);
			break;
#endif
#if defined(PARAM_ENABLE_BLENDER_WOOD)
		case BLENDER_WOOD:
			BlenderWoodTexture_EvaluateFloat(texture, hitPoint, texValues, texValuesSize);
			break;
#endif
#if defined(PARAM_ENABLE_BLENDER_VORONOI)
		case BLENDER_VORONOID:
			BlenderVoronoiTexture_EvaluateFloat(texture, hitPoint, texValues, texValuesSize);
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

#if defined(PARAM_DISABLE_TEX_DYNAMIC_EVALUATION)

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
#if defined(PARAM_ENABLE_TEX_SUBTRACT)
		case SUBTRACT_TEX:
			SubtractTexture_EvaluateSpectrum(texture, hitPoint, texValues, texValuesSize);
			break;
#endif
#if defined(PARAM_ENABLE_WINDY)
		case WINDY:
			WindyTexture_EvaluateSpectrum(texture, hitPoint, texValues, texValuesSize);
			break;
#endif
#if defined(PARAM_ENABLE_BLENDER_BLEND)
		case BLENDER_BLEND:
			BlenderBlendTexture_EvaluateSpectrum(texture, hitPoint, texValues, texValuesSize);
			break;
#endif
#if defined(PARAM_ENABLE_BLENDER_CLOUDS)
		case BLENDER_CLOUDS:
			BlenderCloudsTexture_EvaluateSpectrum(texture, hitPoint, texValues, texValuesSize);
			break;
#endif
#if defined(PARAM_ENABLE_BLENDER_DISTORTED_NOISE)
		case BLENDER_DISTORTED_NOISE:
			BlenderDistortedNoiseTexture_EvaluateSpectrum(texture, hitPoint, texValues, texValuesSize);
			break;
#endif
#if defined(PARAM_ENABLE_BLENDER_MAGIC)
		case BLENDER_MAGIC:
			BlenderMagicTexture_EvaluateSpectrum(texture, hitPoint, texValues, texValuesSize);
			break;
#endif
#if defined(PARAM_ENABLE_BLENDER_MARBLE)
		case BLENDER_MARBLE:
			BlenderMarbleTexture_EvaluateSpectrum(texture, hitPoint, texValues, texValuesSize);
			break;
#endif
#if defined(PARAM_ENABLE_BLENDER_STUCCI)
		case BLENDER_STUCCI:
			BlenderStucciTexture_EvaluateSpectrum(texture, hitPoint, texValues, texValuesSize);
			break;
#endif
#if defined(PARAM_ENABLE_BLENDER_WOOD)
		case BLENDER_WOOD:
			BlenderWoodTexture_EvaluateSpectrum(texture, hitPoint, texValues, texValuesSize);
			break;
#endif
#if defined(PARAM_ENABLE_BLENDER_VORONOI)
		case BLENDER_VORONOI:
			BlenderVoronoiTexture_EvaluateSpectrum(texture, hitPoint, texValues, texValuesSize);
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
