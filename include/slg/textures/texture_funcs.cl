#line 2 "texture_funcs.cl"

/***************************************************************************
 * Copyright 1998-2018 by authors (see AUTHORS.txt)                        *
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

//------------------------------------------------------------------------------
// ConstFloat texture
//
// NOTE: the code for this type of texture is not dynamically generated in order
// to reduce the number of kernels compilations
//------------------------------------------------------------------------------

#if defined(PARAM_ENABLE_TEX_CONST_FLOAT)

OPENCL_FORCE_INLINE float ConstFloatTexture_ConstEvaluateFloat(__global const Texture *tex) {
	return tex->constFloat.value;
}

OPENCL_FORCE_INLINE float3 ConstFloatTexture_ConstEvaluateSpectrum(__global const Texture *tex) {
	return tex->constFloat.value;
}

// Note: ConstFloatTexture_Bump() is defined in texture_bump_funcs.cl

#endif

//------------------------------------------------------------------------------
// ConstFloat3 texture
//
// NOTE: the code for this type of texture is not dynamically generated in order
// to reduce the number of kernels compilations
//------------------------------------------------------------------------------

#if defined(PARAM_ENABLE_TEX_CONST_FLOAT3)

OPENCL_FORCE_INLINE float ConstFloat3Texture_ConstEvaluateFloat(__global const Texture *tex) {
	return Spectrum_Y(VLOAD3F(tex->constFloat3.color.c));
}

OPENCL_FORCE_INLINE float3 ConstFloat3Texture_ConstEvaluateSpectrum(__global const Texture *tex) {
	return VLOAD3F(tex->constFloat3.color.c);
}

// Note: ConstFloat3Texture_Bump() is defined in texture_bump_funcs.cl

#endif

//------------------------------------------------------------------------------
// ImageMap texture
//
// NOTE: the code for this type of texture is not dynamically generated in order
// to reduce the number of kernels compilations
//------------------------------------------------------------------------------

#if defined(PARAM_ENABLE_TEX_IMAGEMAP) && defined(PARAM_HAS_IMAGEMAPS)

OPENCL_FORCE_NOT_INLINE float ImageMapTexture_ConstEvaluateFloat(__global const Texture *tex,
		__global const HitPoint *hitPoint
		IMAGEMAPS_PARAM_DECL) {
	__global const ImageMap *imageMap = &imageMapDescs[tex->imageMapTex.imageMapIndex];

	const float2 uv = VLOAD2F(&hitPoint->uv.u);
	const float2 mapUV = TextureMapping2D_Map(&tex->imageMapTex.mapping, hitPoint);

	return tex->imageMapTex.gain * ImageMap_GetFloat(
			imageMap,
			mapUV.s0, mapUV.s1
			IMAGEMAPS_PARAM);
}

OPENCL_FORCE_NOT_INLINE float3 ImageMapTexture_ConstEvaluateSpectrum(__global const Texture *tex,
		__global const HitPoint *hitPoint
		IMAGEMAPS_PARAM_DECL) {
	__global const ImageMap *imageMap = &imageMapDescs[tex->imageMapTex.imageMapIndex];
	__global const float *pixels = ImageMap_GetPixelsAddress(
			imageMapBuff, imageMap->pageIndex, imageMap->pixelsIndex);

	const float2 uv = VLOAD2F(&hitPoint->uv.u);
	const float2 mapUV = TextureMapping2D_Map(&tex->imageMapTex.mapping, hitPoint);

	return tex->imageMapTex.gain * ImageMap_GetSpectrum(
			imageMap,
			mapUV.s0, mapUV.s1
			IMAGEMAPS_PARAM);
}

// Note: ImageMapTexture_Bump() is defined in texture_bump_funcs.cl

#endif

//------------------------------------------------------------------------------
// Scale texture
//------------------------------------------------------------------------------

#if defined(PARAM_ENABLE_TEX_SCALE)

OPENCL_FORCE_NOT_INLINE float ScaleTexture_ConstEvaluateFloat(__global const HitPoint *hitPoint,
		const float tex1, const float tex2) {
	return tex1 * tex2;
}

OPENCL_FORCE_NOT_INLINE float3 ScaleTexture_ConstEvaluateSpectrum(__global const HitPoint *hitPoint,
		const float3 tex1, const float3 tex2) {
	return tex1 * tex2;
}

#endif

//------------------------------------------------------------------------------
// FresnelApproxN & FresnelApproxK texture
//------------------------------------------------------------------------------

#if defined(PARAM_ENABLE_FRESNEL_APPROX_N)

OPENCL_FORCE_NOT_INLINE float FresnelApproxNTexture_ConstEvaluateFloat(__global const HitPoint *hitPoint,
		const float value) {
	return FresnelApproxN(value);
}

OPENCL_FORCE_NOT_INLINE float3 FresnelApproxNTexture_ConstEvaluateSpectrum(__global const HitPoint *hitPoint,
		const float3 value) {
	return FresnelApproxN3(value);
}

#endif

#if defined(PARAM_ENABLE_FRESNEL_APPROX_K)

OPENCL_FORCE_NOT_INLINE float FresnelApproxKTexture_ConstEvaluateFloat( __global const HitPoint *hitPoint,
		const float value) {
	return FresnelApproxK(value);
}

OPENCL_FORCE_NOT_INLINE float3 FresnelApproxKTexture_ConstEvaluateSpectrum(__global const HitPoint *hitPoint,
		const float3 value) {
	return FresnelApproxK3(value);
}

#endif

//------------------------------------------------------------------------------
// Mix texture
//------------------------------------------------------------------------------

#if defined(PARAM_ENABLE_TEX_MIX)

OPENCL_FORCE_NOT_INLINE float MixTexture_ConstEvaluateFloat(__global const HitPoint *hitPoint,
		const float amt, const float value1, const float value2) {
	return mix(value1, value2, clamp(amt, 0.f, 1.f));
}

OPENCL_FORCE_NOT_INLINE float3 MixTexture_ConstEvaluateSpectrum(__global const HitPoint *hitPoint,
		const float3 amt, const float3 value1, const float3 value2) {
	return mix(value1, value2, clamp(amt, 0.f, 1.f));
}

#endif

//------------------------------------------------------------------------------
// CheckerBoard 2D & 3D texture
//------------------------------------------------------------------------------

#if defined(PARAM_ENABLE_CHECKERBOARD2D)

OPENCL_FORCE_NOT_INLINE float CheckerBoard2DTexture_ConstEvaluateFloat(__global const HitPoint *hitPoint,
		const float value1, const float value2, __global const TextureMapping2D *mapping) {
	const float2 uv = VLOAD2F(&hitPoint->uv.u);
	const float2 mapUV = TextureMapping2D_Map(mapping, hitPoint);

	return ((Floor2Int(mapUV.s0) + Floor2Int(mapUV.s1)) % 2 == 0) ? value1 : value2;
}

OPENCL_FORCE_NOT_INLINE float3 CheckerBoard2DTexture_ConstEvaluateSpectrum(__global const HitPoint *hitPoint,
		const float3 value1, const float3 value2, __global const TextureMapping2D *mapping) {
	const float2 uv = VLOAD2F(&hitPoint->uv.u);
	const float2 mapUV = TextureMapping2D_Map(mapping, hitPoint);

	return ((Floor2Int(mapUV.s0) + Floor2Int(mapUV.s1)) % 2 == 0) ? value1 : value2;
}

#endif

#if defined(PARAM_ENABLE_CHECKERBOARD3D)

OPENCL_FORCE_NOT_INLINE float CheckerBoard3DTexture_ConstEvaluateFloat(__global const HitPoint *hitPoint,
		const float value1, const float value2, __global const TextureMapping3D *mapping) {
	// The +DEFAULT_EPSILON_STATIC is there as workaround for planes placed exactly on 0.0
	const float3 mapP = TextureMapping3D_Map(mapping, hitPoint) +  + DEFAULT_EPSILON_STATIC;

	return ((Floor2Int(mapP.x) + Floor2Int(mapP.y) + Floor2Int(mapP.z)) % 2 == 0) ? value1 : value2;
}

OPENCL_FORCE_NOT_INLINE float3 CheckerBoard3DTexture_ConstEvaluateSpectrum(__global const HitPoint *hitPoint,
		const float3 value1, const float3 value2, __global const TextureMapping3D *mapping) {
	// The +DEFAULT_EPSILON_STATIC is there as workaround for planes placed exactly on 0.0
	const float3 mapP = TextureMapping3D_Map(mapping, hitPoint) +  + DEFAULT_EPSILON_STATIC;

	return ((Floor2Int(mapP.x) + Floor2Int(mapP.y) + Floor2Int(mapP.z)) % 2 == 0) ? value1 : value2;
}

#endif

//------------------------------------------------------------------------------
// Cloud texture
//------------------------------------------------------------------------------

#if defined(PARAM_ENABLE_CLOUD_TEX)

OPENCL_FORCE_INLINE float CloudTexture_CloudNoise(const float3 p, const float omegaValue, const uint octaves) {
	// Compute sum of octaves of noise
	float sum = 0.f, lambda = 1.f, o = 1.f;
	for (uint i = 0; i < octaves; ++i) {
		sum += o * Noise3(lambda * p);
		lambda *= 1.99f;
		o *= omegaValue;
	}
	return sum;
}

OPENCL_FORCE_INLINE float CloudTexture_NoiseMask(const float3 p, const float radius, const float omega) {
	return CloudTexture_CloudNoise(p / radius * 1.4f, omega, 1);
}

OPENCL_FORCE_INLINE float3 CloudTexture_Turbulence(const float3 p, const float noiseScale, const float noiseOffset, const float variability,
                               const uint octaves, const float radius, const float omega, const float baseFlatness, const float3 sphereCentre) {
	float3 noiseCoords[3];
	const float baseFadeDistance = 1.f - baseFlatness;

	noiseCoords[0] = p / noiseScale;
	noiseCoords[1] = noiseCoords[0] + (float3)(noiseOffset, noiseOffset, noiseOffset);
	noiseCoords[2] = noiseCoords[1] + (float3)(noiseOffset, noiseOffset, noiseOffset);

	float noiseAmount = 1.f;

	if (variability < 1.f)
		noiseAmount = Lerp(variability, 1.f, CloudTexture_NoiseMask(p + (float3)(noiseOffset * 4.f, 0.f, 0.f), radius, omega));


	noiseAmount = clamp(noiseAmount, 0.f, 1.f);

	float3 turbulence;

	turbulence.x = CloudTexture_CloudNoise(noiseCoords[0], omega, octaves) - 0.15f;
	turbulence.y = CloudTexture_CloudNoise(noiseCoords[1], omega, octaves) - 0.15f;
	turbulence.z = -CloudTexture_CloudNoise(noiseCoords[2], omega, octaves);
	if (p.z < sphereCentre.z + baseFadeDistance)
		turbulence.z *= (p.z - sphereCentre.z) / (2.f * baseFadeDistance);

	turbulence *= noiseAmount;

	return turbulence;
}

OPENCL_FORCE_INLINE float CloudTexture_CloudShape(const float3 p, const float baseFadeDistance, const float3 sphereCentre, const uint numSpheres, const float radius) {
/*	if (numSpheres > 0) {
		if (SphereFunction(p, numSpheres))		//shows cumulus spheres
			return 1.f;
		else
			return 0.f;
	}
*/
	const float3 fromCentre = p - sphereCentre;

	float amount = 1.f - sqrt(fromCentre.x*fromCentre.x + fromCentre.y*fromCentre.y + fromCentre.z*fromCentre.z) / radius;
	if (amount < 0.f)
		return 0.f;

	// The base below the cloud's height fades out
	if (p.z < sphereCentre.z) {
		if (p.z < sphereCentre.z - radius * 0.4f)
			return 0.f;

		amount *= 1.f - cos((fromCentre.z + baseFadeDistance) /
			baseFadeDistance * M_PI_F * 0.5f);
	}
	return max(amount, 0.f);
}

