#line 2 "film_funcs.cl"

/***************************************************************************
 * Copyright 1998-2020 by authors (see AUTHORS.txt)                        *
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

OPENCL_FORCE_INLINE bool Film_MinPixel(const bool usePixelAtomics, __global float *dst, const float val) {
	if (usePixelAtomics)
		return AtomicMin(&dst[0], val);
	else {
		if (val < dst[0]) {
			dst[0] = val;
			return true;
		} else
			return false;
	}
}

OPENCL_FORCE_INLINE void Film_IncPixelUInt(const bool usePixelAtomics, __global uint *dst) {
	if (usePixelAtomics)
		atomic_inc(dst);
	else
		*dst += 1;
}

OPENCL_FORCE_INLINE void Film_AddPixelVal(const bool usePixelAtomics, __global float *dst, const float val) {
	if (usePixelAtomics)
		AtomicAdd(&dst[0], val);
	else
		dst[0] += val;
}

OPENCL_FORCE_INLINE void Film_AddWeightedPixel2Val(const bool usePixelAtomics, __global float *dst, const float val, const float weight) {
	const float v = val;

	if (!isnan(v) && !isinf(v) &&
		!isnan(weight) && !isinf(weight)) {
		if (usePixelAtomics) {
			AtomicAdd(&dst[0], v * weight);
			AtomicAdd(&dst[1], weight);
		} else {
			dst[0] += v * weight;
			dst[1] += weight;
		}
	}
}

OPENCL_FORCE_INLINE void Film_AddWeightedPixel2(const bool usePixelAtomics, __global float *dst, __global float *val, const float weight) {
	const float v = *val;

	if (!isnan(v) && !isinf(v) &&
		!isnan(weight) && !isinf(weight)) {
		if (usePixelAtomics) {
			AtomicAdd(&dst[0], v * weight);
			AtomicAdd(&dst[1], weight);
		} else {
			dst[0] += v * weight;
			dst[1] += weight;
		}
	}
}

OPENCL_FORCE_INLINE void Film_AddIfValidWeightedPixel4Val(const bool usePixelAtomics, __global float *dst, float3 val, const float weight) {
	const float r = val.x;
	const float g = val.y;
	const float b = val.z;

	if (!isnan(r) && !isinf(r) &&
			!isnan(g) && !isinf(g) &&
			!isnan(b) && !isinf(b) &&
			!isnan(weight) && !isinf(weight)) {
		if (usePixelAtomics) {
			AtomicAdd(&dst[0], r * weight);
			AtomicAdd(&dst[1], g * weight);
			AtomicAdd(&dst[2], b * weight);
			AtomicAdd(&dst[3], weight);
		} else {
			// The following code doesn't work with CUDA
			/*float4 p = VLOAD4F(dst);
			const float4 s = MAKE_FLOAT4(r * weight, g * weight, b * weight, weight);
			p += s;
			VSTORE4F(p, dst);*/

			dst[0] += r * weight;
			dst[1] += g * weight;
			dst[2] += b * weight;
			dst[3] += weight;
		}
	} /*else {
		printf("NaN/Inf. error: (%f, %f, %f) [%f]\n", r, g, b, weight);
	}*/
}

OPENCL_FORCE_INLINE void Film_AddIfValidWeightedPixel4(const bool usePixelAtomics, __global float *dst, __global float *val, const float weight) {
	const float r = val[0];
	const float g = val[1];
	const float b = val[2];

	if (!isnan(r) && !isinf(r) &&
			!isnan(g) && !isinf(g) &&
			!isnan(b) && !isinf(b) &&
			!isnan(weight) && !isinf(weight)) {
		if (usePixelAtomics) {
			AtomicAdd(&dst[0], r * weight);
			AtomicAdd(&dst[1], g * weight);
			AtomicAdd(&dst[2], b * weight);
			AtomicAdd(&dst[3], weight);
		} else {
			// The following code doesn't work with CUDA
			/*float4 p = VLOAD4F(dst);
			const float4 s = MAKE_FLOAT4(r * weight, g * weight, b * weight, weight);
			p += s;
			VSTORE4F(p, dst);*/

			dst[0] += r * weight;
			dst[1] += g * weight;
			dst[2] += b * weight;
			dst[3] += weight;
		}
	} /*else {
		printf("NaN/Inf. error: (%f, %f, %f) [%f]\n", r, g, b, weight);
	}*/
}

