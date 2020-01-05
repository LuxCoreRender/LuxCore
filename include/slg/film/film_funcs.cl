#line 2 "film_funcs.cl"

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

OPENCL_FORCE_INLINE void Film_SetPixel2(__global float *dst, __global  float *val) {
	dst[0] = val[0];
	dst[1] = val[1];
}

OPENCL_FORCE_INLINE void Film_SetPixel3(__global float *dst, __global  float *val) {
	dst[0] = val[0];
	dst[1] = val[1];
	dst[2] = val[2];
}

OPENCL_FORCE_INLINE bool Film_MinPixel(__global float *dst, const float val) {
#if defined(PARAM_USE_PIXEL_ATOMICS)
	return AtomicMin(&dst[0], val);
#else
	if (val < dst[0]) {
		dst[0] = val;
		return true;
	} else
		return false;
#endif
}

OPENCL_FORCE_INLINE void Film_IncPixelUInt(__global uint *dst) {
#if defined(PARAM_USE_PIXEL_ATOMICS)
	atomic_inc(dst);
#else
	*dst += 1;
#endif
}

OPENCL_FORCE_INLINE void Film_AddPixelVal(__global float *dst, const float val) {
#if defined(PARAM_USE_PIXEL_ATOMICS)
	AtomicAdd(&dst[0], val);
#else
	dst[0] += val;
#endif
}

OPENCL_FORCE_INLINE void Film_AddWeightedPixel2Val(__global float *dst, const float val, const float weight) {
#if defined(PARAM_USE_PIXEL_ATOMICS)
	AtomicAdd(&dst[0], val * weight);
	AtomicAdd(&dst[1], weight);
#else
	dst[0] += val * weight;
	dst[1] += weight;
#endif
}

OPENCL_FORCE_INLINE void Film_AddWeightedPixel2(__global float *dst, __global float *val, const float weight) {
#if defined(PARAM_USE_PIXEL_ATOMICS)
	AtomicAdd(&dst[0], val[0] * weight);
	AtomicAdd(&dst[1], weight);
#else
	dst[0] += val[0] * weight;
	dst[1] += weight;
#endif
}

OPENCL_FORCE_INLINE void Film_AddWeightedPixel4Val(__global float *dst, float3 val, const float weight) {
	const float r = val.s0;
	const float g = val.s1;
	const float b = val.s2;

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

OPENCL_FORCE_INLINE void Film_AddWeightedPixel4(__global float *dst, __global float *val, const float weight) {
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

OPENCL_FORCE_INLINE void Film_AddSampleResultColor(const uint x, const uint y,
		__global SampleResult *sampleResult, const float weight
		FILM_PARAM_DECL) {
	const uint index1 = x + y * filmWidth;
	const uint index2 = index1 * 2;
	const uint index4 = index1 * 4;

#if defined(PARAM_FILM_RADIANCE_GROUP_0)
	Film_AddWeightedPixel4(&((filmRadianceGroup[0])[index4]), sampleResult->radiancePerPixelNormalized[0].c, weight);
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_1)
	Film_AddWeightedPixel4(&((filmRadianceGroup[1])[index4]), sampleResult->radiancePerPixelNormalized[1].c, weight);
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_2)
	Film_AddWeightedPixel4(&((filmRadianceGroup[2])[index4]), sampleResult->radiancePerPixelNormalized[2].c, weight);
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_3)
	Film_AddWeightedPixel4(&((filmRadianceGroup[3])[index4]), sampleResult->radiancePerPixelNormalized[3].c, weight);
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_4)
	Film_AddWeightedPixel4(&((filmRadianceGroup[4])[index4]), sampleResult->radiancePerPixelNormalized[4].c, weight);
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_5)
	Film_AddWeightedPixel4(&((filmRadianceGroup[5])[index4]), sampleResult->radiancePerPixelNormalized[5].c, weight);
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_6)
	Film_AddWeightedPixel4(&((filmRadianceGroup[6])[index4]), sampleResult->radiancePerPixelNormalized[6].c, weight);
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_7)
	Film_AddWeightedPixel4(&((filmRadianceGroup[7])[index4]), sampleResult->radiancePerPixelNormalized[7].c, weight);
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_ALPHA)
	Film_AddWeightedPixel2(&filmAlpha[index2], &sampleResult->alpha, weight);
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_DIRECT_DIFFUSE)
	Film_AddWeightedPixel4(&filmDirectDiffuse[index4], sampleResult->directDiffuse.c, weight);
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_DIRECT_GLOSSY)
	Film_AddWeightedPixel4(&filmDirectGlossy[index4], sampleResult->directGlossy.c, weight);
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_EMISSION)
	Film_AddWeightedPixel4(&filmEmission[index4], sampleResult->emission.c, weight);
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_INDIRECT_DIFFUSE)
	Film_AddWeightedPixel4(&filmIndirectDiffuse[index4], sampleResult->indirectDiffuse.c, weight);
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_INDIRECT_GLOSSY)
	Film_AddWeightedPixel4(&filmIndirectGlossy[index4], sampleResult->indirectGlossy.c, weight);
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_INDIRECT_SPECULAR)
	Film_AddWeightedPixel4(&filmIndirectSpecular[index4], sampleResult->indirectSpecular.c, weight);
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
#if defined(PARAM_FILM_CHANNELS_HAS_BY_MATERIAL_ID)
	float3 byMaterialIDColor = BLACK;
	
	if (sampleResult->materialID == PARAM_FILM_BY_MATERIAL_ID) {
#if defined(PARAM_FILM_RADIANCE_GROUP_0)
		byMaterialIDColor += VLOAD3F(sampleResult->radiancePerPixelNormalized[0].c);
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_1)
		byMaterialIDColor += VLOAD3F(sampleResult->radiancePerPixelNormalized[1].c);
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_2)
		byMaterialIDColor += VLOAD3F(sampleResult->radiancePerPixelNormalized[2].c);
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_3)
		byMaterialIDColor += VLOAD3F(sampleResult->radiancePerPixelNormalized[3].c);
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_4)
		byMaterialIDColor += VLOAD3F(sampleResult->radiancePerPixelNormalized[4].c);
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_5)
		byMaterialIDColor += VLOAD3F(sampleResult->radiancePerPixelNormalized[5].c);
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_6)
		byMaterialIDColor += VLOAD3F(sampleResult->radiancePerPixelNormalized[6].c);
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_7)
		byMaterialIDColor += VLOAD3F(sampleResult->radiancePerPixelNormalized[7].c);
