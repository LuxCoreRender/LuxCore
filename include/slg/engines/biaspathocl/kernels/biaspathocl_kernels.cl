#line 2 "biaspatchocl_kernels.cl"

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
//  PARAM_TILE_SIZE
//  PARAM_TILE_PROGRESSIVE_REFINEMENT
//  PARAM_DIRECT_LIGHT_ONE_STRATEGY or PARAM_DIRECT_LIGHT_ALL_STRATEGY
//  PARAM_RADIANCE_CLAMP_MAXVALUE
//  PARAM_AA_SAMPLES
//  PARAM_DIRECT_LIGHT_SAMPLES
//  PARAM_DIFFUSE_SAMPLES
//  PARAM_GLOSSY_SAMPLES
//  PARAM_SPECULAR_SAMPLES
//  PARAM_DEPTH_MAX
//  PARAM_DEPTH_DIFFUSE_MAX
//  PARAM_DEPTH_GLOSSY_MAX
//  PARAM_DEPTH_SPECULAR_MAX
//  PARAM_IMAGE_FILTER_WIDTH
//  PARAM_LOW_LIGHT_THREASHOLD
//  PARAM_NEAR_START_LIGHT

//------------------------------------------------------------------------------
// InitSeed Kernel
//------------------------------------------------------------------------------

__kernel __attribute__((work_group_size_hint(64, 1, 1))) void InitSeed(
		uint seedBase,
		__global GPUTask *tasks) {
	//if (get_global_id(0) == 0)
	//	printf("sizeof(BSDF) = %dbytes\n", sizeof(BSDF));
	//if (get_global_id(0) == 0)
	//	printf("sizeof(HitPoint) = %dbytes\n", sizeof(HitPoint));
	//if (get_global_id(0) == 0)
	//	printf("sizeof(GPUTask) = %dbytes\n", sizeof(GPUTask));
	//if (get_global_id(0) == 0)
	//	printf("sizeof(SampleResult) = %dbytes\n", sizeof(SampleResult));

	const size_t gid = get_global_id(0);
	if (gid >= PARAM_TASK_COUNT)
		return;

	// Initialize the task
	__global GPUTask *task = &tasks[gid];

	// For testing/debugging
	//MangleMemory((__global unsigned char *)task, sizeof(GPUTask));

	// Initialize random number generator
	Seed seed;
	Rnd_Init(seedBase + gid, &seed);

	// Save the seed
	task->seed.s1 = seed.s1;
	task->seed.s2 = seed.s2;
	task->seed.s3 = seed.s3;
}

//------------------------------------------------------------------------------
// InitStats Kernel
//------------------------------------------------------------------------------

__kernel __attribute__((work_group_size_hint(64, 1, 1))) void InitStat(
		__global GPUTaskStats *taskStats) {
	const size_t gid = get_global_id(0);
	if (gid >= PARAM_TASK_COUNT)
		return;

	__global GPUTaskStats *taskStat = &taskStats[gid];
	taskStat->raysCount = 0;
}

//------------------------------------------------------------------------------
// BiasAdvancePaths Kernel
//------------------------------------------------------------------------------

typedef struct {
	uint depth, diffuseDepth, glossyDepth, specularDepth;
} PathDepthInfo;

void PathDepthInfo_Init(PathDepthInfo *depthInfo, const uint val) {
	depthInfo->depth = val;
	depthInfo->diffuseDepth = val;
	depthInfo->glossyDepth = val;
	depthInfo->specularDepth = val;
}

void PathDepthInfo_IncDepths(PathDepthInfo *depthInfo, const BSDFEvent event) {
	++(depthInfo->depth);
	if (event & DIFFUSE)
		++(depthInfo->diffuseDepth);
	if (event & GLOSSY)
		++(depthInfo->glossyDepth);
	if (event & SPECULAR)
		++(depthInfo->specularDepth);
}

bool PathDepthInfo_CheckDepths(PathDepthInfo *depthInfo) {
	return ((depthInfo->depth <= PARAM_DEPTH_MAX) &&
			(depthInfo->diffuseDepth <= PARAM_DEPTH_DIFFUSE_MAX) &&
			(depthInfo->glossyDepth <= PARAM_DEPTH_GLOSSY_MAX) &&
			(depthInfo->specularDepth <= PARAM_DEPTH_SPECULAR_MAX));
}

void SR_RadianceClamp(__global SampleResult *sampleResult) {
	// Initialize only Spectrum fields

#if defined(PARAM_FILM_RADIANCE_GROUP_0)
	sampleResult->radiancePerPixelNormalized[0].r = clamp(sampleResult->radiancePerPixelNormalized[0].r, 0.f, PARAM_RADIANCE_CLAMP_MAXVALUE);
	sampleResult->radiancePerPixelNormalized[0].g = clamp(sampleResult->radiancePerPixelNormalized[0].g, 0.f, PARAM_RADIANCE_CLAMP_MAXVALUE);
	sampleResult->radiancePerPixelNormalized[0].b = clamp(sampleResult->radiancePerPixelNormalized[0].b, 0.f, PARAM_RADIANCE_CLAMP_MAXVALUE);
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_1)
	sampleResult->radiancePerPixelNormalized[1].r = clamp(sampleResult->radiancePerPixelNormalized[1].r, 0.f, PARAM_RADIANCE_CLAMP_MAXVALUE);
	sampleResult->radiancePerPixelNormalized[1].g = clamp(sampleResult->radiancePerPixelNormalized[1].g, 0.f, PARAM_RADIANCE_CLAMP_MAXVALUE);
	sampleResult->radiancePerPixelNormalized[1].b = clamp(sampleResult->radiancePerPixelNormalized[1].b, 0.f, PARAM_RADIANCE_CLAMP_MAXVALUE);
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_2)
	sampleResult->radiancePerPixelNormalized[2].r = clamp(sampleResult->radiancePerPixelNormalized[2].r, 0.f, PARAM_RADIANCE_CLAMP_MAXVALUE);
	sampleResult->radiancePerPixelNormalized[2].g = clamp(sampleResult->radiancePerPixelNormalized[2].g, 0.f, PARAM_RADIANCE_CLAMP_MAXVALUE);
	sampleResult->radiancePerPixelNormalized[2].b = clamp(sampleResult->radiancePerPixelNormalized[2].b, 0.f, PARAM_RADIANCE_CLAMP_MAXVALUE);
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_3)
	sampleResult->radiancePerPixelNormalized[3].r = clamp(sampleResult->radiancePerPixelNormalized[3].r, 0.f, PARAM_RADIANCE_CLAMP_MAXVALUE);
	sampleResult->radiancePerPixelNormalized[3].g = clamp(sampleResult->radiancePerPixelNormalized[3].g, 0.f, PARAM_RADIANCE_CLAMP_MAXVALUE);
	sampleResult->radiancePerPixelNormalized[3].b = clamp(sampleResult->radiancePerPixelNormalized[3].b, 0.f, PARAM_RADIANCE_CLAMP_MAXVALUE);
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_4)
	sampleResult->radiancePerPixelNormalized[4].r = clamp(sampleResult->radiancePerPixelNormalized[4].r, 0.f, PARAM_RADIANCE_CLAMP_MAXVALUE);
	sampleResult->radiancePerPixelNormalized[4].g = clamp(sampleResult->radiancePerPixelNormalized[4].g, 0.f, PARAM_RADIANCE_CLAMP_MAXVALUE);
	sampleResult->radiancePerPixelNormalized[4].b = clamp(sampleResult->radiancePerPixelNormalized[4].b, 0.f, PARAM_RADIANCE_CLAMP_MAXVALUE);
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_5)
	sampleResult->radiancePerPixelNormalized[5].r = clamp(sampleResult->radiancePerPixelNormalized[5].r, 0.f, PARAM_RADIANCE_CLAMP_MAXVALUE);
	sampleResult->radiancePerPixelNormalized[5].g = clamp(sampleResult->radiancePerPixelNormalized[5].g, 0.f, PARAM_RADIANCE_CLAMP_MAXVALUE);
	sampleResult->radiancePerPixelNormalized[5].b = clamp(sampleResult->radiancePerPixelNormalized[5].b, 0.f, PARAM_RADIANCE_CLAMP_MAXVALUE);
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_6)
	sampleResult->radiancePerPixelNormalized[6].r = clamp(sampleResult->radiancePerPixelNormalized[6].r, 0.f, PARAM_RADIANCE_CLAMP_MAXVALUE);
	sampleResult->radiancePerPixelNormalized[6].g = clamp(sampleResult->radiancePerPixelNormalized[6].g, 0.f, PARAM_RADIANCE_CLAMP_MAXVALUE);
	sampleResult->radiancePerPixelNormalized[6].b = clamp(sampleResult->radiancePerPixelNormalized[6].b, 0.f, PARAM_RADIANCE_CLAMP_MAXVALUE);
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_7)
	sampleResult->radiancePerPixelNormalized[7].r = clamp(sampleResult->radiancePerPixelNormalized[7].r, 0.f, PARAM_RADIANCE_CLAMP_MAXVALUE);
	sampleResult->radiancePerPixelNormalized[7].g = clamp(sampleResult->radiancePerPixelNormalized[7].g, 0.f, PARAM_RADIANCE_CLAMP_MAXVALUE);
	sampleResult->radiancePerPixelNormalized[7].b = clamp(sampleResult->radiancePerPixelNormalized[7].b, 0.f, PARAM_RADIANCE_CLAMP_MAXVALUE);