OPENCL_FORCE_INLINE void Film_AddSampleResultColor(const uint x, const uint y,
		__global SampleResult *sampleResult, const float weight
		FILM_PARAM_DECL) {
	const bool usePixelAtomics = film->usePixelAtomics;

	const uint index1 = x + y * filmWidth;
	const uint index2 = index1 * 2;
	const uint index4 = index1 * 4;

	for (uint i = 0; i < FILM_MAX_RADIANCE_GROUP_COUNT; ++i) {
		if (filmRadianceGroup[i])
			Film_AddIfValidWeightedPixel4(usePixelAtomics, &((filmRadianceGroup[i])[index4]), sampleResult->radiancePerPixelNormalized[i].c, weight);
	}

	if (film->hasChannelAlpha)
		Film_AddWeightedPixel2(usePixelAtomics, &filmAlpha[index2], &sampleResult->alpha, weight);
		
	if (film->hasChannelDirectDiffuse) {
		const float3 c = VLOAD3F(sampleResult->directDiffuseReflect.c) + VLOAD3F(sampleResult->directDiffuseTransmit.c);
		Film_AddIfValidWeightedPixel4Val(usePixelAtomics, &filmDirectDiffuse[index4], c, weight);
	}
	if (film->hasChannelDirectDiffuseReflect)
		Film_AddIfValidWeightedPixel4(usePixelAtomics, &filmDirectDiffuseReflect[index4], sampleResult->directDiffuseReflect.c, weight);
	if (film->hasChannelDirectDiffuseTransmit)
		Film_AddIfValidWeightedPixel4(usePixelAtomics, &filmDirectDiffuseTransmit[index4], sampleResult->directDiffuseTransmit.c, weight);

	if (film->hasChannelDirectGlossy) {
		const float3 c = VLOAD3F(sampleResult->directGlossyReflect.c) + VLOAD3F(sampleResult->directGlossyTransmit.c);
		Film_AddIfValidWeightedPixel4Val(usePixelAtomics, &filmDirectGlossy[index4], c, weight);
	}
	if (film->hasChannelDirectGlossyReflect)
		Film_AddIfValidWeightedPixel4(usePixelAtomics, &filmDirectGlossyReflect[index4], sampleResult->directGlossyReflect.c, weight);
	if (film->hasChannelDirectGlossyTransmit)
		Film_AddIfValidWeightedPixel4(usePixelAtomics, &filmDirectGlossyTransmit[index4], sampleResult->directGlossyTransmit.c, weight);

	if (film->hasChannelEmission)
		Film_AddIfValidWeightedPixel4(usePixelAtomics, &filmEmission[index4], sampleResult->emission.c, weight);

	if (film->hasChannelIndirectDiffuse) {
		const float3 c = VLOAD3F(sampleResult->indirectDiffuseReflect.c) + VLOAD3F(sampleResult->indirectDiffuseTransmit.c);
		Film_AddIfValidWeightedPixel4Val(usePixelAtomics, &filmIndirectDiffuse[index4], c, weight);
	}
	if (film->hasChannelIndirectDiffuseReflect)
		Film_AddIfValidWeightedPixel4(usePixelAtomics, &filmIndirectDiffuseReflect[index4], sampleResult->indirectDiffuseReflect.c, weight);
	if (film->hasChannelIndirectDiffuseTransmit)
		Film_AddIfValidWeightedPixel4(usePixelAtomics, &filmIndirectDiffuseTransmit[index4], sampleResult->indirectDiffuseTransmit.c, weight);

	if (film->hasChannelIndirectGlossy) {
		const float3 c = VLOAD3F(sampleResult->indirectGlossyReflect.c) + VLOAD3F(sampleResult->indirectGlossyTransmit.c);
		Film_AddIfValidWeightedPixel4Val(usePixelAtomics, &filmIndirectGlossy[index4], c, weight);
	}
	if (film->hasChannelIndirectGlossyReflect)
		Film_AddIfValidWeightedPixel4(usePixelAtomics, &filmIndirectGlossyReflect[index4], sampleResult->indirectGlossyReflect.c, weight);
	if (film->hasChannelIndirectGlossyTransmit)
		Film_AddIfValidWeightedPixel4(usePixelAtomics, &filmIndirectGlossyTransmit[index4], sampleResult->indirectGlossyTransmit.c, weight);

	if (film->hasChannelIndirectSpecular) {
		const float3 c = VLOAD3F(sampleResult->indirectSpecularReflect.c) + VLOAD3F(sampleResult->indirectSpecularTransmit.c);
		Film_AddIfValidWeightedPixel4Val(usePixelAtomics, &filmIndirectSpecular[index4], c, weight);
	}
	if (film->hasChannelIndirectSpecularReflect)
		Film_AddIfValidWeightedPixel4(usePixelAtomics, &filmIndirectSpecularReflect[index4], sampleResult->indirectSpecularReflect.c, weight);
	if (film->hasChannelIndirectSpecularTransmit)
		Film_AddIfValidWeightedPixel4(usePixelAtomics, &filmIndirectSpecularTransmit[index4], sampleResult->indirectSpecularTransmit.c, weight);

	if (film->hasChannelMaterialIDMask) {
		const float materialIDMask = (sampleResult->materialID == film->channelMaterialIDMask) ? 1.f : 0.f;
		Film_AddWeightedPixel2Val(usePixelAtomics, &filmMaterialIDMask[index2], materialIDMask, weight);
	}
	if (film->hasChannelDirectShadowMask)
		Film_AddWeightedPixel2(usePixelAtomics, &filmDirectShadowMask[index2], &sampleResult->directShadowMask, weight);
	if (film->hasChannelIndirectShadowMask)
		Film_AddWeightedPixel2(usePixelAtomics, &filmIndirectShadowMask[index2], &sampleResult->indirectShadowMask, weight);
	if (film->hasChannelByMaterialID) {
		float3 byMaterialIDColor = BLACK;

		if (sampleResult->materialID == film->channelByMaterialID) {
			for (uint i = 0; i < FILM_MAX_RADIANCE_GROUP_COUNT; ++i) {
				if (filmRadianceGroup[i])
					byMaterialIDColor += VLOAD3F(sampleResult->radiancePerPixelNormalized[i].c);
			}
		}
		Film_AddIfValidWeightedPixel4Val(usePixelAtomics, &filmByMaterialID[index4], byMaterialIDColor, weight);
	}
	if (film->hasChannelIrradiance)
		Film_AddIfValidWeightedPixel4(usePixelAtomics, &filmIrradiance[index4], sampleResult->irradiance.c, weight);
	if (film->hasChannelObjectIDMask) {
		const float objectIDMask = (sampleResult->objectID == film->channelObjectIDMask) ? 1.f : 0.f;
		Film_AddWeightedPixel2Val(usePixelAtomics, &filmObjectIDMask[index2], objectIDMask, weight);
	}
	if (film->hasChannelByObjectID) {
		float3 byObjectIDColor = BLACK;

		if (sampleResult->objectID == film->channelByObjectID) {
			for (uint i = 0; i < FILM_MAX_RADIANCE_GROUP_COUNT; ++i) {
				if (filmRadianceGroup[i])
					byObjectIDColor += VLOAD3F(sampleResult->radiancePerPixelNormalized[i].c);
			}
		}
		Film_AddIfValidWeightedPixel4Val(usePixelAtomics, &filmByObjectID[index4], byObjectIDColor, weight);
	}
	if (film->hasChannelMaterialIDColor) {
		const uint matID = sampleResult->materialID;

		float3 matIDCol;
		matIDCol.x = (matID & 0x0000ffu) * (1.f / 255.f);
		matIDCol.y = ((matID & 0x00ff00u) >> 8) * (1.f / 255.f);
		matIDCol.z = ((matID & 0xff0000u) >> 16) * (1.f / 255.f);

		Film_AddIfValidWeightedPixel4Val(usePixelAtomics, &filmMaterialIDColor[index4], matIDCol, weight);
	}
	if (film->hasChannelAlbedo)
		Film_AddIfValidWeightedPixel4(usePixelAtomics, &filmAlbedo[index4], sampleResult->albedo.c, weight);
	if (film->hasChannelAvgShadingNormal)
		Film_AddIfValidWeightedPixel4(usePixelAtomics, &filmAvgShadingNormal[index4], &sampleResult->shadingNormal.x, weight);
}