#endif
	}
	Film_AddWeightedPixel4Val(&filmByMaterialID[index4], byMaterialIDColor, weight);
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_IRRADIANCE)
	Film_AddWeightedPixel4(&filmIrradiance[index4], sampleResult->irradiance.c, weight);
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_OBJECT_ID_MASK)
	const float objectIDMask = (sampleResult->objectID == PARAM_FILM_MASK_OBJECT_ID) ? 1.f : 0.f;
	Film_AddWeightedPixel2Val(&filmObjectIDMask[index2], objectIDMask, weight);
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_BY_OBJECT_ID)
	float3 byObjectIDColor = BLACK;
	
	if (sampleResult->objectID == PARAM_FILM_BY_OBJECT_ID) {
#if defined(PARAM_FILM_RADIANCE_GROUP_0)
		byObjectIDColor += VLOAD3F(sampleResult->radiancePerPixelNormalized[0].c);
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_1)
		byObjectIDColor += VLOAD3F(sampleResult->radiancePerPixelNormalized[1].c);
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_2)
		byObjectIDColor += VLOAD3F(sampleResult->radiancePerPixelNormalized[2].c);
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_3)
		byObjectIDColor += VLOAD3F(sampleResult->radiancePerPixelNormalized[3].c);
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_4)
		byObjectIDColor += VLOAD3F(sampleResult->radiancePerPixelNormalized[4].c);
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_5)
		byObjectIDColor += VLOAD3F(sampleResult->radiancePerPixelNormalized[5].c);
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_6)
		byObjectIDColor += VLOAD3F(sampleResult->radiancePerPixelNormalized[6].c);
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_7)
		byObjectIDColor += VLOAD3F(sampleResult->radiancePerPixelNormalized[7].c);
