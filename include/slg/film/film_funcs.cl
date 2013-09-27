#line 2 "film_funcs.cl"

/***************************************************************************
 *   Copyright (C) 1998-2013 by authors (see AUTHORS.txt)                  *
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

float SampleResult_Radiance_Y(__global SampleResult *sampleResult) {
	float y = 0.f;
#if defined(PARAM_FILM_RADIANCE_GROUP_0)
	y += Spectrum_Y(VLOAD3F(&sampleResult->radiancePerPixelNormalized[0].r));
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_1)
	y += Spectrum_Y(VLOAD3F(&sampleResult->radiancePerPixelNormalized[1].r));
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_2)
	y += Spectrum_Y(VLOAD3F(&sampleResult->radiancePerPixelNormalized[2].r));
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_3)
	y += Spectrum_Y(VLOAD3F(&sampleResult->radiancePerPixelNormalized[3].r));
#endif

	return y;
}

#if defined(PARAM_USE_PIXEL_ATOMICS)
void AtomicAdd(__global float *val, const float delta) {
	union {
		float f;
		unsigned int i;
	} oldVal;
	union {
		float f;
		unsigned int i;
	} newVal;

	do {
		oldVal.f = *val;
		newVal.f = oldVal.f + delta;
	} while (atomic_cmpxchg((__global unsigned int *)val, oldVal.i, newVal.i) != oldVal.i);
}

void AtomicMin(__global float *val, const float val2) {
	union {
		float f;
		unsigned int i;
	} oldVal;
	union {
		float f;
		unsigned int i;
	} newVal;

	bool result = false;
	do {
		oldVal.f = *val;
		if (val2 >= oldVal.f)
			return false;
		else
			newVal.f = val2;
	} while (atomic_cmpxchg((__global unsigned int *)val, oldVal.i, newVal.i) != oldVal.i);

	return true;
}
#endif

void Film_SetPixel2(__global float *dst, __global  float *val) {
	dst[0] = val[0];
	dst[1] = val[1];
}

void Film_SetPixel3(__global float *dst, __global  float *val) {
	dst[0] = val[0];
	dst[1] = val[1];
	dst[2] = val[2];
}

bool Film_MinPixel(__global float *dst, const float val) {
#if defined(PARAM_USE_PIXEL_ATOMICS)
	AtomicMin(&dst[0], val);
#else
	if (val < dst[0]) {
		dst[0] = val;
		return true;
	} else
		return false;
#endif
}

void Film_AddWeightedPixel2Val(__global float *dst, const float val, const float weight) {
#if defined(PARAM_USE_PIXEL_ATOMICS)
	AtomicAdd(&dst[0], val * weight);
	AtomicAdd(&dst[1], weight);
#else
	dst[0] += val * weight;
	dst[1] += weight;
#endif
}

void Film_AddWeightedPixel2(__global float *dst, __global float *val, const float weight) {
#if defined(PARAM_USE_PIXEL_ATOMICS)
	AtomicAdd(&dst[0], val[0] * weight);
	AtomicAdd(&dst[1], weight);
#else
	dst[0] += val[0] * weight;
	dst[1] += weight;
#endif
}

void Film_AddWeightedPixel4(__global float *dst, __global float *val, const float weight) {
	const float r = val[0];
	const float g = val[1];
	const float b = val[2];

	if (!isnan(r) && !isinf(r) &&
			!isnan(g) && !isinf(g) &&
			!isnan(b) && !isinf(b) &&
			!isnan(weight) && !isinf(weight)) {
#if defined(PARAM_USE_PIXEL_ATOMICS)
		AtomicAdd(&dst[0], r * weight);
		AtomicAdd(&dst[1], g * weight);
		AtomicAdd(&dst[2], b * weight);
		AtomicAdd(&dst[3], weight);
#else
		float4 p = VLOAD4F(dst);
		const float4 s = (float4)(r * weight, g * weight, b * weight, weight);
		p += s;
		VSTORE4F(p, dst);
#endif
	} /*else {
		printf("NaN/Inf. error: (%f, %f, %f) [%f]\n", r, g, b, weight);
	}*/
}

