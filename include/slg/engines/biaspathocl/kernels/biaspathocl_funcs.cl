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

void SR_RadianceClamp(__global SampleResult *sampleResult) {
	// Initialize only Spectrum fields

#if defined(PARAM_FILM_RADIANCE_GROUP_0)
	sampleResult->radiancePerPixelNormalized[0].c[0] = clamp(sampleResult->radiancePerPixelNormalized[0].c[0], 0.f, PARAM_RADIANCE_CLAMP_MAXVALUE);
	sampleResult->radiancePerPixelNormalized[0].c[1] = clamp(sampleResult->radiancePerPixelNormalized[0].c[1], 0.f, PARAM_RADIANCE_CLAMP_MAXVALUE);
	sampleResult->radiancePerPixelNormalized[0].c[2] = clamp(sampleResult->radiancePerPixelNormalized[0].c[2], 0.f, PARAM_RADIANCE_CLAMP_MAXVALUE);
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_1)
	sampleResult->radiancePerPixelNormalized[1].c[0] = clamp(sampleResult->radiancePerPixelNormalized[1].c[0], 0.f, PARAM_RADIANCE_CLAMP_MAXVALUE);
	sampleResult->radiancePerPixelNormalized[1].c[1] = clamp(sampleResult->radiancePerPixelNormalized[1].c[1], 0.f, PARAM_RADIANCE_CLAMP_MAXVALUE);
	sampleResult->radiancePerPixelNormalized[1].c[2] = clamp(sampleResult->radiancePerPixelNormalized[1].c[2], 0.f, PARAM_RADIANCE_CLAMP_MAXVALUE);
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_2)
	sampleResult->radiancePerPixelNormalized[2].c[0] = clamp(sampleResult->radiancePerPixelNormalized[2].c[0], 0.f, PARAM_RADIANCE_CLAMP_MAXVALUE);
	sampleResult->radiancePerPixelNormalized[2].c[1] = clamp(sampleResult->radiancePerPixelNormalized[2].c[1], 0.f, PARAM_RADIANCE_CLAMP_MAXVALUE);
	sampleResult->radiancePerPixelNormalized[2].c[2] = clamp(sampleResult->radiancePerPixelNormalized[2].c[2], 0.f, PARAM_RADIANCE_CLAMP_MAXVALUE);
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_3)
	sampleResult->radiancePerPixelNormalized[3].c[0] = clamp(sampleResult->radiancePerPixelNormalized[3].c[0], 0.f, PARAM_RADIANCE_CLAMP_MAXVALUE);
	sampleResult->radiancePerPixelNormalized[3].c[1] = clamp(sampleResult->radiancePerPixelNormalized[3].c[1], 0.f, PARAM_RADIANCE_CLAMP_MAXVALUE);
	sampleResult->radiancePerPixelNormalized[3].c[2] = clamp(sampleResult->radiancePerPixelNormalized[3].c[2], 0.f, PARAM_RADIANCE_CLAMP_MAXVALUE);
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_4)
	sampleResult->radiancePerPixelNormalized[4].c[0] = clamp(sampleResult->radiancePerPixelNormalized[4].c[0], 0.f, PARAM_RADIANCE_CLAMP_MAXVALUE);
	sampleResult->radiancePerPixelNormalized[4].c[1] = clamp(sampleResult->radiancePerPixelNormalized[4].c[1], 0.f, PARAM_RADIANCE_CLAMP_MAXVALUE);
	sampleResult->radiancePerPixelNormalized[4].c[2] = clamp(sampleResult->radiancePerPixelNormalized[4].c[2], 0.f, PARAM_RADIANCE_CLAMP_MAXVALUE);
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_5)
	sampleResult->radiancePerPixelNormalized[5].c[0] = clamp(sampleResult->radiancePerPixelNormalized[5].c[0], 0.f, PARAM_RADIANCE_CLAMP_MAXVALUE);
	sampleResult->radiancePerPixelNormalized[5].c[1] = clamp(sampleResult->radiancePerPixelNormalized[5].c[1], 0.f, PARAM_RADIANCE_CLAMP_MAXVALUE);
	sampleResult->radiancePerPixelNormalized[5].c[2] = clamp(sampleResult->radiancePerPixelNormalized[5].c[2], 0.f, PARAM_RADIANCE_CLAMP_MAXVALUE);
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_6)
	sampleResult->radiancePerPixelNormalized[6].c[0] = clamp(sampleResult->radiancePerPixelNormalized[6].c[0], 0.f, PARAM_RADIANCE_CLAMP_MAXVALUE);
	sampleResult->radiancePerPixelNormalized[6].c[1] = clamp(sampleResult->radiancePerPixelNormalized[6].c[1], 0.f, PARAM_RADIANCE_CLAMP_MAXVALUE);
	sampleResult->radiancePerPixelNormalized[6].c[2] = clamp(sampleResult->radiancePerPixelNormalized[6].c[2], 0.f, PARAM_RADIANCE_CLAMP_MAXVALUE);
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_7)
	sampleResult->radiancePerPixelNormalized[7].c[0] = clamp(sampleResult->radiancePerPixelNormalized[7].c[0], 0.f, PARAM_RADIANCE_CLAMP_MAXVALUE);
	sampleResult->radiancePerPixelNormalized[7].c[1] = clamp(sampleResult->radiancePerPixelNormalized[7].c[1], 0.f, PARAM_RADIANCE_CLAMP_MAXVALUE);
	sampleResult->radiancePerPixelNormalized[7].c[2] = clamp(sampleResult->radiancePerPixelNormalized[7].c[2], 0.f, PARAM_RADIANCE_CLAMP_MAXVALUE);