#endif
}

void SampleGrid(Seed *seed, const uint size,
		const uint ix, const uint iy, float *u0, float *u1) {
	*u0 = Rnd_FloatValue(seed);
	*u1 = Rnd_FloatValue(seed);

	if (size > 1) {
		const float idim = 1.f / size;
		*u0 = (ix + *u0) * idim;
		*u1 = (iy + *u1) * idim;
	}
}

//------------------------------------------------------------------------------

void GenerateCameraRay(
		Seed *seed,
		__global GPUTask *task,
		__global SampleResult *sampleResult,
		__global Camera *camera,
		__global float *pixelFilterDistribution,
		const uint sampleX, const uint sampleY, const int sampleIndex,
		const uint tileStartX, const uint tileStartY, 
		const uint engineFilmWidth, const uint engineFilmHeight,
		Ray *ray) {
	//if (get_global_id(0) == 0)
	//	printf("tileSampleIndex: %d (%d, %d)\n", sampleIndex, sampleIndex % PARAM_AA_SAMPLES, sampleIndex / PARAM_AA_SAMPLES);

	float u0, u1;
	SampleGrid(seed, PARAM_AA_SAMPLES,
			sampleIndex % PARAM_AA_SAMPLES, sampleIndex / PARAM_AA_SAMPLES,
			&u0, &u1);

	float2 xy;
	float distPdf;
	Distribution2D_SampleContinuous(pixelFilterDistribution, u0, u1, &xy, &distPdf);

	const float filmX = sampleX + .5f + (xy.x - .5f) * PARAM_IMAGE_FILTER_WIDTH_X;
	const float filmY = sampleY + .5f + (xy.y - .5f) * PARAM_IMAGE_FILTER_WIDTH_Y;
	sampleResult->filmX = filmX;
	sampleResult->filmY = filmY;

#if defined(PARAM_CAMERA_HAS_DOF)
	const float dofSampleX = Rnd_FloatValue(seed);
	const float dofSampleY = Rnd_FloatValue(seed);
#endif

	Camera_GenerateRay(camera, engineFilmWidth, engineFilmHeight, ray, tileStartX + filmX, tileStartY + filmY
#if defined(PARAM_CAMERA_HAS_DOF)
			, dofSampleX, dofSampleY
#endif
			);	
}

#if defined(PARAM_HAS_SKYLIGHT) || defined(PARAM_HAS_INFINITELIGHT) || defined(PARAM_HAS_SUNLIGHT)
void DirectHitInfiniteLight(
		const bool firstPathVertex,
		const BSDFEvent lastBSDFEvent,
		const BSDFEvent pathBSDFEvent,
		__global BSDFEvent *lightVisibility,
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
		const uint infiniteLightIndex = PARAM_TRIANGLE_LIGHT_COUNT
#if defined(PARAM_HAS_SUNLIGHT)
			+ 1
#endif
		;

		if (firstPathVertex || (lightVisibility[infiniteLightIndex] & (pathBSDFEvent & (DIFFUSE | GLOSSY | SPECULAR)))) {
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
				AddEmission(firstPathVertex, pathBSDFEvent, lightID, sampleResult, lightRadiance);
			}
		}
	}
#endif
#if defined(PARAM_HAS_SKYLIGHT)
	{
		const uint skyLightIndex = PARAM_TRIANGLE_LIGHT_COUNT
#if defined(PARAM_HAS_SUNLIGHT)
			+ 1
#endif
		;

		if (firstPathVertex || (lightVisibility[skyLightIndex] & (pathBSDFEvent & (DIFFUSE | GLOSSY | SPECULAR)))) {
			float directPdfW;
			const float3 skyRadiance = SkyLight_GetRadiance(skyLight, eyeDir, &directPdfW);
			if (!Spectrum_IsBlack(skyRadiance)) {
				// MIS between BSDF sampling and direct light sampling
				const float lightPickProb = Scene_SampleAllLightPdf(lightsDistribution, skyLight->lightSceneIndex);
				const float weight = ((lastBSDFEvent & SPECULAR) ? 1.f : PowerHeuristic(lastPdfW, directPdfW * lightPickProb));
				const float3 lightRadiance = weight * throughput * skyRadiance;

				const uint lightID = min(skyLight->lightID, PARAM_FILM_RADIANCE_GROUP_COUNT - 1u);
				AddEmission(firstPathVertex, pathBSDFEvent, lightID, sampleResult, lightRadiance);
			}
		}
	}
#endif
#if defined(PARAM_HAS_SUNLIGHT)
	{
		const uint sunLightIndex = PARAM_TRIANGLE_LIGHT_COUNT;

		if (firstPathVertex || (lightVisibility[sunLightIndex] & (pathBSDFEvent & (DIFFUSE | GLOSSY | SPECULAR)))) {
			float directPdfW;
			const float3 sunRadiance = SunLight_GetRadiance(sunLight, eyeDir, &directPdfW);
			if (!Spectrum_IsBlack(sunRadiance)) {
				// MIS between BSDF sampling and direct light sampling
				const float lightPickProb = Scene_SampleAllLightPdf(lightsDistribution, sunLight->lightSceneIndex);
				const float weight = ((lastBSDFEvent & SPECULAR) ? 1.f : PowerHeuristic(lastPdfW, directPdfW * lightPickProb));
				const float3 lightRadiance = weight * throughput * sunRadiance;

				const uint lightID = min(sunLight->lightID, PARAM_FILM_RADIANCE_GROUP_COUNT - 1u);
				AddEmission(firstPathVertex, pathBSDFEvent, lightID, sampleResult, lightRadiance);
			}
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
		__global BSDFEvent *lightVisibility,
		__global float *lightsDistribution,
		__global TriangleLight *triLightDefs,
		__global const Spectrum *pathThroughput, const float distance, __global BSDF *bsdf,
		const float lastPdfW, __global SampleResult *sampleResult
		MATERIALS_PARAM_DECL) {
	if (firstPathVertex || (lightVisibility[bsdf->triangleLightSourceIndex] & (pathBSDFEvent & (DIFFUSE | GLOSSY | SPECULAR)))) {
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
			AddEmission(firstPathVertex, pathBSDFEvent, lightID, sampleResult, lightRadiance);
		}
	}
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
		__global HitPoint *directLightHitPoint,
#endif
		__global float *lightsDistribution,
		const float u0, const float u1, const float u2,
		__global const Spectrum *pathThroughput, __global BSDF *bsdf,
		Ray *shadowRay, __global Spectrum *radiance, __global uint *ID
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
			&triLightDefs[lightIndex], directLightHitPoint,
			VLOAD3F(&bsdf->hitPoint.p.x),
			u0, u1, u2,
			&lightRayDir, &distance, &directPdfW
			MATERIALS_PARAM);
		lightID = mats[triLightDefs[lightIndex].materialIndex].lightID;
	}
