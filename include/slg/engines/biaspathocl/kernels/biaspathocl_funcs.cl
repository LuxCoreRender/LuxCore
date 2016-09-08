#line 2 "biaspathocl_funcs.cl"

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

#if defined(RENDER_ENGINE_RTBIASPATHOCL)

// Morton decode from https://fgiesen.wordpress.com/2009/12/13/decoding-morton-codes/

// Inverse of Part1By1 - "delete" all odd-indexed bits

uint Compact1By1(uint x) {
	x &= 0x55555555;					// x = -f-e -d-c -b-a -9-8 -7-6 -5-4 -3-2 -1-0
	x = (x ^ (x >> 1)) & 0x33333333;	// x = --fe --dc --ba --98 --76 --54 --32 --10
	x = (x ^ (x >> 2)) & 0x0f0f0f0f;	// x = ---- fedc ---- ba98 ---- 7654 ---- 3210
	x = (x ^ (x >> 4)) & 0x00ff00ff;	// x = ---- ---- fedc ba98 ---- ---- 7654 3210
	x = (x ^ (x >> 8)) & 0x0000ffff;	// x = ---- ---- ---- ---- fedc ba98 7654 3210
	return x;
}

// Inverse of Part1By2 - "delete" all bits not at positions divisible by 3

uint Compact1By2(uint x) {
	x &= 0x09249249;					// x = ---- 9--8 --7- -6-- 5--4 --3- -2-- 1--0
	x = (x ^ (x >> 2)) & 0x030c30c3;	// x = ---- --98 ---- 76-- --54 ---- 32-- --10
	x = (x ^ (x >> 4)) & 0x0300f00f;	// x = ---- --98 ---- ---- 7654 ---- ---- 3210
	x = (x ^ (x >> 8)) & 0xff0000ff;	// x = ---- --98 ---- ---- ---- ---- 7654 3210
	x = (x ^ (x >> 16)) & 0x000003ff;	// x = ---- ---- ---- ---- ---- --98 7654 3210
	return x;
}

uint DecodeMorton2X(const uint code) {
	return Compact1By1(code >> 0);
}

uint DecodeMorton2Y(const uint code) {
	return Compact1By1(code >> 1);
}

//------------------------------------------------------------------------------
// RTBIASPATHOCL preview phase
//------------------------------------------------------------------------------

uint RT_PreviewResolutionReduction(const uint pass) {
	return max(1, PARAM_RTBIASPATHOCL_PREVIEW_RESOLUTION_REDUCTION >>
			min(pass / PARAM_RTBIASPATHOCL_PREVIEW_RESOLUTION_REDUCTION_STEP, 16u));
}

bool RT_IsPreview(const uint pass) {
	const uint previewResolutionReduction = RT_PreviewResolutionReduction(pass);

	return (previewResolutionReduction >= PARAM_RTBIASPATHOCL_RESOLUTION_REDUCTION);
}

void RT_GetSampleXY_PreviewResolutionReductionPhase(const uint pass,
		const uint tileTotalWidth,
		uint *samplePixelX, uint *samplePixelY) {
	const size_t gid = get_global_id(0);
	const uint previewResolutionReduction = RT_PreviewResolutionReduction(pass);

	const uint samplePixelIndex = gid * previewResolutionReduction;
	*samplePixelX = samplePixelIndex % tileTotalWidth;
	*samplePixelY = samplePixelIndex / tileTotalWidth * previewResolutionReduction;
}

void RT_GetSampleResultIndex_PreviewResolutionReductionPhase(const uint pass,
		const uint tileTotalWidth,
		const uint pixelX, const uint pixelY, uint *index) {
	const uint previewResolutionReduction = RT_PreviewResolutionReduction(pass);

	*index = pixelX / previewResolutionReduction +
			(pixelY / previewResolutionReduction) * (tileTotalWidth / previewResolutionReduction);
}

//------------------------------------------------------------------------------
// RTBIASPATHOCL normal phase
//------------------------------------------------------------------------------

void RT_GetSampleXY_ResolutionReductionPhase(const uint pass, const uint index,
		const uint tileTotalWidth, const uint tileTotalHeight,
		uint *samplePixelX, uint *samplePixelY) {
	// Rendering according a Morton curve
	const uint step = pass % (PARAM_RTBIASPATHOCL_RESOLUTION_REDUCTION * PARAM_RTBIASPATHOCL_RESOLUTION_REDUCTION);
	const uint mortonX = DecodeMorton2X(step);
	const uint mortonY = DecodeMorton2Y(step);

	const uint samplePixelIndex = index * PARAM_RTBIASPATHOCL_RESOLUTION_REDUCTION;

	*samplePixelX = samplePixelIndex % tileTotalWidth +
			mortonX;
	*samplePixelY = samplePixelIndex / tileTotalWidth * PARAM_RTBIASPATHOCL_RESOLUTION_REDUCTION +
			mortonY;
}

bool RT_GetSampleResultIndex_ResolutionReductionPhase(const uint pass,
		const uint tileTotalWidth, const uint tileTotalHeight,
		const uint pixelX, const uint pixelY,
		uint *index) {
	*index = pixelX / PARAM_RTBIASPATHOCL_RESOLUTION_REDUCTION +
			(pixelY / PARAM_RTBIASPATHOCL_RESOLUTION_REDUCTION) * (tileTotalWidth / PARAM_RTBIASPATHOCL_RESOLUTION_REDUCTION);

	uint samplePixelX, samplePixelY;
	RT_GetSampleXY_ResolutionReductionPhase(pass, *index, tileTotalWidth, tileTotalHeight, &samplePixelX, &samplePixelY);

	// Check if it is one of the pixel rendered during this pass
	return ((pixelX == samplePixelX) && (pixelY == samplePixelY));
}