#endif
}

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

#if defined(PARAM_HAS_ENVLIGHTS)
void DirectHitInfiniteLight(
		const bool firstPathVertex,
		const BSDFEvent lastBSDFEvent,
		const BSDFEvent pathBSDFEvent,
		__global const Spectrum *pathThroughput,
		const float3 eyeDir, const float lastPdfW,
		__global SampleResult *sampleResult
		LIGHTS_PARAM_DECL) {
	const float3 throughput = VLOAD3F(&pathThroughput->c[0]);

	for (uint i = 0; i < envLightCount; ++i) {
		__global LightSource *light = &lights[envLightIndices[i]];

		if (firstPathVertex || (light->visibility & (pathBSDFEvent & (DIFFUSE | GLOSSY | SPECULAR)))) {
			float directPdfW;
			const float3 lightRadiance = EnvLight_GetRadiance(light, eyeDir, &directPdfW
					LIGHTS_PARAM);

			if (!Spectrum_IsBlack(lightRadiance)) {
				// MIS between BSDF sampling and direct light sampling
				const float lightPickProb = Scene_SampleAllLightPdf(lightsDistribution, light->lightSceneIndex);
				const float weight = ((lastBSDFEvent & SPECULAR) ? 1.f : PowerHeuristic(lastPdfW, directPdfW * lightPickProb));
				const float3 radiance = weight * throughput * lightRadiance;

				const uint lightID = min(light->lightID, PARAM_FILM_RADIANCE_GROUP_COUNT - 1u);
				AddEmission(firstPathVertex, pathBSDFEvent, lightID, sampleResult, radiance);
			}
		}
	}
}
#endif

#if (PARAM_TRIANGLE_LIGHT_COUNT > 0)
void DirectHitFiniteLight(
		const bool firstPathVertex,
		const BSDFEvent lastBSDFEvent,
		const BSDFEvent pathBSDFEvent,
		__global const Spectrum *pathThroughput, const float distance, __global BSDF *bsdf,
		const float lastPdfW, __global SampleResult *sampleResult
		LIGHTS_PARAM_DECL) {
	if (firstPathVertex || (lights[bsdf->triangleLightSourceIndex].visibility & (pathBSDFEvent & (DIFFUSE | GLOSSY | SPECULAR)))) {
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
			const float3 lightRadiance = weight * VLOAD3F(&pathThroughput->c[0]) * emittedRadiance;

			const uint lightID =  min(BSDF_GetLightID(bsdf
					MATERIALS_PARAM), PARAM_FILM_RADIANCE_GROUP_COUNT - 1u);
			AddEmission(firstPathVertex, pathBSDFEvent, lightID, sampleResult, lightRadiance);
		}
	}
}
#endif