#endif

	// Setup the shadow ray
	const float cosThetaToLight = fabs(dot(lightRayDir, VLOAD3F(&bsdf->hitPoint.shadeN.x)));
	if (((Spectrum_Y(lightRadiance) * cosThetaToLight / directPdfW) > PARAM_LOW_LIGHT_THREASHOLD) &&
			(distance > PARAM_NEAR_START_LIGHT)) {
		BSDFEvent event;
		float bsdfPdfW;
		const float3 bsdfEval = BSDF_Evaluate(bsdf,
				lightRayDir, &event, &bsdfPdfW
				MATERIALS_PARAM);

		if (!Spectrum_IsBlack(bsdfEval)) {
			const float directLightSamplingPdfW = directPdfW * lightPickPdf;
			const float factor = cosThetaToLight / directLightSamplingPdfW;

			// MIS between direct light sampling and BSDF sampling
			const float weight = PowerHeuristic(directLightSamplingPdfW, bsdfPdfW);

			VSTORE3F((weight * factor) * VLOAD3F(&pathThroughput->r) * bsdfEval * lightRadiance, &radiance->r);
			*ID = min(lightID, PARAM_FILM_RADIANCE_GROUP_COUNT - 1u);

			// Setup the shadow ray
			const float3 hitPoint = VLOAD3F(&bsdf->hitPoint.p.x);
			const float epsilon = fmax(MachineEpsilon_E_Float3(hitPoint), MachineEpsilon_E(distance));

			Ray_Init4_Private(shadowRay, hitPoint, lightRayDir,
				epsilon,
				distance - epsilon);

			return true;
		}
	}

	return false;
}

bool DirectLightSampling_ONE(
		const bool firstPathVertex,
		Seed *seed,
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
		__global HitPoint *directLightHitPoint,
#endif
		__global float *lightsDistribution,
		__global const Spectrum *pathThroughput, __global BSDF *bsdf,
		__global SampleResult *sampleResult,
		Ray *shadowRay, __global Spectrum *radiance, __global uint *ID
		MATERIALS_PARAM_DECL) {
	// Pick a light source to sample
	float lightPickPdf;
	const uint lightIndex = Scene_SampleAllLights(lightsDistribution, Rnd_FloatValue(seed), &lightPickPdf);

	const bool illuminated = DirectLightSampling(
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
		directLightHitPoint,
#endif
		lightsDistribution,
		Rnd_FloatValue(seed), Rnd_FloatValue(seed), Rnd_FloatValue(seed),
		pathThroughput, bsdf,
		shadowRay, radiance, ID
		MATERIALS_PARAM);

#if defined(PARAM_FILM_CHANNELS_HAS_DIRECT_SHADOW_MASK)
	if (firstPathVertex && !illuminated)
		sampleResult->directShadowMask += 1.f;
#endif

	return illuminated;
}

#if defined(PARAM_DIRECT_LIGHT_ALL_STRATEGY)
bool DirectLightSampling_ALL(
		__global uint *currentLightIndex,
		__global uint *currentLightSampleIndex,
		__global int *lightSamples,
		Seed *seed,
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
		__global HitPoint *directLightHitPoint,
#endif
		__global float *lightsDistribution,
		__global const Spectrum *pathThroughput, __global BSDF *bsdf,
		__global SampleResult *sampleResult,
		Ray *shadowRay, __global Spectrum *radiance, __global uint *ID
		MATERIALS_PARAM_DECL) {
	for (; *currentLightIndex < PARAM_LIGHT_COUNT; ++(*currentLightIndex)) {
		const int lightSamplesCount = lightSamples[*currentLightIndex];
		const uint sampleCount = (lightSamplesCount < 0) ? PARAM_DIRECT_LIGHT_SAMPLES : (uint)lightSamplesCount;
		const uint sampleCount2 = sampleCount * sampleCount;

		for (; *currentLightSampleIndex < sampleCount2; ++(*currentLightSampleIndex)) {
			//if (get_global_id(0) == 0)
			//	printf("DirectLightSampling_ALL() ==> currentLightIndex: %d  currentLightSampleIndex: %d\n", *currentLightIndex, *currentLightSampleIndex);

			float u0, u1;
			SampleGrid(seed, sampleCount,
					(*currentLightSampleIndex) % sampleCount, (*currentLightSampleIndex) / sampleCount,
					&u0, &u1);

			const float scaleFactor = 1.f / sampleCount2;
			const bool illuminated = DirectLightSampling(
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
				directLightHitPoint,
#endif
				lightsDistribution,
				u0, u1, Rnd_FloatValue(seed),
				pathThroughput, bsdf,
				shadowRay, radiance, ID
				MATERIALS_PARAM);

			if (illuminated) {
				VSTORE3F(scaleFactor * VLOAD3F(&radiance->r), &radiance->r);
				return true;
			}
#if defined(PARAM_FILM_CHANNELS_HAS_DIRECT_SHADOW_MASK)
			else {
				sampleResult->directShadowMask += scaleFactor;
			}
#endif
		}

		*currentLightSampleIndex = 0;
	}

	return false;
}
#endif

__kernel __attribute__((work_group_size_hint(64, 1, 1))) void RenderSample(
		const uint tileStartX,
		const uint tileStartY,
		const int tileSampleIndex,
		const uint engineFilmWidth, const uint engineFilmHeight,
		__global GPUTask *tasks,
		__global GPUTaskDirectLight *tasksDirectLight,
		__global GPUTaskPathVertexN *tasksPathVertexN,
		__global GPUTaskStats *taskStats,
		__global SampleResult *taskResults,
		__global float *pixelFilterDistribution,
		__global int *lightSamples,
		__global BSDFEvent *lightVisibility,
		__global int *materialSamples,
		__global BSDFEvent *materialVisibility,
		// Film parameters
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
		,
		// Scene parameters
		const float worldCenterX,
		const float worldCenterY,
		const float worldCenterZ,
		const float worldRadius,
		__global Material *mats,
		__global Texture *texs,
		__global uint *meshMats,
		__global Mesh *meshDescs,
		__global Point *vertices,
#if defined(PARAM_HAS_NORMALS_BUFFER)
		__global Vector *vertNormals,
#endif
#if defined(PARAM_HAS_UVS_BUFFER)
		__global UV *vertUVs,
#endif
#if defined(PARAM_HAS_COLS_BUFFER)
		__global Spectrum *vertCols,
#endif
#if defined(PARAM_HAS_ALPHAS_BUFFER)
		__global float *vertAlphas,
#endif
		__global Triangle *triangles,
		__global Camera *camera,
		__global float *lightsDistribution
#if defined(PARAM_HAS_INFINITELIGHT)
		, __global InfiniteLight *infiniteLight
		, __global float *infiniteLightDistribution
#endif
#if defined(PARAM_HAS_SUNLIGHT)
		, __global SunLight *sunLight
#endif
#if defined(PARAM_HAS_SKYLIGHT)
		, __global SkyLight *skyLight
#endif
#if (PARAM_TRIANGLE_LIGHT_COUNT > 0)
		, __global TriangleLight *triLightDefs
		, __global uint *meshTriLightDefsOffset
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_0)
		, __global ImageMap *imageMapDescs, __global float *imageMapBuff0
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_1)
		, __global float *imageMapBuff1
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_2)
		, __global float *imageMapBuff2
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_3)
		, __global float *imageMapBuff3
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_4)
		, __global float *imageMapBuff4
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_5)
		, __global float *imageMapBuff5
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_6)
		, __global float *imageMapBuff6
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_7)
		, __global float *imageMapBuff7