/*
OPENCL_FORCE_INLINE bool CloudTexture_SphereFunction(const float3 p, uint numSpheres) const {
// Returns whether a point is inside one of the cumulus spheres
	for (uint i = 0; i < numSpheres; ++i) {
		if ((p - spheres[i].position).Length() < spheres[i].radius)
			return true;
	}
	return false;
}
*/

OPENCL_FORCE_NOT_INLINE float CloudTexture_ConstEvaluateFloat(__global const HitPoint *hitPoint,
		const float radius, const uint numSpheres, const uint spheresize, const float sharpness,
		const float baseFadeDistance, const float baseFlatness, const float variability,
		const float omega, const float firstNoiseScale, const float noiseOffset, const float turbulenceAmount,
		const uint numOctaves, __global const TextureMapping3D *mapping) {

	const float3 mapP = TextureMapping3D_Map(mapping, hitPoint);
	const float3 sphereCentre = (float3)(.5f, .5f, 1.f / 3.f);
	const float amount = CloudTexture_CloudShape(mapP + turbulenceAmount * CloudTexture_Turbulence(mapP, firstNoiseScale, noiseOffset, variability, numOctaves, radius, omega, baseFlatness, sphereCentre), baseFadeDistance, sphereCentre, numSpheres, radius);
	const float finalValue = pow(amount * pow(10.f, .7f), sharpness);

	return min(finalValue, 1.f);
}