bool DirectLightSampling(
		__global LightSource *light,
		const float lightPickPdf,
#if defined(PARAM_HAS_ENVLIGHTS)
		const float worldCenterX,
		const float worldCenterY,
		const float worldCenterZ,
		const float worldRadius,
#endif
#if (PARAM_TRIANGLE_LIGHT_COUNT > 0)
		__global HitPoint *tmpHitPoint,
#endif
		const float u0, const float u1, const float u2,
#if defined(PARAM_HAS_PASSTHROUGH)
		const float u3,
#endif
		__global const Spectrum *pathThroughput, __global BSDF *bsdf,
		Ray *shadowRay, __global Spectrum *radiance, __global uint *ID
		LIGHTS_PARAM_DECL) {
	float3 lightRayDir;
	float distance, directPdfW;
	const float3 lightRadiance = Light_Illuminate(
			light,
			VLOAD3F(&bsdf->hitPoint.p.x),
			u0, u1, u2,
#if defined(PARAM_HAS_PASSTHROUGH)
			u3,
#endif
#if defined(PARAM_HAS_ENVLIGHTS)
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
		BSDFEvent event;
		float bsdfPdfW;
		const float3 bsdfEval = BSDF_Evaluate(bsdf,
				lightRayDir, &event, &bsdfPdfW
				MATERIALS_PARAM);

		if (!Spectrum_IsBlack(bsdfEval)) {
			const float directLightSamplingPdfW = directPdfW * lightPickPdf;
			const float factor = 1.f / directLightSamplingPdfW;

			// MIS between direct light sampling and BSDF sampling
			const float weight = Light_IsEnvOrIntersecable(light) ?
				PowerHeuristic(directLightSamplingPdfW, bsdfPdfW) : 1.f;

			VSTORE3F((weight * factor) * VLOAD3F(&pathThroughput->c[0]) * bsdfEval * lightRadiance, &radiance->c[0]);
			*ID = min(light->lightID, PARAM_FILM_RADIANCE_GROUP_COUNT - 1u);

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
#if defined(PARAM_HAS_ENVLIGHTS)
		const float worldCenterX,
		const float worldCenterY,
		const float worldCenterZ,
		const float worldRadius,
#endif
#if (PARAM_TRIANGLE_LIGHT_COUNT > 0)
		__global HitPoint *tmpHitPoint,
#endif
		__global const Spectrum *pathThroughput, __global BSDF *bsdf,
		__global SampleResult *sampleResult,
		Ray *shadowRay, __global Spectrum *radiance, __global uint *ID
		LIGHTS_PARAM_DECL) {
	// Pick a light source to sample
	float lightPickPdf;
	const uint lightIndex = Scene_SampleAllLights(lightsDistribution, Rnd_FloatValue(seed), &lightPickPdf);

	const bool illuminated = DirectLightSampling(
		&lights[lightIndex],
		lightPickPdf,
#if defined(PARAM_HAS_ENVLIGHTS)
		worldCenterX,
		worldCenterY,
		worldCenterZ,
		worldRadius,
#endif
#if (PARAM_TRIANGLE_LIGHT_COUNT > 0)
		tmpHitPoint,
#endif
		Rnd_FloatValue(seed), Rnd_FloatValue(seed), Rnd_FloatValue(seed),
#if defined(PARAM_HAS_PASSTHROUGH)
		Rnd_FloatValue(seed),
#endif
		pathThroughput, bsdf,
		shadowRay, radiance, ID
		LIGHTS_PARAM);

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
		Seed *seed,
#if defined(PARAM_HAS_ENVLIGHTS)
		const float worldCenterX,
		const float worldCenterY,
		const float worldCenterZ,
		const float worldRadius,
#endif
#if (PARAM_TRIANGLE_LIGHT_COUNT > 0)
		__global HitPoint *tmpHitPoint,
#endif
		__global const Spectrum *pathThroughput, __global BSDF *bsdf,
		__global SampleResult *sampleResult,
		Ray *shadowRay, __global Spectrum *radiance, __global uint *ID
		LIGHTS_PARAM_DECL) {
	for (; *currentLightIndex < PARAM_LIGHT_COUNT; ++(*currentLightIndex)) {
		const int lightSamplesCount = lights[*currentLightIndex].samples;
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
				&lights[*currentLightIndex],
				1.f,
#if defined(PARAM_HAS_ENVLIGHTS)
				worldCenterX,
				worldCenterY,
				worldCenterZ,
				worldRadius,
#endif
#if (PARAM_TRIANGLE_LIGHT_COUNT > 0)
				tmpHitPoint,
#endif
				u0, u1, Rnd_FloatValue(seed),
#if defined(PARAM_HAS_PASSTHROUGH)
				Rnd_FloatValue(seed),
#endif
				pathThroughput, bsdf,
				shadowRay, radiance, ID
				LIGHTS_PARAM);

			if (illuminated) {
				VSTORE3F(scaleFactor * VLOAD3F(&radiance->c[0]), &radiance->c[0]);
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