#endif
		ACCELERATOR_INTERSECT_PARAM_DECL
		) {
	const size_t gid = get_global_id(0);

#if defined(PARAM_TILE_PROGRESSIVE_REFINEMENT)
	const uint sampleIndex = tileSampleIndex;
	const uint samplePixelX = gid % PARAM_TILE_SIZE;
	const uint samplePixelY = gid / PARAM_TILE_SIZE;

	if ((gid >= PARAM_TILE_SIZE * PARAM_TILE_SIZE) ||
			(tileStartX + samplePixelX >= engineFilmWidth) ||
			(tileStartY + samplePixelY >= engineFilmHeight))
		return;
#else
	const uint sampleIndex = gid % (PARAM_AA_SAMPLES * PARAM_AA_SAMPLES);
	const uint samplePixelIndex = gid / (PARAM_AA_SAMPLES * PARAM_AA_SAMPLES);
	const uint samplePixelX = samplePixelIndex % PARAM_TILE_SIZE;
	const uint samplePixelY = samplePixelIndex / PARAM_TILE_SIZE;

	if ((gid >= PARAM_TILE_SIZE * PARAM_TILE_SIZE * PARAM_AA_SAMPLES * PARAM_AA_SAMPLES) ||
			(tileStartX + samplePixelX >= engineFilmWidth) ||
			(tileStartY + samplePixelY >= engineFilmHeight))
		return;
#endif

	__global GPUTask *task = &tasks[gid];
	__global GPUTaskDirectLight *taskDirectLight = &tasksDirectLight[gid];
	__global GPUTaskPathVertexN *taskPathVertexN = &tasksPathVertexN[gid];
	__global GPUTaskStats *taskStat = &taskStats[gid];

	//--------------------------------------------------------------------------
	// Initialize image maps page pointer table
	//--------------------------------------------------------------------------

#if defined(PARAM_HAS_IMAGEMAPS)
	__global float *imageMapBuff[PARAM_IMAGEMAPS_COUNT];
#if defined(PARAM_IMAGEMAPS_PAGE_0)
	imageMapBuff[0] = imageMapBuff0;
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_1)
	imageMapBuff[1] = imageMapBuff1;
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_2)
	imageMapBuff[2] = imageMapBuff2;
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_3)
	imageMapBuff[3] = imageMapBuff3;
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_4)
	imageMapBuff[4] = imageMapBuff4;
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_5)
	imageMapBuff[5] = imageMapBuff5;
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_6)
	imageMapBuff[6] = imageMapBuff6;
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_7)
	imageMapBuff[7] = imageMapBuff7;
#endif
#endif

	//--------------------------------------------------------------------------
	// Initialize the first ray
	//--------------------------------------------------------------------------

	// Read the seed
	Seed seed;
	seed.s1 = task->seed.s1;
	seed.s2 = task->seed.s2;
	seed.s3 = task->seed.s3;

	__global SampleResult *sampleResult = &taskResults[gid];
	SampleResult_Init(sampleResult);
#if defined(PARAM_FILM_CHANNELS_HAS_DIRECT_SHADOW_MASK)
	sampleResult->directShadowMask = 0.f;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_INDIRECT_SHADOW_MASK)
	sampleResult->indirectShadowMask = 0.f;
#endif

	Ray ray;
	RayHit rayHit;
	GenerateCameraRay(&seed, task, sampleResult,
			camera, pixelFilterDistribution,
			samplePixelX, samplePixelY, sampleIndex,
			tileStartX, tileStartY, 
			engineFilmWidth, engineFilmHeight, &ray);

	//--------------------------------------------------------------------------
	// Render a sample
	//--------------------------------------------------------------------------

	taskPathVertexN->vertex1SampleComponent = DIFFUSE;
	taskPathVertexN->vertex1SampleIndex = 0;

	uint tracedRaysCount = taskStat->raysCount;
	uint pathState = PATH_VERTEX_1 | NEXT_VERTEX_TRACE_RAY;
	PathDepthInfo depthInfo;
	PathDepthInfo_Init(&depthInfo, 0);
	float lastPdfW = 1.f;
	BSDFEvent pathBSDFEvent = NONE;
	BSDFEvent lastBSDFEvent = SPECULAR;

	__global BSDF *currentBSDF = &task->bsdfPathVertex1;
	__global Spectrum *currentThroughput = &task->throughputPathVertex1;
	VSTORE3F(WHITE, &task->throughputPathVertex1.r);
#if defined(PARAM_HAS_PASSTHROUGH)
	// This is a bit tricky. I store the passThroughEvent in the BSDF
	// before of the initialization because it can be used during the
	// tracing of next path vertex ray.

	task->bsdfPathVertex1.hitPoint.passThroughEvent = Rnd_FloatValue(&seed);
#endif

	//if (get_global_id(0) == 0) {
	//	printf("============================================================\n");
	//	printf("== Begin loop\n");
	//	printf("==\n");
	//	printf("== task->bsdfPathVertex1: %x\n", &task->bsdfPathVertex1);
	//	printf("== taskDirectLight->directLightBSDF: %x\n", &taskDirectLight->directLightBSDF);
	//	printf("== taskPathVertexN->bsdfPathVertexN: %x\n", &taskPathVertexN->bsdfPathVertexN);
	//	printf("============================================================\n");
	//}

	while (!(pathState & DONE)) {
		//if (get_global_id(0) == 0)
		//	printf("Depth: %d  [pathState: %d|%d][currentBSDF: %x][currentThroughput: %x]\n",
		//			depthInfo.depth, pathState >> 16, pathState & LOW_STATE_MASK, currentBSDF, currentThroughput);

		//----------------------------------------------------------------------
		// Ray trace step
		//----------------------------------------------------------------------

		Accelerator_Intersect(&ray, &rayHit
			ACCELERATOR_INTERSECT_PARAM);
		++tracedRaysCount;

		if (rayHit.meshIndex != NULL_INDEX) {
			// Something was hit, initialize the BSDF
			BSDF_Init(currentBSDF,
					meshDescs,
					meshMats,
#if (PARAM_TRIANGLE_LIGHT_COUNT > 0)
					meshTriLightDefsOffset,
#endif
					vertices,
#if defined(PARAM_HAS_NORMALS_BUFFER)
					vertNormals,
#endif
#if defined(PARAM_HAS_UVS_BUFFER)
					vertUVs,
#endif
#if defined(PARAM_HAS_COLS_BUFFER)
					vertCols,
#endif
#if defined(PARAM_HAS_ALPHAS_BUFFER)
					vertAlphas,
#endif
					triangles, &ray, &rayHit
#if defined(PARAM_HAS_PASSTHROUGH)
					, currentBSDF->hitPoint.passThroughEvent
#endif
#if defined(PARAM_HAS_BUMPMAPS) || defined(PARAM_HAS_NORMALMAPS)
					MATERIALS_PARAM
#endif
					);

#if defined(PARAM_HAS_PASSTHROUGH)
			const float3 passThroughTrans = BSDF_GetPassThroughTransparency(currentBSDF
					MATERIALS_PARAM);
			if (!Spectrum_IsBlack(passThroughTrans)) {
				const float3 pathThroughput = VLOAD3F(&currentThroughput->r) * passThroughTrans;
				VSTORE3F(pathThroughput, &currentThroughput->r);

				// It is a pass through point, continue to trace the ray
				ray.mint = rayHit.t + MachineEpsilon_E(rayHit.t);

				continue;
			}
#endif
		}

		//----------------------------------------------------------------------
		// Advance the finite state machine step
		//----------------------------------------------------------------------

		//----------------------------------------------------------------------
		// Evaluation of the finite state machine.
		//
		// From: * | NEXT_VERTEX_TRACE_RAY
		// To: DONE or
		//     (* | DIRECT_LIGHT_GENERATE_RAY)
		//     (PATH_VERTEX_N | NEXT_GENERATE_TRACE_RAY)
		//----------------------------------------------------------------------

		if (pathState & NEXT_VERTEX_TRACE_RAY) {
			const bool firstPathVertex = (pathState & PATH_VERTEX_1);

			if (rayHit.meshIndex != NULL_INDEX) {
				//--------------------------------------------------------------
				// Something was hit
				//--------------------------------------------------------------

				if (firstPathVertex) {
					// Save the path first vertex information
#if defined(PARAM_FILM_CHANNELS_HAS_ALPHA)
					sampleResult->alpha = 1.f;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_DEPTH)
					sampleResult->depth = rayHit.t;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_POSITION)
					sampleResult->position = task->bsdfPathVertex1.hitPoint.p;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_GEOMETRY_NORMAL)
					sampleResult->geometryNormal = task->bsdfPathVertex1.hitPoint.geometryN;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_SHADING_NORMAL)
					sampleResult->shadingNormal = task->bsdfPathVertex1.hitPoint.shadeN;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_MATERIAL_ID)
					sampleResult->materialID = BSDF_GetMaterialID(&task->bsdfPathVertex1
						MATERIALS_PARAM);
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_UV)
					sampleResult->uv = task->bsdfPathVertex1.hitPoint.uv;