//------------------------------------------------------------------------------
// RTBIASPATHOCL long run phase
//------------------------------------------------------------------------------

#if (PARAM_RTBIASPATHOCL_LONGRUN_RESOLUTION_REDUCTION_STEP > 0)

uint RT_IsLongRun(const uint pass) {
	return (pass >=
			(1 << PARAM_RTBIASPATHOCL_PREVIEW_RESOLUTION_REDUCTION_STEP) +
			PARAM_RTBIASPATHOCL_RESOLUTION_REDUCTION * PARAM_RTBIASPATHOCL_RESOLUTION_REDUCTION *
				PARAM_RTBIASPATHOCL_LONGRUN_RESOLUTION_REDUCTION_STEP);
}

void RT_GetSampleXY_LongRunResolutionReductionPhase(const uint pass, const uint index,
		const uint tileTotalWidth, const uint tileTotalHeight,
		uint *samplePixelX, uint *samplePixelY) {
	// Rendering according a Morton curve
	const uint step = pass % (PARAM_RTBIASPATHOCL_LONGRUN_RESOLUTION_REDUCTION * PARAM_RTBIASPATHOCL_LONGRUN_RESOLUTION_REDUCTION);
	const uint mortonX = DecodeMorton2X(step);
	const uint mortonY = DecodeMorton2Y(step);

	const uint samplePixelIndex = index * PARAM_RTBIASPATHOCL_LONGRUN_RESOLUTION_REDUCTION;

	*samplePixelX = samplePixelIndex % tileTotalWidth +
			mortonX;
	*samplePixelY = samplePixelIndex / tileTotalWidth * PARAM_RTBIASPATHOCL_LONGRUN_RESOLUTION_REDUCTION +
			mortonY;
}

bool RT_GetSampleResultIndex_LongRunResolutionReductionPhase(const uint pass,
		const uint tileTotalWidth, const uint tileTotalHeight,
		const uint pixelX, const uint pixelY,
		uint *index) {
	*index = pixelX / PARAM_RTBIASPATHOCL_LONGRUN_RESOLUTION_REDUCTION +
			(pixelY / PARAM_RTBIASPATHOCL_LONGRUN_RESOLUTION_REDUCTION) * (tileTotalWidth / PARAM_RTBIASPATHOCL_LONGRUN_RESOLUTION_REDUCTION);

	uint samplePixelX, samplePixelY;
	RT_GetSampleXY_LongRunResolutionReductionPhase(pass, *index, tileTotalWidth, tileTotalHeight, &samplePixelX, &samplePixelY);

	// Check if it is one of the pixel rendered during this pass
	return ((pixelX == samplePixelX) && (pixelY == samplePixelY));
}
#endif

#endif

void GetSampleXY(const uint pass, const uint tileTotalWidth, const uint tileTotalHeight,
		uint *samplePixelX, uint *samplePixelY, uint *sampleIndex) {
	const size_t gid = get_global_id(0);

#if defined(RENDER_ENGINE_RTBIASPATHOCL)
	// RTBIASPATHOCL renders first passes at a lower resolution:
	//   PARAM_RTBIASPATHOCL_PREVIEW_RESOLUTION_REDUCTION x PARAM_RTBIASPATHOCL_PREVIEW_RESOLUTION_REDUCTION
	// than renders one sample every:
	//   PARAM_RTBIASPATHOCL_RESOLUTION_REDUCTION x PARAM_RTBIASPATHOCL_RESOLUTION_REDUCTION
	// Than (optionally) renders:
	//   PARAM_RTBIASPATHOCL_LONGRUN_RESOLUTION_REDUCTION x PARAM_RTBIASPATHOCL_LONGRUN_RESOLUTION_REDUCTION

	if (RT_IsPreview(pass))
		RT_GetSampleXY_PreviewResolutionReductionPhase(pass, tileTotalWidth, samplePixelX, samplePixelY);
	else {
#if (PARAM_RTBIASPATHOCL_LONGRUN_RESOLUTION_REDUCTION_STEP > 0)
		if (RT_IsLongRun(pass))
			RT_GetSampleXY_LongRunResolutionReductionPhase(pass, gid, tileTotalWidth, tileTotalHeight, samplePixelX, samplePixelY);
		else
#endif
			RT_GetSampleXY_ResolutionReductionPhase(pass, gid, tileTotalWidth, tileTotalHeight, samplePixelX, samplePixelY);
	}

	*sampleIndex = 0;
#else
	// Normal BIASPATHOCL
	*sampleIndex = gid % (PARAM_AA_SAMPLES * PARAM_AA_SAMPLES);
	const uint samplePixelIndex = gid / (PARAM_AA_SAMPLES * PARAM_AA_SAMPLES);
	*samplePixelX = samplePixelIndex % tileTotalWidth;
	*samplePixelY = samplePixelIndex / tileTotalWidth;
#endif
}

bool GetSampleResultIndex(const uint pass, const uint tileTotalWidth, const uint tileTotalHeight,
		const uint pixelX, const uint pixelY, uint *index) {
#if defined(RENDER_ENGINE_RTBIASPATHOCL)
	if (RT_IsPreview(pass)) {
		RT_GetSampleResultIndex_PreviewResolutionReductionPhase(pass,
				tileTotalWidth, pixelX, pixelY, index);
		return true;
	} else {
#if (PARAM_RTBIASPATHOCL_LONGRUN_RESOLUTION_REDUCTION_STEP > 0)
		if (RT_IsLongRun(pass)) {
			return RT_GetSampleResultIndex_LongRunResolutionReductionPhase(pass,
					tileTotalWidth, tileTotalHeight, pixelX, pixelY, index);
		} else
#endif
		{
			return RT_GetSampleResultIndex_ResolutionReductionPhase(pass,
					tileTotalWidth, tileTotalHeight, pixelX, pixelY, index);
		}
	}
#else
	// Normal BIASPATHOCL
	const size_t gid = get_global_id(0);
	*index = gid * PARAM_AA_SAMPLES * PARAM_AA_SAMPLES;

	return true;
#endif	
}