#endif
	}
	Film_AddWeightedPixel4Val(&filmByObjectID[index4], byObjectIDColor, weight);
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_MATERIAL_ID_COLOR)
	const uint matID = sampleResult->materialID;

	float3 matIDCol;
	matIDCol.s0 = (matID & 0x0000ffu) * (1.f / 255.f);
	matIDCol.s1 = ((matID & 0x00ff00u) >> 8) * (1.f / 255.f);
	matIDCol.s2 = ((matID & 0xff0000u) >> 16) * (1.f / 255.f);

	Film_AddWeightedPixel4Val(&filmMaterialIDColor[index4], matIDCol, weight);
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_ALBEDO)
	Film_AddWeightedPixel4(&filmAlbedo[index4], sampleResult->albedo.c, weight);
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_AVG_SHADING_NORMAL)
	Film_AddWeightedPixel4(&filmAvgShadingNormal[index4], &sampleResult->shadingNormal.x, weight);
#endif
}

OPENCL_FORCE_INLINE void Film_AddSampleResultData(const uint x, const uint y,
		__global SampleResult *sampleResult
		FILM_PARAM_DECL) {
	const uint index1 = x + y * filmWidth;
	const uint index2 = index1 * 2;
	const uint index3 = index1 * 3;

	bool depthWrite = true;
#if defined(PARAM_FILM_CHANNELS_HAS_DEPTH)
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
#if defined(PARAM_FILM_CHANNELS_HAS_OBJECT_ID)
		const uint objectID = sampleResult->objectID;
		if (objectID != NULL_INDEX)
			filmObjectID[index1] = sampleResult->objectID;
#endif
	}

#if defined(PARAM_FILM_CHANNELS_HAS_RAYCOUNT)
	Film_AddPixelVal(&filmRayCount[index1], sampleResult->rayCount);
#endif

#if defined(PARAM_FILM_CHANNELS_HAS_SAMPLECOUNT)
	Film_IncPixelUInt(&filmSampleCount[index1]);
#endif
}

OPENCL_FORCE_NOT_INLINE void Film_AddSample(const uint x, const uint y,
		__global SampleResult *sampleResult, const float weight
		FILM_PARAM_DECL) {
#if defined(PARAM_FILM_DENOISER)
	// Add the sample to film denoiser sample accumulator
	FilmDenoiser_AddSample(
			x, y, sampleResult, weight,
			filmWidth, filmHeight
			FILM_DENOISER_PARAM);
#endif

	Film_AddSampleResultColor(x, y, sampleResult, weight
			FILM_PARAM);
	Film_AddSampleResultData(x, y, sampleResult
			FILM_PARAM);
}

//------------------------------------------------------------------------------
// Film kernel parameters
//------------------------------------------------------------------------------

#if defined(PARAM_FILM_RADIANCE_GROUP_0)
#define KERNEL_ARGS_FILM_RADIANCE_GROUP_0 \
		, __global float *filmRadianceGroup0
#else
#define KERNEL_ARGS_FILM_RADIANCE_GROUP_0
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_1)
#define KERNEL_ARGS_FILM_RADIANCE_GROUP_1 \
		, __global float *filmRadianceGroup1
#else
#define KERNEL_ARGS_FILM_RADIANCE_GROUP_1
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_2)
#define KERNEL_ARGS_FILM_RADIANCE_GROUP_2 \
		, __global float *filmRadianceGroup2
#else
#define KERNEL_ARGS_FILM_RADIANCE_GROUP_2
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_3)
#define KERNEL_ARGS_FILM_RADIANCE_GROUP_3 \
		, __global float *filmRadianceGroup3
#else
#define KERNEL_ARGS_FILM_RADIANCE_GROUP_3
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_4)
#define KERNEL_ARGS_FILM_RADIANCE_GROUP_4 \
		, __global float *filmRadianceGroup4
#else
#define KERNEL_ARGS_FILM_RADIANCE_GROUP_4
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_5)
#define KERNEL_ARGS_FILM_RADIANCE_GROUP_5 \
		, __global float *filmRadianceGroup5
#else
#define KERNEL_ARGS_FILM_RADIANCE_GROUP_5
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_6)
#define KERNEL_ARGS_FILM_RADIANCE_GROUP_6 \
		, __global float *filmRadianceGroup6
#else
#define KERNEL_ARGS_FILM_RADIANCE_GROUP_6
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_7)
#define KERNEL_ARGS_FILM_RADIANCE_GROUP_7 \
		, __global float *filmRadianceGroup7
#else
#define KERNEL_ARGS_FILM_RADIANCE_GROUP_7
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_ALPHA)
#define KERNEL_ARGS_FILM_CHANNELS_ALPHA \
		, __global float *filmAlpha
