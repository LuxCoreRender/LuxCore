#line 2 "biaspatchocl_funcs.cl"

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

void SR_Accumulate(__global SampleResult *src, SampleResult *dst) {
#if defined(PARAM_FILM_RADIANCE_GROUP_0)
	dst->radiancePerPixelNormalized[0].c[0] += src->radiancePerPixelNormalized[0].c[0];
	dst->radiancePerPixelNormalized[0].c[1] += src->radiancePerPixelNormalized[0].c[1];
	dst->radiancePerPixelNormalized[0].c[2] += src->radiancePerPixelNormalized[0].c[2];
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_1)
	dst->radiancePerPixelNormalized[1].c[0] += src->radiancePerPixelNormalized[1].c[0];
	dst->radiancePerPixelNormalized[1].c[1] += src->radiancePerPixelNormalized[1].c[1];
	dst->radiancePerPixelNormalized[1].c[2] += src->radiancePerPixelNormalized[1].c[2];
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_2)
	dst->radiancePerPixelNormalized[2].c[0] += src->radiancePerPixelNormalized[2].c[0];
	dst->radiancePerPixelNormalized[2].c[1] += src->radiancePerPixelNormalized[2].c[1];
	dst->radiancePerPixelNormalized[2].c[2] += src->radiancePerPixelNormalized[2].c[2];
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_3)
	dst->radiancePerPixelNormalized[3].c[0] += src->radiancePerPixelNormalized[3].c[0];
	dst->radiancePerPixelNormalized[3].c[1] += src->radiancePerPixelNormalized[3].c[1];
	dst->radiancePerPixelNormalized[3].c[2] += src->radiancePerPixelNormalized[3].c[2];
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_4)
	dst->radiancePerPixelNormalized[4].c[0] += src->radiancePerPixelNormalized[4].c[0];
	dst->radiancePerPixelNormalized[4].c[1] += src->radiancePerPixelNormalized[4].c[1];
	dst->radiancePerPixelNormalized[4].c[2] += src->radiancePerPixelNormalized[4].c[2];
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_5)
	dst->radiancePerPixelNormalized[5].c[0] += src->radiancePerPixelNormalized[5].c[0];
	dst->radiancePerPixelNormalized[5].c[1] += src->radiancePerPixelNormalized[5].c[1];
	dst->radiancePerPixelNormalized[5].c[2] += src->radiancePerPixelNormalized[5].c[2];
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_6)
	dst->radiancePerPixelNormalized[6].c[0] += src->radiancePerPixelNormalized[6].c[0];
	dst->radiancePerPixelNormalized[6].c[1] += src->radiancePerPixelNormalized[6].c[1];
	dst->radiancePerPixelNormalized[6].c[2] += src->radiancePerPixelNormalized[6].c[2];
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_7)
	dst->radiancePerPixelNormalized[7].c[0] += src->radiancePerPixelNormalized[7].c[0];
	dst->radiancePerPixelNormalized[7].c[1] += src->radiancePerPixelNormalized[7].c[1];
	dst->radiancePerPixelNormalized[7].c[2] += src->radiancePerPixelNormalized[7].c[2];
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_ALPHA)
	dst->alpha += dst->alpha + src->alpha;
#endif

#if defined(PARAM_FILM_CHANNELS_HAS_DIRECT_DIFFUSE)
	dst->directDiffuse.c[0] += src->directDiffuse.c[0];
	dst->directDiffuse.c[1] += src->directDiffuse.c[1];
	dst->directDiffuse.c[2] += src->directDiffuse.c[2];
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_DIRECT_GLOSSY)
	dst->directGlossy.c[0] += src->directGlossy.c[0];
	dst->directGlossy.c[1] += src->directGlossy.c[1];
	dst->directGlossy.c[2] += src->directGlossy.c[2];
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_EMISSION)
	dst->emission.c[0] += src->emission.c[0];
	dst->emission.c[1] += src->emission.c[1];
	dst->emission.c[2] += src->emission.c[2];
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_INDIRECT_DIFFUSE)
	dst->indirectDiffuse.c[0] += src->indirectDiffuse.c[0];
	dst->indirectDiffuse.c[1] += src->indirectDiffuse.c[1];
	dst->indirectDiffuse.c[2] += src->indirectDiffuse.c[2];
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_INDIRECT_GLOSSY)
	dst->indirectGlossy.c[0] += src->indirectGlossy.c[0];
	dst->indirectGlossy.c[1] += src->indirectGlossy.c[1];
	dst->indirectGlossy.c[2] += src->indirectGlossy.c[2];
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_INDIRECT_SPECULAR)
	dst->indirectSpecular.c[0] += src->indirectSpecular.c[0];
	dst->indirectSpecular.c[1] += src->indirectSpecular.c[1];
	dst->indirectSpecular.c[2] += src->indirectSpecular.c[2];
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
		// Note: MATERIAL_ID_MASK and BY_MATERIAL_ID are calculated starting from materialID field
		dst->materialID = src->materialID;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_UV)
		dst->uv = src->uv;