void SampleGrid(Seed *seed, const uint size,
		const uint ix, const uint iy, float *u0, float *u1) {
	*u0 = Rnd_FloatValue(seed);
	*u1 = Rnd_FloatValue(seed);

	// RTBIASPATHOCL uses a plain random sampler
#if !defined(RENDER_ENGINE_RTBIASPATHOCL)
	if (size > 1) {
		const float idim = 1.f / size;
		*u0 = (ix + *u0) * idim;
		*u1 = (iy + *u1) * idim;
	}
#endif
}

//------------------------------------------------------------------------------

void GenerateCameraRay(
		Seed *seed,
		__global GPUTask *task,
		__global SampleResult *sampleResult,
		__global const Camera* restrict camera,
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

	// Sample according the pixel filter distribution
	FilterDistribution_SampleContinuous(pixelFilterDistribution, u0, u1, &u0, &u1);

	const float filmX = sampleX + .5f + u0;
	const float filmY = sampleY + .5f + u1;
	sampleResult->filmX = filmX;
	sampleResult->filmY = filmY;

	const float dofSampleX = Rnd_FloatValue(seed);
	const float dofSampleY = Rnd_FloatValue(seed);

	Camera_GenerateRay(camera, engineFilmWidth, engineFilmHeight, ray,
			tileStartX + filmX, tileStartY + filmY, Rnd_FloatValue(seed),
			dofSampleX, dofSampleY);
}

//------------------------------------------------------------------------------

uint BIASPATHOCL_Scene_Intersect(
#if defined(PARAM_HAS_VOLUMES)
		__global PathVolumeInfo *volInfo,
		__global HitPoint *tmpHitPoint,
#endif
#if defined(PARAM_HAS_PASSTHROUGH)
		const float initialPassThrough,
#endif
#if !defined(RENDER_ENGINE_BIASPATHOCL) && !defined(RENDER_ENGINE_RTBIASPATHOCL)
		__global
#endif
		Ray *ray,
#if !defined(RENDER_ENGINE_BIASPATHOCL) && !defined(RENDER_ENGINE_RTBIASPATHOCL)
		__global
#endif
		RayHit *rayHit,
		__global BSDF *bsdf,
		float3 *connectionThroughput,  const float3 pathThroughput,
		__global SampleResult *sampleResult,
		// BSDF_Init parameters
		__global const Mesh* restrict meshDescs,
		__global const SceneObject* restrict sceneObjs,
		__global const uint *meshTriLightDefsOffset,
		__global const Point* restrict vertices,
		__global const Vector *vertNormals,
		__global const UV* restrict vertUVs,
		__global const Spectrum* restrict vertCols,
		__global const float* restrict vertAlphas,
		__global const Triangle* restrict triangles
		MATERIALS_PARAM_DECL
		// Accelerator_Intersect parameters
		ACCELERATOR_INTERSECT_PARAM_DECL
		) {
	*connectionThroughput = WHITE;
	uint tracedRaysCount = 0;
#if defined(PARAM_HAS_PASSTHROUGH)
	float passThrough = initialPassThrough;
#endif

	for (;;) {
		Accelerator_Intersect(ray, rayHit
			ACCELERATOR_INTERSECT_PARAM);
		++tracedRaysCount;

		float3 connectionSegmentThroughput;
		const bool continueToTrace = Scene_Intersect(
#if defined(PARAM_HAS_VOLUMES)
			volInfo,
			tmpHitPoint,
#endif
#if defined(PARAM_HAS_PASSTHROUGH)
			passThrough,
#endif
			ray, rayHit, bsdf,
			&connectionSegmentThroughput, pathThroughput * (*connectionThroughput),
			sampleResult,
			// BSDF_Init parameters
			meshDescs,
			sceneObjs,
			meshTriLightDefsOffset,
			vertices,
			vertNormals,
			vertUVs,
			vertCols,
			vertAlphas,
			triangles
			MATERIALS_PARAM
			);
		*connectionThroughput *= connectionSegmentThroughput;
		if (!continueToTrace)
			break;

#if defined(PARAM_HAS_PASSTHROUGH)
		// I generate a new random variable starting from the previous one. I'm
		// not really sure about the kind of correlation introduced by this
		// trick.
		passThrough = fabs(passThrough - .5f) * 2.f;
#endif
	}
	
	return tracedRaysCount;
}

//------------------------------------------------------------------------------
// Direct hit  on lights
//------------------------------------------------------------------------------

void DirectHitFiniteLight(
		const BSDFEvent lastBSDFEvent,
		const float3 pathThroughput, const float distance, __global BSDF *bsdf,
		const float lastPdfW, __global SampleResult *sampleResult
		LIGHTS_PARAM_DECL) {
	if (sampleResult->firstPathVertex ||
			(lights[bsdf->triangleLightSourceIndex].visibility &
			(sampleResult->firstPathVertexEvent & (DIFFUSE | GLOSSY | SPECULAR)))) {
		float directPdfA;
		const float3 emittedRadiance = BSDF_GetEmittedRadiance(bsdf, &directPdfA
				LIGHTS_PARAM);

		if (!Spectrum_IsBlack(emittedRadiance)) {
			// Add emitted radiance
			float weight = 1.f;
			if (!(lastBSDFEvent & SPECULAR)) {
				const float lightPickProb = Scene_SampleAllLightPdf(lightsDistribution,
						lights[bsdf->triangleLightSourceIndex].lightSceneIndex);
				const float directPdfW = PdfAtoW(directPdfA, distance,
					fabs(dot(VLOAD3F(&bsdf->hitPoint.fixedDir.x), VLOAD3F(&bsdf->hitPoint.shadeN.x))));

				// MIS between BSDF sampling and direct light sampling
				weight = PowerHeuristic(lastPdfW, directPdfW * lightPickProb);
			}

			SampleResult_AddEmission(sampleResult, BSDF_GetLightID(bsdf
					MATERIALS_PARAM), pathThroughput, weight * emittedRadiance);
		}
	}
}