#endif
				}

				if (firstPathVertex || (materialVisibility[currentBSDF->materialIndex] & (pathBSDFEvent & (DIFFUSE | GLOSSY | SPECULAR)))) {
#if (PARAM_TRIANGLE_LIGHT_COUNT > 0)
					// Check if it is a light source (note: I can hit only triangle area light sources)
					if (BSDF_IsLightSource(currentBSDF) && (rayHit.t > PARAM_NEAR_START_LIGHT)) {
						DirectHitFiniteLight(firstPathVertex, lastBSDFEvent,
								pathBSDFEvent, lightVisibility, lightsDistribution,
								triLightDefs, currentThroughput,
								rayHit.t, currentBSDF, lastPdfW,
								sampleResult
								MATERIALS_PARAM);
					}
#endif
					// Before Direct Lighting in order to have a correct MIS
					if (PathDepthInfo_CheckDepths(&depthInfo)) {
#if defined(PARAM_DIRECT_LIGHT_ALL_STRATEGY)
						taskDirectLight->lightIndex = 0;
						taskDirectLight->lightSampleIndex = 0;
#endif
						pathState = (pathState & HIGH_STATE_MASK) | DIRECT_LIGHT_GENERATE_RAY;
					} else {
						pathState = firstPathVertex ? DONE :
							(PATH_VERTEX_1 | NEXT_VERTEX_GENERATE_RAY);
					}
				} else
					pathState = PATH_VERTEX_1 | NEXT_VERTEX_GENERATE_RAY;
			} else {
				//--------------------------------------------------------------
				// Nothing was hit, add environmental lights radiance
				//--------------------------------------------------------------

#if defined(PARAM_HAS_SKYLIGHT) || defined(PARAM_HAS_INFINITELIGHT) || defined(PARAM_HAS_SUNLIGHT)
				const float3 rayDir = (float3)(ray.d.x, ray.d.y, ray.d.z);
				DirectHitInfiniteLight(
						firstPathVertex,
						lastBSDFEvent,
						pathBSDFEvent,
						lightVisibility, 
						lightsDistribution,
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
						currentThroughput,
						-rayDir, lastPdfW,
						sampleResult
						IMAGEMAPS_PARAM);
#endif

				if (firstPathVertex) {
#if defined(PARAM_FILM_CHANNELS_HAS_ALPHA)
					sampleResult->alpha = 0.f;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_DEPTH)
					sampleResult->depth = INFINITY;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_POSITION)
					sampleResult->position.x = INFINITY;
					sampleResult->position.y = INFINITY;
					sampleResult->position.z = INFINITY;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_GEOMETRY_NORMAL)
					sampleResult->geometryNormal.x = INFINITY;
					sampleResult->geometryNormal.y = INFINITY;
					sampleResult->geometryNormal.z = INFINITY;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_SHADING_NORMAL)
					sampleResult->shadingNormal.x = INFINITY;
					sampleResult->shadingNormal.y = INFINITY;
					sampleResult->shadingNormal.z = INFINITY;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_MATERIAL_ID)
					sampleResult->materialID = NULL_INDEX;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_UV)
					sampleResult->uv.u = INFINITY;
					sampleResult->uv.v = INFINITY;