#endif
	}

#if defined(PARAM_FILM_CHANNELS_HAS_RAYCOUNT)
	dst->rayCount += src->rayCount;
#endif

#if defined(PARAM_FILM_CHANNELS_HAS_IRRADIANCE)
	dst->irradiance.c[0] += src->irradiance.c[0];
	dst->irradiance.c[1] += src->irradiance.c[1];
	dst->irradiance.c[2] += src->irradiance.c[2];
#endif
}

void SR_Normalize(SampleResult *dst, const float k) {
#if defined(PARAM_FILM_RADIANCE_GROUP_0)
	dst->radiancePerPixelNormalized[0].c[0] *= k;
	dst->radiancePerPixelNormalized[0].c[1] *= k;
	dst->radiancePerPixelNormalized[0].c[2] *= k;
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_1)
	dst->radiancePerPixelNormalized[1].c[0] *= k;
	dst->radiancePerPixelNormalized[1].c[1] *= k;
	dst->radiancePerPixelNormalized[1].c[2] *= k;
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_2)
	dst->radiancePerPixelNormalized[2].c[0] *= k;
	dst->radiancePerPixelNormalized[2].c[1] *= k;
	dst->radiancePerPixelNormalized[2].c[2] *= k;
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_3)
	dst->radiancePerPixelNormalized[3].c[0] *= k;
	dst->radiancePerPixelNormalized[3].c[1] *= k;
	dst->radiancePerPixelNormalized[3].c[2] *= k;
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_4)
	dst->radiancePerPixelNormalized[4].c[0] *= k;
	dst->radiancePerPixelNormalized[4].c[1] *= k;
	dst->radiancePerPixelNormalized[4].c[2] *= k;
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_5)
	dst->radiancePerPixelNormalized[5].c[0] *= k;
	dst->radiancePerPixelNormalized[5].c[1] *= k;
	dst->radiancePerPixelNormalized[5].c[2] *= k;
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_6)
	dst->radiancePerPixelNormalized[6].c[0] *= k;
	dst->radiancePerPixelNormalized[6].c[1] *= k;
	dst->radiancePerPixelNormalized[6].c[2] *= k;
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_7)
	dst->radiancePerPixelNormalized[7].c[0] *= k;
	dst->radiancePerPixelNormalized[7].c[1] *= k;
	dst->radiancePerPixelNormalized[7].c[2] *= k;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_ALPHA)
	dst->alpha *= k;
#endif

#if defined(PARAM_FILM_CHANNELS_HAS_DIRECT_DIFFUSE)
	dst->directDiffuse.c[0] *= k;
	dst->directDiffuse.c[1] *= k;
	dst->directDiffuse.c[2] *= k;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_DIRECT_GLOSSY)
	dst->directGlossy.c[0] *= k;
	dst->directGlossy.c[1] *= k;
	dst->directGlossy.c[2] *= k;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_EMISSION)
	dst->emission.c[0] *= k;
	dst->emission.c[1] *= k;
	dst->emission.c[2] *= k;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_INDIRECT_DIFFUSE)
	dst->indirectDiffuse.c[0] *= k;
	dst->indirectDiffuse.c[1] *= k;
	dst->indirectDiffuse.c[2] *= k;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_INDIRECT_GLOSSY)
	dst->indirectGlossy.c[0] *= k;
	dst->indirectGlossy.c[1] *= k;
	dst->indirectGlossy.c[2] *= k;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_INDIRECT_SPECULAR)
	dst->indirectSpecular.c[0] *= k;
	dst->indirectSpecular.c[1] *= k;
	dst->indirectSpecular.c[2] *= k;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_DIRECT_SHADOW_MASK)
	dst->directShadowMask *= k;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_INDIRECT_SHADOW_MASK)
	dst->indirectShadowMask *= k;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_IRRADIANCE)
	dst->irradiance.c[0] *= k;
	dst->irradiance.c[1] *= k;
	dst->irradiance.c[2] *= k;
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