#if defined(PARAM_HAS_ENVLIGHTS)
void DirectHitInfiniteLight(
		const BSDFEvent lastBSDFEvent,
		const float3 pathThroughput,
		const float3 eyeDir, const float lastPdfW,
		__global SampleResult *sampleResult
		LIGHTS_PARAM_DECL) {
	for (uint i = 0; i < envLightCount; ++i) {
		__global const LightSource* restrict light = &lights[envLightIndices[i]];

		if (sampleResult->firstPathVertex || (light->visibility & (sampleResult->firstPathVertexEvent & (DIFFUSE | GLOSSY | SPECULAR)))) {
			float directPdfW;
			const float3 envRadiance = EnvLight_GetRadiance(light, eyeDir, &directPdfW
					LIGHTS_PARAM);

			if (!Spectrum_IsBlack(envRadiance)) {
				// MIS between BSDF sampling and direct light sampling
				const float lightPickProb = Scene_SampleAllLightPdf(lightsDistribution, light->lightSceneIndex);
				const float weight = ((lastBSDFEvent & SPECULAR) ? 1.f : PowerHeuristic(lastPdfW, directPdfW * lightPickProb));

				SampleResult_AddEmission(sampleResult, light->lightID, pathThroughput, weight * envRadiance);
			}
		}
	}
}
#endif

//------------------------------------------------------------------------------
// Direct light sampling
//------------------------------------------------------------------------------

bool DirectLightSamplingInit(
		__global const LightSource* restrict light,
		const float lightPickPdf,
		const float worldCenterX,
		const float worldCenterY,
		const float worldCenterZ,
		const float worldRadius,
		__global HitPoint *tmpHitPoint,
		const float time, const float u0, const float u1,
#if defined(PARAM_HAS_PASSTHROUGH)
		const float passThroughEvent,
#endif
		__global BSDF *bsdf, BSDFEvent *event,
		__global SampleResult *sampleResult,
		Ray *shadowRay,
		float3 *radiance,
#if defined(PARAM_FILM_CHANNELS_HAS_IRRADIANCE)
		float3 *irradiance,
#endif
		uint *ID
		LIGHTS_PARAM_DECL) {
	float3 lightRayDir;
	float distance, directPdfW;
	const float3 lightRadiance = Light_Illuminate(
			light,
			VLOAD3F(&bsdf->hitPoint.p.x),
			u0, u1,
#if defined(PARAM_HAS_PASSTHROUGH)
			passThroughEvent,
#endif
			worldCenterX, worldCenterY, worldCenterZ, worldRadius,
			tmpHitPoint,		
			&lightRayDir, &distance, &directPdfW
			LIGHTS_PARAM);

	// Setup the shadow ray
	const float cosThetaToLight = fabs(dot(lightRayDir, VLOAD3F(&bsdf->hitPoint.shadeN.x)));
	if (((Spectrum_Y(lightRadiance) * cosThetaToLight / directPdfW) > PARAM_LOW_LIGHT_THREASHOLD) &&
			(distance > PARAM_NEAR_START_LIGHT)) {
		float bsdfPdfW;
		const float3 bsdfEval = BSDF_Evaluate(bsdf,
				lightRayDir, event, &bsdfPdfW
				MATERIALS_PARAM);

		if (!Spectrum_IsBlack(bsdfEval)) {
			const float directLightSamplingPdfW = directPdfW * lightPickPdf;
			const float factor = 1.f / directLightSamplingPdfW;

			// MIS between direct light sampling and BSDF sampling
			const float weight = (!sampleResult->lastPathVertex && Light_IsEnvOrIntersectable(light)) ?
				PowerHeuristic(directLightSamplingPdfW, bsdfPdfW) : 1.f;

			*radiance = bsdfEval * (weight * factor) * lightRadiance;
#if defined(PARAM_FILM_CHANNELS_HAS_IRRADIANCE)
			*irradiance = factor * lightRadiance;
#endif
			*ID = light->lightID;

			// Setup the shadow ray
			const float3 hitPoint = VLOAD3F(&bsdf->hitPoint.p.x);
			Ray_Init4_Private(shadowRay, hitPoint, lightRayDir, 0.f, distance, time);

			return true;
		}
	}

	return false;
}

