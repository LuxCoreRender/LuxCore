/***************************************************************************
 *   Copyright (C) 1998-2010 by authors (see AUTHORS.txt )                 *
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

//------------------------------------------------------------------------------
// Init Kernel
//------------------------------------------------------------------------------

__kernel void Init(
		__global GPUTask *tasks,
		__global GPUTaskStats *taskStats,
		__global Ray *rays
#if defined(PARAM_CAMERA_DYNAMIC)
		, __global float *cameraData
#endif
#if (PARAM_SAMPLER_TYPE == 3)
		, __local float *localMemTempBuff
#endif
		) {
	const size_t gid = get_global_id(0);

	//if (gid == 0)
	//	printf(\"GPUTask: %d\\n\", sizeof(GPUTask));

	// Initialize the task
	__global GPUTask *task = &tasks[gid];

	// Initialize random number generator
	Seed seed;
	InitRandomGenerator(PARAM_SEED + gid, &seed);

	// Initialize the sample
	Sampler_Init(gid,
#if (PARAM_SAMPLER_TYPE == 3)
			localMemTempBuff,
#endif
			&seed, &task->sample);

	// Initialize the path
	GenerateCameraPath(task, &rays[gid], &seed
#if defined(PARAM_CAMERA_DYNAMIC)
			, cameraData
#endif
			);

	// Save the seed
	task->seed.s1 = seed.s1;
	task->seed.s2 = seed.s2;
	task->seed.s3 = seed.s3;

	__global GPUTaskStats *taskStat = &taskStats[gid];
	taskStat->sampleCount = 0;
}

//------------------------------------------------------------------------------
// InitFrameBuffer Kernel
//------------------------------------------------------------------------------

__kernel void InitFrameBuffer(
		__global Pixel *frameBuffer
		) {
	const size_t gid = get_global_id(0);
	if (gid >= PARAM_IMAGE_WIDTH * PARAM_IMAGE_HEIGHT)
		return;

	__global Pixel *p = &frameBuffer[gid];
	p->c.r = 0.f;
	p->c.g = 0.f;
	p->c.b = 0.f;
	p->count = 0.f;
}

//------------------------------------------------------------------------------
// AdvancePaths Kernel
//------------------------------------------------------------------------------

__kernel void AdvancePaths(
		__global GPUTask *tasks,
		__global Ray *rays,
		__global RayHit *rayHits,
		__global Pixel *frameBuffer,
		__global Material *mats,
		__global uint *meshMats,
		__global uint *meshIDs,
		__global Spectrum *vertColors,
		__global Vector *vertNormals,
		__global Triangle *triangles
#if defined(PARAM_CAMERA_DYNAMIC)
		, __global float *cameraData
#endif
#if defined(PARAM_HAVE_INFINITELIGHT)
		, __global Spectrum *infiniteLightMap
#endif
#if defined(PARAM_DIRECT_LIGHT_SAMPLING)
		, __global TriangleLight *triLights
#endif
#if defined(PARAM_HAS_TEXTUREMAPS)
        , __global Spectrum *texMapRGBBuff
#if defined(PARAM_HAS_ALPHA_TEXTUREMAPS)
		, __global float *texMapAlphaBuff
#endif
        , __global TexMap *texMapDescBuff
        , __global unsigned int *meshTexsBuff
        , __global UV *vertUVs
#endif
		) {
	const size_t gid = get_global_id(0);

	__global GPUTask *task = &tasks[gid];
	uint pathState = task->pathState.state;

#if (PARAM_SAMPLER_TYPE == 0)
	// Read the seed
	Seed seed;
	seed.s1 = task->seed.s1;
	seed.s2 = task->seed.s2;
	seed.s3 = task->seed.s3;
#endif

	__global Sample *sample = &task->sample;

	__global Ray *ray = &rays[gid];
	__global RayHit *rayHit = &rayHits[gid];
	const uint currentTriangleIndex = rayHit->index;

	const float hitPointT = rayHit->t;
    const float hitPointB1 = rayHit->b1;
    const float hitPointB2 = rayHit->b2;

    Vector rayDir = ray->d;

	Point hitPoint;
    hitPoint.x = ray->o.x + rayDir.x * hitPointT;
    hitPoint.y = ray->o.y + rayDir.y * hitPointT;
    hitPoint.z = ray->o.z + rayDir.z * hitPointT;

	Spectrum throughput = task->pathState.throughput;
	const Spectrum prevThroughput = throughput;

	switch (pathState) {
		case PATH_STATE_NEXT_VERTEX: {
			if (currentTriangleIndex != 0xffffffffu) {
				// Something was hit

				uint pathDepth = task->pathState.depth;
#if (PARAM_SAMPLER_TYPE == 1) || (PARAM_SAMPLER_TYPE == 3)
				__global float *sampleData = &sample->u[IDX_BSDF_OFFSET + SAMPLE_SIZE * pathDepth];
#elif (PARAM_SAMPLER_TYPE == 2)
				__global float *sampleData = &sample->u[sample->proposed][IDX_BSDF_OFFSET + SAMPLE_SIZE * pathDepth];
#endif

				const uint meshIndex = meshIDs[currentTriangleIndex];
				__global Material *hitPointMat = &mats[meshMats[meshIndex]];
				uint matType = hitPointMat->type;

				// Interpolate Color
				Spectrum shadeColor;
				Mesh_InterpolateColor(vertColors, triangles, currentTriangleIndex, hitPointB1, hitPointB2, &shadeColor);

#if defined(PARAM_HAS_TEXTUREMAPS)
				// Interpolate UV coordinates
				UV uv;
				Mesh_InterpolateUV(vertUVs, triangles, currentTriangleIndex, hitPointB1, hitPointB2, &uv);

				// Check it the mesh has a texture map
				unsigned int texIndex = meshTexsBuff[meshIndex];
				if (texIndex != 0xffffffffu) {
					__global TexMap *texMap = &texMapDescBuff[texIndex];

#if defined(PARAM_HAS_ALPHA_TEXTUREMAPS)
					// Check if it has an alpha channel
					if (texMap->alphaOffset != 0xffffffffu) {
						const float alpha = TexMap_GetAlpha(&texMapAlphaBuff[texMap->alphaOffset], texMap->width, texMap->height, uv.u, uv.v);

#if (PARAM_SAMPLER_TYPE == 0)
						const float texAlphaSample = RndFloatValue(&seed);
#elif (PARAM_SAMPLER_TYPE == 1) || (PARAM_SAMPLER_TYPE == 2) || (PARAM_SAMPLER_TYPE == 3)
						const float texAlphaSample = sampleData[IDX_TEX_ALPHA];
#endif

						if ((alpha == 0.0f) || ((alpha < 1.f) && (texAlphaSample > alpha))) {
							// Continue to trace the ray
							matType = MAT_NULL;
						}
					}
#endif

					Spectrum texColor;
					TexMap_GetColor(&texMapRGBBuff[texMap->rgbOffset], texMap->width, texMap->height, uv.u, uv.v, &texColor);

					shadeColor.r *= texColor.r;
					shadeColor.g *= texColor.g;
					shadeColor.b *= texColor.b;
				}
#endif

				throughput.r *= shadeColor.r;
				throughput.g *= shadeColor.g;
				throughput.b *= shadeColor.b;

				// Interpolate the normal
				Vector N;
				Mesh_InterpolateNormal(vertNormals, triangles, currentTriangleIndex, hitPointB1, hitPointB2, &N);

				// Flip the normal if required
				Vector shadeN;
				const float nFlip = (Dot(&rayDir, &N) > 0.f) ? -1.f : 1.f;
				shadeN.x = nFlip * N.x;
				shadeN.y = nFlip * N.y;
				shadeN.z = nFlip * N.z;

#if (PARAM_SAMPLER_TYPE == 0)
				const float u0 = RndFloatValue(&seed);
				const float u1 = RndFloatValue(&seed);
#elif (PARAM_SAMPLER_TYPE == 1) ||(PARAM_SAMPLER_TYPE == 2) || (PARAM_SAMPLER_TYPE == 3)
				const float u0 = sampleData[IDX_BSDF_X];
				const float u1 = sampleData[IDX_BSDF_Y];
#endif

#if defined(PARAM_ENABLE_MAT_MATTEMIRROR) || defined(PARAM_ENABLE_MAT_MATTEMETAL) || defined(PARAM_ENABLE_MAT_ALLOY)
#if (PARAM_SAMPLER_TYPE == 0)
				const float u2 = RndFloatValue(&seed);
#elif (PARAM_SAMPLER_TYPE == 1) ||(PARAM_SAMPLER_TYPE == 2) || (PARAM_SAMPLER_TYPE == 3)
				const float u2 = sampleData[IDX_BSDF_Z];
#endif
#endif

				Vector wo;
				wo.x = -rayDir.x;
				wo.y = -rayDir.y;
				wo.z = -rayDir.z;

				Vector wi;
				float pdf;
				Spectrum f;

				switch (matType) {

#if defined(PARAM_ENABLE_MAT_MATTE)
					case MAT_MATTE:
						Matte_Sample_f(&hitPointMat->param.matte, &wo, &wi, &pdf, &f, &shadeN, u0, u1
#if defined(PARAM_DIRECT_LIGHT_SAMPLING)
								, &task->pathState.specularBounce
#endif
								);
						break;
#endif

#if defined(PARAM_ENABLE_MAT_AREALIGHT)
					case MAT_AREALIGHT: {
#if defined(PARAM_DIRECT_LIGHT_SAMPLING)
						if (task->pathState.specularBounce) {
#endif
							Spectrum Le;
							AreaLight_Le(&hitPointMat->param.areaLight, &wo, &N, &Le);
							sample->radiance.r += throughput.r * Le.r;
							sample->radiance.g += throughput.g * Le.g;
							sample->radiance.b += throughput.b * Le.b;

#if defined(PARAM_DIRECT_LIGHT_SAMPLING)
						}
#endif

						pdf = 0.f;
						break;
					}
#endif

#if defined(PARAM_ENABLE_MAT_MIRROR)
					case MAT_MIRROR:
						Mirror_Sample_f(&hitPointMat->param.mirror, &wo, &wi, &pdf, &f, &shadeN
#if defined(PARAM_DIRECT_LIGHT_SAMPLING)
								, &task->pathState.specularBounce
#endif
								);
						break;
#endif

#if defined(PARAM_ENABLE_MAT_GLASS)
					case MAT_GLASS:
						Glass_Sample_f(&hitPointMat->param.glass, &wo, &wi, &pdf, &f, &N, &shadeN, u0
#if defined(PARAM_DIRECT_LIGHT_SAMPLING)
								, &task->pathState.specularBounce
#endif
								);
						break;
#endif

#if defined(PARAM_ENABLE_MAT_MATTEMIRROR)
					case MAT_MATTEMIRROR:
						MatteMirror_Sample_f(&hitPointMat->param.matteMirror, &wo, &wi, &pdf, &f, &shadeN, u0, u1, u2
#if defined(PARAM_DIRECT_LIGHT_SAMPLING)
								, &task->pathState.specularBounce
#endif
								);
						break;
#endif

#if defined(PARAM_ENABLE_MAT_METAL)
					case MAT_METAL:
						Metal_Sample_f(&hitPointMat->param.metal, &wo, &wi, &pdf, &f, &shadeN, u0, u1
#if defined(PARAM_DIRECT_LIGHT_SAMPLING)
								, &task->pathState.specularBounce
#endif
								);
						break;
#endif

#if defined(PARAM_ENABLE_MAT_MATTEMETAL)
					case MAT_MATTEMETAL:
						MatteMetal_Sample_f(&hitPointMat->param.matteMetal, &wo, &wi, &pdf, &f, &shadeN, u0, u1, u2
#if defined(PARAM_DIRECT_LIGHT_SAMPLING)
								, &task->pathState.specularBounce
#endif
								);
						break;
#endif

#if defined(PARAM_ENABLE_MAT_ALLOY)
					case MAT_ALLOY:
						Alloy_Sample_f(&hitPointMat->param.alloy, &wo, &wi, &pdf, &f, &shadeN, u0, u1, u2
#if defined(PARAM_DIRECT_LIGHT_SAMPLING)
								, &task->pathState.specularBounce
#endif
								);
						break;
#endif

#if defined(PARAM_ENABLE_MAT_ARCHGLASS)
					case MAT_ARCHGLASS:
						ArchGlass_Sample_f(&hitPointMat->param.archGlass, &wo, &wi, &pdf, &f, &N, &shadeN, u0
#if defined(PARAM_DIRECT_LIGHT_SAMPLING)
								, &task->pathState.specularBounce
#endif
								);
						break;
#endif

					case MAT_NULL:
						wi = rayDir;
						pdf = 1.f;
						f.r = 1.f;
						f.g = 1.f;
						f.b = 1.f;

						// I have also to restore the original throughput
						throughput = prevThroughput;
						break;

					default:
						// Huston, we have a problem...
						pdf = 0.f;
						break;
				}

				pathDepth += 1;
				const float invPdf = ((pdf <= 0.f) || (pathDepth >= PARAM_MAX_PATH_DEPTH)) ? 0.f : (1.f / pdf);

				//if (pathDepth > 2)
				//	printf(\"Depth: %d Throughput: (%f, %f, %f) f: (%f, %f, %f) Pdf: %f\\n\", pathDepth, throughput.r, throughput.g, throughput.b, f.r, f.g, f.b, pdf);

				throughput.r *= f.r * invPdf;
				throughput.g *= f.g * invPdf;
				throughput.b *= f.b * invPdf;

				// Russian roulette

#if (PARAM_SAMPLER_TYPE == 0)
				const float rrSample = RndFloatValue(&seed);
#elif (PARAM_SAMPLER_TYPE == 1) || (PARAM_SAMPLER_TYPE == 2) || (PARAM_SAMPLER_TYPE == 3)
				const float rrSample = sampleData[IDX_RR];
#endif

				const float rrProb = max(max(throughput.r, max(throughput.g, throughput.b)), (float) PARAM_RR_CAP);
				const float invRRProb = (pathDepth > PARAM_RR_DEPTH) ? ((rrProb >= rrSample) ? 0.f : (1.f / rrProb)) : 1.f;
				throughput.r *= invRRProb;
				throughput.g *= invRRProb;
				throughput.b *= invRRProb;

				//if (pathDepth > 2)
				//	printf(\"Depth: %d Throughput: (%f, %f, %f)\\n\", pathDepth, throughput.r, throughput.g, throughput.b);

#if defined(PARAM_DIRECT_LIGHT_SAMPLING)
				float directLightPdf;
				switch (matType) {
					case MAT_MATTE:
						directLightPdf = 1.f;
						break;
					case MAT_MATTEMIRROR:
						directLightPdf = 1.f / hitPointMat->param.matteMirror.mattePdf;
						break;
					case MAT_MATTEMETAL:
						directLightPdf = 1.f / hitPointMat->param.matteMetal.mattePdf;
						break;
					case MAT_ALLOY: {
						// Schilick's approximation
						const float c = 1.f + Dot(&rayDir, &shadeN);
						const float R0 = hitPointMat->param.alloy.R0;
						const float Re = R0 + (1.f - R0) * c * c * c * c * c;

						const float P = .25f + .5f * Re;

						directLightPdf = (1.f - P) / (1.f - Re);
						break;
					}
					default:
						directLightPdf = 0.f;
						break;
				}

				if (directLightPdf > 0.f) {
#if (PARAM_SAMPLER_TYPE == 0)
					const float ul0 = RndFloatValue(&seed);
					const float ul1 = RndFloatValue(&seed);
					const float ul2 = RndFloatValue(&seed);
#elif (PARAM_SAMPLER_TYPE == 1) || (PARAM_SAMPLER_TYPE == 2) || (PARAM_SAMPLER_TYPE == 3)
					const float ul1 = sampleData[IDX_DIRECTLIGHT_X];
					const float ul2 = sampleData[IDX_DIRECTLIGHT_Y];
					const float ul0 = sampleData[IDX_DIRECTLIGHT_Z];
#endif

					// Select a light source to sample
					const uint lightIndex = min((uint)floor(PARAM_DL_LIGHT_COUNT * ul0), (uint)(PARAM_DL_LIGHT_COUNT - 1));
					__global TriangleLight *l = &triLights[lightIndex];

					// Setup the shadow ray
					Spectrum Le;
					float lightPdf;
					Ray shadowRay;
					TriangleLight_Sample_L(l, &hitPoint, &lightPdf, &Le, &shadowRay, ul1, ul2);

					const float dp = Dot(&shadeN, &shadowRay.d);
					const float matPdf = (dp <= 0.f) ? 0.f : 1.f;

					const float pdf = lightPdf * matPdf * directLightPdf;
					if (pdf > 0.f) {
						Spectrum throughputLightDir = prevThroughput;
						throughputLightDir.r *= shadeColor.r;
						throughputLightDir.g *= shadeColor.g;
						throughputLightDir.b *= shadeColor.b;

						const float k = dp * PARAM_DL_LIGHT_COUNT / (pdf * M_PI);
						// NOTE: I assume all matte mixed material have a MatteParam as first field
						task->pathState.lightRadiance.r = throughputLightDir.r * hitPointMat->param.matte.r * k * Le.r;
						task->pathState.lightRadiance.g = throughputLightDir.g * hitPointMat->param.matte.g * k * Le.g;
						task->pathState.lightRadiance.b = throughputLightDir.b * hitPointMat->param.matte.b * k * Le.b;

						*ray = shadowRay;

						// Save data for next path vertex
						task->pathState.nextPathRay.o = hitPoint;
						task->pathState.nextPathRay.d = wi;
						task->pathState.nextPathRay.mint = PARAM_RAY_EPSILON;
						task->pathState.nextPathRay.maxt = FLT_MAX;

						task->pathState.nextThroughput = throughput;

						pathState = PATH_STATE_SAMPLE_LIGHT;
					} else {
						// Skip the shadow ray tracing step

						if ((throughput.r <= 0.f) && (throughput.g <= 0.f) && (throughput.b <= 0.f))
							pathState = PATH_STATE_DONE;
						else {
							ray->o = hitPoint;
							ray->d = wi;
							ray->mint = PARAM_RAY_EPSILON;
							ray->maxt = FLT_MAX;

							task->pathState.throughput = throughput;
							task->pathState.depth = pathDepth;

							pathState = PATH_STATE_NEXT_VERTEX;
						}
					}
				} else {
					// Skip the shadow ray tracing step

					if ((throughput.r <= 0.f) && (throughput.g <= 0.f) && (throughput.b <= 0.f))
						pathState = PATH_STATE_DONE;
					else {
						ray->o = hitPoint;
						ray->d = wi;
						ray->mint = PARAM_RAY_EPSILON;
						ray->maxt = FLT_MAX;

						task->pathState.throughput = throughput;
						task->pathState.depth = pathDepth;

						pathState = PATH_STATE_NEXT_VERTEX;
					}
				}

#else

				if ((throughput.r <= 0.f) && (throughput.g <= 0.f) && (throughput.b <= 0.f))
					pathState = PATH_STATE_DONE;
				else {
					// Setup next ray
					ray->o = hitPoint;
					ray->d = wi;
					ray->mint = PARAM_RAY_EPSILON;
					ray->maxt = FLT_MAX;

					task->pathState.throughput = throughput;
					task->pathState.depth = pathDepth;

					pathState = PATH_STATE_NEXT_VERTEX;
				}
#endif

			} else {
#if defined(PARAM_HAVE_INFINITELIGHT)
				Spectrum Le;
				InfiniteLight_Le(infiniteLightMap, &Le, &rayDir);

				/*if (task->pathState.depth > 0)
					printf(\"Throughput: (%f, %f, %f) Le: (%f, %f, %f)\\n\", throughput.r, throughput.g, throughput.b, Le.r, Le.g, Le.b);*/

				sample->radiance.r += throughput.r * Le.r;
				sample->radiance.g += throughput.g * Le.g;
				sample->radiance.b += throughput.b * Le.b;