typedef struct {
	uint depth, diffuseDepth, glossyDepth, specularDepth;
} PathDepthInfo;

void PathDepthInfo_Init(PathDepthInfo *depthInfo) {
	depthInfo->depth = 0;
	depthInfo->diffuseDepth = 0;
	depthInfo->glossyDepth = 0;
	depthInfo->specularDepth = 0;
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

bool PathDepthInfo_IsLastPathVertex(const PathDepthInfo *depthInfo, const BSDFEvent event) {
	return (depthInfo->depth + 1 == PARAM_DEPTH_MAX) ||
			((event & DIFFUSE) && (depthInfo->diffuseDepth + 1 == PARAM_DEPTH_DIFFUSE_MAX)) ||
			((event & GLOSSY) && (depthInfo->glossyDepth + 1 == PARAM_DEPTH_GLOSSY_MAX)) ||
			((event & SPECULAR) && (depthInfo->specularDepth + 1 == PARAM_DEPTH_SPECULAR_MAX));
}

bool PathDepthInfo_CheckComponentDepths(const BSDFEvent component) {
	return ((component & DIFFUSE) && (PARAM_DEPTH_DIFFUSE_MAX > 0)) ||
			((component & GLOSSY) && (PARAM_DEPTH_GLOSSY_MAX > 0)) ||
			((component & SPECULAR) && (PARAM_DEPTH_SPECULAR_MAX > 0));
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

	Camera_GenerateRay(camera, engineFilmWidth, engineFilmHeight, ray,
			tileStartX + filmX, tileStartY + filmY, Rnd_FloatValue(seed)
#if defined(PARAM_CAMERA_HAS_DOF)
			, dofSampleX, dofSampleY
#endif
			);	
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
		__global Mesh *meshDescs,
		__global uint *meshMats,
#if (PARAM_TRIANGLE_LIGHT_COUNT > 0)
		__global uint *meshTriLightDefsOffset,
#endif
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
		__global Triangle *triangles
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

#if (PARAM_TRIANGLE_LIGHT_COUNT > 0)
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
#endif

#if defined(PARAM_HAS_ENVLIGHTS)
void DirectHitInfiniteLight(
		const BSDFEvent lastBSDFEvent,
		const float3 pathThroughput,
		const float3 eyeDir, const float lastPdfW,
		__global SampleResult *sampleResult
		LIGHTS_PARAM_DECL) {
	for (uint i = 0; i < envLightCount; ++i) {
		__global LightSource *light = &lights[envLightIndices[i]];

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
		__global LightSource *light,
		const float lightPickPdf,
#if defined(PARAM_HAS_INFINITELIGHTS)
		const float worldCenterX,
		const float worldCenterY,
		const float worldCenterZ,
		const float worldRadius,
#endif
#if (PARAM_TRIANGLE_LIGHT_COUNT > 0)
		__global HitPoint *tmpHitPoint,
#endif
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
#if defined(PARAM_HAS_INFINITELIGHTS)
			worldCenterX, worldCenterY, worldCenterZ, worldRadius,
#endif
#if (PARAM_TRIANGLE_LIGHT_COUNT > 0)
			tmpHitPoint,
#endif		
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
			const float epsilon = fmax(MachineEpsilon_E_Float3(hitPoint), MachineEpsilon_E(distance));

			Ray_Init4_Private(shadowRay, hitPoint, lightRayDir,
				epsilon,
				distance - epsilon, time);

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
#if (PARAM_TRIANGLE_LIGHT_COUNT > 0) || defined(PARAM_HAS_VOLUMES)
		__global HitPoint *tmpHitPoint,
#endif
#if defined(PARAM_HAS_INFINITELIGHTS)
		const float worldCenterX,
		const float worldCenterY,
		const float worldCenterZ,
		const float worldRadius,
#endif
		const float time,
		const float3 pathThroughput,
		__global BSDF *bsdf, __global BSDF *directLightBSDF,
		__global SampleResult *sampleResult,
		// BSDF_Init parameters
		__global Mesh *meshDescs,
		__global uint *meshMats,
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
		__global Triangle *triangles
		// Accelerator_Intersect parameters
		ACCELERATOR_INTERSECT_PARAM_DECL
		// Light related parameters
		LIGHTS_PARAM_DECL) {
	// ONE direct light sampling strategy

	// Pick a light source to sample
	float lightPickPdf;
	const uint lightIndex = Scene_SampleAllLights(lightsDistribution, Rnd_FloatValue(seed), &lightPickPdf);

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
#if defined(PARAM_HAS_INFINITELIGHTS)
		worldCenterX,
		worldCenterY,
		worldCenterZ,
		worldRadius,
#endif
#if (PARAM_TRIANGLE_LIGHT_COUNT > 0)
		tmpHitPoint,
#endif
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
				triangles
				MATERIALS_PARAM
				// Accelerator_Intersect parameters
				ACCELERATOR_INTERSECT_PARAM
				);

		if (shadowRayHit.meshIndex == NULL_INDEX) {
			// Nothing was hit, the light source is visible
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
	}
	
	return tracedRaysCount;
}

uint DirectLightSampling_ALL(
		Seed *seed,
#if defined(PARAM_HAS_VOLUMES)
		__global PathVolumeInfo *volInfo,
#endif
#if (PARAM_TRIANGLE_LIGHT_COUNT > 0) || defined(PARAM_HAS_VOLUMES)
		__global HitPoint *tmpHitPoint,
#endif
#if defined(PARAM_HAS_INFINITELIGHTS)
		const float worldCenterX,
		const float worldCenterY,
		const float worldCenterZ,
		const float worldRadius,
#endif
		const float time,
		const float3 pathThroughput,
		__global BSDF *bsdf, __global BSDF *directLightBSDF,
		__global SampleResult *sampleResult,
		// BSDF_Init parameters
		__global Mesh *meshDescs,
		__global uint *meshMats,
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
		__global Triangle *triangles
		// Accelerator_Intersect parameters
		ACCELERATOR_INTERSECT_PARAM_DECL
		// Light related parameters
		LIGHTS_PARAM_DECL) {
	uint tracedRaysCount = 0;

	for (uint samples = 0; samples < PARAM_FIRST_VERTEX_DL_COUNT; ++samples) {
		// Pick a light source to sample
		float lightPickPdf;
		const uint lightIndex = Scene_SampleAllLights(lightsDistribution, Rnd_FloatValue(seed), &lightPickPdf);

		__global LightSource *light = &lights[lightIndex];
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
#if defined(PARAM_HAS_INFINITELIGHTS)
				worldCenterX,
				worldCenterY,
				worldCenterZ,
				worldRadius,
#endif
#if (PARAM_TRIANGLE_LIGHT_COUNT > 0)
				tmpHitPoint,
#endif
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
						triangles
						MATERIALS_PARAM
						// Accelerator_Intersect parameters
						ACCELERATOR_INTERSECT_PARAM
						);

				if (shadowRayHit.meshIndex == NULL_INDEX) {
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
			}
		}
	}

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
#if (PARAM_TRIANGLE_LIGHT_COUNT > 0) || defined(PARAM_HAS_VOLUMES)
		__global HitPoint *tmpHitPoint,
#endif
#if defined(PARAM_HAS_INFINITELIGHTS)
		const float worldCenterX,
		const float worldCenterY,
		const float worldCenterZ,
		const float worldRadius,
#endif
		PathDepthInfo *depthInfo,
		Ray *ray,
		const float time,
		float3 pathThroughput,
		BSDFEvent lastBSDFEvent, float lastPdfW,
		__global BSDF *bsdfPathVertexN, __global BSDF *directLightBSDF,
		__global SampleResult *sampleResult,
		// BSDF_Init parameters
		__global Mesh *meshDescs,
		__global uint *meshMats,
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
		__global Triangle *triangles
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
			// Add environmental lights radiance
			const float3 rayDir = (float3)(ray->d.x, ray->d.y, ray->d.z);
			DirectHitInfiniteLight(
					lastBSDFEvent,
					pathThroughput,
					-rayDir, lastPdfW,
					sampleResult
					LIGHTS_PARAM);
#endif

			break;
		}

		// Something was hit

		// Check if it is visible in indirect paths
		if (!(mats[bsdfPathVertexN->materialIndex].visibility &
				(sampleResult->firstPathVertexEvent & (DIFFUSE | GLOSSY | SPECULAR))))
			break;

