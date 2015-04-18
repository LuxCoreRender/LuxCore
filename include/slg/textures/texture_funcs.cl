#line 2 "texture_funcs.cl"

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

#endif

//------------------------------------------------------------------------------
// ImageMap texture
//------------------------------------------------------------------------------

#if defined(PARAM_ENABLE_TEX_IMAGEMAP)

float ImageMapTexture_ConstEvaluateFloat(__global HitPoint *hitPoint,
		const float gain, const uint imageMapIndex, __global const TextureMapping2D *mapping
		IMAGEMAPS_PARAM_DECL) {
	__global const ImageMap *imageMap = &imageMapDescs[imageMapIndex];

	const float2 uv = VLOAD2F(&hitPoint->uv.u);
	const float2 mapUV = TextureMapping2D_Map(mapping, hitPoint);

	return gain * ImageMap_GetFloat(
			imageMap,
			mapUV.s0, mapUV.s1
			IMAGEMAPS_PARAM);
}

float3 ImageMapTexture_ConstEvaluateSpectrum(__global HitPoint *hitPoint,
		const float gain, const uint imageMapIndex, __global const TextureMapping2D *mapping
		IMAGEMAPS_PARAM_DECL) {
	__global const ImageMap *imageMap = &imageMapDescs[imageMapIndex];
	__global const float *pixels = ImageMap_GetPixelsAddress(
			imageMapBuff, imageMap->pageIndex, imageMap->pixelsIndex);

	const float2 uv = VLOAD2F(&hitPoint->uv.u);
	const float2 mapUV = TextureMapping2D_Map(mapping, hitPoint);

	return gain * ImageMap_GetSpectrum(
			imageMap,
			mapUV.s0, mapUV.s1
			IMAGEMAPS_PARAM);
}

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

#endif

//------------------------------------------------------------------------------
// FresnelApproxN & FresnelApproxK texture
//------------------------------------------------------------------------------

#if defined(PARAM_ENABLE_FRESNEL_APPROX_N)

float FresnelApproxNTexture_ConstEvaluateFloat(__global HitPoint *hitPoint,
		const float value) {
	return FresnelApproxN(value);
}

float3 FresnelApproxNTexture_ConstEvaluateSpectrum(__global HitPoint *hitPoint,
		const float3 value) {
	return FresnelApproxN3(value);
}

#endif

#if defined(PARAM_ENABLE_FRESNEL_APPROX_K)

float FresnelApproxKTexture_ConstEvaluateFloat( __global HitPoint *hitPoint,
		const float value) {
	return FresnelApproxK(value);
}

float3 FresnelApproxKTexture_ConstEvaluateSpectrum(__global HitPoint *hitPoint,
		const float3 value) {
	return FresnelApproxK3(value);
}

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

#endif

//------------------------------------------------------------------------------
// CheckerBoard 2D & 3D texture
//------------------------------------------------------------------------------

#if defined(PARAM_ENABLE_CHECKERBOARD2D)

float CheckerBoard2DTexture_ConstEvaluateFloat(__global HitPoint *hitPoint,
		const float value1, const float value2, __global const TextureMapping2D *mapping) {
	const float2 uv = VLOAD2F(&hitPoint->uv.u);
	const float2 mapUV = TextureMapping2D_Map(mapping, hitPoint);

	return ((Floor2Int(mapUV.s0) + Floor2Int(mapUV.s1)) % 2 == 0) ? value1 : value2;
}

float3 CheckerBoard2DTexture_ConstEvaluateSpectrum(__global HitPoint *hitPoint,
		const float3 value1, const float3 value2, __global const TextureMapping2D *mapping) {
	const float2 uv = VLOAD2F(&hitPoint->uv.u);
	const float2 mapUV = TextureMapping2D_Map(mapping, hitPoint);

	return ((Floor2Int(mapUV.s0) + Floor2Int(mapUV.s1)) % 2 == 0) ? value1 : value2;
}

#endif

#if defined(PARAM_ENABLE_CHECKERBOARD3D)

float CheckerBoard3DTexture_ConstEvaluateFloat(__global HitPoint *hitPoint,
		const float value1, const float value2, __global const TextureMapping3D *mapping) {
	const float3 mapP = TextureMapping3D_Map(mapping, hitPoint);

	return ((Floor2Int(mapP.x) + Floor2Int(mapP.y) + Floor2Int(mapP.z)) % 2 == 0) ? value1 : value2;
}

float3 CheckerBoard3DTexture_ConstEvaluateSpectrum(__global HitPoint *hitPoint,
		const float3 value1, const float3 value2, __global const TextureMapping3D *mapping) {
	const float3 mapP = TextureMapping3D_Map(mapping, hitPoint);

	return ((Floor2Int(mapP.x) + Floor2Int(mapP.y) + Floor2Int(mapP.z)) % 2 == 0) ? value1 : value2;
}

#endif

//------------------------------------------------------------------------------
// FBM texture
//------------------------------------------------------------------------------

#if defined(PARAM_ENABLE_FBM_TEX)

float FBMTexture_ConstEvaluateFloat(__global HitPoint *hitPoint,
		const float omega, const int octaves, __global const TextureMapping3D *mapping) {
	const float3 mapP = TextureMapping3D_Map(mapping, hitPoint);

	return FBm(mapP, omega, octaves);
}