#endif

				pathState = PATH_STATE_DONE;
			}
			break;
		}

#if defined(PARAM_DIRECT_LIGHT_SAMPLING)
		case PATH_STATE_SAMPLE_LIGHT: {
			if (currentTriangleIndex != 0xffffffffu) {
				// The shadow ray has hit something

#if defined(PARAM_HAS_TEXTUREMAPS) && defined(PARAM_HAS_ALPHA_TEXTUREMAPS)
				// Check if I have to continue to trace the shadow ray

				const uint pathDepth = task->pathState.depth;
#if (PARAM_SAMPLER_TYPE == 1) || (PARAM_SAMPLER_TYPE == 3)
				__global float *sampleData = &sample->u[IDX_BSDF_OFFSET + SAMPLE_SIZE * pathDepth];
#elif (PARAM_SAMPLER_TYPE == 2)
				__global float *sampleData = &sample->u[sample->proposed][IDX_BSDF_OFFSET + SAMPLE_SIZE * pathDepth];
#endif

				const uint meshIndex = meshIDs[currentTriangleIndex];
				__global Material *hitPointMat = &mats[meshMats[meshIndex]];
				uint matType = hitPointMat->type;

				// Interpolate UV coordinates
				UV uv;
				Mesh_InterpolateUV(vertUVs, triangles, currentTriangleIndex, hitPointB1, hitPointB2, &uv);

				// Check it the mesh has a texture map
				unsigned int texIndex = meshTexsBuff[meshIndex];
				if (texIndex != 0xffffffffu) {
					__global TexMap *texMap = &texMapDescBuff[texIndex];

					// Check if it has an alpha channel
					if (texMap->alphaOffset != 0xffffffffu) {
						const float alpha = TexMap_GetAlpha(&texMapAlphaBuff[texMap->alphaOffset], texMap->width, texMap->height, uv.u, uv.v);

#if (PARAM_SAMPLER_TYPE == 0)
						const float texAlphaSample = RndFloatValue(&seed);
#elif (PARAM_SAMPLER_TYPE == 1) || (PARAM_SAMPLER_TYPE == 2) || (PARAM_SAMPLER_TYPE == 3)
						const float texAlphaSample = sampleData[IDX_TEX_ALPHA];
#endif

						if ((alpha == 0.0f) || ((alpha < 1.f) && (texAlphaSample > alpha))) {
							// Continue to trace the ray
							matType = MAT_NULL;
						}
					}
				}

				if (matType == MAT_ARCHGLASS) {
					task->pathState.lightRadiance.r *= hitPointMat->param.archGlass.refrct_r;
					task->pathState.lightRadiance.g *= hitPointMat->param.archGlass.refrct_g;
					task->pathState.lightRadiance.b *= hitPointMat->param.archGlass.refrct_b;
				}

				if ((matType == MAT_ARCHGLASS) || (matType == MAT_NULL)) {
					const float hitPointT = rayHit->t;

					Point hitPoint;
					hitPoint.x = ray->o.x + rayDir.x * hitPointT;
					hitPoint.y = ray->o.y + rayDir.y * hitPointT;
					hitPoint.z = ray->o.z + rayDir.z * hitPointT;

					// Continue to trace the ray
					ray->o = hitPoint;
					ray->maxt -= hitPointT;
				} else
					pathState = PATH_STATE_NEXT_VERTEX;

#else
				// The light is source is not visible

				pathState = PATH_STATE_NEXT_VERTEX;
#endif

			} else {
				// The light source is visible

				sample->radiance.r += task->pathState.lightRadiance.r;
				sample->radiance.g += task->pathState.lightRadiance.g;
				sample->radiance.b += task->pathState.lightRadiance.b;

				pathState = PATH_STATE_NEXT_VERTEX;
			}


			if (pathState == PATH_STATE_NEXT_VERTEX) {
				Spectrum throughput = task->pathState.nextThroughput;
				if ((throughput.r <= 0.f) && (throughput.g <= 0.f) && (throughput.b <= 0.f))
					pathState = PATH_STATE_DONE;
				else {
					// Restore the ray for the next path vertex
					*ray = task->pathState.nextPathRay;

					task->pathState.throughput = throughput;

					// Increase path depth
					task->pathState.depth += 1;
				}
			}
			break;
		}