OPENCL_FORCE_INLINE void Film_AddSampleResultData(const uint x, const uint y,
		__global SampleResult *sampleResult
		FILM_PARAM_DECL) {
	const bool usePixelAtomics = film->usePixelAtomics;

	const uint index1 = x + y * filmWidth;
	const uint index2 = index1 * 2;
	const uint index3 = index1 * 3;

	bool depthWrite = true;
	if (film->hasChannelDepth)
		depthWrite = Film_MinPixel(usePixelAtomics, &filmDepth[index1], sampleResult->depth);

	if (depthWrite) {
		if (film->hasChannelPosition)
			Film_SetPixel3(&filmPosition[index3], &sampleResult->position.x);
		if (film->hasChannelGeometryNormal)
			Film_SetPixel3(&filmGeometryNormal[index3], &sampleResult->geometryNormal.x);
		if (film->hasChannelShadingNormal)
			Film_SetPixel3(&filmShadingNormal[index3], &sampleResult->shadingNormal.x);
		if (film->hasChannelMaterialID)
			filmMaterialID[index1] = sampleResult->materialID;
		if (film->hasChannelUV)
			Film_SetPixel2(&filmUV[index2], &sampleResult->uv.u);
		if (film->hasChannelObjectID) {
			const uint objectID = sampleResult->objectID;
			if (objectID != NULL_INDEX)
				filmObjectID[index1] = sampleResult->objectID;
		}
	}

	if (film->hasChannelRayCount)
		Film_AddPixelVal(usePixelAtomics, &filmRayCount[index1], sampleResult->rayCount);

	if (film->hasChannelSampleCount)
		Film_IncPixelUInt(usePixelAtomics, &filmSampleCount[index1]);
}