uint DirectLightSampling_ONE(
		Seed *seed,
#if defined(PARAM_HAS_VOLUMES)
		__global PathVolumeInfo *volInfo,
#endif
		__global HitPoint *tmpHitPoint,
		const float worldCenterX,
		const float worldCenterY,
		const float worldCenterZ,
		const float worldRadius,
		const float time,
		const float3 pathThroughput,
		__global BSDF *bsdf, __global BSDF *directLightBSDF,
		__global SampleResult *sampleResult,
		// BSDF_Init parameters
		__global const Mesh* restrict meshDescs,
		__global const SceneObject* restrict sceneObjs,
		__global const Point* restrict vertices,
		__global const Vector* restrict vertNormals,
		__global const UV* restrict vertUVs,
		__global const Spectrum* restrict vertCols,
		__global const float* restrict vertAlphas,
		__global const Triangle* restrict triangles,
		bool *isLightVisible

		// Accelerator_Intersect parameters
		ACCELERATOR_INTERSECT_PARAM_DECL
		// Light related parameters
		LIGHTS_PARAM_DECL) {
	// ONE direct light sampling strategy

	*isLightVisible = false;

	// Select the light strategy to use
	__global const float* restrict lightDist = BSDF_IsShadowCatcherOnlyInfiniteLights(bsdf MATERIALS_PARAM) ?
		infiniteLightSourcesDistribution : lightsDistribution;

	// Pick a light source to sample
	float lightPickPdf;
	const uint lightIndex = Scene_SampleAllLights(lightDist, Rnd_FloatValue(seed), &lightPickPdf);

	Ray shadowRay;
	uint lightID;
	BSDFEvent event;
	float3 lightRadiance;
#if defined(PARAM_FILM_CHANNELS_HAS_IRRADIANCE)
	float3 lightIrradiance;
#endif
	const bool illuminated = DirectLightSamplingInit(
		&lights[lightIndex],
		lightPickPdf,
		worldCenterX,
		worldCenterY,
		worldCenterZ,
		worldRadius,
		tmpHitPoint,
		time, Rnd_FloatValue(seed), Rnd_FloatValue(seed),
#if defined(PARAM_HAS_PASSTHROUGH)
		Rnd_FloatValue(seed),
#endif
		bsdf, &event, sampleResult,
		&shadowRay, &lightRadiance,
#if defined(PARAM_FILM_CHANNELS_HAS_IRRADIANCE)
		&lightIrradiance,
#endif
		&lightID
		LIGHTS_PARAM);

	uint tracedRaysCount = 0;
	if (illuminated) {
		// Trace the shadow ray

		float3 connectionThroughput;
		RayHit shadowRayHit;
		tracedRaysCount += BIASPATHOCL_Scene_Intersect(
#if defined(PARAM_HAS_VOLUMES)
				volInfo,
				tmpHitPoint,
#endif
#if defined(PARAM_HAS_PASSTHROUGH)
				Rnd_FloatValue(seed),
#endif
				&shadowRay, &shadowRayHit,
				directLightBSDF,
				&connectionThroughput, pathThroughput,
				sampleResult,
				// BSDF_Init parameters
				meshDescs,
				sceneObjs,
				meshTriLightDefsOffset,
				vertices,
				vertNormals,
				vertUVs,
				vertCols,
				vertAlphas,
				triangles
				MATERIALS_PARAM
				// Accelerator_Intersect parameters
				ACCELERATOR_INTERSECT_PARAM
				);

		if (shadowRayHit.meshIndex == NULL_INDEX) {
			// Nothing was hit, the light source is visible
			
			if (!BSDF_IsShadowCatcher(bsdf MATERIALS_PARAM)) {
				SampleResult_AddDirectLight(sampleResult, lightID, event, pathThroughput,
						connectionThroughput * lightRadiance, 1.f);

#if defined(PARAM_FILM_CHANNELS_HAS_IRRADIANCE)
				// The first path vertex is not handled by AddDirectLight(). This is valid
				// for irradiance AOV only if it is not a SPECULAR material and first path vertex
				if ((sampleResult->firstPathVertex) && !(BSDF_GetEventTypes(bsdf
							MATERIALS_PARAM) & SPECULAR))
					VSTORE3F(VLOAD3F(sampleResult->irradiance.c) +
							M_1_PI_F * fabs(dot(VLOAD3F(&bsdf->hitPoint.shadeN.x),
							(float3)(shadowRay.d.x, shadowRay.d.y, shadowRay.d.z))) *
								connectionThroughput * lightIrradiance,
							sampleResult->irradiance.c);
#endif
			}

			*isLightVisible = true;
		}
	}

	return tracedRaysCount;
}