#else
#define KERNEL_ARGS_FILM_CHANNELS_ALPHA
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_DEPTH)
#define KERNEL_ARGS_FILM_CHANNELS_DEPTH \
		, __global float *filmDepth
#else
#define KERNEL_ARGS_FILM_CHANNELS_DEPTH
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_POSITION)
#define KERNEL_ARGS_FILM_CHANNELS_POSITION \
		, __global float *filmPosition
#else
#define KERNEL_ARGS_FILM_CHANNELS_POSITION
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_GEOMETRY_NORMAL)
#define KERNEL_ARGS_FILM_CHANNELS_GEOMETRY_NORMAL \
		, __global float *filmGeometryNormal
#else
#define KERNEL_ARGS_FILM_CHANNELS_GEOMETRY_NORMAL
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_SHADING_NORMAL)
#define KERNEL_ARGS_FILM_CHANNELS_SHADING_NORMAL \
		, __global float *filmShadingNormal
#else
#define KERNEL_ARGS_FILM_CHANNELS_SHADING_NORMAL
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_MATERIAL_ID)
#define KERNEL_ARGS_FILM_CHANNELS_MATERIAL_ID \
		, __global uint *filmMaterialID
#else
#define KERNEL_ARGS_FILM_CHANNELS_MATERIAL_ID
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_DIRECT_DIFFUSE)
#define KERNEL_ARGS_FILM_CHANNELS_DIRECT_DIFFUSE \
		, __global float *filmDirectDiffuse
#else
#define KERNEL_ARGS_FILM_CHANNELS_DIRECT_DIFFUSE
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_DIRECT_GLOSSY)
#define KERNEL_ARGS_FILM_CHANNELS_DIRECT_GLOSSY \
		, __global float *filmDirectGlossy
#else
#define KERNEL_ARGS_FILM_CHANNELS_DIRECT_GLOSSY
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_EMISSION)
#define KERNEL_ARGS_FILM_CHANNELS_EMISSION \
		, __global float *filmEmission
#else
#define KERNEL_ARGS_FILM_CHANNELS_EMISSION
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_INDIRECT_DIFFUSE)
#define KERNEL_ARGS_FILM_CHANNELS_INDIRECT_DIFFUSE \
		, __global float *filmIndirectDiffuse
#else
#define KERNEL_ARGS_FILM_CHANNELS_INDIRECT_DIFFUSE
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_INDIRECT_GLOSSY)
#define KERNEL_ARGS_FILM_CHANNELS_INDIRECT_GLOSSY \
		, __global float *filmIndirectGlossy
#else
#define KERNEL_ARGS_FILM_CHANNELS_INDIRECT_GLOSSY
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_INDIRECT_SPECULAR)
#define KERNEL_ARGS_FILM_CHANNELS_INDIRECT_SPECULAR \
		, __global float *filmIndirectSpecular
#else
#define KERNEL_ARGS_FILM_CHANNELS_INDIRECT_SPECULAR
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_MATERIAL_ID_MASK)
#define KERNEL_ARGS_FILM_CHANNELS_MATERIAL_ID_MASK \
		, __global float *filmMaterialIDMask
#else
#define KERNEL_ARGS_FILM_CHANNELS_MATERIAL_ID_MASK
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_DIRECT_SHADOW_MASK)
#define KERNEL_ARGS_FILM_CHANNELS_DIRECT_SHADOW_MASK \
		, __global float *filmDirectShadowMask
#else
#define KERNEL_ARGS_FILM_CHANNELS_DIRECT_SHADOW_MASK
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_INDIRECT_SHADOW_MASK)
#define KERNEL_ARGS_FILM_CHANNELS_INDIRECT_SHADOW_MASK \
		, __global float *filmIndirectShadowMask
#else
#define KERNEL_ARGS_FILM_CHANNELS_INDIRECT_SHADOW_MASK
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_UV)
#define KERNEL_ARGS_FILM_CHANNELS_UV \
		, __global float *filmUV
#else
#define KERNEL_ARGS_FILM_CHANNELS_UV
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_RAYCOUNT)
#define KERNEL_ARGS_FILM_CHANNELS_RAYCOUNT \
		, __global float *filmRayCount
#else
#define KERNEL_ARGS_FILM_CHANNELS_RAYCOUNT
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_BY_MATERIAL_ID)
#define KERNEL_ARGS_FILM_CHANNELS_BY_MATERIAL_ID \
		, __global float *filmByMaterialID