OPENCL_FORCE_NOT_INLINE float3 CloudTexture_ConstEvaluateSpectrum(__global const HitPoint *hitPoint,
		const float radius, const uint numSpheres, const uint spheresize, const float sharpness,
		const float baseFadeDistance, const float baseFlatness, const float variability,
		const float omega, const float firstNoiseScale, const float noiseOffset, const float turbulenceAmount,
		const uint numOctaves, __global const TextureMapping3D *mapping) {

	const float3 mapP = TextureMapping3D_Map(mapping, hitPoint);
	const float3 sphereCentre = (float3)(.5f, .5f, 1.f / 3.f);
	const float amount = CloudTexture_CloudShape(mapP + turbulenceAmount * CloudTexture_Turbulence(mapP, firstNoiseScale, noiseOffset, variability, numOctaves, radius, omega, baseFlatness, sphereCentre), baseFadeDistance, sphereCentre, numSpheres, radius);
	const float finalValue = pow(amount * pow(10.f, .7f), sharpness);

	return min(finalValue, 1.f);
}


#endif

//------------------------------------------------------------------------------
// FBM texture
//------------------------------------------------------------------------------

#if defined(PARAM_ENABLE_FBM_TEX)

OPENCL_FORCE_NOT_INLINE float FBMTexture_ConstEvaluateFloat(__global const HitPoint *hitPoint,
	const float omega, const int octaves, __global const TextureMapping3D *mapping) {
	const float3 mapP = TextureMapping3D_Map(mapping, hitPoint);

	return FBm(mapP, omega, octaves);
}

OPENCL_FORCE_NOT_INLINE float3 FBMTexture_ConstEvaluateSpectrum(__global const HitPoint *hitPoint,
	const float omega, const int octaves, __global const TextureMapping3D *mapping) {
	const float3 mapP = TextureMapping3D_Map(mapping, hitPoint);

	return FBm(mapP, omega, octaves);
}

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