void Film_AddSampleResultColor(const uint x, const uint y,
		__global SampleResult *sampleResult, const float weight
		FILM_PARAM_DECL) {
	const uint index1 = x + y * filmWidth;
	const uint index2 = index1 * 2;
	const uint index4 = index1 * 4;

#if defined (PARAM_FILM_RADIANCE_GROUP_0)
	Film_AddWeightedPixel4(&((filmRadianceGroup[0])[index4]), &sampleResult->radiancePerPixelNormalized[0].r, weight);
#endif
#if defined (PARAM_FILM_RADIANCE_GROUP_1)
	Film_AddWeightedPixel4(&((filmRadianceGroup[1])[index4]), &sampleResult->radiancePerPixelNormalized[1].r, weight);
#endif
#if defined (PARAM_FILM_RADIANCE_GROUP_2)
	Film_AddWeightedPixel4(&((filmRadianceGroup[2])[index4]), &sampleResult->radiancePerPixelNormalized[2].r, weight);
#endif
#if defined (PARAM_FILM_RADIANCE_GROUP_3)
	Film_AddWeightedPixel4(&((filmRadianceGroup[3])[index4]), &sampleResult->radiancePerPixelNormalized[3].r, weight);
#endif
#if defined (PARAM_FILM_RADIANCE_GROUP_3)
	Film_AddWeightedPixel4(&((filmRadianceGroup[3])[index4]), &sampleResult->radiancePerPixelNormalized[3].r, weight);
#endif
#if defined (PARAM_FILM_CHANNELS_HAS_ALPHA)
	Film_AddWeightedPixel2(&filmAlpha[index2], &sampleResult->alpha, weight);
#endif
#if defined (PARAM_FILM_CHANNELS_HAS_DIRECT_DIFFUSE)
	Film_AddWeightedPixel4(&filmDirectDiffuse[index4], &sampleResult->directDiffuse.r, weight);
#endif
#if defined (PARAM_FILM_CHANNELS_HAS_DIRECT_GLOSSY)
	Film_AddWeightedPixel4(&filmDirectGlossy[index4], &sampleResult->directGlossy.r, weight);
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_EMISSION)
	Film_AddWeightedPixel4(&filmEmission[index4], &sampleResult->emission.r, weight);
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_INDIRECT_DIFFUSE)
	Film_AddWeightedPixel4(&filmIndirectDiffuse[index4], &sampleResult->indirectDiffuse.r, weight);
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_INDIRECT_GLOSSY)
	Film_AddWeightedPixel4(&filmIndirectGlossy[index4], &sampleResult->indirectGlossy.r, weight);
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_INDIRECT_SPECULAR)
	Film_AddWeightedPixel4(&filmIndirectSpecular[index4], &sampleResult->indirectSpecular.r, weight);
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_MATERIAL_ID_MASK)
	const float materialIDMask = (sampleResult->materialID == PARAM_FILM_MASK_MATERIAL_ID) ? 1.f : 0.f;
	Film_AddWeightedPixel2Val(&filmMaterialIDMask[index2], materialIDMask, weight);
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_DIRECT_SHADOW_MASK)
	Film_AddWeightedPixel2(&filmDirectShadowMask[index2], &sampleResult->directShadowMask, weight);
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_INDIRECT_SHADOW_MASK)
	Film_AddWeightedPixel2(&filmIndirectShadowMask[index2], &sampleResult->indirectShadowMask, weight);
#endif
}

void Film_AddSampleResultData(const uint x, const uint y,
		__global SampleResult *sampleResult
		FILM_PARAM_DECL) {
	const uint index1 = x + y * filmWidth;
	const uint index2 = index1 * 2;
	const uint index3 = index1 * 3;

	bool depthWrite = true;
#if defined (PARAM_FILM_CHANNELS_HAS_DEPTH)
	depthWrite = Film_MinPixel(&filmDepth[index1], sampleResult->depth);
#endif

	if (depthWrite) {
#if defined(PARAM_FILM_CHANNELS_HAS_POSITION)
		Film_SetPixel3(&filmPosition[index3], &sampleResult->position.x);
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_GEOMETRY_NORMAL)
		Film_SetPixel3(&filmGeometryNormal[index3], &sampleResult->geometryNormal.x);
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_SHADING_NORMAL)
		Film_SetPixel3(&filmShadingNormal[index3], &sampleResult->shadingNormal.x);
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_MATERIAL_ID)
		filmMaterialID[index1] = sampleResult->materialID;
#endif
#if defined(PARAM_FILM_CHANNELS_UV)
		Film_SetPixel2(&filmUV[index2], &sampleResult->UV.u);