#else
#define KERNEL_ARGS_FILM_CHANNELS_BY_MATERIAL_ID
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_IRRADIANCE)
#define KERNEL_ARGS_FILM_CHANNELS_IRRADIANCE \
		, __global float *filmIrradiance
#else
#define KERNEL_ARGS_FILM_CHANNELS_IRRADIANCE
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_OBJECT_ID)
#define KERNEL_ARGS_FILM_CHANNELS_OBJECT_ID \
		, __global uint *filmObjectID
#else
#define KERNEL_ARGS_FILM_CHANNELS_OBJECT_ID
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_OBJECT_ID_MASK)
#define KERNEL_ARGS_FILM_CHANNELS_OBJECT_ID_MASK \
		, __global float *filmObjectIDMask
#else
#define KERNEL_ARGS_FILM_CHANNELS_OBJECT_ID_MASK
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_BY_OBJECT_ID)
#define KERNEL_ARGS_FILM_CHANNELS_BY_OBJECT_ID \
		, __global float *filmByObjectID
#else
#define KERNEL_ARGS_FILM_CHANNELS_BY_OBJECT_ID
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_SAMPLECOUNT)
#define KERNEL_ARGS_FILM_CHANNELS_SAMPLECOUNT \
		, __global uint *filmSampleCount
#else
#define KERNEL_ARGS_FILM_CHANNELS_SAMPLECOUNT
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_CONVERGENCE)
#define KERNEL_ARGS_FILM_CHANNELS_CONVERGENCE \
		, __global float *filmConvergence
#else
#define KERNEL_ARGS_FILM_CHANNELS_CONVERGENCE
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_MATERIAL_ID_COLOR)
#define KERNEL_ARGS_FILM_CHANNELS_MATERIAL_ID_COLOR \
		, __global float *filmMaterialIDColor
#else
#define KERNEL_ARGS_FILM_CHANNELS_MATERIAL_ID_COLOR
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_ALBEDO)
#define KERNEL_ARGS_FILM_CHANNELS_ALBEDO \
		, __global float *filmAlbedo
#else
#define KERNEL_ARGS_FILM_CHANNELS_ALBEDO
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_AVG_SHADING_NORMAL)
#define KERNEL_ARGS_FILM_CHANNELS_AVG_SHADING_NORMAL \
		, __global float *filmAvgShadingNormal
#else
#define KERNEL_ARGS_FILM_CHANNELS_AVG_SHADING_NORMAL
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_NOISE)
#define KERNEL_ARGS_FILM_CHANNELS_NOISE \
		, __global float *filmNoise
#else
#define KERNEL_ARGS_FILM_CHANNELS_NOISE
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_USER_IMPORTANCE)
#define KERNEL_ARGS_FILM_CHANNELS_USER_IMPORTANCE \
		, __global float *filmUserImportance
#else
#define KERNEL_ARGS_FILM_CHANNELS_USER_IMPORTANCE
#endif
//------------------------------------------------------------------------------

#if defined(PARAM_FILM_DENOISER)

#if defined(PARAM_FILM_RADIANCE_GROUP_0)
#define KERNEL_ARGS_FILM_RADIANCE_GROUP_SCALE_0 \
		, float filmRadianceGroupScale0_R \
		, float filmRadianceGroupScale0_G \
		, float filmRadianceGroupScale0_B
#else
#define KERNEL_ARGS_FILM_RADIANCE_GROUP_SCALE_0
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_1)
#define KERNEL_ARGS_FILM_RADIANCE_GROUP_SCALE_1 \
		, float filmRadianceGroupScale1_R \
		, float filmRadianceGroupScale1_G \
		, float filmRadianceGroupScale1_B
#else
#define KERNEL_ARGS_FILM_RADIANCE_GROUP_SCALE_1
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_2)
#define KERNEL_ARGS_FILM_RADIANCE_GROUP_SCALE_2 \
		, float filmRadianceGroupScale2_R \
		, float filmRadianceGroupScale2_G \
		, float filmRadianceGroupScale2_B
#else
#define KERNEL_ARGS_FILM_RADIANCE_GROUP_SCALE_2
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_3)
#define KERNEL_ARGS_FILM_RADIANCE_GROUP_SCALE_3 \
		, float filmRadianceGroupScale3_R \
		, float filmRadianceGroupScale3_G \
		, float filmRadianceGroupScale3_B