// CUDA want to inline this function due to the large number of arguments
#if defined (LUXRAYS_CUDA_DEVICE)
OPENCL_FORCE_INLINE
#else
OPENCL_FORCE_NOT_INLINE
#endif
void Film_AddSample(
		const uint x, const uint y,
		__global SampleResult *sampleResult, const float weight
		FILM_PARAM_DECL) {
	if (film->bcdDenoiserEnable) {
		// Add the sample to film denoiser sample accumulator
		FilmDenoiser_AddSample(film,
				x, y, sampleResult, weight,
				filmWidth, filmHeight
				FILM_DENOISER_PARAM);
	}

	Film_AddSampleResultColor(x, y, sampleResult, weight
			FILM_PARAM);
	Film_AddSampleResultData(x, y, sampleResult
			FILM_PARAM);
}

//------------------------------------------------------------------------------
// Film kernel parameters
//------------------------------------------------------------------------------

#define KERNEL_ARGS_FILM_RADIANCE_GROUP_SCALE_0 \
		, float filmRadianceGroupScale0_R \
		, float filmRadianceGroupScale0_G \
		, float filmRadianceGroupScale0_B
#define KERNEL_ARGS_FILM_RADIANCE_GROUP_SCALE_1 \
		, float filmRadianceGroupScale1_R \
		, float filmRadianceGroupScale1_G \
		, float filmRadianceGroupScale1_B
#define KERNEL_ARGS_FILM_RADIANCE_GROUP_SCALE_2 \
		, float filmRadianceGroupScale2_R \
		, float filmRadianceGroupScale2_G \
		, float filmRadianceGroupScale2_B
#define KERNEL_ARGS_FILM_RADIANCE_GROUP_SCALE_3 \
		, float filmRadianceGroupScale3_R \
		, float filmRadianceGroupScale3_G \
		, float filmRadianceGroupScale3_B