#endif
	}
}

#if (PARAM_IMAGE_FILTER_TYPE == 0)

void Film_SplatSample(__global SampleResult *sampleResult, const float weight
	FILM_PARAM_DECL) {
	const int x = Ceil2Int(sampleResult->filmX - .5f);
	const int y = Ceil2Int(sampleResult->filmY - .5f);

	if ((x >= 0) && (x < (int)filmWidth) && (y >= 0) && (y < (int)filmHeight)) {
		Film_AddSampleResultColor((uint)x, (uint)y, sampleResult, weight
				FILM_PARAM);
		Film_AddSampleResultData((uint)x, (uint)y, sampleResult
				FILM_PARAM);
	}
}

#elif (PARAM_IMAGE_FILTER_TYPE == 1) || (PARAM_IMAGE_FILTER_TYPE == 2) || (PARAM_IMAGE_FILTER_TYPE == 3)

void Film_AddSampleFilteredResultColor(const int x, const int y,
		const float distX, const float distY,
		__global SampleResult *sampleResult, const float weight
		FILM_PARAM_DECL) {
	if ((x >= 0) && (x < (int)filmWidth) && (y >= 0) && (y < (int)filmHeight)) {
		const float filterWeight = ImageFilter_Evaluate(distX, distY);

		Film_AddSampleResultColor(x, y, sampleResult, weight * filterWeight
			FILM_PARAM);
	}
}

void Film_SplatSample(__global SampleResult *sampleResult, const float weight
	FILM_PARAM_DECL) {
	const float px = sampleResult->filmX;
	const float py = sampleResult->filmY;

	//----------------------------------------------------------------------
	// Add all data related information (not filtered)
	//----------------------------------------------------------------------

#if defined (PARAM_FILM_CHANNELS_HAS_DEPTH) || defined (PARAM_FILM_CHANNELS_HAS_POSITION) || defined(PARAM_FILM_CHANNELS_HAS_GEOMETRY_NORMAL) || defined(PARAM_FILM_CHANNELS_HAS_SHADING_NORMAL) || defined(PARAM_FILM_CHANNELS_HAS_MATERIAL_ID) || defined(PARAM_FILM_CHANNELS_UV)
	{
		const int x = Ceil2Int(px - .5f);
		const int y = Ceil2Int(py - .5f);
		if ((x >= 0) && (x < (int)filmWidth) && (y >= 0) && (y < (int)filmHeight)) {
			Film_AddSampleResultData((uint)x, (uint)y, sampleResult
					FILM_PARAM);
		}
	}
#endif

	//----------------------------------------------------------------------
	// Add all color related information (filtered)
	//----------------------------------------------------------------------

	const uint x = min((uint)floor(px), (uint)(filmWidth - 1));
	const uint y = min((uint)floor(py), (uint)(filmHeight - 1));

	const float sx = px - (float)x;
	const float sy = py - (float)y;

	Film_AddSampleFilteredResultColor(x - 1, y - 1, sx + 1.f, sy + 1.f,
			sampleResult, weight
			FILM_PARAM);
	Film_AddSampleFilteredResultColor(x, y - 1, sx, sy + 1.f,
			sampleResult, weight
			FILM_PARAM);
	Film_AddSampleFilteredResultColor(x + 1, y - 1, sx - 1.f, sy + 1.f,
			sampleResult, weight
			FILM_PARAM);

	Film_AddSampleFilteredResultColor(x - 1, y, sx + 1.f, sy,
			sampleResult, weight
			FILM_PARAM);
	Film_AddSampleFilteredResultColor(x, y, sx, sy,
			sampleResult, weight
			FILM_PARAM);
	Film_AddSampleFilteredResultColor(x + 1, y, sx - 1.f, sy,
			sampleResult, weight
			FILM_PARAM);

	Film_AddSampleFilteredResultColor(x - 1, y + 1, sx + 1.f, sy - 1.f,
			sampleResult, weight
			FILM_PARAM);
	Film_AddSampleFilteredResultColor(x, y + 1, sx, sy - 1.f,
			sampleResult, weight
			FILM_PARAM);
	Film_AddSampleFilteredResultColor(x + 1, y + 1, sx - 1.f, sy - 1.f,
			sampleResult, weight
			FILM_PARAM);
}

#else

Error: unknown image filter !!!

#endif