#else
#define KERNEL_ARGS_FILM_RADIANCE_GROUP_SCALE_3
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_4)
#define KERNEL_ARGS_FILM_RADIANCE_GROUP_SCALE_4 \
		, float filmRadianceGroupScale4_R \
		, float filmRadianceGroupScale4_G \
		, float filmRadianceGroupScale4_B
#else
#define KERNEL_ARGS_FILM_RADIANCE_GROUP_SCALE_4
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_5)
#define KERNEL_ARGS_FILM_RADIANCE_GROUP_SCALE_5 \
		, float filmRadianceGroupScale5_R \
		, float filmRadianceGroupScale5_G \
		, float filmRadianceGroupScale5_B
#else
#define KERNEL_ARGS_FILM_RADIANCE_GROUP_SCALE_5
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_6)
#define KERNEL_ARGS_FILM_RADIANCE_GROUP_SCALE_6 \
		, float filmRadianceGroupScale6_R \
		, float filmRadianceGroupScale6_G \
		, float filmRadianceGroupScale6_B
#else
#define KERNEL_ARGS_FILM_RADIANCE_GROUP_SCALE_6
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_7)
#define KERNEL_ARGS_FILM_RADIANCE_GROUP_SCALE_7 \
		, float filmRadianceGroupScale7_R \
		, float filmRadianceGroupScale7_G \
		, float filmRadianceGroupScale7_B
#else
#define KERNEL_ARGS_FILM_RADIANCE_GROUP_SCALE_7
#endif

#define KERNEL_ARGS_FILM_DENOISER \
	, const int filmDenoiserWarmUpDone \
	, const float filmDenoiserGamma \
	, const float filmDenoiserMaxValue \
	, const float filmDenoiserSampleScale \
	, const uint filmDenoiserNbOfBins \
	, __global float *filmDenoiserNbOfSamplesImage \
	, __global float *filmDenoiserSquaredWeightSumsImage \
	, __global float *filmDenoiserMeanImage \
	, __global float *filmDenoiserCovarImage \
	, __global float *filmDenoiserHistoImage \
	KERNEL_ARGS_FILM_RADIANCE_GROUP_SCALE_0 \
	KERNEL_ARGS_FILM_RADIANCE_GROUP_SCALE_1 \
	KERNEL_ARGS_FILM_RADIANCE_GROUP_SCALE_2 \
	KERNEL_ARGS_FILM_RADIANCE_GROUP_SCALE_3 \
	KERNEL_ARGS_FILM_RADIANCE_GROUP_SCALE_4 \
	KERNEL_ARGS_FILM_RADIANCE_GROUP_SCALE_5 \
	KERNEL_ARGS_FILM_RADIANCE_GROUP_SCALE_6 \
	KERNEL_ARGS_FILM_RADIANCE_GROUP_SCALE_7
	
#else
#define KERNEL_ARGS_FILM_DENOISER
#endif

//------------------------------------------------------------------------------