#endif
				}

				pathState = firstPathVertex ? DONE : (PATH_VERTEX_1 | NEXT_VERTEX_GENERATE_RAY);
			}
		}

		//----------------------------------------------------------------------
		// Evaluation of the finite state machine.
		//
		// From: * | DIRECT_LIGHT_TRACE_RAY
		// To: (* | NEXT_VERTEX_GENERATE_RAY) or
		//     (* | DIRECT_LIGHT_GENERATE_RAY)
		//----------------------------------------------------------------------

		if (pathState & DIRECT_LIGHT_TRACE_RAY) {
			const bool firstPathVertex = (pathState & PATH_VERTEX_1);

			if (rayHit.meshIndex == NULL_INDEX) {
				//--------------------------------------------------------------
				// Nothing was hit, the light source is visible
				//--------------------------------------------------------------

				// currentThroughput contains the shadow ray throughput
				const float3 lightRadiance = VLOAD3F(&currentThroughput->r) * VLOAD3F(&taskDirectLight->lightRadiance.r);
				const uint lightID = taskDirectLight->lightID;
				VADD3F(&sampleResult->radiancePerPixelNormalized[lightID].r, lightRadiance);

				//if (get_global_id(0) == 0)
				//	printf("DIRECT_LIGHT_TRACE_RAY => lightRadiance: %f %f %f [%d]\n", lightRadiance.s0, lightRadiance.s1, lightRadiance.s2, lightID);

				if (firstPathVertex) {
					if (BSDF_GetEventTypes(&task->bsdfPathVertex1
							MATERIALS_PARAM) & DIFFUSE) {
#if defined(PARAM_FILM_CHANNELS_HAS_DIRECT_DIFFUSE)
						VADD3F(&sampleResult->directDiffuse.r, lightRadiance);
#endif
					} else {
#if defined(PARAM_FILM_CHANNELS_HAS_DIRECT_GLOSSY)
						VADD3F(&sampleResult->directGlossy.r, lightRadiance);
#endif
					}
				} else {
#if defined(PARAM_FILM_CHANNELS_HAS_INDIRECT_SHADOW_MASK)
					sample->result.indirectShadowMask = 0.f;
#endif

					if (pathBSDFEvent & DIFFUSE) {
#if defined(PARAM_FILM_CHANNELS_HAS_INDIRECT_DIFFUSE)
						VADD3F(&sample->result.indirectDiffuse.r, lightRadiance);
#endif
					} else if (pathBSDFEvent & GLOSSY) {
#if defined(PARAM_FILM_CHANNELS_HAS_INDIRECT_GLOSSY)
						VADD3F(&sample->result.indirectGlossy.r, lightRadiance);
#endif
					} else if (pathBSDFEvent & SPECULAR) {
#if defined(PARAM_FILM_CHANNELS_HAS_INDIRECT_SPECULAR)
						VADD3F(&sample->result.indirectSpecular.r, lightRadiance);
#endif
					}
				}
			}
#if defined(PARAM_FILM_CHANNELS_HAS_DIRECT_SHADOW_MASK)
			else {
				if (firstPathVertex) {
					const int lightSamplesCount = lightSamples[*currentLightIndex];
					const uint sampleCount = (lightSamplesCount < 0) ? PARAM_DIRECT_LIGHT_SAMPLES : (uint)lightSamplesCount;

					sampleResult->directShadowMask += 1.f / (sampleCount * sampleCount);
				}
			}
#endif

#if defined(PARAM_DIRECT_LIGHT_ALL_STRATEGY)
			if (firstPathVertex) {
				const uint lightIndex = taskDirectLight->lightIndex;
				if (lightIndex <= PARAM_LIGHT_COUNT) {
					++(taskDirectLight->lightSampleIndex);
					pathState = PATH_VERTEX_1 | DIRECT_LIGHT_GENERATE_RAY;
				} else {
					// Move to the next path vertex
					pathState = PATH_VERTEX_1 | NEXT_VERTEX_GENERATE_RAY;
				}
			} else {
				// Move to the next path vertex
				pathState = PATH_VERTEX_N | NEXT_VERTEX_GENERATE_RAY;
			}
#else
			// Move to the next path vertex
			pathState = (pathState & HIGH_STATE_MASK) | NEXT_VERTEX_GENERATE_RAY;
#endif
		}

		//----------------------------------------------------------------------
		// Evaluation of the finite state machine.
		//
		// From: * | DIRECT_LIGHT_GENERATE_RAY
		// To: (* | NEXT_VERTEX_GENERATE_RAY) or 
		//     (* | DIRECT_LIGHT_TRACE_RAY[continue])
		//     (* | NEXT_VERTEX_GENERATE_RAY)
		//----------------------------------------------------------------------

		if (pathState & DIRECT_LIGHT_GENERATE_RAY) {
			const bool firstPathVertex = (pathState & PATH_VERTEX_1);

			if (BSDF_IsDelta(firstPathVertex ? &task->bsdfPathVertex1 : &taskPathVertexN->bsdfPathVertexN
				MATERIALS_PARAM)) {
				// Move to the next path vertex
				pathState = (pathState & HIGH_STATE_MASK) | NEXT_VERTEX_GENERATE_RAY;
			} else {
				const bool illuminated =
#if defined(PARAM_DIRECT_LIGHT_ALL_STRATEGY)
				(!firstPathVertex) ?
#endif
					DirectLightSampling_ONE(
						firstPathVertex,
						&seed,
#if defined(PARAM_HAS_INFINITELIGHT) || defined(PARAM_HAS_SKYLIGHT)
						worldCenterX, worldCenterY, worldCenterZ, worldRadius,
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
						&taskDirectLight->directLightHitPoint,
#endif
						lightsDistribution,
						firstPathVertex ? &task->throughputPathVertex1 : &taskPathVertexN->throughputPathVertexN,
						firstPathVertex ? &task->bsdfPathVertex1 : &taskPathVertexN->bsdfPathVertexN,
						sampleResult,
						&ray, &taskDirectLight->lightRadiance, &taskDirectLight->lightID
						MATERIALS_PARAM)
#if defined(PARAM_DIRECT_LIGHT_ALL_STRATEGY)
				: DirectLightSampling_ALL(
						&taskDirectLight->lightIndex,
						&taskDirectLight->lightSampleIndex,
						lightSamples,
						&seed,
#if defined(PARAM_HAS_INFINITELIGHT) || defined(PARAM_HAS_SKYLIGHT)
						worldCenterX, worldCenterY, worldCenterZ, worldRadius,
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
						&taskDirectLight->directLightHitPoint,
#endif
						lightsDistribution,
						&task->throughputPathVertex1, &task->bsdfPathVertex1,
						sampleResult,
						&ray, &taskDirectLight->lightRadiance, &taskDirectLight->lightID
						MATERIALS_PARAM)
#endif
				;
				
				if (illuminated) {
					// Trace the shadow ray
					currentBSDF = &taskDirectLight->directLightBSDF;
					currentThroughput = &taskDirectLight->directLightThroughput;
					VSTORE3F(WHITE, &currentThroughput->r);
#if defined(PARAM_HAS_PASSTHROUGH)
					// This is a bit tricky. I store the passThroughEvent in the BSDF
					// before of the initialization because it can be use during the
					// tracing of next path vertex ray.
					taskDirectLight->directLightBSDF.hitPoint.passThroughEvent = Rnd_FloatValue(&seed);
#endif

					pathState = (pathState & HIGH_STATE_MASK) | DIRECT_LIGHT_TRACE_RAY;
				} else {
					// Move to the next path vertex
					pathState = (pathState & HIGH_STATE_MASK) | NEXT_VERTEX_GENERATE_RAY;
				}
			}
		}

		//----------------------------------------------------------------------
		// Evaluation of the finite state machine.
		//
		// From: PATH_VERTEX_N | NEXT_VERTEX_GENERATE_RAY
		// To: PATH_VERTEX_1 | NEXT_VERTEX_GENERATE_RAY or (PATH_VERTEX_N | NEXT_VERTEX_TRACE_RAY[continue])
		//----------------------------------------------------------------------

		if (pathState == (PATH_VERTEX_N | NEXT_VERTEX_GENERATE_RAY)) {
			//if (get_global_id(0) == 0)
			//	printf("(PATH_VERTEX_N | NEXT_VERTEX_GENERATE_RAY)) ==> Depth: %d  [pathState: %d|%d][currentThroughput: %f %f %f]\n",
			//			depthInfo.depth, pathState >> 16, pathState & LOW_STATE_MASK, currentThroughput->r, currentThroughput->g, currentThroughput->b);

			// Sample the BSDF
			float3 sampledDir;
			float cosSampledDir;
			BSDFEvent event;
			const float3 bsdfSample = BSDF_Sample(&taskPathVertexN->bsdfPathVertexN,
				Rnd_FloatValue(&seed),
				Rnd_FloatValue(&seed),
				&sampledDir, &lastPdfW, &cosSampledDir, &event, ALL
				MATERIALS_PARAM);

			PathDepthInfo_IncDepths(&depthInfo, event);

			if (!Spectrum_IsBlack(bsdfSample)) {
				float3 throughput = VLOAD3F(&taskPathVertexN->throughputPathVertexN.r);
				throughput *= bsdfSample * (cosSampledDir / max(PARAM_PDF_CLAMP_VALUE, lastPdfW));
				VSTORE3F(throughput, &taskPathVertexN->throughputPathVertexN.r);

				Ray_Init2_Private(&ray, VLOAD3F(&taskPathVertexN->bsdfPathVertexN.hitPoint.p.x), sampledDir);

				lastBSDFEvent = event;

#if defined(PARAM_HAS_PASSTHROUGH)
				// This is a bit tricky. I store the passThroughEvent in the BSDF
				// before of the initialization because it can be use during the
				// tracing of next path vertex ray.
				taskPathVertexN->bsdfPathVertexN.hitPoint.passThroughEvent = Rnd_FloatValue(&seed);
#endif
				currentBSDF = &taskPathVertexN->bsdfPathVertexN;
				currentThroughput = &taskPathVertexN->throughputPathVertexN;

				pathState = PATH_VERTEX_N | NEXT_VERTEX_TRACE_RAY;
			} else
				pathState = PATH_VERTEX_1 | NEXT_VERTEX_GENERATE_RAY;
		}

		//----------------------------------------------------------------------
		// Evaluation of the finite state machine.
		//
		// From: PATH_VERTEX_1 | NEXT_VERTEX_GENERATE_RAY
		// To: DONE or (PATH_VERTEX_N | NEXT_VERTEX_TRACE_RAY[continue])
		//----------------------------------------------------------------------

		if (pathState == (PATH_VERTEX_1 | NEXT_VERTEX_GENERATE_RAY)) {
			//if (get_global_id(0) == 0)
			//	printf("(PATH_VERTEX_1 | NEXT_VERTEX_GENERATE_RAY)) ==> Depth: %d  [pathState: %d|%d][currentThroughput: %f %f %f]\n",
			//			depthInfo.depth, pathState >> 16, pathState & LOW_STATE_MASK, currentThroughput->r, currentThroughput->g, currentThroughput->b);

			BSDFEvent vertex1SampleComponent = taskPathVertexN->vertex1SampleComponent;
			uint vertex1SampleIndex = taskPathVertexN->vertex1SampleIndex;
			const BSDFEvent materialEventTypes = BSDF_GetEventTypes(&task->bsdfPathVertex1
				MATERIALS_PARAM);

			for (;;) {
				const int matSamplesCount = materialSamples[task->bsdfPathVertex1.materialIndex];
				const uint globalMatSamplesCount = ((vertex1SampleComponent == DIFFUSE) ? PARAM_DIFFUSE_SAMPLES :
					((vertex1SampleComponent == GLOSSY) ? PARAM_GLOSSY_SAMPLES :
						PARAM_SPECULAR_SAMPLES));
				const uint sampleCount = (matSamplesCount < 0) ? globalMatSamplesCount : (uint)matSamplesCount;
				const uint sampleCount2 = sampleCount * sampleCount;

				if (!(materialEventTypes & vertex1SampleComponent) || (vertex1SampleIndex >= sampleCount2)) {
					// Move to next component
					vertex1SampleComponent = (vertex1SampleComponent == DIFFUSE) ? GLOSSY :
						((vertex1SampleComponent == GLOSSY) ? SPECULAR : NONE);

					if (vertex1SampleComponent == NONE) {
						pathState = DONE;
						break;
					}

					vertex1SampleIndex = 0;
				} else {
					// Sample the BSDF
					float3 sampledDir;
					float cosSampledDir;
					BSDFEvent event;

					float u0, u1;
					SampleGrid(&seed, sampleCount,
						vertex1SampleIndex % sampleCount, vertex1SampleIndex / sampleCount,
						&u0, &u1);

					// Now, I can increment vertex1SampleIndex
					++vertex1SampleIndex;

#if defined(PARAM_HAS_PASSTHROUGH)
					// This is a bit tricky. I store the passThroughEvent in the BSDF
					// before of the initialization because it can be use during the
					// tracing of next path vertex ray.
					task->bsdfPathVertex1.hitPoint.passThroughEvent = Rnd_FloatValue(&seed);
#endif
					const float3 bsdfSample = BSDF_Sample(&task->bsdfPathVertex1,
							u0,
							u1,
							&sampledDir, &lastPdfW, &cosSampledDir, &event,
							vertex1SampleComponent | REFLECT | TRANSMIT
							MATERIALS_PARAM);

					PathDepthInfo_Init(&depthInfo, 0);
					PathDepthInfo_IncDepths(&depthInfo, event);

					if (!Spectrum_IsBlack(bsdfSample)) {
						const float scaleFactor = 1.f / sampleCount2;
						float3 throughput = VLOAD3F(&task->throughputPathVertex1.r);
						throughput *= bsdfSample * (scaleFactor * cosSampledDir / max(PARAM_PDF_CLAMP_VALUE, lastPdfW));
						VSTORE3F(throughput, &taskPathVertexN->throughputPathVertexN.r);

						Ray_Init2_Private(&ray, VLOAD3F(&task->bsdfPathVertex1.hitPoint.p.x), sampledDir);

						pathBSDFEvent = event;
						lastBSDFEvent = event;

#if defined(PARAM_HAS_PASSTHROUGH)
						// This is a bit tricky. I store the passThroughEvent in the BSDF
						// before of the initialization because it can be use during the
						// tracing of next path vertex ray.
						taskPathVertexN->bsdfPathVertexN.hitPoint.passThroughEvent = Rnd_FloatValue(&seed);
#endif
						currentBSDF = &taskPathVertexN->bsdfPathVertexN;
						currentThroughput = &taskPathVertexN->throughputPathVertexN;

						pathState = PATH_VERTEX_N | NEXT_VERTEX_TRACE_RAY;

						// Save vertex1SampleComponent and vertex1SampleIndex
						taskPathVertexN->vertex1SampleComponent = vertex1SampleComponent;
						taskPathVertexN->vertex1SampleIndex = vertex1SampleIndex;
						break;
					}
				}
			}
		}
	}

	//if (get_global_id(0) == 0) {
	//	printf("============================================================\n");
	//	printf("== End loop\n");
	//	printf("============================================================\n");
	//}

