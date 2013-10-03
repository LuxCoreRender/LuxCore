#line 2 "patchoclbase_kernels.cl"

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

// List of symbols defined at compile time:
//  PARAM_TASK_COUNT
//  PARAM_RAY_EPSILON_MIN
//  PARAM_RAY_EPSILON_MAX
//  PARAM_HAS_IMAGEMAPS
//  PARAM_HAS_PASSTHROUGH
//  PARAM_USE_PIXEL_ATOMICS
//  PARAM_HAS_BUMPMAPS
//  PARAM_HAS_NORMALMAPS
//  PARAM_ACCEL_BVH or PARAM_ACCEL_MBVH or PARAM_ACCEL_QBVH or PARAM_ACCEL_MQBVH
//  PARAM_DEVICE_INDEX
//  PARAM_DEVICE_COUNT
//  PARAM_LIGHT_WORLD_RADIUS_SCALE
//  PARAM_TRIANGLE_LIGHT_COUNT
//  PARAM_LIGHT_COUNT

// To enable single material support
//  PARAM_ENABLE_MAT_MATTE
//  PARAM_ENABLE_MAT_MIRROR
//  PARAM_ENABLE_MAT_GLASS
//  PARAM_ENABLE_MAT_METAL
//  PARAM_ENABLE_MAT_ARCHGLASS
//  PARAM_ENABLE_MAT_MIX
//  PARAM_ENABLE_MAT_NULL
//  PARAM_ENABLE_MAT_MATTETRANSLUCENT

// To enable single texture support
//  PARAM_ENABLE_TEX_CONST_FLOAT
//  PARAM_ENABLE_TEX_CONST_FLOAT3
//  PARAM_ENABLE_TEX_CONST_FLOAT4
//  PARAM_ENABLE_TEX_IMAGEMAP
//  PARAM_ENABLE_TEX_SCALE

// Film related parameters:
//  PARAM_FILM_RADIANCE_GROUP_COUNT
//  PARAM_FILM_CHANNELS_HAS_ALPHA
//  PARAM_FILM_CHANNELS_HAS_DEPTH
//  PARAM_FILM_CHANNELS_HAS_POSITION
//  PARAM_FILM_CHANNELS_HAS_GEOMETRY_NORMAL
//  PARAM_FILM_CHANNELS_HAS_SHADING_NORMAL
//  PARAM_FILM_CHANNELS_HAS_MATERIAL_ID
//  PARAM_FILM_CHANNELS_HAS_MATERIAL_ID
//  PARAM_FILM_CHANNELS_HAS_DIRECT_DIFFUSE
//  PARAM_FILM_CHANNELS_HAS_DIRECT_GLOSSY
//  PARAM_FILM_CHANNELS_HAS_EMISSION
//  PARAM_FILM_CHANNELS_HAS_INDIRECT_DIFFUSE
//  PARAM_FILM_CHANNELS_HAS_INDIRECT_GLOSSY
//  PARAM_FILM_CHANNELS_HAS_INDIRECT_SPECULAR
//  PARAM_FILM_CHANNELS_HAS_MATERIAL_ID_MASK (and PARAM_FILM_MASK_MATERIAL_ID)
//  PARAM_FILM_CHANNELS_HAS_DIRECT_SHADOW_MASK
//  PARAM_FILM_CHANNELS_HAS_INDIRECT_SHADOW_MASK
//  PARAM_FILM_CHANNELS_HAS_UV
//  PARAM_FILM_CHANNELS_HAS_RAYCOUNT

// (optional)
//  PARAM_CAMERA_HAS_DOF

// (optional)
//  PARAM_HAS_INFINITELIGHT

// (optional)
//  PARAM_HAS_SUNLIGHT

// (optional)
//  PARAM_HAS_SKYLIGHT

// (optional)
//  PARAM_HAS_NORMALS_BUFFER
//  PARAM_HAS_UVS_BUFFER
//  PARAM_HAS_COLS_BUFFER
//  PARAM_HAS_ALPHAS_BUFFER