#define KERNEL_ARGS_FILM_RADIANCE_GROUP_SCALE_4 \
		, float filmRadianceGroupScale4_R \
		, float filmRadianceGroupScale4_G \
		, float filmRadianceGroupScale4_B
#define KERNEL_ARGS_FILM_RADIANCE_GROUP_SCALE_5 \
		, float filmRadianceGroupScale5_R \
		, float filmRadianceGroupScale5_G \
		, float filmRadianceGroupScale5_B
#define KERNEL_ARGS_FILM_RADIANCE_GROUP_SCALE_6 \
		, float filmRadianceGroupScale6_R \
		, float filmRadianceGroupScale6_G \
		, float filmRadianceGroupScale6_B
#define KERNEL_ARGS_FILM_RADIANCE_GROUP_SCALE_7 \
		, float filmRadianceGroupScale7_R \
		, float filmRadianceGroupScale7_G \
		, float filmRadianceGroupScale7_B

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

//------------------------------------------------------------------------------

#define KERNEL_ARGS_FILM \
		, const uint filmWidth, const uint filmHeight \
		, const uint filmSubRegion0, const uint filmSubRegion1 \
		, const uint filmSubRegion2, const uint filmSubRegion3 \
		, __global float *filmRadianceGroup0 \
		, __global float *filmRadianceGroup1 \
		, __global float *filmRadianceGroup2 \
		, __global float *filmRadianceGroup3 \
		, __global float *filmRadianceGroup4 \
		, __global float *filmRadianceGroup5 \
		, __global float *filmRadianceGroup6 \
		, __global float *filmRadianceGroup7 \
		, __global float *filmAlpha \
		, __global float *filmDepth \
		, __global float *filmPosition \
		, __global float *filmGeometryNormal \
		, __global float *filmShadingNormal \
		, __global uint *filmMaterialID \
		, __global float *filmDirectDiffuse \
		, __global float *filmDirectDiffuseReflect \
		, __global float *filmDirectDiffuseTransmit \
		, __global float *filmDirectGlossy \
		, __global float *filmDirectGlossyReflect \
		, __global float *filmDirectGlossyTransmit \
		, __global float *filmEmission \
		, __global float *filmIndirectDiffuse \
		, __global float *filmIndirectDiffuseReflect \
		, __global float *filmIndirectDiffuseTransmit \
		, __global float *filmIndirectGlossy \
		, __global float *filmIndirectGlossyReflect \
		, __global float *filmIndirectGlossyTransmit \
		, __global float *filmIndirectSpecular \
		, __global float *filmIndirectSpecularReflect \
		, __global float *filmIndirectSpecularTransmit \
		, __global float *filmMaterialIDMask \
		, __global float *filmDirectShadowMask \
		, __global float *filmIndirectShadowMask \
		, __global float *filmUV \
		, __global float *filmRayCount \
		, __global float *filmByMaterialID \
		, __global float *filmIrradiance \
		, __global uint *filmObjectID \
		, __global float *filmObjectIDMask \
		, __global float *filmByObjectID \
		, __global uint *filmSampleCount \
		, __global float *filmConvergence \
		, __global float *filmMaterialIDColor \
		, __global float *filmAlbedo \
		, __global float *filmAvgShadingNormal \
		, __global float *filmNoise \
		, __global float *filmUserImportance \
		KERNEL_ARGS_FILM_DENOISER

//------------------------------------------------------------------------------
// Film_Clear Kernel
//------------------------------------------------------------------------------