#if (PARAM_TRIANGLE_LIGHT_COUNT > 0)
		// Check if it is a light source (note: I can hit only triangle area light sources)
		if (BSDF_IsLightSource(bsdfPathVertexN) && (rayHit.t > PARAM_NEAR_START_LIGHT)) {
			DirectHitFiniteLight(lastBSDFEvent,
					pathThroughput,
					rayHit.t, bsdfPathVertexN, lastPdfW,
					sampleResult
					LIGHTS_PARAM);
		}
#endif

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
#if (PARAM_TRIANGLE_LIGHT_COUNT > 0) || defined(PARAM_HAS_VOLUMES)
				tmpHitPoint,
#endif
#if defined(PARAM_HAS_INFINITELIGHTS)
				worldCenterX, worldCenterY, worldCenterZ, worldRadius,
#endif
				time,
				pathThroughput,
				bsdfPathVertexN, directLightBSDF,
				sampleResult,
				// BSDF_Init parameters
				meshDescs,
				meshMats,
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
				triangles
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

		const float3 bsdfSample = BSDF_Sample(bsdfPathVertexN,
			Rnd_FloatValue(seed),
			Rnd_FloatValue(seed),
			&sampledDir, &lastPdfW, &cosSampledDir, &lastBSDFEvent,
			ALL
			MATERIALS_PARAM);

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
#if defined(PARAM_HAS_VOLUMES)
		__global PathVolumeInfo *volInfoPathVertex1,
		__global PathVolumeInfo *volInfoPathVertexN,
		__global PathVolumeInfo *directLightVolInfo,