void AddEmission(const bool firstPathVertex, const BSDFEvent pathBSDFEvent,
		__global SampleResult *sampleResult, const float3 emission) {
	if (firstPathVertex) {
#if defined(PARAM_FILM_CHANNELS_HAS_EMISSION)
		VADD3F(&sampleResult->emission.r, emission);
#endif
	} else {
#if defined(PARAM_FILM_CHANNELS_HAS_INDIRECT_SHADOW_MASK)
		sampleResult->indirectShadowMask = 0.f;
#endif
		if (pathBSDFEvent & DIFFUSE) {
#if defined(PARAM_FILM_CHANNELS_HAS_INDIRECT_DIFFUSE)
			VADD3F(&sampleResult->indirectDiffuse.r, emission);
#endif
		} else if (pathBSDFEvent & GLOSSY) {
#if defined(PARAM_FILM_CHANNELS_HAS_INDIRECT_GLOSSY)
			VADD3F(&sampleResult->indirectGlossy.r, emission);
#endif
		} else if (pathBSDFEvent & SPECULAR) {
#if defined(PARAM_FILM_CHANNELS_HAS_INDIRECT_SPECULAR)
			VADD3F(&sampleResult->indirectSpecular.r, emission);
#endif
		}
	}
}

#if defined(PARAM_HAS_SKYLIGHT) || defined(PARAM_HAS_INFINITELIGHT) || defined(PARAM_HAS_SUNLIGHT)
void DirectHitInfiniteLight(
		const bool firstPathVertex,
		const BSDFEvent lastBSDFEvent,
		const BSDFEvent pathBSDFEvent,
		__global float *lightsDistribution,
#if defined(PARAM_HAS_INFINITELIGHT)
		__global InfiniteLight *infiniteLight,
		__global float *infiniteLightDistribution,
#endif
#if defined(PARAM_HAS_SUNLIGHT)
		__global SunLight *sunLight,
#endif
#if defined(PARAM_HAS_SKYLIGHT)
		__global SkyLight *skyLight,
#endif
		__global const Spectrum *pathThroughput,
		const float3 eyeDir, const float lastPdfW,
		__global SampleResult *sampleResult
		IMAGEMAPS_PARAM_DECL) {
	const float3 throughput = VLOAD3F(&pathThroughput->r);

#if defined(PARAM_HAS_INFINITELIGHT)
	{
		float directPdfW;
		const float3 infiniteLightRadiance = InfiniteLight_GetRadiance(infiniteLight,
				infiniteLightDistribution, eyeDir, &directPdfW
				IMAGEMAPS_PARAM);
		if (!Spectrum_IsBlack(infiniteLightRadiance)) {
			// MIS between BSDF sampling and direct light sampling
			const float lightPickProb = Scene_SampleAllLightPdf(lightsDistribution, infiniteLight->lightSceneIndex);
			const float weight = ((lastBSDFEvent & SPECULAR) ? 1.f : PowerHeuristic(lastPdfW, directPdfW * lightPickProb));
			const float3 lightRadiance = weight * throughput * infiniteLightRadiance;

			const uint lightID = min(infiniteLight->lightID, PARAM_FILM_RADIANCE_GROUP_COUNT - 1u);
			VADD3F(&sampleResult->radiancePerPixelNormalized[lightID].r, lightRadiance);

			AddEmission(firstPathVertex, pathBSDFEvent, sampleResult, lightRadiance);
		}
	}
#endif
#if defined(PARAM_HAS_SKYLIGHT)
	{
		float directPdfW;
		const float3 skyRadiance = SkyLight_GetRadiance(skyLight, eyeDir, &directPdfW);
		if (!Spectrum_IsBlack(skyRadiance)) {
			// MIS between BSDF sampling and direct light sampling
			const float lightPickProb = Scene_SampleAllLightPdf(lightsDistribution, skyLight->lightSceneIndex);
			const float weight = ((lastBSDFEvent & SPECULAR) ? 1.f : PowerHeuristic(lastPdfW, directPdfW * lightPickProb));
			const float3 lightRadiance = weight * throughput * skyRadiance;

			const uint lightID = min(skyLight->lightID, PARAM_FILM_RADIANCE_GROUP_COUNT - 1u);
			VADD3F(&sampleResult->radiancePerPixelNormalized[lightID].r, lightRadiance);

			AddEmission(firstPathVertex, pathBSDFEvent, sampleResult, lightRadiance);
		}
	}
#endif
#if defined(PARAM_HAS_SUNLIGHT)
	{
		float directPdfW;
		const float3 sunRadiance = SunLight_GetRadiance(sunLight, eyeDir, &directPdfW);
		if (!Spectrum_IsBlack(sunRadiance)) {
			// MIS between BSDF sampling and direct light sampling
			const float lightPickProb = Scene_SampleAllLightPdf(lightsDistribution, sunLight->lightSceneIndex);
			const float weight = ((lastBSDFEvent & SPECULAR) ? 1.f : PowerHeuristic(lastPdfW, directPdfW * lightPickProb));
			const float3 lightRadiance = weight * throughput * sunRadiance;

			const uint lightID = min(sunLight->lightID, PARAM_FILM_RADIANCE_GROUP_COUNT - 1u);
			VADD3F(&sampleResult->radiancePerPixelNormalized[lightID].r, lightRadiance);

			AddEmission(firstPathVertex, pathBSDFEvent, sampleResult, lightRadiance);
		}
	}
#endif
}
#endif