#if defined(PARAM_FILM_CHANNELS_HAS_RAYCOUNT)
	sampleResult->rayCount = tracedRaysCount;
#endif

	// Radiance clamping
	SR_RadianceClamp(sampleResult);

#if defined(PARAM_TILE_PROGRESSIVE_REFINEMENT)
	// Initialize Film radiance group pointer table
	__global float *filmRadianceGroup[PARAM_FILM_RADIANCE_GROUP_COUNT];
#if defined(PARAM_FILM_RADIANCE_GROUP_0)
		filmRadianceGroup[0] = filmRadianceGroup0;
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_1)
		filmRadianceGroup[1] = filmRadianceGroup1;
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_2)
		filmRadianceGroup[2] = filmRadianceGroup2;
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_3)
		filmRadianceGroup[3] = filmRadianceGroup3;
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_4)
		filmRadianceGroup[3] = filmRadianceGroup4;
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_5)
		filmRadianceGroup[3] = filmRadianceGroup5;
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_6)
		filmRadianceGroup[3] = filmRadianceGroup6;
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_7)
		filmRadianceGroup[3] = filmRadianceGroup7;
#endif

	Film_AddSample(samplePixelX, samplePixelY, sampleResult, 1.f
			FILM_PARAM);
#endif

	taskStat->raysCount = tracedRaysCount;

	// Save the seed
	task->seed.s1 = seed.s1;
	task->seed.s2 = seed.s2;
	task->seed.s3 = seed.s3;
}

//------------------------------------------------------------------------------
// MergePixelSamples
//------------------------------------------------------------------------------

void SR_Accumulate(__global SampleResult *src, SampleResult *dst) {
#if defined(PARAM_FILM_RADIANCE_GROUP_0)
	dst->radiancePerPixelNormalized[0].r += src->radiancePerPixelNormalized[0].r;
	dst->radiancePerPixelNormalized[0].g += src->radiancePerPixelNormalized[0].g;
	dst->radiancePerPixelNormalized[0].b += src->radiancePerPixelNormalized[0].b;
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_1)
	dst->radiancePerPixelNormalized[1].r += src->radiancePerPixelNormalized[1].r;
	dst->radiancePerPixelNormalized[1].g += src->radiancePerPixelNormalized[1].g;
	dst->radiancePerPixelNormalized[1].b += src->radiancePerPixelNormalized[1].b;
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_2)
	dst->radiancePerPixelNormalized[2].r += src->radiancePerPixelNormalized[2].r;
	dst->radiancePerPixelNormalized[2].g += src->radiancePerPixelNormalized[2].g;
	dst->radiancePerPixelNormalized[2].b += src->radiancePerPixelNormalized[2].b;
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_3)
	dst->radiancePerPixelNormalized[3].r += src->radiancePerPixelNormalized[3].r;
	dst->radiancePerPixelNormalized[3].g += src->radiancePerPixelNormalized[3].g;
	dst->radiancePerPixelNormalized[3].b += src->radiancePerPixelNormalized[3].b;
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_4)
	dst->radiancePerPixelNormalized[4].r += src->radiancePerPixelNormalized[4].r;
	dst->radiancePerPixelNormalized[4].g += src->radiancePerPixelNormalized[4].g;
	dst->radiancePerPixelNormalized[4].b += src->radiancePerPixelNormalized[4].b;
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_5)
	dst->radiancePerPixelNormalized[5].r += src->radiancePerPixelNormalized[5].r;
	dst->radiancePerPixelNormalized[5].g += src->radiancePerPixelNormalized[5].g;
	dst->radiancePerPixelNormalized[5].b += src->radiancePerPixelNormalized[5].b;
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_6)
	dst->radiancePerPixelNormalized[6].r += src->radiancePerPixelNormalized[6].r;
	dst->radiancePerPixelNormalized[6].g += src->radiancePerPixelNormalized[6].g;
	dst->radiancePerPixelNormalized[6].b += src->radiancePerPixelNormalized[6].b;
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_7)
	dst->radiancePerPixelNormalized[7].r += src->radiancePerPixelNormalized[7].r;
	dst->radiancePerPixelNormalized[7].g += src->radiancePerPixelNormalized[7].g;
	dst->radiancePerPixelNormalized[7].b += src->radiancePerPixelNormalized[7].b;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_ALPHA)
	dst->alpha += dst->alpha + src->alpha;
#endif