#endif
#if (PARAM_TRIANGLE_LIGHT_COUNT > 0) || defined(PARAM_HAS_VOLUMES)
		__global HitPoint *tmpHitPoint,
#endif
#if defined(PARAM_HAS_INFINITELIGHTS)
		const float worldCenterX,
		const float worldCenterY,
		const float worldCenterZ,
		const float worldRadius,
#endif
		const float time,
		const BSDFEvent requestedEventTypes,
		const uint size,
		const float3 throughputPathVertex1,
		__global BSDF *bsdfPathVertex1, __global BSDF *bsdfPathVertexN,
		__global BSDF *directLightBSDF,
		__global SampleResult *sampleResult,
		// BSDF_Init parameters
		__global Mesh *meshDescs,
		__global uint *meshMats,
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
		__global Triangle *triangles
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
	for (uint currentBSDFSampleIndex = 0; currentBSDFSampleIndex < sampleCount; ++currentBSDFSampleIndex) {
#if defined(PARAM_FILM_CHANNELS_HAS_INDIRECT_SHADOW_MASK)
		sampleResult->indirectShadowMask = 1.f;
#endif

		float u0, u1;
		SampleGrid(seed, size,
			currentBSDFSampleIndex % size, currentBSDFSampleIndex / size,
			&u0, &u1);

		// Sample the BSDF on the first path vertex
		float3 sampledDir;
		float pdfW, cosSampledDir;
		BSDFEvent event;

		const float3 bsdfSample = BSDF_Sample(bsdfPathVertex1,
			u0,
			u1,
			&sampledDir, &pdfW, &cosSampledDir, &event,
			requestedEventTypes | REFLECT | TRANSMIT
			MATERIALS_PARAM);

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
#if (PARAM_TRIANGLE_LIGHT_COUNT > 0) || defined(PARAM_HAS_VOLUMES)
					tmpHitPoint,
#endif
#if defined(PARAM_HAS_INFINITELIGHTS)
					worldCenterX, worldCenterY, worldCenterZ, worldRadius,
#endif
					&depthInfo, &continueRay,
					time,
					continuePathThroughput,
					event, pdfW,
					bsdfPathVertexN, directLightBSDF,
					sampleResult,
					// BSDF_Init parameters
					meshDescs,
					meshMats,
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

//------------------------------------------------------------------------------
// Kernel parameters
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
#define KERNEL_ARGS_FILM_CHANNELS_ID_MASK \
		, __global float *filmMaterialIDMask
#else
#define KERNEL_ARGS_FILM_CHANNELS_ID_MASK
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

#define KERNEL_ARGS_FILM \
		, const uint filmWidth, const uint filmHeight \
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
		KERNEL_ARGS_FILM_CHANNELS_ID_MASK \
		KERNEL_ARGS_FILM_CHANNELS_DIRECT_SHADOW_MASK \
		KERNEL_ARGS_FILM_CHANNELS_INDIRECT_SHADOW_MASK \
		KERNEL_ARGS_FILM_CHANNELS_UV \
		KERNEL_ARGS_FILM_CHANNELS_RAYCOUNT \
		KERNEL_ARGS_FILM_CHANNELS_BY_MATERIAL_ID \
		KERNEL_ARGS_FILM_CHANNELS_IRRADIANCE

#if defined(PARAM_HAS_INFINITELIGHTS)
#define KERNEL_ARGS_INFINITELIGHTS \
		, const float worldCenterX \
		, const float worldCenterY \
		, const float worldCenterZ \
		, const float worldRadius
#else
#define KERNEL_ARGS_INFINITELIGHTS
#endif

#if defined(PARAM_HAS_NORMALS_BUFFER)
#define KERNEL_ARGS_NORMALS_BUFFER \
		, __global Vector *vertNormals
#else
#define KERNEL_ARGS_NORMALS_BUFFER
#endif
#if defined(PARAM_HAS_UVS_BUFFER)
#define KERNEL_ARGS_UVS_BUFFER \
		, __global UV *vertUVs
#else
#define KERNEL_ARGS_UVS_BUFFER
#endif
#if defined(PARAM_HAS_COLS_BUFFER)
#define KERNEL_ARGS_COLS_BUFFER \
		, __global Spectrum *vertCols
#else
#define KERNEL_ARGS_COLS_BUFFER
#endif
#if defined(PARAM_HAS_ALPHAS_BUFFER)
#define KERNEL_ARGS_ALPHAS_BUFFER \
		__global float *vertAlphas,
#else
#define KERNEL_ARGS_ALPHAS_BUFFER
#endif

#if defined(PARAM_HAS_ENVLIGHTS)
#define KERNEL_ARGS_ENVLIGHTS \
		, __global uint *envLightIndices \
		, const uint envLightCount
#else
#define KERNEL_ARGS_ENVLIGHTS
#endif
#if defined(PARAM_HAS_INFINITELIGHT)
#define KERNEL_ARGS_INFINITELIGHT \
		, __global float *infiniteLightDistribution
#else
#define KERNEL_ARGS_INFINITELIGHT
#endif

#if defined(PARAM_IMAGEMAPS_PAGE_0)
#define KERNEL_ARGS_IMAGEMAPS_PAGE_0 \
		, __global ImageMap *imageMapDescs, __global float *imageMapBuff0
#else
#define KERNEL_ARGS_IMAGEMAPS_PAGE_0
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_1)
#define KERNEL_ARGS_IMAGEMAPS_PAGE_1 \
		, __global float *imageMapBuff1