#if (PARAM_TRIANGLE_LIGHT_COUNT > 0)
void DirectHitFiniteLight(
		const bool firstPathVertex,
		const BSDFEvent lastBSDFEvent,
		const BSDFEvent pathBSDFEvent,
		__global float *lightsDistribution,
		__global TriangleLight *triLightDefs,
		__global const Spectrum *pathThroughput, const float distance, __global BSDF *bsdf,
		const float lastPdfW, __global SampleResult *sampleResult
		MATERIALS_PARAM_DECL) {
	float directPdfA;
	const float3 emittedRadiance = BSDF_GetEmittedRadiance(bsdf,
			triLightDefs, &directPdfA
			MATERIALS_PARAM);

	if (!Spectrum_IsBlack(emittedRadiance)) {
		// Add emitted radiance
		float weight = 1.f;
		if (!(lastBSDFEvent & SPECULAR)) {
			const float lightPickProb = Scene_SampleAllLightPdf(lightsDistribution,
					triLightDefs[bsdf->triangleLightSourceIndex].lightSceneIndex);
			const float directPdfW = PdfAtoW(directPdfA, distance,
				fabs(dot(VLOAD3F(&bsdf->hitPoint.fixedDir.x), VLOAD3F(&bsdf->hitPoint.shadeN.x))));

			// MIS between BSDF sampling and direct light sampling
			weight = PowerHeuristic(lastPdfW, directPdfW * lightPickProb);
		}
		const float3 lightRadiance = weight * VLOAD3F(&pathThroughput->r) * emittedRadiance;

		const uint lightID =  min(BSDF_GetLightID(bsdf
				MATERIALS_PARAM), PARAM_FILM_RADIANCE_GROUP_COUNT - 1u);
		VADD3F(&sampleResult->radiancePerPixelNormalized[lightID].r, lightRadiance);

		AddEmission(firstPathVertex, pathBSDFEvent, sampleResult, lightRadiance);
	}
}
#endif

#if !defined(DIRECTLIGHTSAMPLING_ONE_PARAM_DISABLE_RR)
float RussianRouletteProb(const float3 color) {
	return clamp(Spectrum_Filter(color), PARAM_RR_CAP, 1.f);
}
#endif