#endif
	}

	if (pathState == PATH_STATE_DONE) {
#if (PARAM_IMAGE_FILTER_TYPE == 0)

#if (PARAM_SAMPLER_TYPE == 0) || (PARAM_SAMPLER_TYPE == 1) || (PARAM_SAMPLER_TYPE == 3)
		Spectrum radiance = sample->radiance;
		SplatSample(frameBuffer, sample->pixelIndex, &radiance, 1.f);
#elif (PARAM_SAMPLER_TYPE == 2)


#if (PARAM_SAMPLER_TYPE != 0)
		// Read the seed
		Seed seed;
		seed.s1 = task->seed.s1;
		seed.s2 = task->seed.s2;
		seed.s3 = task->seed.s3;
#endif

		Sampler_MLT_SplatSample(frameBuffer, &seed, sample);

#if (PARAM_SAMPLER_TYPE != 0)
		// Save the seed
		task->seed.s1 = seed.s1;
		task->seed.s2 = seed.s2;
		task->seed.s3 = seed.s3;
#endif

#endif

#else

#if (PARAM_SAMPLER_TYPE == 0) || (PARAM_SAMPLER_TYPE == 1) || (PARAM_SAMPLER_TYPE == 3)
		__global float *sampleData = &sample->u[IDX_SCREEN_X];
		const float sx = sampleData[IDX_SCREEN_X] - .5f;
		const float sy = sampleData[IDX_SCREEN_Y] - .5f;

		Spectrum radiance = sample->radiance;
		SplatSample(frameBuffer, sample->pixelIndex, sx, sy, &radiance, 1.f);
#elif (PARAM_SAMPLER_TYPE == 2)

#if (PARAM_SAMPLER_TYPE != 0)
		// Read the seed
		Seed seed;
		seed.s1 = task->seed.s1;
		seed.s2 = task->seed.s2;
		seed.s3 = task->seed.s3;
#endif

		Sampler_MLT_SplatSample(frameBuffer, &seed, sample);

#if (PARAM_SAMPLER_TYPE != 0)
		// Save the seed
		task->seed.s1 = seed.s1;
		task->seed.s2 = seed.s2;
		task->seed.s3 = seed.s3;
#endif

#endif

#endif
	}

	task->pathState.state = pathState;

#if (PARAM_SAMPLER_TYPE == 0)
	// Save the seed
	task->seed.s1 = seed.s1;
	task->seed.s2 = seed.s2;
	task->seed.s3 = seed.s3;
#endif
}