#else
#define KERNEL_ARGS_IMAGEMAPS_PAGE_1
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_2)
#define KERNEL_ARGS_IMAGEMAPS_PAGE_2 \
		, __global float *imageMapBuff2
#else
#define KERNEL_ARGS_IMAGEMAPS_PAGE_2
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_3)
#define KERNEL_ARGS_IMAGEMAPS_PAGE_3 \
		, __global float *imageMapBuff3
#else
#define KERNEL_ARGS_IMAGEMAPS_PAGE_3
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_4)
#define KERNEL_ARGS_IMAGEMAPS_PAGE_4 \
		, __global float *imageMapBuff4
#else
#define KERNEL_ARGS_IMAGEMAPS_PAGE_4
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_5)
#define KERNEL_ARGS_IMAGEMAPS_PAGE_5 \
		, __global float *imageMapBuff5
#else
#define KERNEL_ARGS_IMAGEMAPS_PAGE_5
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_6)
#define KERNEL_ARGS_IMAGEMAPS_PAGE_6 \
		, __global float *imageMapBuff6
#else
#define KERNEL_ARGS_IMAGEMAPS_PAGE_6
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_7)
#define KERNEL_ARGS_IMAGEMAPS_PAGE_7 \
		, __global float *imageMapBuff7
#else
#define KERNEL_ARGS_IMAGEMAPS_PAGE_7
#endif
#define KERNEL_ARGS_IMAGEMAPS_PAGES \
		KERNEL_ARGS_IMAGEMAPS_PAGE_0 \
		KERNEL_ARGS_IMAGEMAPS_PAGE_1 \
		KERNEL_ARGS_IMAGEMAPS_PAGE_2 \
		KERNEL_ARGS_IMAGEMAPS_PAGE_3 \
		KERNEL_ARGS_IMAGEMAPS_PAGE_4 \
		KERNEL_ARGS_IMAGEMAPS_PAGE_5 \
		KERNEL_ARGS_IMAGEMAPS_PAGE_6 \
		KERNEL_ARGS_IMAGEMAPS_PAGE_7