#define KERNEL_ARGS_FILM \
		, const uint filmWidth, const uint filmHeight \
		, const uint filmSubRegion0, const uint filmSubRegion1 \
		, const uint filmSubRegion2, const uint filmSubRegion3 \
		KERNEL_ARGS_FILM_RADIANCE_GROUP_0 \
		KERNEL_ARGS_FILM_RADIANCE_GROUP_1 \
		KERNEL_ARGS_FILM_RADIANCE_GROUP_2 \
		KERNEL_ARGS_FILM_RADIANCE_GROUP_3 \
		KERNEL_ARGS_FILM_RADIANCE_GROUP_4 \
		KERNEL_ARGS_FILM_RADIANCE_GROUP_5 \
		KERNEL_ARGS_FILM_RADIANCE_GROUP_6 \
		KERNEL_ARGS_FILM_RADIANCE_GROUP_7 \
		KERNEL_ARGS_FILM_CHANNELS_ALPHA \
		KERNEL_ARGS_FILM_CHANNELS_DEPTH \
		KERNEL_ARGS_FILM_CHANNELS_POSITION \
		KERNEL_ARGS_FILM_CHANNELS_GEOMETRY_NORMAL \
		KERNEL_ARGS_FILM_CHANNELS_SHADING_NORMAL \
		KERNEL_ARGS_FILM_CHANNELS_MATERIAL_ID \
		KERNEL_ARGS_FILM_CHANNELS_DIRECT_DIFFUSE \
		KERNEL_ARGS_FILM_CHANNELS_DIRECT_GLOSSY \
		KERNEL_ARGS_FILM_CHANNELS_EMISSION \
		KERNEL_ARGS_FILM_CHANNELS_INDIRECT_DIFFUSE \
		KERNEL_ARGS_FILM_CHANNELS_INDIRECT_GLOSSY \
		KERNEL_ARGS_FILM_CHANNELS_INDIRECT_SPECULAR \
		KERNEL_ARGS_FILM_CHANNELS_MATERIAL_ID_MASK \
		KERNEL_ARGS_FILM_CHANNELS_DIRECT_SHADOW_MASK \
		KERNEL_ARGS_FILM_CHANNELS_INDIRECT_SHADOW_MASK \
		KERNEL_ARGS_FILM_CHANNELS_UV \
		KERNEL_ARGS_FILM_CHANNELS_RAYCOUNT \
		KERNEL_ARGS_FILM_CHANNELS_BY_MATERIAL_ID \
		KERNEL_ARGS_FILM_CHANNELS_IRRADIANCE \
		KERNEL_ARGS_FILM_CHANNELS_OBJECT_ID \
		KERNEL_ARGS_FILM_CHANNELS_OBJECT_ID_MASK \
		KERNEL_ARGS_FILM_CHANNELS_BY_OBJECT_ID \
		KERNEL_ARGS_FILM_CHANNELS_SAMPLECOUNT \
		KERNEL_ARGS_FILM_CHANNELS_CONVERGENCE \
		KERNEL_ARGS_FILM_CHANNELS_MATERIAL_ID_COLOR \
		KERNEL_ARGS_FILM_CHANNELS_ALBEDO \
		KERNEL_ARGS_FILM_CHANNELS_AVG_SHADING_NORMAL \
		KERNEL_ARGS_FILM_CHANNELS_NOISE \
		KERNEL_ARGS_FILM_CHANNELS_USER_IMPORTANCE \
		KERNEL_ARGS_FILM_DENOISER

//------------------------------------------------------------------------------
// Film_Clear Kernel
//------------------------------------------------------------------------------