uint DirectLightSampling_ALL(
		Seed *seed,
#if defined(PARAM_HAS_VOLUMES)
		__global PathVolumeInfo *volInfo,
#endif
		__global HitPoint *tmpHitPoint,
		const float worldCenterX,
		const float worldCenterY,
		const float worldCenterZ,
		const float worldRadius,
		const float time,
		const float3 pathThroughput,
		__global BSDF *bsdf, __global BSDF *directLightBSDF,
		__global SampleResult *sampleResult,
		// BSDF_Init parameters
		__global const Mesh* restrict meshDescs,
		__global const SceneObject* restrict sceneObjs,
		__global const Point* restrict vertices,
		__global const Vector* restrict vertNormals,
		__global const UV* restrict vertUVs,
		__global const Spectrum* restrict vertCols,
		__global const float* restrict vertAlphas,
		__global const Triangle* restrict triangles,
		float *lightsVisibility
		// Accelerator_Intersect parameters
		ACCELERATOR_INTERSECT_PARAM_DECL
		// Light related parameters
		LIGHTS_PARAM_DECL) {
	// Select the light strategy to use
	__global const float* restrict lightDist = BSDF_IsShadowCatcherOnlyInfiniteLights(bsdf MATERIALS_PARAM) ?
		infiniteLightSourcesDistribution : lightsDistribution;

	uint tracedRaysCount = 0;
	*lightsVisibility = 0.f;
	uint totalSampleCount = 0;

	for (uint samples = 0; samples < PARAM_FIRST_VERTEX_DL_COUNT; ++samples) {
		// Pick a light source to sample
		float lightPickPdf;
		const uint lightIndex = Scene_SampleAllLights(lightsDistribution, Rnd_FloatValue(seed), &lightPickPdf);

		__global const LightSource* restrict light = &lights[lightIndex];
		const int lightSamplesCount = light->samples;
		const uint sampleCount = (lightSamplesCount < 0) ? PARAM_DIRECT_LIGHT_SAMPLES : (uint)lightSamplesCount;
		const uint sampleCount2 = sampleCount * sampleCount;

		const float scaleFactor = 1.f / (sampleCount2 * PARAM_FIRST_VERTEX_DL_COUNT);
		for (uint currentLightSampleIndex = 0; currentLightSampleIndex < sampleCount2; ++currentLightSampleIndex) {
			float u0, u1;
			SampleGrid(seed, sampleCount,
					currentLightSampleIndex % sampleCount, currentLightSampleIndex / sampleCount,
					&u0, &u1);

			Ray shadowRay;
			float3 lightRadiance;
#if defined(PARAM_FILM_CHANNELS_HAS_IRRADIANCE)
			float3 lightIrradiance;
#endif
			uint lightID;
			BSDFEvent event;
			const bool illuminated = DirectLightSamplingInit(
				light,
				lightPickPdf,
				worldCenterX,
				worldCenterY,
				worldCenterZ,
				worldRadius,
				tmpHitPoint,
				time, u0, u1,
#if defined(PARAM_HAS_PASSTHROUGH)
				Rnd_FloatValue(seed),
#endif
				bsdf, &event, sampleResult,
				&shadowRay, &lightRadiance,
#if defined(PARAM_FILM_CHANNELS_HAS_IRRADIANCE)
				&lightIrradiance,
#endif
				&lightID
				LIGHTS_PARAM);

			if (illuminated) {
				// Trace the shadow ray

				float3 connectionThroughput;
				RayHit shadowRayHit;
				tracedRaysCount += BIASPATHOCL_Scene_Intersect(
#if defined(PARAM_HAS_VOLUMES)
						volInfo,
						tmpHitPoint,
#endif
#if defined(PARAM_HAS_PASSTHROUGH)
						Rnd_FloatValue(seed),
#endif
						&shadowRay, &shadowRayHit,
						directLightBSDF,
						&connectionThroughput, pathThroughput,
						sampleResult,
						// BSDF_Init parameters
						meshDescs,
						sceneObjs,
						meshTriLightDefsOffset,
						vertices,
						vertNormals,
						vertUVs,
						vertCols,
						vertAlphas,
						triangles
						MATERIALS_PARAM
						// Accelerator_Intersect parameters
						ACCELERATOR_INTERSECT_PARAM
						);

				if (shadowRayHit.meshIndex == NULL_INDEX) {
					// Nothing was hit, the light source is visible

					if (!BSDF_IsShadowCatcher(bsdf MATERIALS_PARAM)) {
						// Nothing was hit, the light source is visible
						const float3 incomingRadiance = scaleFactor * connectionThroughput * lightRadiance;
						SampleResult_AddDirectLight(sampleResult, lightID, event,
								pathThroughput, incomingRadiance, scaleFactor);

#if defined(PARAM_FILM_CHANNELS_HAS_IRRADIANCE)
						// The first path vertex is not handled by AddDirectLight(). This is valid
						// for irradiance AOV only if it is not a SPECULAR material and first path vertex
						if ((sampleResult->firstPathVertex) && !(BSDF_GetEventTypes(bsdf
									MATERIALS_PARAM) & SPECULAR))
							VSTORE3F(VLOAD3F(sampleResult->irradiance.c) +
									(M_1_PI_F * fabs(dot(VLOAD3F(&bsdf->hitPoint.shadeN.x),
									(float3)(shadowRay.d.x, shadowRay.d.y, shadowRay.d.z))) * scaleFactor) *
										connectionThroughput * lightIrradiance,
									sampleResult->irradiance.c);
#endif
					}

					*lightsVisibility += 1.f;
				}
			}
		}

		totalSampleCount += sampleCount2;
	}
	
	*lightsVisibility /= totalSampleCount;

	return tracedRaysCount;
}

//------------------------------------------------------------------------------
// Indirect light sampling
//------------------------------------------------------------------------------