__kernel void Film_Clear(
	const int dummy // This dummy variable is required by KERNEL_ARGS_FILM macro
	KERNEL_ARGS_FILM) {
	const size_t gid = get_global_id(0);
	if (gid >= filmWidth * filmHeight)
		return;

	__global float *filmRadianceGroup[FILM_MAX_RADIANCE_GROUP_COUNT];
	filmRadianceGroup[0] = filmRadianceGroup0;
	filmRadianceGroup[1] = filmRadianceGroup1;
	filmRadianceGroup[2] = filmRadianceGroup2;
	filmRadianceGroup[3] = filmRadianceGroup3;
	filmRadianceGroup[4] = filmRadianceGroup4;
	filmRadianceGroup[5] = filmRadianceGroup5;
	filmRadianceGroup[6] = filmRadianceGroup6;
	filmRadianceGroup[7] = filmRadianceGroup7;
	
	for (uint i = 0; i < FILM_MAX_RADIANCE_GROUP_COUNT; ++i) {
		if (filmRadianceGroup[i]) {
			filmRadianceGroup[i][gid * 4] = 0.f;
			filmRadianceGroup[i][gid * 4 + 1] = 0.f;
			filmRadianceGroup[i][gid * 4 + 2] = 0.f;
			filmRadianceGroup[i][gid * 4 + 3] = 0.f;
		}
	}

	if (filmAlpha) {
		filmAlpha[gid * 2] = 0.f;
		filmAlpha[gid * 2 + 1] = 0.f;	
	}

	if (filmDepth)
		filmDepth[gid] = INFINITY;

	if (filmPosition) {
		filmPosition[gid * 3] = INFINITY;
		filmPosition[gid * 3 + 1] = INFINITY;
		filmPosition[gid * 3 + 2] = INFINITY;
	}

	if (filmGeometryNormal) {
		filmGeometryNormal[gid * 3] = 0.f;
		filmGeometryNormal[gid * 3 + 1] = 0.f;
		filmGeometryNormal[gid * 3 + 2] = 0.f;
	}

	if (filmShadingNormal) {
		filmShadingNormal[gid * 3] = 0.f;
		filmShadingNormal[gid * 3 + 1] = 0.f;
		filmShadingNormal[gid * 3 + 2] = 0.f;
	}

	if (filmMaterialID)
		filmMaterialID[gid] = NULL_INDEX;


	if (filmDirectDiffuse) {
		filmDirectDiffuse[gid * 4] = 0.f;
		filmDirectDiffuse[gid * 4 + 1] = 0.f;
		filmDirectDiffuse[gid * 4 + 2] = 0.f;
		filmDirectDiffuse[gid * 4 + 3] = 0.f;
	}
	
	if (filmDirectGlossy) {
		filmDirectGlossy[gid * 4] = 0.f;
		filmDirectGlossy[gid * 4 + 1] = 0.f;
		filmDirectGlossy[gid * 4 + 2] = 0.f;
		filmDirectGlossy[gid * 4 + 3] = 0.f;
	}
	
	if (filmEmission) {
		filmEmission[gid * 4] = 0.f;
		filmEmission[gid * 4 + 1] = 0.f;
		filmEmission[gid * 4 + 2] = 0.f;
		filmEmission[gid * 4 + 3] = 0.f;
	}
	
	if (filmIndirectDiffuse) {
		filmIndirectDiffuse[gid * 4] = 0.f;
		filmIndirectDiffuse[gid * 4 + 1] = 0.f;
		filmIndirectDiffuse[gid * 4 + 2] = 0.f;
		filmIndirectDiffuse[gid * 4 + 3] = 0.f;
	}

	if (filmIndirectGlossy) {
		filmIndirectGlossy[gid * 4] = 0.f;
		filmIndirectGlossy[gid * 4 + 1] = 0.f;
		filmIndirectGlossy[gid * 4 + 2] = 0.f;
		filmIndirectGlossy[gid * 4 + 3] = 0.f;
	}

	if (filmIndirectSpecular) {
		filmIndirectSpecular[gid * 4] = 0.f;
		filmIndirectSpecular[gid * 4 + 1] = 0.f;
		filmIndirectSpecular[gid * 4 + 2] = 0.f;
		filmIndirectSpecular[gid * 4 + 3] = 0.f;
	}

	if (filmMaterialIDMask) {
		filmMaterialIDMask[gid * 2] = 0.f;
		filmMaterialIDMask[gid * 2 + 1] = 0.f;
	}

	if (filmDirectShadowMask) {
		filmDirectShadowMask[gid * 2] = 0.f;
		filmDirectShadowMask[gid * 2 + 1] = 0.f;
	}

	if (filmIndirectShadowMask) {
		filmIndirectShadowMask[gid * 2] = 0.f;
		filmIndirectShadowMask[gid * 2 + 1] = 0.f;
	}

	if (filmUV) {
		filmUV[gid * 2] = INFINITY;
		filmUV[gid * 2 + 1] = INFINITY;
	}

	if (filmRayCount)
		filmRayCount[gid] = 0;

	if (filmByMaterialID) {
		filmByMaterialID[gid * 4] = 0.f;
		filmByMaterialID[gid * 4 + 1] = 0.f;
		filmByMaterialID[gid * 4 + 2] = 0.f;
		filmByMaterialID[gid * 4 + 3] = 0.f;
	}
	
	if (filmIrradiance) {
		filmIrradiance[gid * 4] = 0.f;
		filmIrradiance[gid * 4 + 1] = 0.f;
		filmIrradiance[gid * 4 + 2] = 0.f;
		filmIrradiance[gid * 4 + 3] = 0.f;
	}
	
	if (filmObjectID)
		filmObjectID[gid] = NULL_INDEX;

	if (filmObjectIDMask) {
		filmObjectIDMask[gid * 2] = 0.f;
		filmObjectIDMask[gid * 2 + 1] = 0.f;
	}

	if (filmByObjectID) {
		filmByObjectID[gid * 4] = 0.f;
		filmByObjectID[gid * 4 + 1] = 0.f;
		filmByObjectID[gid * 4 + 2] = 0.f;
		filmByObjectID[gid * 4 + 3] = 0.f;
	}
	
	if (filmSampleCount)
		filmSampleCount[gid] = 0;

	if (filmConvergence)
		filmConvergence[gid] = INFINITY;

	if (filmMaterialIDColor) {
		filmMaterialIDColor[gid * 4] = 0.f;
		filmMaterialIDColor[gid * 4 + 1] = 0.f;
		filmMaterialIDColor[gid * 4 + 2] = 0.f;
		filmMaterialIDColor[gid * 4 + 3] = 0.f;
	}
	
	if (filmAlbedo) {
		filmAlbedo[gid * 4] = 0.f;
		filmAlbedo[gid * 4 + 1] = 0.f;
		filmAlbedo[gid * 4 + 2] = 0.f;
		filmAlbedo[gid * 4 + 3] = 0.f;
	}

	if (filmAvgShadingNormal) {
		filmAvgShadingNormal[gid * 4] = 0.f;
		filmAvgShadingNormal[gid * 4 + 1] = 0.f;
		filmAvgShadingNormal[gid * 4 + 2] = 0.f;
		filmAvgShadingNormal[gid * 4 + 3] = 0.f;
	}

	if (filmNoise)
		filmNoise[gid] = INFINITY;

	if (filmUserImportance)
		filmUserImportance[gid] = 1.f;

	//--------------------------------------------------------------------------
	// Film denoiser buffers
	//--------------------------------------------------------------------------

	if (filmDenoiserNbOfSamplesImage)
		filmDenoiserNbOfSamplesImage[gid] = 0.f;
	if (filmDenoiserSquaredWeightSumsImage)
		filmDenoiserSquaredWeightSumsImage[gid] = 0.f;

	if (filmDenoiserMeanImage) {
		filmDenoiserMeanImage[gid * 3 + 0] = 0.f;
		filmDenoiserMeanImage[gid * 3 + 1] = 0.f;
		filmDenoiserMeanImage[gid * 3 + 2] = 0.f;
	}

	if (filmDenoiserCovarImage) {
		filmDenoiserCovarImage[gid * 6 + 0] = 0.f;
		filmDenoiserCovarImage[gid * 6 + 1] = 0.f;
		filmDenoiserCovarImage[gid * 6 + 2] = 0.f;
		filmDenoiserCovarImage[gid * 6 + 3] = 0.f;
		filmDenoiserCovarImage[gid * 6 + 4] = 0.f;
		filmDenoiserCovarImage[gid * 6 + 5] = 0.f;
	}

	if (filmDenoiserHistoImage) {
		for (uint channelIndex = 0; channelIndex < 3; ++channelIndex)
			for (uint i = 0; i < filmDenoiserNbOfBins; ++i)
				filmDenoiserHistoImage[gid * filmDenoiserNbOfBins * 3 + channelIndex * filmDenoiserNbOfBins + i] = 0.f;
	}
}