bool DirectLightSampling(
		const uint lightIndex,
		const float lightPickPdf,
#if defined(PARAM_HAS_INFINITELIGHT) || defined(PARAM_HAS_SKYLIGHT)
		const float worldCenterX,
		const float worldCenterY,
		const float worldCenterZ,
		const float worldRadius,
#endif
#if defined(PARAM_HAS_INFINITELIGHT)
		__global InfiniteLight *infiniteLight,
		__global float *infiniteLightDistribution,
#endif
#if defined(PARAM_HAS_SUNLIGHT)
		__global SunLight *sunLight,
#endif
#if defined(PARAM_HAS_SKYLIGHT)
		__global SkyLight *skyLight,
#endif
#if (PARAM_TRIANGLE_LIGHT_COUNT > 0)
		__global TriangleLight *triLightDefs,
		__global HitPoint *tmpHitPoint,
#endif
		__global float *lightsDistribution,
#if defined(PARAM_HAS_PASSTHROUGH)
		const float u3,
		__global float *shadowPassThrought,
#endif
		const float u0, const float u1, const float u2,
#if !defined(DIRECTLIGHTSAMPLING_ONE_PARAM_DISABLE_RR)
		const uint depth,
#endif
		__global const Spectrum *pathThroughput, __global BSDF *bsdf,
#if !defined(DIRECTLIGHTSAMPLING_PARAM_MEM_SPACE_PRIVATE)
		__global
#endif
		Ray *shadowRay, __global float *radiance, __global uint *ID
		MATERIALS_PARAM_DECL) {
	float3 lightRayDir;
	float distance, directPdfW;
	float3 lightRadiance;
	uint lightID;

#if defined(PARAM_HAS_INFINITELIGHT)
	const uint infiniteLightIndex = PARAM_TRIANGLE_LIGHT_COUNT
#if defined(PARAM_HAS_SUNLIGHT)
		+ 1
#endif
	;

	if (lightIndex == infiniteLightIndex) {
		lightRadiance = InfiniteLight_Illuminate(
			infiniteLight,
			infiniteLightDistribution,
			worldCenterX, worldCenterY, worldCenterZ, worldRadius,
			u0, u1,
			VLOAD3F(&bsdf->hitPoint.p.x),
			&lightRayDir, &distance, &directPdfW
			IMAGEMAPS_PARAM);
		lightID = infiniteLight->lightID;
	}
#endif

#if defined(PARAM_HAS_SKYLIGHT)
	const uint skyLightIndex = PARAM_TRIANGLE_LIGHT_COUNT
#if defined(PARAM_HAS_SUNLIGHT)
		+ 1
#endif
	;

	if (lightIndex == skyLightIndex) {
		lightRadiance = SkyLight_Illuminate(
			skyLight,
			worldCenterX, worldCenterY, worldCenterZ, worldRadius,
			u0, u1,
			VLOAD3F(&bsdf->hitPoint.p.x),
			&lightRayDir, &distance, &directPdfW);
		lightID = skyLight->lightID;
	}
#endif

#if defined(PARAM_HAS_SUNLIGHT)
	const uint sunLightIndex = PARAM_TRIANGLE_LIGHT_COUNT;
	if (lightIndex == sunLightIndex) {
		lightRadiance = SunLight_Illuminate(
			sunLight,
			u0, u1,
			&lightRayDir, &distance, &directPdfW);
		lightID = sunLight->lightID;
	}
#endif

#if (PARAM_TRIANGLE_LIGHT_COUNT > 0)
	if (lightIndex < PARAM_TRIANGLE_LIGHT_COUNT) {
		lightRadiance = TriangleLight_Illuminate(
			&triLightDefs[lightIndex], tmpHitPoint,
			VLOAD3F(&bsdf->hitPoint.p.x),
			u0, u1, u2,
			&lightRayDir, &distance, &directPdfW
			MATERIALS_PARAM);
		lightID = mats[triLightDefs[lightIndex].materialIndex].lightID;
	}
#endif

	// Setup the shadow ray
	if (!Spectrum_IsBlack(lightRadiance)) {
		BSDFEvent event;
		float bsdfPdfW;
		const float3 bsdfEval = BSDF_Evaluate(bsdf,
				lightRayDir, &event, &bsdfPdfW
				MATERIALS_PARAM);

		if (!Spectrum_IsBlack(bsdfEval)) {
			const float cosThetaToLight = fabs(dot(lightRayDir, VLOAD3F(&bsdf->hitPoint.shadeN.x)));
			const float directLightSamplingPdfW = directPdfW * lightPickPdf;
			const float factor = cosThetaToLight / directLightSamplingPdfW;

#if !defined(DIRECTLIGHTSAMPLING_ONE_PARAM_DISABLE_RR)
			// Russian Roulette
			bsdfPdfW *= (depth >= PARAM_RR_DEPTH) ? RussianRouletteProb(bsdfEval) : 1.f;
#endif

			// MIS between direct light sampling and BSDF sampling
			const float weight = PowerHeuristic(directLightSamplingPdfW, bsdfPdfW);

			VSTORE3F((weight * factor) * VLOAD3F(&pathThroughput->r) * bsdfEval * lightRadiance, radiance);
			*ID = min(lightID, PARAM_FILM_RADIANCE_GROUP_COUNT - 1u);
#if defined(PARAM_HAS_PASSTHROUGH)
			*shadowPassThrought = u3;
#endif

			// Setup the shadow ray
			const float3 hitPoint = VLOAD3F(&bsdf->hitPoint.p.x);
			const float epsilon = fmax(MachineEpsilon_E_Float3(hitPoint), MachineEpsilon_E(distance));
#if !defined(DIRECTLIGHTSAMPLING_PARAM_MEM_SPACE_PRIVATE)
			Ray_Init4(shadowRay, hitPoint, lightRayDir,
				epsilon,
				distance - epsilon);
#else
			Ray_Init4_Private(shadowRay, hitPoint, lightRayDir,
				epsilon,
				distance - epsilon);
#endif

			return true;
		}
	}

	return false;
}

