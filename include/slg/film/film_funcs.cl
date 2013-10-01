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

void SampleResult_Init(__global SampleResult *sampleResult) {
	// Initialize only Spectrum fields

#if defined(PARAM_FILM_RADIANCE_GROUP_0)
	VSTORE3F(BLACK, &sampleResult->radiancePerPixelNormalized[0].r);
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_1)
	VSTORE3F(BLACK, &sampleResult->radiancePerPixelNormalized[1].r);
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_2)
	VSTORE3F(BLACK, &sampleResult->radiancePerPixelNormalized[2].r);
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_3)
	VSTORE3F(BLACK, &sampleResult->radiancePerPixelNormalized[3].r);
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_4)
	VSTORE3F(BLACK, &sampleResult->radiancePerPixelNormalized[4].r);
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_5)
	VSTORE3F(BLACK, &sampleResult->radiancePerPixelNormalized[5].r);
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_6)
	VSTORE3F(BLACK, &sampleResult->radiancePerPixelNormalized[6].r);
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_7)
	VSTORE3F(BLACK, &sampleResult->radiancePerPixelNormalized[7].r);
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_DIRECT_DIFFUSE)
	VSTORE3F(BLACK, &sampleResult->directDiffuse.r);
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_DIRECT_GLOSSY)
	VSTORE3F(BLACK, &sampleResult->directGlossy.r);
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_EMISSION)
	VSTORE3F(BLACK, &sampleResult->emission.r);
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_INDIRECT_DIFFUSE)
	VSTORE3F(BLACK, &sampleResult->indirectDiffuse.r);
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_INDIRECT_GLOSSY)
	VSTORE3F(BLACK, &sampleResult->indirectGlossy.r);
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_INDIRECT_SPECULAR)
	VSTORE3F(BLACK, &sampleResult->indirectSpecular.r);
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_DIRECT_SHADOW_MASK)
	sampleResult->directShadowMask = 1.f;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_INDIRECT_SHADOW_MASK)
	sampleResult->indirectShadowMask = 1.f;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_RAYCOUNT)
	sampleResult->rayCount = 0.f;
#endif
}

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
#if defined(PARAM_FILM_RADIANCE_GROUP_4)
	y += Spectrum_Y(VLOAD3F(&sampleResult->radiancePerPixelNormalized[4].r));
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_5)
	y += Spectrum_Y(VLOAD3F(&sampleResult->radiancePerPixelNormalized[5].r));
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_6)
	y += Spectrum_Y(VLOAD3F(&sampleResult->radiancePerPixelNormalized[6].r));
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_7)
	y += Spectrum_Y(VLOAD3F(&sampleResult->radiancePerPixelNormalized[7].r));
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

bool AtomicMin(__global float *val, const float val2) {
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

void Film_AddPixelVal(__global float *dst, const float val) {
#if defined(PARAM_USE_PIXEL_ATOMICS)
	AtomicAdd(&dst, val);
#else
	dst[0] += val;
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
	Film_AddWeightedPixel4(&((filmRadianceGroup[3])[index4]), &sampleResult->radiancePerPixelNormalized[4].r, weight);
#endif
#if defined (PARAM_FILM_RADIANCE_GROUP_4)
	Film_AddWeightedPixel4(&((filmRadianceGroup[4])[index4]), &sampleResult->radiancePerPixelNormalized[5].r, weight);
#endif
#if defined (PARAM_FILM_RADIANCE_GROUP_5)
	Film_AddWeightedPixel4(&((filmRadianceGroup[5])[index4]), &sampleResult->radiancePerPixelNormalized[6].r, weight);
#endif
#if defined (PARAM_FILM_RADIANCE_GROUP_6)
	Film_AddWeightedPixel4(&((filmRadianceGroup[6])[index4]), &sampleResult->radiancePerPixelNormalized[7].r, weight);
#endif
#if defined (PARAM_FILM_RADIANCE_GROUP_7)
	Film_AddWeightedPixel4(&((filmRadianceGroup[7])[index4]), &sampleResult->radiancePerPixelNormalized[8].r, weight);
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
#if defined(PARAM_FILM_CHANNELS_HAS_UV)
		Film_SetPixel2(&filmUV[index2], &sampleResult->uv.u);
#endif
	}

#if defined(PARAM_FILM_CHANNELS_HAS_RAYCOUNT)
	Film_AddPixelVal(&filmRayCount[index1], sampleResult->rayCount);
#endif
}