#define KERNEL_ARGS \
		const uint engineFilmWidth, const uint engineFilmHeight \
		, __global GPUTask *tasks \
		, __global GPUTaskDirectLight *tasksDirectLight \
		, __global GPUTaskPathVertexN *tasksPathVertexN \
		, __global GPUTaskStats *taskStats \
		, __global SampleResult *taskResults \
		, __global float *pixelFilterDistribution \
		/* Film parameters */ \
		KERNEL_ARGS_FILM \
		/* Scene parameters */ \
		KERNEL_ARGS_INFINITELIGHTS \
		, __global Material *mats \
		, __global Texture *texs \
		, __global uint *meshMats \
		, __global Mesh *meshDescs \
		, __global Point *vertices \
		KERNEL_ARGS_NORMALS_BUFFER \
		KERNEL_ARGS_UVS_BUFFER \
		KERNEL_ARGS_COLS_BUFFER \
		KERNEL_ARGS_ALPHAS_BUFFER \
		, __global Triangle *triangles \
		, __global Camera *camera \
		/* Lights */ \
		, __global LightSource *lights \
		KERNEL_ARGS_ENVLIGHTS \
		, __global uint *meshTriLightDefsOffset \
		KERNEL_ARGS_INFINITELIGHT \
		, __global float *lightsDistribution \
		/* Images */ \
		KERNEL_ARGS_IMAGEMAPS_PAGES \
		ACCELERATOR_INTERSECT_PARAM_DECL


//------------------------------------------------------------------------------
// To initialize image maps page pointer table
//------------------------------------------------------------------------------