__kernel __attribute__((work_group_size_hint(64, 1, 1))) void Film_Clear(
	const int dummy // This dummy variable is required by KERNEL_ARGS_FILM macro
	KERNEL_ARGS_FILM) {
	const size_t gid = get_global_id(0);
	if (gid >= filmWidth * filmHeight)
		return;

#if defined(PARAM_FILM_RADIANCE_GROUP_0)
	filmRadianceGroup0[gid * 4] = 0.f;
	filmRadianceGroup0[gid * 4 + 1] = 0.f;
	filmRadianceGroup0[gid * 4 + 2] = 0.f;
	filmRadianceGroup0[gid * 4 + 3] = 0.f;
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_1)
	filmRadianceGroup1[gid * 4] = 0.f;
	filmRadianceGroup1[gid * 4 + 1] = 0.f;
	filmRadianceGroup1[gid * 4 + 2] = 0.f;
	filmRadianceGroup1[gid * 4 + 3] = 0.f;
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_2)
	filmRadianceGroup2[gid * 4] = 0.f;
	filmRadianceGroup2[gid * 4 + 1] = 0.f;
	filmRadianceGroup2[gid * 4 + 2] = 0.f;
	filmRadianceGroup2[gid * 4 + 3] = 0.f;
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_3)
	filmRadianceGroup3[gid * 4] = 0.f;
	filmRadianceGroup3[gid * 4 + 1] = 0.f;
	filmRadianceGroup3[gid * 4 + 2] = 0.f;
	filmRadianceGroup3[gid * 4 + 3] = 0.f;
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_4)
	filmRadianceGroup4[gid * 4] = 0.f;
	filmRadianceGroup4[gid * 4 + 1] = 0.f;
	filmRadianceGroup4[gid * 4 + 2] = 0.f;
	filmRadianceGroup4[gid * 4 + 3] = 0.f;
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_5)
	filmRadianceGroup5[gid * 4] = 0.f;
	filmRadianceGroup5[gid * 4 + 1] = 0.f;
	filmRadianceGroup5[gid * 4 + 2] = 0.f;
	filmRadianceGroup5[gid * 4 + 3] = 0.f;
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_6)
	filmRadianceGroup6[gid * 4] = 0.f;
	filmRadianceGroup6[gid * 4 + 1] = 0.f;
	filmRadianceGroup6[gid * 4 + 2] = 0.f;
	filmRadianceGroup6[gid * 4 + 3] = 0.f;
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_7)
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
	filmGeometryNormal[gid * 3] = 0.f;
	filmGeometryNormal[gid * 3 + 1] = 0.f;
	filmGeometryNormal[gid * 3 + 2] = 0.f;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_SHADING_NORMAL)
	filmShadingNormal[gid * 3] = 0.f;
	filmShadingNormal[gid * 3 + 1] = 0.f;
	filmShadingNormal[gid * 3 + 2] = 0.f;
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
#if defined(PARAM_FILM_CHANNELS_HAS_BY_MATERIAL_ID)
	filmByMaterialID[gid * 4] = 0.f;
	filmByMaterialID[gid * 4 + 1] = 0.f;
	filmByMaterialID[gid * 4 + 2] = 0.f;
	filmByMaterialID[gid * 4 + 3] = 0.f;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_IRRADIANCE)
	filmIrradiance[gid * 4] = 0.f;
	filmIrradiance[gid * 4 + 1] = 0.f;
	filmIrradiance[gid * 4 + 2] = 0.f;
	filmIrradiance[gid * 4 + 3] = 0.f;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_OBJECT_ID)
	filmObjectID[gid] = NULL_INDEX;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_OBJECT_ID_MASK)
	filmObjectIDMask[gid * 2] = 0.f;
	filmObjectIDMask[gid * 2 + 1] = 0.f;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_BY_OBJECT_ID)
	filmByObjectID[gid * 4] = 0.f;
	filmByObjectID[gid * 4 + 1] = 0.f;
	filmByObjectID[gid * 4 + 2] = 0.f;
	filmByObjectID[gid * 4 + 3] = 0.f;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_SAMPLECOUNT)
	filmSampleCount[gid] = 0;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_CONVERGENCE)
	filmConvergence[gid] = INFINITY;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_MATERIAL_ID_COLOR)
	filmMaterialIDColor[gid * 4] = 0.f;
	filmMaterialIDColor[gid * 4 + 1] = 0.f;
	filmMaterialIDColor[gid * 4 + 2] = 0.f;
	filmMaterialIDColor[gid * 4 + 3] = 0.f;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_ALBEDO)
	filmAlbedo[gid * 4] = 0.f;
	filmAlbedo[gid * 4 + 1] = 0.f;
	filmAlbedo[gid * 4 + 2] = 0.f;
	filmAlbedo[gid * 4 + 3] = 0.f;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_AVG_SHADING_NORMAL)
	filmAvgShadingNormal[gid * 4] = 0.f;
	filmAvgShadingNormal[gid * 4 + 1] = 0.f;
	filmAvgShadingNormal[gid * 4 + 2] = 0.f;
	filmAvgShadingNormal[gid * 4 + 3] = 0.f;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_NOISE)
	filmNoise[gid] = INFINITY;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_USER_IMPORTANCE)
	filmNoise[gid] = 1.f;
#endif

	//--------------------------------------------------------------------------
	// Film denoiser buffers
	//--------------------------------------------------------------------------

#if defined(PARAM_FILM_DENOISER)
	filmDenoiserNbOfSamplesImage[gid] = 0.f;
	filmDenoiserSquaredWeightSumsImage[gid] = 0.f;

	filmDenoiserMeanImage[gid * 3 + 0] = 0.f;
	filmDenoiserMeanImage[gid * 3 + 1] = 0.f;
	filmDenoiserMeanImage[gid * 3 + 2] = 0.f;

	filmDenoiserCovarImage[gid * 6 + 0] = 0.f;
	filmDenoiserCovarImage[gid * 6 + 1] = 0.f;
	filmDenoiserCovarImage[gid * 6 + 2] = 0.f;
	filmDenoiserCovarImage[gid * 6 + 3] = 0.f;
	filmDenoiserCovarImage[gid * 6 + 4] = 0.f;
	filmDenoiserCovarImage[gid * 6 + 5] = 0.f;

	for (uint channelIndex = 0; channelIndex < 3; ++channelIndex)
		for (uint i = 0; i < filmDenoiserNbOfBins; ++i)
			filmDenoiserHistoImage[gid * filmDenoiserNbOfBins * 3 + channelIndex * filmDenoiserNbOfBins + i] = 0.f;
#endif
}