OPENCL_FORCE_INLINE float3 MarbleTexture_Evaluate(__global const HitPoint *hitPoint, const float scale,
		const float omega, const int octaves, const float variation,
		__global const TextureMapping3D *mapping) {
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

OPENCL_FORCE_NOT_INLINE float MarbleTexture_ConstEvaluateFloat(__global const HitPoint *hitPoint, const float scale,
		const float omega, const int octaves, const float variation,
		__global const TextureMapping3D *mapping) {
	return Spectrum_Y(MarbleTexture_Evaluate(hitPoint, scale, omega, octaves,
			variation, mapping));
}

OPENCL_FORCE_NOT_INLINE float3 MarbleTexture_ConstEvaluateSpectrum(__global const HitPoint *hitPoint, const float scale,
		const float omega, const int octaves, const float variation,
		__global const TextureMapping3D *mapping) {
	return MarbleTexture_Evaluate(hitPoint, scale, omega, octaves,
			variation, mapping);
}

#endif

//------------------------------------------------------------------------------
// Dots texture
//------------------------------------------------------------------------------

#if defined(PARAM_ENABLE_DOTS)

OPENCL_FORCE_INLINE bool DotsTexture_Evaluate(__global const HitPoint *hitPoint, __global const TextureMapping2D *mapping) {
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

OPENCL_FORCE_NOT_INLINE float DotsTexture_ConstEvaluateFloat(__global const HitPoint *hitPoint,
		const float value1, const float value2, __global const TextureMapping2D *mapping) {
	return DotsTexture_Evaluate(hitPoint, mapping) ? value1 : value2;
}

OPENCL_FORCE_NOT_INLINE float3 DotsTexture_ConstEvaluateSpectrum(__global const HitPoint *hitPoint,
		const float3 value1, const float3 value2, __global const TextureMapping2D *mapping) {
	return DotsTexture_Evaluate(hitPoint, mapping) ? value1 : value2;
}

#endif

//------------------------------------------------------------------------------
// Brick texture
//------------------------------------------------------------------------------

#if defined(PARAM_ENABLE_BRICK)

OPENCL_FORCE_INLINE bool BrickTexture_RunningAlternate(const float3 p, float3 *i, float3 *b,
		const float run, const float mortarwidth,
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

OPENCL_FORCE_INLINE bool BrickTexture_Basket(const float3 p, float3 *i,
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

OPENCL_FORCE_INLINE bool BrickTexture_Herringbone(const float3 p, float3 *i,
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

OPENCL_FORCE_INLINE bool BrickTexture_Running(const float3 p, float3 *i, float3 *b,
		const float run, const float mortarwidth,
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

OPENCL_FORCE_INLINE bool BrickTexture_English(const float3 p, float3 *i, float3 *b,
		const float run, const float mortarwidth,
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

OPENCL_FORCE_INLINE bool BrickTexture_Evaluate(__global const HitPoint *hitPoint,
		const MasonryBond bond,
		const float brickwidth, const float brickheight,
		const float brickdepth, const float mortarsize,
		const float3 offset,
		const float run, const float mortarwidth,
		const float mortarheight, const float mortardepth,
		const float proportion, const float invproportion,
		__global const TextureMapping3D *mapping) {
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

OPENCL_FORCE_NOT_INLINE float BrickTexture_ConstEvaluateFloat(__global const HitPoint *hitPoint,
		const float value1, const float value2, const float value3,
		const MasonryBond bond,
		const float brickwidth, const float brickheight,
		const float brickdepth, const float mortarsize,
		const float3 offset,
		const float run, const float mortarwidth,
		const float mortarheight, const float mortardepth,
		const float proportion, const float invproportion,
		__global const TextureMapping3D *mapping) {
	return BrickTexture_Evaluate(hitPoint,
			bond,
			brickwidth, brickheight,
			brickdepth, mortarsize,
			offset,
			run, mortarwidth,
			mortarheight, mortardepth,
			proportion, invproportion,
			mapping) ? (value1 * value3) : value2;
}

OPENCL_FORCE_NOT_INLINE float3 BrickTexture_ConstEvaluateSpectrum(__global const HitPoint *hitPoint,
		const float3 value1, const float3 value2, const float3 value3,
		const MasonryBond bond,
		const float brickwidth, const float brickheight,
		const float brickdepth, const float mortarsize,
		const float3 offset,
		const float run, const float mortarwidth,
		const float mortarheight, const float mortardepth,
		const float proportion, const float invproportion,
		__global const TextureMapping3D *mapping) {
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

#endif

//------------------------------------------------------------------------------
// Add texture
//------------------------------------------------------------------------------

#if defined(PARAM_ENABLE_TEX_ADD)

OPENCL_FORCE_NOT_INLINE float AddTexture_ConstEvaluateFloat(__global const HitPoint *hitPoint,
		const float value1, const float value2) {
	return value1 + value2;
}

OPENCL_FORCE_NOT_INLINE float3 AddTexture_ConstEvaluateSpectrum(__global const HitPoint *hitPoint,
		const float3 value1, const float3 value2) {
	return value1 + value2;
}

#endif

//------------------------------------------------------------------------------
// Subtract texture
//------------------------------------------------------------------------------

#if defined(PARAM_ENABLE_TEX_SUBTRACT)

OPENCL_FORCE_NOT_INLINE float SubtractTexture_ConstEvaluateFloat(__global const HitPoint *hitPoint,
		const float value1, const float value2) {
	return value1 - value2;
}

OPENCL_FORCE_NOT_INLINE float3 SubtractTexture_ConstEvaluateSpectrum(__global const HitPoint *hitPoint,
		const float3 value1, const float3 value2) {
	return value1 - value2;
}

#endif

//------------------------------------------------------------------------------
// Windy texture
//------------------------------------------------------------------------------

#if defined(PARAM_ENABLE_WINDY)

OPENCL_FORCE_NOT_INLINE float WindyTexture_ConstEvaluateFloat(__global const HitPoint *hitPoint,
		__global const TextureMapping3D *mapping) {
	const float3 mapP = TextureMapping3D_Map(mapping, hitPoint);

	const float windStrength = FBm(.1f * mapP, .5f, 3);
	const float waveHeight = FBm(mapP, .5f, 6);

	return fabs(windStrength) * waveHeight;
}

OPENCL_FORCE_INLINE float3 WindyTexture_ConstEvaluateSpectrum(__global const HitPoint *hitPoint,
		__global const TextureMapping3D *mapping) {
	return WindyTexture_ConstEvaluateFloat(hitPoint, mapping);
}

#endif

//------------------------------------------------------------------------------
// Wrinkled texture
//------------------------------------------------------------------------------

#if defined(PARAM_ENABLE_WRINKLED)

OPENCL_FORCE_NOT_INLINE float WrinkledTexture_ConstEvaluateFloat(__global const HitPoint *hitPoint,
		const float omega, const int octaves,
		__global const TextureMapping3D *mapping) {
	const float3 mapP = TextureMapping3D_Map(mapping, hitPoint);

	return Turbulence(mapP, omega, octaves);
}

OPENCL_FORCE_INLINE float3 WrinkledTexture_ConstEvaluateSpectrum(__global const HitPoint *hitPoint,
		const float omega, const int octaves,
		__global const TextureMapping3D *mapping) {
	return WrinkledTexture_ConstEvaluateFloat(hitPoint, omega, octaves, mapping);
}

#endif

//------------------------------------------------------------------------------
// UV texture
//------------------------------------------------------------------------------

#if defined(PARAM_ENABLE_TEX_UV)

OPENCL_FORCE_NOT_INLINE float UVTexture_ConstEvaluateFloat(__global const HitPoint *hitPoint,
		__global const TextureMapping2D *mapping) {
	const float2 uv = TextureMapping2D_Map(mapping, hitPoint);

	return Spectrum_Y((float3)(uv.s0 - Floor2Int(uv.s0), uv.s1 - Floor2Int(uv.s1), 0.f));
}

OPENCL_FORCE_NOT_INLINE float3 UVTexture_ConstEvaluateSpectrum(__global const HitPoint *hitPoint,
		__global const TextureMapping2D *mapping) {
	const float2 uv = TextureMapping2D_Map(mapping, hitPoint);

	return (float3)(uv.s0 - Floor2Int(uv.s0), uv.s1 - Floor2Int(uv.s1), 0.f);
}

#endif

//------------------------------------------------------------------------------
// Band texture
//------------------------------------------------------------------------------

#if defined(PARAM_ENABLE_TEX_BAND)

OPENCL_FORCE_NOT_INLINE float3 BandTexture_ConstEvaluateSpectrum(__global const HitPoint *hitPoint,
		const InterpolationType interpType,
		const uint size, __global const float *offsets,
		__global const Spectrum *values, const float3 amt) {
	const float a = clamp(Spectrum_Y(amt), 0.f, 1.f);

	const uint last = size - 1;
	if (a < offsets[0])
		return VLOAD3F(values[0].c);
	else if (a >= offsets[last])
		return VLOAD3F(values[last].c);
	else {
		int p = 0;
		for (; p <= last; ++p) {
			if (a < offsets[p])
				break;
		}

		const float o1 = offsets[p - 1];
		const float o0 = offsets[p];
		const float factor = (a - o1) / (o0 - o1);

		if (interpType == INTERP_NONE)
			return VLOAD3F(values[p - 1].c);
		else if (interpType == INTERP_LINEAR) {
			const float3 p0 = VLOAD3F(values[p - 1].c);
			const float3 p1 = VLOAD3F(values[p].c);

			return Lerp3(factor, p0, p1);
		} else if (interpType == INTERP_CUBIC) {
			const float3 p0 = VLOAD3F(values[max(p - 2, 0)].c);
			const float3 p1 = VLOAD3F(values[p - 1].c);
			const float3 p2 = VLOAD3F(values[p].c);
			const float3 p3 = VLOAD3F(values[min(p + 1, (int)last)].c);

			return Cerp3(factor, p0, p1, p2, p3);
		} else
			return 0.f;
	}
}

OPENCL_FORCE_INLINE float BandTexture_ConstEvaluateFloat(__global const HitPoint *hitPoint,
		const InterpolationType interpType,
		const uint size, __global const float *offsets,
		__global const Spectrum *values, const float amt) {
	return Spectrum_Y(BandTexture_ConstEvaluateSpectrum(hitPoint,
			interpType, size, offsets, values, amt));
}

#endif

//------------------------------------------------------------------------------
// HitPointColor texture
//------------------------------------------------------------------------------

#if defined(PARAM_ENABLE_TEX_HITPOINTCOLOR)

OPENCL_FORCE_INLINE float HitPointColorTexture_ConstEvaluateFloat(__global const HitPoint *hitPoint) {
	return Spectrum_Y(VLOAD3F(hitPoint->color.c));
}

OPENCL_FORCE_INLINE float3 HitPointColorTexture_ConstEvaluateSpectrum(__global const HitPoint *hitPoint) {
	return VLOAD3F(hitPoint->color.c);
}

#endif

//------------------------------------------------------------------------------
// HitPointAlpha texture
//------------------------------------------------------------------------------

#if defined(PARAM_ENABLE_TEX_HITPOINTALPHA)

OPENCL_FORCE_INLINE float HitPointAlphaTexture_ConstEvaluateFloat(__global const HitPoint *hitPoint) {
	return hitPoint->alpha;
}

OPENCL_FORCE_INLINE float3 HitPointAlphaTexture_ConstEvaluateSpectrum(__global const HitPoint *hitPoint) {
	const float alpha = hitPoint->alpha;
	return (float3)(alpha, alpha, alpha);
}

#endif

//------------------------------------------------------------------------------
// HitPointGrey texture
//------------------------------------------------------------------------------

#if defined(PARAM_ENABLE_TEX_HITPOINTGREY)

OPENCL_FORCE_NOT_INLINE float HitPointGreyTexture_ConstEvaluateFloat(__global const HitPoint *hitPoint, const uint channel) {
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

OPENCL_FORCE_NOT_INLINE float3 HitPointGreyTexture_ConstEvaluateSpectrum(__global const HitPoint *hitPoint, const uint channel) {
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

#endif

//------------------------------------------------------------------------------
// NormalMap texture
//------------------------------------------------------------------------------

#if defined(PARAM_ENABLE_TEX_NORMALMAP)

OPENCL_FORCE_INLINE float NormalMapTexture_ConstEvaluateFloat(__global const Texture *tex) {
    return 0.f;
}

OPENCL_FORCE_INLINE float3 NormalMapTexture_ConstEvaluateSpectrum(__global const Texture *tex) {
	return 0.f;
}

// Note: NormalMapTexture_Bump() is defined in texture_bump_funcs.cl

#endif

//------------------------------------------------------------------------------
// Divide texture
//------------------------------------------------------------------------------

#if defined(PARAM_ENABLE_TEX_DIVIDE)

OPENCL_FORCE_NOT_INLINE float DivideTexture_ConstEvaluateFloat(__global const HitPoint *hitPoint,
		const float tex1, const float tex2) {
	if (tex2 == 0.f)
		return 0.f;
	return tex1 / tex2;
}

OPENCL_FORCE_NOT_INLINE float3 DivideTexture_ConstEvaluateSpectrum(__global const HitPoint *hitPoint,
		const float3 tex1, const float3 tex2) {
	if (Spectrum_IsBlack(tex2))
		return BLACK;
	return tex1 / tex2;
}

#endif

//------------------------------------------------------------------------------
// Remap texture
//------------------------------------------------------------------------------

#if defined(PARAM_ENABLE_TEX_REMAP)

OPENCL_FORCE_NOT_INLINE float RemapTexture_Remap(const float value,
		const float sourceMin, const float sourceMax,
		const float targetMin, const float targetMax) {
	if (sourceMin == sourceMax)
		return sourceMin;

	return (value - sourceMin)
	       * (targetMax - targetMin)
	       / (sourceMax - sourceMin)
	       + targetMin;
}

OPENCL_FORCE_NOT_INLINE float RemapTexture_ConstEvaluateFloat(__global const HitPoint *hitPoint,
		const float value, const float sourceMin, const float sourceMax,
		const float targetMin, const float targetMax) {
	const float clampedValue = clamp(value, sourceMin, sourceMax);
	const float result = RemapTexture_Remap(clampedValue, sourceMin, sourceMax, targetMin, targetMax);
	return clamp(result, targetMin, targetMax);
}

OPENCL_FORCE_NOT_INLINE float3 RemapTexture_ConstEvaluateSpectrum(__global const HitPoint *hitPoint,
		const float3 value, const float sourceMin, const float sourceMax,
		const float targetMin, const float targetMax) {
	const float3 clampedValue = clamp(value, sourceMin, sourceMax);
	float3 result;
	result.x = RemapTexture_Remap(clampedValue.x, sourceMin, sourceMax, targetMin, targetMax);
	result.y = RemapTexture_Remap(clampedValue.y, sourceMin, sourceMax, targetMin, targetMax);
	result.z = RemapTexture_Remap(clampedValue.z, sourceMin, sourceMax, targetMin, targetMax);
	return clamp(result, targetMin, targetMax);
}

#endif

//------------------------------------------------------------------------------
// ObjectID texture
//------------------------------------------------------------------------------

#if defined(PARAM_ENABLE_TEX_OBJECTID)

OPENCL_FORCE_INLINE float ObjectIDTexture_ConstEvaluateFloat(__global const HitPoint *hitPoint) {
	return (float)hitPoint->objectID;
}

OPENCL_FORCE_INLINE float3 ObjectIDTexture_ConstEvaluateSpectrum(__global const HitPoint *hitPoint) {
	const unsigned int id = hitPoint->objectID;
	return (float3)(id, id, id);
}

#endif

//------------------------------------------------------------------------------
// ObjectIDColor texture
//------------------------------------------------------------------------------

#if defined(PARAM_ENABLE_TEX_OBJECTID_COLOR)

OPENCL_FORCE_NOT_INLINE float3 ObjectIDColorTexture_IDToSpectrum(const unsigned int id) {
	return (float3)((id & 0x0000ffu) * ( 1.f / 255.f),
	                ((id & 0x00ff00u) >> 8) * ( 1.f / 255.f),
	                ((id & 0xff0000u) >> 16) * ( 1.f / 255.f));
}

OPENCL_FORCE_NOT_INLINE float ObjectIDColorTexture_ConstEvaluateFloat(__global const HitPoint *hitPoint) {
	return Spectrum_Y(ObjectIDColorTexture_IDToSpectrum(hitPoint->objectID));
}

OPENCL_FORCE_NOT_INLINE float3 ObjectIDColorTexture_ConstEvaluateSpectrum(__global const HitPoint *hitPoint) {
	return ObjectIDColorTexture_IDToSpectrum(hitPoint->objectID);
}

#endif

//------------------------------------------------------------------------------
// ObjectIDNormalized texture
//------------------------------------------------------------------------------

#if defined(PARAM_ENABLE_TEX_OBJECTID_NORMALIZED)

OPENCL_FORCE_INLINE float ObjectIDNormalizedTexture_ConstEvaluateFloat(__global const HitPoint *hitPoint) {
	return ((float)hitPoint->objectID) * (1.f / 0xffffffffu);
}

OPENCL_FORCE_INLINE float3 ObjectIDNormalizedTexture_ConstEvaluateSpectrum(__global const HitPoint *hitPoint) {
	const float normalized = ((float)hitPoint->objectID) * (1.f / 0xffffffffu);
	return (float3)(normalized, normalized, normalized);
}

#endif

//------------------------------------------------------------------------------
// DotProduct texture
//------------------------------------------------------------------------------

#if defined(PARAM_ENABLE_TEX_DOT_PRODUCT)

OPENCL_FORCE_INLINE float DotProductTexture_ConstEvaluateFloat(__global const HitPoint *hitPoint,
		const float3 tex1, const float3 tex2) {
	return dot(tex1, tex2);
}

OPENCL_FORCE_INLINE float3 DotProductTexture_ConstEvaluateSpectrum(__global const HitPoint *hitPoint,
		const float3 tex1, const float3 tex2) {
	const float result = dot(tex1, tex2);
	return (float3)(result, result, result);
}

#endif

//------------------------------------------------------------------------------
// Greater Than texture
//------------------------------------------------------------------------------

#if defined(PARAM_ENABLE_TEX_GREATER_THAN)

OPENCL_FORCE_INLINE float GreaterThanTexture_ConstEvaluateFloat(__global const HitPoint *hitPoint,
		const float tex1, const float tex2) {
	return (tex1 > tex2) ? 1.f : 0.f;
}

OPENCL_FORCE_INLINE float3 GreaterThanTexture_ConstEvaluateSpectrum(__global const HitPoint *hitPoint,
		const float tex1, const float tex2) {
	const float result = (tex1 > tex2) ? 1.f : 0.f;
	return (float3)(result, result, result);
}

#endif

//------------------------------------------------------------------------------
// Less Than texture
//------------------------------------------------------------------------------

#if defined(PARAM_ENABLE_TEX_LESS_THAN)

OPENCL_FORCE_INLINE float LessThanTexture_ConstEvaluateFloat(__global const HitPoint *hitPoint,
		const float tex1, const float tex2) {
	return (tex1 < tex2) ? 1.f : 0.f;
}

OPENCL_FORCE_INLINE float3 LessThanTexture_ConstEvaluateSpectrum(__global const HitPoint *hitPoint,
		const float tex1, const float tex2) {
	const float result = (tex1 < tex2) ? 1.f : 0.f;
	return (float3)(result, result, result);
}

#endif

//------------------------------------------------------------------------------
// Power texture
//------------------------------------------------------------------------------

#if defined(PARAM_ENABLE_TEX_POWER)

OPENCL_FORCE_NOT_INLINE float PowerTexture_SafePow(const float base, const float exponent) {
	if (base < 0.f && exponent != ((int)exponent))
		return 0.f;
	return pow(base, exponent);
}

OPENCL_FORCE_NOT_INLINE float PowerTexture_ConstEvaluateFloat(__global const HitPoint *hitPoint,
		const float base, const float exponent) {
	return PowerTexture_SafePow(base, exponent);
}

OPENCL_FORCE_NOT_INLINE float3 PowerTexture_ConstEvaluateSpectrum(__global const HitPoint *hitPoint,
		const float base, const float exponent) {
	const float result = PowerTexture_SafePow(base, exponent);
	return (float3)(result, result, result);
}

#endif

//------------------------------------------------------------------------------
// Shading Normal texture
//------------------------------------------------------------------------------

#if defined(PARAM_ENABLE_TEX_SHADING_NORMAL)

OPENCL_FORCE_INLINE float ShadingNormalTexture_ConstEvaluateFloat(__global const HitPoint *hitPoint) {
	// This method doesn't really make sense for a vector - just return the first element
	return hitPoint->shadeN.x;
}

OPENCL_FORCE_INLINE float3 ShadingNormalTexture_ConstEvaluateSpectrum(__global const HitPoint *hitPoint) {
	return (float3)(hitPoint->shadeN.x, hitPoint->shadeN.y, hitPoint->shadeN.z);
}

#endif

//------------------------------------------------------------------------------
// Position texture
//------------------------------------------------------------------------------

#if defined(PARAM_ENABLE_TEX_POSITION)

OPENCL_FORCE_INLINE float PositionTexture_ConstEvaluateFloat(__global const HitPoint *hitPoint) {
	// This method doesn't really make sense for a vector - just return the first element
	return hitPoint->p.x;
}

OPENCL_FORCE_INLINE float3 PositionTexture_ConstEvaluateSpectrum(__global const HitPoint *hitPoint) {
	return (float3)(hitPoint->p.x, hitPoint->p.y, hitPoint->p.z);
}

#endif

//------------------------------------------------------------------------------
// Split float3 texture
//------------------------------------------------------------------------------

#if defined(PARAM_ENABLE_TEX_SPLIT_FLOAT3)

OPENCL_FORCE_INLINE float SplitFloat3Texture_ConstEvaluateFloat(__global const HitPoint *hitPoint,
		const float3 tex, const uint channel) {
	switch (channel)
	{
	case 0:
		return tex.x;
	case 1:
		return tex.y;
	case 2:
		return tex.z;
	default:
		return 0.f;
	}
}

OPENCL_FORCE_INLINE float3 SplitFloat3Texture_ConstEvaluateSpectrum(__global const HitPoint *hitPoint,
		const float3 tex, const uint channel) {
	float result;
	switch (channel)
	{
	case 0:
		result = tex.x;
		break;
	case 1:
		result = tex.y;
		break;
	case 2:
		result = tex.z;
		break;
	default:
		result = 0.f;
		break;
	}
	return (float3)(result, result, result);
}

#endif

//------------------------------------------------------------------------------
// Make float3 texture
//------------------------------------------------------------------------------

#if defined(PARAM_ENABLE_TEX_MAKE_FLOAT3)

OPENCL_FORCE_INLINE float MakeFloat3Texture_ConstEvaluateFloat(__global const HitPoint *hitPoint,
		const float tex1, const float tex2, const float tex3) {
	return Spectrum_Y((float3)(tex1, tex2, tex3));
}

OPENCL_FORCE_INLINE float3 MakeFloat3Texture_ConstEvaluateSpectrum(__global const HitPoint *hitPoint,
		const float tex1, const float tex2, const float tex3) {
	return (float3)(tex1, tex2, tex3);
}

#endif

//------------------------------------------------------------------------------
// Rounding texture
//------------------------------------------------------------------------------

#if defined(PARAM_ENABLE_TEX_ROUNDING)

OPENCL_FORCE_NOT_INLINE float RoundingTexture_ConstEvaluateFloat(__global const HitPoint *hitPoint,
                                                                 const float tex1,
                                                                 const float tex2) {
    if(tex1 == tex2 || tex2 == 0) {
        return tex1;
    } else {
        const float innerBound = tex2 * (int) (tex1 / tex2);
        const float outerBound = (tex1 < 0 ? innerBound - tex2 : innerBound + tex2);
        return fabs(outerBound - tex1) < fabs(innerBound - tex1) ?
            outerBound : innerBound;
    }
}

OPENCL_FORCE_INLINE float3 RoundingTexture_ConstEvaluateSpectrum(__global const HitPoint *hitPoint,
                                                                const float tex1,
                                                                const float tex2) {
    const float result = RoundingTexture_ConstEvaluateFloat(hitPoint, tex1, tex2);
    return (float3)(result, result, result);
}

#endif

//------------------------------------------------------------------------------
// Modulo texture
//------------------------------------------------------------------------------

#if defined(PARAM_ENABLE_TEX_MODULO)

OPENCL_FORCE_NOT_INLINE float ModuloTexture_ConstEvaluateFloat(__global const HitPoint *hitPoint,
                                                               const float tex1,
                                                               const float tex2) {
    if(tex2 == 0) {
        return 0.f;
    }

    return fmod(tex1, tex2);
}

OPENCL_FORCE_INLINE float3 ModuloTexture_ConstEvaluateSpectrum(__global const HitPoint *hitPoint,
                                                               const float tex1,
                                                               const float tex2) {
    const float result = ModuloTexture_ConstEvaluateFloat(hitPoint, tex1, tex2);
    return (float3)(result, result, result);
}

#endif