bool DirectLightSampling_ONE(
#if defined(PARAM_HAS_INFINITELIGHT) || defined(PARAM_HAS_SKYLIGHT)
		const float worldCenterX,
		const float worldCenterY,
		const float worldCenterZ,
		const float worldRadius,
#endif
#if defined(PARAM_HAS_INFINITELIGHT)
		__global InfiniteLight *infiniteLight,
		__global float *infiniteLightDistribution,
#endif
#if defined(PARAM_HAS_SUNLIGHT)
		__global SunLight *sunLight,
#endif
#if defined(PARAM_HAS_SKYLIGHT)
		__global SkyLight *skyLight,
#endif
#if (PARAM_TRIANGLE_LIGHT_COUNT > 0)
		__global TriangleLight *triLightDefs,
		__global HitPoint *tmpHitPoint,
#endif
		__global float *lightsDistribution,
#if defined(PARAM_HAS_PASSTHROUGH)
		const float u4,
		__global float *shadowPassThrought,
#endif
		const float u0, const float u1, const float u2, const float u3,
#if !defined(DIRECTLIGHTSAMPLING_ONE_PARAM_DISABLE_RR)
		const uint depth,
#endif
		__global const Spectrum *pathThroughput, __global BSDF *bsdf,
#if !defined(DIRECTLIGHTSAMPLING_PARAM_MEM_SPACE_PRIVATE)
		__global
#endif
		Ray *shadowRay, __global float *radiance, __global uint *ID
		MATERIALS_PARAM_DECL) {
	// Pick a light source to sample
	float lightPickPdf;
	const uint lightIndex = Scene_SampleAllLights(lightsDistribution, u0, &lightPickPdf);

	return DirectLightSampling(
		lightIndex,
		lightPickPdf,
#if defined(PARAM_HAS_INFINITELIGHT) || defined(PARAM_HAS_SKYLIGHT)
		worldCenterX,
		worldCenterY,
		worldCenterZ,
		worldRadius,
#endif
#if defined(PARAM_HAS_INFINITELIGHT)
		infiniteLight,
		infiniteLightDistribution,
#endif
#if defined(PARAM_HAS_SUNLIGHT)
		sunLight,
#endif
#if defined(PARAM_HAS_SKYLIGHT)
		skyLight,
#endif
#if (PARAM_TRIANGLE_LIGHT_COUNT > 0)
		triLightDefs,
		tmpHitPoint,
#endif
		lightsDistribution,
#if defined(PARAM_HAS_PASSTHROUGH)
		u4,
		shadowPassThrought,
#endif
		u1, u2, u3,
#if !defined(DIRECTLIGHTSAMPLING_ONE_PARAM_DISABLE_RR)
		depth,
#endif
		pathThroughput, bsdf,
		shadowRay, radiance, ID
		MATERIALS_PARAM);
}