#if defined(PARAM_IMAGEMAPS_PAGE_0)
#define INIT_IMAGEMAPS_PAGE_0 imageMapBuff[0] = imageMapBuff0;
#else
#define INIT_IMAGEMAPS_PAGE_0
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_1)
#define INIT_IMAGEMAPS_PAGE_1 imageMapBuff[1] = imageMapBuff1;
#else
#define INIT_IMAGEMAPS_PAGE_1
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_2)
#define INIT_IMAGEMAPS_PAGE_2 imageMapBuff[2] = imageMapBuff2;
#else
#define INIT_IMAGEMAPS_PAGE_2
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_3)
#define INIT_IMAGEMAPS_PAGE_3 imageMapBuff[3] = imageMapBuff3;
#else
#define INIT_IMAGEMAPS_PAGE_3
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_4)
#define INIT_IMAGEMAPS_PAGE_4 imageMapBuff[4] = imageMapBuff4;
#else
#define INIT_IMAGEMAPS_PAGE_4
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_5)
#define INIT_IMAGEMAPS_PAGE_5 imageMapBuff[5] = imageMapBuff5;
#else
#define INIT_IMAGEMAPS_PAGE_5
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_6)
#define INIT_IMAGEMAPS_PAGE_6 imageMapBuff[6] = imageMapBuff6;
#else
#define INIT_IMAGEMAPS_PAGE_6
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_7)
#define INIT_IMAGEMAPS_PAGE_7 imageMapBuff[7] = imageMapBuff7;
#else
#define INIT_IMAGEMAPS_PAGE_7
#endif

#if defined(PARAM_HAS_IMAGEMAPS)
#define INIT_IMAGEMAPS_PAGES \
	__global float *imageMapBuff[PARAM_IMAGEMAPS_COUNT]; \
	INIT_IMAGEMAPS_PAGE_0 \
	INIT_IMAGEMAPS_PAGE_1 \
	INIT_IMAGEMAPS_PAGE_2 \
	INIT_IMAGEMAPS_PAGE_3 \
	INIT_IMAGEMAPS_PAGE_4 \
	INIT_IMAGEMAPS_PAGE_5 \
	INIT_IMAGEMAPS_PAGE_6 \
	INIT_IMAGEMAPS_PAGE_7
#else
#define INIT_IMAGEMAPS_PAGES
#endif

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
// MergePixelSamples
//------------------------------------------------------------------------------

__kernel __attribute__((work_group_size_hint(64, 1, 1))) void MergePixelSamples(
		const uint tileStartX
		, const uint tileStartY
		, const uint engineFilmWidth, const uint engineFilmHeight
		, __global SampleResult *taskResults
		// Film parameters
		KERNEL_ARGS_FILM
		) {
	const size_t gid = get_global_id(0);

	uint sampleX, sampleY;
	sampleX = gid % PARAM_TILE_WIDTH;
	sampleY = gid / PARAM_TILE_WIDTH;

	if ((gid >= PARAM_TILE_WIDTH * PARAM_TILE_HEIGHT) ||
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
	filmRadianceGroup[4] = filmRadianceGroup4;
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_5)
	filmRadianceGroup[5] = filmRadianceGroup5;
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_6)
	filmRadianceGroup[6] = filmRadianceGroup6;
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_7)
	filmRadianceGroup[7] = filmRadianceGroup7;
#endif

	//--------------------------------------------------------------------------
	// Radiance clamping
	//--------------------------------------------------------------------------

	if (PARAM_RADIANCE_CLAMP_MAXVALUE > 0.f) {
		for (uint i = 0; i < PARAM_AA_SAMPLES * PARAM_AA_SAMPLES; ++i)
			SampleResult_ClampRadiance(&sampleResult[i], PARAM_RADIANCE_CLAMP_MAXVALUE);
	}

	//--------------------------------------------------------------------------
	// Merge all samples and accumulate statistics
	//--------------------------------------------------------------------------

#if (PARAM_AA_SAMPLES == 1)
	Film_AddSample(sampleX, sampleY, &sampleResult[0], PARAM_AA_SAMPLES * PARAM_AA_SAMPLES
			FILM_PARAM);
#else
	SampleResult result = sampleResult[0];
	uint totalRaysCount = 0;
	for (uint i = 1; i < PARAM_AA_SAMPLES * PARAM_AA_SAMPLES; ++i) {
		SR_Accumulate(&sampleResult[i], &result);
	}
	SR_Normalize(&result, 1.f / (PARAM_AA_SAMPLES * PARAM_AA_SAMPLES));

	// I have to save result in __global space in order to be able
	// to use Film_AddSample(). OpenCL can be so stupid some time...
	sampleResult[0] = result;
	Film_AddSample(sampleX, sampleY, &sampleResult[0], PARAM_AA_SAMPLES * PARAM_AA_SAMPLES
			FILM_PARAM);
#endif
}