#if defined(PARAM_FILM_CHANNELS_HAS_DIRECT_DIFFUSE)
	dst->directDiffuse.r += src->directDiffuse.r;
	dst->directDiffuse.g += src->directDiffuse.g;
	dst->directDiffuse.b += src->directDiffuse.b;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_DIRECT_GLOSSY)
	dst->directGlossy.r += src->directGlossy.r;
	dst->directGlossy.g += src->directGlossy.g;
	dst->directGlossy.b += src->directGlossy.b;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_EMISSION)
	dst->emission.r += src->emission.r;
	dst->emission.g += src->emission.g;
	dst->emission.b += src->emission.b;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_INDIRECT_DIFFUSE)
	dst->indirectDiffuse.r += src->indirectDiffuse.r;
	dst->indirectDiffuse.g += src->indirectDiffuse.g;
	dst->indirectDiffuse.b += src->indirectDiffuse.b;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_INDIRECT_GLOSSY)
	dst->indirectGlossy.r += src->indirectGlossy.r;
	dst->indirectGlossy.g += src->indirectGlossy.g;
	dst->indirectGlossy.b += src->indirectGlossy.b;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_INDIRECT_SPECULAR)
	dst->indirectSpecular.r += src->indirectSpecular.r;
	dst->indirectSpecular.g += src->indirectSpecular.g;
	dst->indirectSpecular.b += src->indirectSpecular.b;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_DIRECT_SHADOW_MASK)
	dst->directShadowMask += src->directShadowMask;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_INDIRECT_SHADOW_MASK)
	dst->indirectShadowMask += src->indirectShadowMask;
#endif

	bool depthWrite = true;
#if defined(PARAM_FILM_CHANNELS_HAS_DEPTH)
	const float srcDepthValue = src->depth;
	if (srcDepthValue <= dst->depth)
		dst->depth = srcDepthValue;
	else
		depthWrite = false;
#endif
	if (depthWrite) {
#if defined(PARAM_FILM_CHANNELS_HAS_POSITION)
		dst->position = src->position;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_GEOMETRY_NORMAL)
		dst->geometryNormal = src->geometryNormal;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_SHADING_NORMAL)
		dst->shadingNormal = src->shadingNormal;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_MATERIAL_ID)
		// Note: MATERIAL_ID_MASK is calculated starting from materialID field
		dst->materialID = src->materialID;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_UV)
		dst->uv = src->uv;
#endif
	}

#if defined(PARAM_FILM_CHANNELS_HAS_RAYCOUNT)
	dst->rayCount += src->rayCount;
#endif
}

void SR_Normalize(SampleResult *dst, const float k) {
#if defined(PARAM_FILM_RADIANCE_GROUP_0)
	dst->radiancePerPixelNormalized[0].r *= k;
	dst->radiancePerPixelNormalized[0].g *= k;
	dst->radiancePerPixelNormalized[0].b *= k;
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_1)
	dst->radiancePerPixelNormalized[1].r *= k;
	dst->radiancePerPixelNormalized[1].g *= k;
	dst->radiancePerPixelNormalized[1].b *= k;
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_2)
	dst->radiancePerPixelNormalized[2].r *= k;
	dst->radiancePerPixelNormalized[2].g *= k;
	dst->radiancePerPixelNormalized[2].b *= k;
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_3)
	dst->radiancePerPixelNormalized[3].r *= k;
	dst->radiancePerPixelNormalized[3].g *= k;
	dst->radiancePerPixelNormalized[3].b *= k;
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_4)
	dst->radiancePerPixelNormalized[4].r *= k;
	dst->radiancePerPixelNormalized[4].g *= k;
	dst->radiancePerPixelNormalized[4].b *= k;
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_5)
	dst->radiancePerPixelNormalized[5].r *= k;
	dst->radiancePerPixelNormalized[5].g *= k;
	dst->radiancePerPixelNormalized[5].b *= k;
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_6)
	dst->radiancePerPixelNormalized[6].r *= k;
	dst->radiancePerPixelNormalized[6].g *= k;
	dst->radiancePerPixelNormalized[6].b *= k;
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_7)
	dst->radiancePerPixelNormalized[7].r *= k;
	dst->radiancePerPixelNormalized[7].g *= k;
	dst->radiancePerPixelNormalized[7].b *= k;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_ALPHA)
	dst->alpha *= k;
#endif

#if defined(PARAM_FILM_CHANNELS_HAS_DIRECT_DIFFUSE)
	dst->directDiffuse.r *= k;
	dst->directDiffuse.g *= k;
	dst->directDiffuse.b *= k;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_DIRECT_GLOSSY)
	dst->directGlossy.r *= k;
	dst->directGlossy.g *= k;
	dst->directGlossy.b *= k;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_EMISSION)
	dst->emission.r *= k;
	dst->emission.g *= k;
	dst->emission.b *= k;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_INDIRECT_DIFFUSE)
	dst->indirectDiffuse.r *= k;
	dst->indirectDiffuse.g *= k;
	dst->indirectDiffuse.b *= k;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_INDIRECT_GLOSSY)
	dst->indirectGlossy.r *= k;
	dst->indirectGlossy.g *= k;
	dst->indirectGlossy.b *= k;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_INDIRECT_SPECULAR)
	dst->indirectSpecular.r *= k;
	dst->indirectSpecular.g *= k;
	dst->indirectSpecular.b *= k;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_DIRECT_SHADOW_MASK)
	dst->directShadowMask *= k;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_INDIRECT_SHADOW_MASK)
	dst->indirectShadowMask *= k;
#endif
}

__kernel __attribute__((work_group_size_hint(64, 1, 1))) void MergePixelSamples(
		const uint tileStartX,
		const uint tileStartY,
		const uint engineFilmWidth, const uint engineFilmHeight,
		__global SampleResult *taskResults,
		__global GPUTaskStats *taskStats,
		// Film parameters
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

	uint sampleX, sampleY;
	sampleX = gid % PARAM_TILE_SIZE;
	sampleY = gid / PARAM_TILE_SIZE;

	if ((gid >= PARAM_TILE_SIZE * PARAM_TILE_SIZE) ||
			(tileStartX + sampleX >= engineFilmWidth) ||
			(tileStartY + sampleY >= engineFilmHeight))
		return;

	const uint index = gid * PARAM_AA_SAMPLES * PARAM_AA_SAMPLES;
	__global SampleResult *sampleResult = &taskResults[index];

	//--------------------------------------------------------------------------
	// Initialize Film radiance group pointer table
	//--------------------------------------------------------------------------

	__global float *filmRadianceGroup[PARAM_FILM_RADIANCE_GROUP_COUNT];
#if defined(PARAM_FILM_RADIANCE_GROUP_0)
	filmRadianceGroup[0] = filmRadianceGroup0;
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_1)
	filmRadianceGroup[1] = filmRadianceGroup1;
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_2)
	filmRadianceGroup[2] = filmRadianceGroup2;
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_3)
	filmRadianceGroup[3] = filmRadianceGroup3;
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_4)
	filmRadianceGroup[3] = filmRadianceGroup4;
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_5)
	filmRadianceGroup[3] = filmRadianceGroup5;
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_6)
	filmRadianceGroup[3] = filmRadianceGroup6;
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_7)
	filmRadianceGroup[3] = filmRadianceGroup7;
#endif

	//--------------------------------------------------------------------------
	// Merge all samples and accumulate statistics
	//--------------------------------------------------------------------------

#if (PARAM_AA_SAMPLES == 1)
	Film_AddSample(sampleX, sampleY, &sampleResult[0], PARAM_AA_SAMPLES * PARAM_AA_SAMPLES
			FILM_PARAM);
#else
	__global GPUTaskStats *taskStat = &taskStats[index];

	SampleResult result = sampleResult[0];
	uint totalRaysCount = 0;
	for (uint i = 1; i < PARAM_AA_SAMPLES * PARAM_AA_SAMPLES; ++i) {
		SR_Accumulate(&sampleResult[i], &result);
		totalRaysCount += taskStat[i].raysCount;
	}
	SR_Normalize(&result, 1.f / (PARAM_AA_SAMPLES * PARAM_AA_SAMPLES));

	// I have to save result in __global space in order to be able
	// to use Film_AddSample(). OpenCL can be so stupid some time...
	sampleResult[0] = result;
	Film_AddSample(sampleX, sampleY, &sampleResult[0], PARAM_AA_SAMPLES * PARAM_AA_SAMPLES
			FILM_PARAM);

	taskStat[0].raysCount = totalRaysCount;
#endif
}