void Film_AddSample(const uint x, const uint y,
		__global SampleResult *sampleResult, const float weight
		FILM_PARAM_DECL) {
	Film_AddSampleResultColor(x, y, sampleResult, weight
			FILM_PARAM);
	Film_AddSampleResultData(x, y, sampleResult
			FILM_PARAM);
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

#if defined (PARAM_FILM_CHANNELS_HAS_DEPTH) || defined (PARAM_FILM_CHANNELS_HAS_POSITION) || defined(PARAM_FILM_CHANNELS_HAS_GEOMETRY_NORMAL) || defined(PARAM_FILM_CHANNELS_HAS_SHADING_NORMAL) || defined(PARAM_FILM_CHANNELS_HAS_MATERIAL_ID) || defined(PARAM_FILM_CHANNELS_HAS_UV) || defined(PARAM_FILM_CHANNELS_HAS_RAYCOUNT)
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

//------------------------------------------------------------------------------
// Film_Clear Kernel
//------------------------------------------------------------------------------

__kernel __attribute__((work_group_size_hint(64, 1, 1))) void Film_Clear(
		const uint filmWidth, const uint filmHeight
#if defined(PARAM_FILM_RADIANCE_GROUP_0)
		, __global float *filmRadianceGroup0
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_1)
		, __global float *filmRadianceGroup1
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_2)
		, __global float *filmRadianceGroup2
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_3)
		, __global float *filmRadianceGroup3
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_4)
		, __global float *filmRadianceGroup4
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_5)
		, __global float *filmRadianceGroup5
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_6)
		, __global float *filmRadianceGroup6
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_7)
		, __global float *filmRadianceGroup7
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_ALPHA)
		, __global float *filmAlpha
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_DEPTH)
		, __global float *filmDepth
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_POSITION)
		, __global float *filmPosition
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_GEOMETRY_NORMAL)
		, __global float *filmGeometryNormal
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_SHADING_NORMAL)
		, __global float *filmShadingNormal
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_MATERIAL_ID)
		, __global uint *filmMaterialID
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_DIRECT_DIFFUSE)
		, __global float *filmDirectDiffuse
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_DIRECT_GLOSSY)
		, __global float *filmDirectGlossy
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_EMISSION)
		, __global float *filmEmission
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_INDIRECT_DIFFUSE)
		, __global float *filmIndirectDiffuse
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_INDIRECT_GLOSSY)
		, __global float *filmIndirectGlossy
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_INDIRECT_SPECULAR)
		, __global float *filmIndirectSpecular
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_MATERIAL_ID_MASK)
		, __global float *filmMaterialIDMask
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_DIRECT_SHADOW_MASK)
		, __global float *filmDirectShadowMask
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_INDIRECT_SHADOW_MASK)
		, __global float *filmIndirectShadowMask
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_UV)
		, __global float *filmUV
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_RAYCOUNT)
		, __global float *filmRayCount