uint ContinueTracePath(
		Seed *seed,
#if defined(PARAM_HAS_VOLUMES)
		__global PathVolumeInfo *volInfoPathVertexN,
		__global PathVolumeInfo *directLightVolInfo,
#endif
		__global HitPoint *tmpHitPoint,
		const float worldCenterX,
		const float worldCenterY,
		const float worldCenterZ,
		const float worldRadius,
		PathDepthInfo *depthInfo,
		Ray *ray,
		const float time,
		float3 pathThroughput,
		BSDFEvent lastBSDFEvent, float lastPdfW,
		__global BSDF *bsdfPathVertexN, __global BSDF *directLightBSDF,
		__global SampleResult *sampleResult,
		// BSDF_Init parameters
		__global const Mesh* restrict meshDescs,
		__global const SceneObject* restrict sceneObjs,
		__global const Point* restrict vertices,
		__global const Vector* restrict vertNormals,
		__global const UV* restrict vertUVs,
		__global const Spectrum* restrict vertCols,
		__global const float* restrict vertAlphas,
		__global const Triangle* restrict triangles
		// Accelerator_Intersect parameters
		ACCELERATOR_INTERSECT_PARAM_DECL
		// Light related parameters
		LIGHTS_PARAM_DECL) {
	uint tracedRaysCount = 0;

	for (;;) {
		//----------------------------------------------------------------------
		// Trace the ray
		//----------------------------------------------------------------------

		float3 connectionThroughput;
		RayHit rayHit;
		tracedRaysCount += BIASPATHOCL_Scene_Intersect(
#if defined(PARAM_HAS_VOLUMES)
			volInfoPathVertexN,
			tmpHitPoint,
#endif
#if defined(PARAM_HAS_PASSTHROUGH)
			Rnd_FloatValue(seed),
#endif
			ray, &rayHit,
			bsdfPathVertexN,
			&connectionThroughput, pathThroughput,
			sampleResult,
			// BSDF_Init parameters
			meshDescs,
			sceneObjs,
			meshTriLightDefsOffset,
			vertices,
			vertNormals,
			vertUVs,
			vertCols,
			vertAlphas,
			triangles
			MATERIALS_PARAM
			// Accelerator_Intersect parameters
			ACCELERATOR_INTERSECT_PARAM);
		pathThroughput *= connectionThroughput;
#if defined(PARAM_FILM_CHANNELS_HAS_IRRADIANCE)
		// Update also irradiance AOV path throughput
		VSTORE3F(VLOAD3F(sampleResult->irradiancePathThroughput.c) * connectionThroughput, sampleResult->irradiancePathThroughput.c);
#endif

		if (rayHit.meshIndex == NULL_INDEX) {
			// Nothing was hit, look for env. lights

#if defined(PARAM_HAS_ENVLIGHTS)
#if defined(PARAM_FORCE_BLACK_BACKGROUND)
			if (!sampleResult->passThroughPath) {
#endif
				// Add environmental lights radiance
				const float3 rayDir = (float3)(ray->d.x, ray->d.y, ray->d.z);
				DirectHitInfiniteLight(
						lastBSDFEvent,
						pathThroughput,
						-rayDir, lastPdfW,
						sampleResult
						LIGHTS_PARAM);
#if defined(PARAM_FORCE_BLACK_BACKGROUND)
			}
#endif
#endif

			break;
		}

		// Something was hit

		// Check if it is visible in indirect paths
		if (!(mats[bsdfPathVertexN->materialIndex].visibility &
				(sampleResult->firstPathVertexEvent & (DIFFUSE | GLOSSY | SPECULAR))))
			break;

		// Check if it is a light source (note: I can hit only triangle area light sources)
		if (BSDF_IsLightSource(bsdfPathVertexN) && (rayHit.t > PARAM_NEAR_START_LIGHT)) {
			DirectHitFiniteLight(lastBSDFEvent,
					pathThroughput,
					rayHit.t, bsdfPathVertexN, lastPdfW,
					sampleResult
					LIGHTS_PARAM);
		}

		//------------------------------------------------------------------
		// Direct light sampling
		//------------------------------------------------------------------

		// I avoid to do DL on the last vertex otherwise it introduces a lot of
		// noise because I can not use MIS
		sampleResult->lastPathVertex = PathDepthInfo_IsLastPathVertex(depthInfo,
				BSDF_GetEventTypes(bsdfPathVertexN
					MATERIALS_PARAM));
		if (sampleResult->lastPathVertex)
			break;

		bool isLightVisible = false;

		// Only if it is not a SPECULAR BSDF
		if (!BSDF_IsDelta(bsdfPathVertexN
				MATERIALS_PARAM)) {
#if defined(PARAM_HAS_VOLUMES)
			// I need to work on a copy of volume information of the path vertex
			*directLightVolInfo = *volInfoPathVertexN;
#endif

			tracedRaysCount += DirectLightSampling_ONE(
				seed,
#if defined(PARAM_HAS_VOLUMES)
				directLightVolInfo,
#endif
				tmpHitPoint,
				worldCenterX, worldCenterY, worldCenterZ, worldRadius,
				time,
				pathThroughput,
				bsdfPathVertexN, directLightBSDF,
				sampleResult,
				// BSDF_Init parameters
				meshDescs,
				sceneObjs,
				vertices,
				vertNormals,
				vertUVs,
				vertCols,
				vertAlphas,
				triangles,
				&isLightVisible
				// Accelerator_Intersect parameters
				ACCELERATOR_INTERSECT_PARAM
				// Light related parameters
				LIGHTS_PARAM);
		}

		//------------------------------------------------------------------
		// Build the next path vertex ray
		//------------------------------------------------------------------

		float3 sampledDir;
		float cosSampledDir;
		float3 bsdfSample;

		if (BSDF_IsShadowCatcher(bsdfPathVertexN MATERIALS_PARAM) && isLightVisible) {
			bsdfSample = BSDF_ShadowCatcherSample(bsdfPathVertexN,
					&sampledDir, &lastPdfW, &cosSampledDir, &lastBSDFEvent
					MATERIALS_PARAM);
		} else {
			bsdfSample = BSDF_Sample(bsdfPathVertexN,
					Rnd_FloatValue(seed),
					Rnd_FloatValue(seed),
					&sampledDir, &lastPdfW, &cosSampledDir, &lastBSDFEvent,
					ALL
					MATERIALS_PARAM);
			sampleResult->passThroughPath = false;
		}

		if (Spectrum_IsBlack(bsdfSample))
			break;

		// Increment path depth informations
		PathDepthInfo_IncDepths(depthInfo, lastBSDFEvent);

		// Update volume information
#if defined(PARAM_HAS_VOLUMES)
		PathVolumeInfo_Update(volInfoPathVertexN, lastBSDFEvent, bsdfPathVertexN
				MATERIALS_PARAM);
#endif

		// Continue to trace the path
		const float3 throughputFactor = bsdfSample *
			((lastBSDFEvent & SPECULAR) ? 1.f : min(1.f, lastPdfW / PARAM_PDF_CLAMP_VALUE));
		pathThroughput *= throughputFactor;
#if defined(PARAM_FILM_CHANNELS_HAS_IRRADIANCE)
		// Update also irradiance AOV path throughput
		VSTORE3F(VLOAD3F(sampleResult->irradiancePathThroughput.c) * throughputFactor, sampleResult->irradiancePathThroughput.c);
#endif

		Ray_Init2_Private(ray, VLOAD3F(&bsdfPathVertexN->hitPoint.p.x), sampledDir, time);
	}

	return tracedRaysCount;
}