bool DirectLightSampling_ALL(
		__global uint *currentLightIndex,
#if defined(PARAM_HAS_INFINITELIGHT) || defined(PARAM_HAS_SKYLIGHT)
		const float worldCenterX,
		const float worldCenterY,
		const float worldCenterZ,
		const float worldRadius,
#endif
#if defined(PARAM_HAS_INFINITELIGHT)
		__global InfiniteLight *infiniteLight,
		__global float *infiniteLightDistribution,
#endif
#if defined(PARAM_HAS_SUNLIGHT)
		__global SunLight *sunLight,
#endif
#if defined(PARAM_HAS_SKYLIGHT)
		__global SkyLight *skyLight,
#endif
#if (PARAM_TRIANGLE_LIGHT_COUNT > 0)
		__global TriangleLight *triLightDefs,
		__global HitPoint *tmpHitPoint,
#endif
		__global float *lightsDistribution,
#if defined(PARAM_HAS_PASSTHROUGH)
		const float u4,
		__global float *shadowPassThrought,
#endif
		const float u0, const float u1, const float u2, const float u3,
#if !defined(DIRECTLIGHTSAMPLING_ONE_PARAM_DISABLE_RR)
		const uint depth,
#endif
		__global const Spectrum *pathThroughput, __global BSDF *bsdf,
#if !defined(DIRECTLIGHTSAMPLING_PARAM_MEM_SPACE_PRIVATE)
		__global
#endif
		Ray *shadowRay, __global float *radiance, __global uint *ID
		MATERIALS_PARAM_DECL) {
	for (; *currentLightIndex < PARAM_LIGHT_COUNT; ++(*currentLightIndex)) {
		if (DirectLightSampling(
			*currentLightIndex,
			1.f,
	#if defined(PARAM_HAS_INFINITELIGHT) || defined(PARAM_HAS_SKYLIGHT)
			worldCenterX,
			worldCenterY,
			worldCenterZ,
			worldRadius,
	#endif
	#if defined(PARAM_HAS_INFINITELIGHT)
			infiniteLight,
			infiniteLightDistribution,
	#endif
	#if defined(PARAM_HAS_SUNLIGHT)
			sunLight,
	#endif
	#if defined(PARAM_HAS_SKYLIGHT)
			skyLight,
	#endif
	#if (PARAM_TRIANGLE_LIGHT_COUNT > 0)
			triLightDefs,
			tmpHitPoint,
	#endif
			lightsDistribution,
	#if defined(PARAM_HAS_PASSTHROUGH)
			u4,
			shadowPassThrought,
	#endif
			u1, u2, u3,
	#if !defined(DIRECTLIGHTSAMPLING_ONE_PARAM_DISABLE_RR)
			depth,
	#endif
			pathThroughput, bsdf,
			shadowRay, radiance, ID
			MATERIALS_PARAM)) {
			return true;
		}
	}

	return false;
}