#endif
		) {
	const size_t gid = get_global_id(0);
	if (gid >= filmWidth * filmHeight)
		return;

#if defined (PARAM_FILM_RADIANCE_GROUP_0)
	filmRadianceGroup0[gid * 4] = 0.f;
	filmRadianceGroup0[gid * 4 + 1] = 0.f;
	filmRadianceGroup0[gid * 4 + 2] = 0.f;
	filmRadianceGroup0[gid * 4 + 3] = 0.f;
#endif
#if defined (PARAM_FILM_RADIANCE_GROUP_1)
	filmRadianceGroup1[gid * 4] = 0.f;
	filmRadianceGroup1[gid * 4 + 1] = 0.f;
	filmRadianceGroup1[gid * 4 + 2] = 0.f;
	filmRadianceGroup1[gid * 4 + 3] = 0.f;
#endif
#if defined (PARAM_FILM_RADIANCE_GROUP_2)
	filmRadianceGroup2[gid * 4] = 0.f;
	filmRadianceGroup2[gid * 4 + 1] = 0.f;
	filmRadianceGroup2[gid * 4 + 2] = 0.f;
	filmRadianceGroup2[gid * 4 + 3] = 0.f;
#endif
#if defined (PARAM_FILM_RADIANCE_GROUP_3)
	filmRadianceGroup3[gid * 4] = 0.f;
	filmRadianceGroup3[gid * 4 + 1] = 0.f;
	filmRadianceGroup3[gid * 4 + 2] = 0.f;
	filmRadianceGroup3[gid * 4 + 3] = 0.f;
#endif
#if defined (PARAM_FILM_RADIANCE_GROUP_4)
	filmRadianceGroup4[gid * 4] = 0.f;
	filmRadianceGroup4[gid * 4 + 1] = 0.f;
	filmRadianceGroup4[gid * 4 + 2] = 0.f;
	filmRadianceGroup4[gid * 4 + 3] = 0.f;
#endif
#if defined (PARAM_FILM_RADIANCE_GROUP_5)
	filmRadianceGroup5[gid * 4] = 0.f;
	filmRadianceGroup5[gid * 4 + 1] = 0.f;
	filmRadianceGroup5[gid * 4 + 2] = 0.f;
	filmRadianceGroup5[gid * 4 + 3] = 0.f;
#endif
#if defined (PARAM_FILM_RADIANCE_GROUP_6)
	filmRadianceGroup6[gid * 4] = 0.f;
	filmRadianceGroup6[gid * 4 + 1] = 0.f;
	filmRadianceGroup6[gid * 4 + 2] = 0.f;
	filmRadianceGroup6[gid * 4 + 3] = 0.f;
#endif
#if defined (PARAM_FILM_RADIANCE_GROUP_7)
	filmRadianceGroup7[gid * 4] = 0.f;
	filmRadianceGroup7[gid * 4 + 1] = 0.f;
	filmRadianceGroup7[gid * 4 + 2] = 0.f;
	filmRadianceGroup7[gid * 4 + 3] = 0.f;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_ALPHA)
	filmAlpha[gid * 2] = 0.f;
	filmAlpha[gid * 2 + 1] = 0.f;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_DEPTH)
	filmDepth[gid] = INFINITY;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_POSITION)
	filmPosition[gid * 3] = INFINITY;
	filmPosition[gid * 3 + 1] = INFINITY;
	filmPosition[gid * 3 + 2] = INFINITY;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_GEOMETRY_NORMAL)
	filmGeometryNormal[gid * 3] = INFINITY;
	filmGeometryNormal[gid * 3 + 1] = INFINITY;
	filmGeometryNormal[gid * 3 + 2] = INFINITY;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_SHADING_NORMAL)
	filmShadingNormal[gid * 3] = INFINITY;
	filmShadingNormal[gid * 3 + 1] = INFINITY;
	filmShadingNormal[gid * 3 + 2] = INFINITY;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_MATERIAL_ID)
	filmMaterialID[gid] = NULL_INDEX;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_DIRECT_DIFFUSE)
	filmDirectDiffuse[gid * 4] = 0.f;
	filmDirectDiffuse[gid * 4 + 1] = 0.f;
	filmDirectDiffuse[gid * 4 + 2] = 0.f;
	filmDirectDiffuse[gid * 4 + 3] = 0.f;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_DIRECT_GLOSSY)
	filmDirectGlossy[gid * 4] = 0.f;
	filmDirectGlossy[gid * 4 + 1] = 0.f;
	filmDirectGlossy[gid * 4 + 2] = 0.f;
	filmDirectGlossy[gid * 4 + 3] = 0.f;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_EMISSION)
	filmEmission[gid * 4] = 0.f;
	filmEmission[gid * 4 + 1] = 0.f;
	filmEmission[gid * 4 + 2] = 0.f;
	filmEmission[gid * 4 + 3] = 0.f;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_INDIRECT_DIFFUSE)
	filmIndirectDiffuse[gid * 4] = 0.f;
	filmIndirectDiffuse[gid * 4 + 1] = 0.f;
	filmIndirectDiffuse[gid * 4 + 2] = 0.f;
	filmIndirectDiffuse[gid * 4 + 3] = 0.f;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_INDIRECT_GLOSSY)
	filmIndirectGlossy[gid * 4] = 0.f;
	filmIndirectGlossy[gid * 4 + 1] = 0.f;
	filmIndirectGlossy[gid * 4 + 2] = 0.f;
	filmIndirectGlossy[gid * 4 + 3] = 0.f;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_INDIRECT_SPECULAR)
	filmIndirectSpecular[gid * 4] = 0.f;
	filmIndirectSpecular[gid * 4 + 1] = 0.f;
	filmIndirectSpecular[gid * 4 + 2] = 0.f;
	filmIndirectSpecular[gid * 4 + 3] = 0.f;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_MATERIAL_ID_MASK)
	filmMaterialIDMask[gid * 2] = 0.f;
	filmMaterialIDMask[gid * 2 + 1] = 0.f;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_DIRECT_SHADOW_MASK)
	filmDirectShadowMask[gid * 2] = 0.f;
	filmDirectShadowMask[gid * 2 + 1] = 0.f;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_INDIRECT_SHADOW_MASK)
	filmIndirectShadowMask[gid * 2] = 0.f;
	filmIndirectShadowMask[gid * 2 + 1] = 0.f;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_UV)
	filmUV[gid * 2] = INFINITY;
	filmUV[gid * 2 + 1] = INFINITY;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_RAYCOUNT)
	filmRayCount[gid] = 0;
#endif
}