uint SampleComponent(
		Seed *seed,
		const float lightsVisibility,
#if defined(PARAM_HAS_VOLUMES)
		__global PathVolumeInfo *volInfoPathVertex1,
		__global PathVolumeInfo *volInfoPathVertexN,
		__global PathVolumeInfo *directLightVolInfo,
#endif
		__global HitPoint *tmpHitPoint,
		const float worldCenterX,
		const float worldCenterY,
		const float worldCenterZ,
		const float worldRadius,
		const float time,
		const BSDFEvent requestedEventTypes,
		const uint size,
		const float3 throughputPathVertex1,
		__global BSDF *bsdfPathVertex1, __global BSDF *bsdfPathVertexN,
		__global BSDF *directLightBSDF,
		__global SampleResult *sampleResult,
		// BSDF_Init parameters
		__global const Mesh* restrict meshDescs,
		__global const SceneObject* restrict sceneObjs,
		__global const Point* restrict vertices,
		__global const Vector* restrict vertNormals,
		__global const UV* restrict vertUVs,
		__global const Spectrum* restrict vertCols,
		__global const float* restrict vertAlphas,
		__global const Triangle* restrict triangles
		// Accelerator_Intersect parameters
		ACCELERATOR_INTERSECT_PARAM_DECL
		// Light related parameters
		LIGHTS_PARAM_DECL) {
	uint tracedRaysCount = 0;

	const uint sampleCount = size * size;
	const float scaleFactor = 1.f / sampleCount;
#if defined(PARAM_FILM_CHANNELS_HAS_INDIRECT_SHADOW_MASK)
	float indirectShadowMask = 0.f;
#endif
	const bool passThroughPath = sampleResult->passThroughPath;
	for (uint currentBSDFSampleIndex = 0; currentBSDFSampleIndex < sampleCount; ++currentBSDFSampleIndex) {
#if defined(PARAM_FILM_CHANNELS_HAS_INDIRECT_SHADOW_MASK)
		sampleResult->indirectShadowMask = 1.f;
#endif
		sampleResult->passThroughPath = passThroughPath;

		float u0, u1;
		SampleGrid(seed, size,
			currentBSDFSampleIndex % size, currentBSDFSampleIndex / size,
			&u0, &u1);

		// Sample the BSDF on the first path vertex
		float3 sampledDir;
		float pdfW, cosSampledDir;
		BSDFEvent event;
		float3 bsdfSample;

		if (BSDF_IsShadowCatcher(bsdfPathVertex1 MATERIALS_PARAM) && (1.f - lightsVisibility <= Rnd_FloatValue(seed))) {
			bsdfSample = BSDF_ShadowCatcherSample(bsdfPathVertex1,
					&sampledDir, &pdfW, &cosSampledDir, &event
					MATERIALS_PARAM);

#if defined(PARAM_FILM_CHANNELS_HAS_ALPHA)
			// In this case I have also to set the value of the alpha channel to 0.0
			sampleResult->alpha = 0.f;
#endif
		} else {
			bsdfSample = BSDF_Sample(bsdfPathVertex1,
					u0,
					u1,
					&sampledDir, &pdfW, &cosSampledDir, &event,
					requestedEventTypes | REFLECT | TRANSMIT
					MATERIALS_PARAM);
			sampleResult->passThroughPath = false;
		}

		if (!Spectrum_IsBlack(bsdfSample)) {
			PathDepthInfo depthInfo;
			PathDepthInfo_Init(&depthInfo);
			PathDepthInfo_IncDepths(&depthInfo, event);

			// Update information about the first path BSDF 
			sampleResult->firstPathVertexEvent = event;

			// Update volume information
#if defined(PARAM_HAS_VOLUMES)
			// I need to work on a copy of volume information of the first path vertex
			*volInfoPathVertexN = *volInfoPathVertex1;
			PathVolumeInfo_Update(volInfoPathVertexN, event, bsdfPathVertexN
					MATERIALS_PARAM);
#endif

			// Continue to trace the path
			const float pdfFactor = scaleFactor * ((event & SPECULAR) ? 1.f : min(1.f, pdfW / PARAM_PDF_CLAMP_VALUE));
			const float3 continuePathThroughput = throughputPathVertex1 * bsdfSample * pdfFactor;

#if defined(PARAM_FILM_CHANNELS_HAS_IRRADIANCE)
			// This is valid for irradiance AOV only if it is not a SPECULAR material and
			// first path vertex. Set or update sampleResult.irradiancePathThroughput
			if (!(BSDF_GetEventTypes(bsdfPathVertex1
						MATERIALS_PARAM) & SPECULAR))
				VSTORE3F(M_1_PI_F * fabs(dot(
							VLOAD3F(&bsdfPathVertex1->hitPoint.shadeN.x),
							sampledDir)),
						sampleResult->irradiancePathThroughput.c);
			else
				VSTORE3F(BLACK, sampleResult->irradiancePathThroughput.c);
#endif

			Ray continueRay;
			Ray_Init2_Private(&continueRay, VLOAD3F(&bsdfPathVertex1->hitPoint.p.x), sampledDir, time);

			tracedRaysCount += ContinueTracePath(
					seed,
#if defined(PARAM_HAS_VOLUMES)
					volInfoPathVertexN,
					directLightVolInfo,
#endif
					tmpHitPoint,
					worldCenterX, worldCenterY, worldCenterZ, worldRadius,
					&depthInfo, &continueRay,
					time,
					continuePathThroughput,
					event, pdfW,
					bsdfPathVertexN, directLightBSDF,
					sampleResult,
					// BSDF_Init parameters
					meshDescs,
					sceneObjs,
					vertices,
					vertNormals,
					vertUVs,
					vertCols,
					vertAlphas,
					triangles
					// Accelerator_Intersect parameters
					ACCELERATOR_INTERSECT_PARAM
					// Light related parameters
					LIGHTS_PARAM);
		}

#if defined(PARAM_FILM_CHANNELS_HAS_INDIRECT_SHADOW_MASK)
		// sampleResult->indirectShadowMask requires special handling: the
		// end result must be the average of all path results
		indirectShadowMask += scaleFactor * sampleResult->indirectShadowMask;
#endif
	}

#if defined(PARAM_FILM_CHANNELS_HAS_INDIRECT_SHADOW_MASK)
	sampleResult->indirectShadowMask = indirectShadowMask;
#endif

	return tracedRaysCount;
}