float3 FBMTexture_ConstEvaluateSpectrum(__global HitPoint *hitPoint,
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

float3 MarbleTexture_Evaluate(__global HitPoint *hitPoint, const float scale,
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

float MarbleTexture_ConstEvaluateFloat(__global HitPoint *hitPoint, const float scale,
		const float omega, const int octaves, const float variation,
		__global const TextureMapping3D *mapping) {
	return Spectrum_Y(MarbleTexture_Evaluate(hitPoint, scale, omega, octaves,
			variation, mapping));
}

float3 MarbleTexture_ConstEvaluateSpectrum(__global HitPoint *hitPoint, const float scale,
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

bool DotsTexture_Evaluate(__global HitPoint *hitPoint, __global const TextureMapping2D *mapping) {
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
		const float value1, const float value2, __global const TextureMapping2D *mapping) {
	return DotsTexture_Evaluate(hitPoint, mapping) ? value1 : value2;
}

float3 DotsTexture_ConstEvaluateSpectrum(__global HitPoint *hitPoint,
		const float3 value1, const float3 value2, __global const TextureMapping2D *mapping) {
	return DotsTexture_Evaluate(hitPoint, mapping) ? value1 : value2;
}

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

float BrickTexture_ConstEvaluateFloat(__global HitPoint *hitPoint,
		const float value1, const float value2, const float value3,
		const MasonryBond bond,
		const float brickwidth, const float brickheight,
		const float brickdepth, const float mortarsize,
		const float3 offset,
		const float run , const float mortarwidth,
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

float3 BrickTexture_ConstEvaluateSpectrum(__global HitPoint *hitPoint,
		const float3 value1, const float3 value2, const float3 value3,
		const MasonryBond bond,
		const float brickwidth, const float brickheight,
		const float brickdepth, const float mortarsize,
		const float3 offset,
		const float run , const float mortarwidth,
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

float AddTexture_ConstEvaluateFloat(__global HitPoint *hitPoint,
		const float value1, const float value2) {
	return value1 + value2;
}

float3 AddTexture_ConstEvaluateSpectrum(__global HitPoint *hitPoint,
		const float3 value1, const float3 value2) {
	return value1 + value2;
}

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

#endif

//------------------------------------------------------------------------------
// Windy texture
//------------------------------------------------------------------------------

#if defined(PARAM_ENABLE_WINDY)

float WindyTexture_ConstEvaluateFloat(__global HitPoint *hitPoint,
		__global const TextureMapping3D *mapping) {
	const float3 mapP = TextureMapping3D_Map(mapping, hitPoint);

	const float windStrength = FBm(.1f * mapP, .5f, 3);
	const float waveHeight = FBm(mapP, .5f, 6);

	return fabs(windStrength) * waveHeight;
}

float3 WindyTexture_ConstEvaluateSpectrum(__global HitPoint *hitPoint,
		__global const TextureMapping3D *mapping) {
	return WindyTexture_ConstEvaluateFloat(hitPoint, mapping);
}

#endif

//------------------------------------------------------------------------------
// Wrinkled texture
//------------------------------------------------------------------------------

#if defined(PARAM_ENABLE_WRINKLED)

float WrinkledTexture_ConstEvaluateFloat(__global HitPoint *hitPoint,
		const float omega, const int octaves,
		__global const TextureMapping3D *mapping) {
	const float3 mapP = TextureMapping3D_Map(mapping, hitPoint);

	return Turbulence(mapP, omega, octaves);
}

float3 WrinkledTexture_ConstEvaluateSpectrum(__global HitPoint *hitPoint,
		const float omega, const int octaves,
		__global const TextureMapping3D *mapping) {
	return WrinkledTexture_ConstEvaluateFloat(hitPoint, omega, octaves, mapping);
}

#endif

//------------------------------------------------------------------------------
// UV texture
//------------------------------------------------------------------------------

#if defined(PARAM_ENABLE_TEX_UV)

float UVTexture_ConstEvaluateFloat(__global HitPoint *hitPoint,
		__global const TextureMapping2D *mapping) {
	const float2 uv = TextureMapping2D_Map(mapping, hitPoint);

	return Spectrum_Y((float3)(uv.s0 - Floor2Int(uv.s0), uv.s1 - Floor2Int(uv.s1), 0.f));
}

float3 UVTexture_ConstEvaluateSpectrum(__global HitPoint *hitPoint,
		__global const TextureMapping2D *mapping) {
	const float2 uv = TextureMapping2D_Map(mapping, hitPoint);

	return (float3)(uv.s0 - Floor2Int(uv.s0), uv.s1 - Floor2Int(uv.s1), 0.f);
}

#endif

//------------------------------------------------------------------------------
// Band texture
//------------------------------------------------------------------------------

#if defined(PARAM_ENABLE_TEX_BAND)

float3 BandTexture_ConstEvaluateSpectrum(__global HitPoint *hitPoint,
		const InterpolationType interpType,
		const uint size, __global float *offsets,
		__global Spectrum *values, const float3 amt) {
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

float BandTexture_ConstEvaluateFloat(__global HitPoint *hitPoint,
		const InterpolationType interpType,
		const uint size, __global float *offsets,
		__global Spectrum *values, const float amt) {
	return Spectrum_Y(BandTexture_ConstEvaluateSpectrum(hitPoint,
			interpType, size, offsets, values, amt));
}

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

#endif
