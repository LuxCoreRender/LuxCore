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
		__global Ray *rays,
		__global Camera *camera
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
	GenerateCameraPath(task, &rays[gid], &seed, camera);

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
#if defined(PARAM_ENABLE_ALPHA_CHANNEL)
		, __global AlphaPixel *alphaFrameBuffer
#endif
		) {
	const size_t gid = get_global_id(0);
	if (gid >= (PARAM_IMAGE_WIDTH + 2) * (PARAM_IMAGE_HEIGHT + 2))
		return;

	__global Pixel *p = &frameBuffer[gid];
	p->c.r = 0.f;
	p->c.g = 0.f;
	p->c.b = 0.f;
	p->count = 0.f;

#if defined(PARAM_ENABLE_ALPHA_CHANNEL)
	__global AlphaPixel *ap = &alphaFrameBuffer[gid];
	ap->alpha = 0.f;
#endif
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
#if defined(PARAM_ACCEL_MQBVH)
		__global uint *meshFirstTriangleOffset,
		__global Mesh *meshDescs,
#endif
		__global Vector *vertNormals,
		__global Point *vertices,
		__global Triangle *triangles,
		__global Camera *camera
#if defined(PARAM_HAS_INFINITELIGHT)
		, __global InfiniteLight *infiniteLight
		, __global Spectrum *infiniteLightMap
#endif
#if defined(PARAM_HAS_SUNLIGHT)
		, __global SunLight *sunLight
#endif
#if defined(PARAM_HAS_SKYLIGHT)
		, __global SkyLight *skyLight
#endif
#if (PARAM_DL_LIGHT_COUNT > 0)
		, __global TriangleLight *triLights
#endif
#if defined(PARAM_HAS_TEXTUREMAPS)
        , __global Spectrum *texMapRGBBuff
#if defined(PARAM_HAS_ALPHA_TEXTUREMAPS)
		, __global float *texMapAlphaBuff
#endif
        , __global TexMap *texMapDescBuff
        , __global unsigned int *meshTexsBuff
#if defined(PARAM_HAS_BUMPMAPS)
        , __global unsigned int *meshBumpsBuff
		, __global float *meshBumpsScaleBuff
#endif
#if defined(PARAM_HAS_NORMALMAPS)
        , __global unsigned int *meshNormalMapsBuff
#endif
		, __global UV *vertUVs
#endif
#if defined(PARAM_ENABLE_ALPHA_CHANNEL)
		, __global AlphaPixel *alphaFrameBuffer
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
			uint pathDepth = task->pathState.depth;

			if (currentTriangleIndex != 0xffffffffu) {
				// Something was hit

#if (PARAM_SAMPLER_TYPE == 1) || (PARAM_SAMPLER_TYPE == 3)
				__global float *sampleData = &sample->u[IDX_BSDF_OFFSET + SAMPLE_SIZE * pathDepth];
#elif (PARAM_SAMPLER_TYPE == 2)
				__global float *sampleData = &sample->u[sample->proposed][IDX_BSDF_OFFSET + SAMPLE_SIZE * pathDepth];
#endif

				const uint meshIndex = meshIDs[currentTriangleIndex];
#if defined(PARAM_ACCEL_MQBVH)
				__global Mesh *meshDesc = &meshDescs[meshIndex];
				__global Point *iVertices = &vertices[meshDesc->vertsOffset];
				__global Vector *iVertNormals = &vertNormals[meshDesc->vertsOffset];
#if defined(PARAM_HAS_TEXTUREMAPS)
				__global UV *iVertUVs = &vertUVs[meshDesc->vertsOffset];
#endif
				__global Triangle *iTriangles = &triangles[meshDesc->trisOffset];
				const uint triangleID = currentTriangleIndex - meshFirstTriangleOffset[meshIndex];
#endif
				__global Material *hitPointMat = &mats[meshMats[meshIndex]];
				uint matType = hitPointMat->type;

#if defined(PARAM_HAS_TEXTUREMAPS)
				Spectrum shadeColor;

				// Interpolate UV coordinates
				UV uv;
#if defined(PARAM_ACCEL_MQBVH)
				Mesh_InterpolateUV(iVertUVs, iTriangles, triangleID, hitPointB1, hitPointB2, &uv);
#else
				Mesh_InterpolateUV(vertUVs, triangles, currentTriangleIndex, hitPointB1, hitPointB2, &uv);
#endif
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

					shadeColor.r = texColor.r;
					shadeColor.g = texColor.g;
					shadeColor.b = texColor.b;
				} else {
					shadeColor.r = 1.f;
					shadeColor.g = 1.f;
					shadeColor.b = 1.f;
				}

				throughput.r *= shadeColor.r;
				throughput.g *= shadeColor.g;
				throughput.b *= shadeColor.b;
#endif

				// Interpolate the normal
				Vector N;
#if defined(PARAM_ACCEL_MQBVH)
				Mesh_InterpolateNormal(iVertNormals, iTriangles, triangleID, hitPointB1, hitPointB2, &N);
				TransformNormal(meshDesc->invTrans, &N);
#else
				Mesh_InterpolateNormal(vertNormals, triangles, currentTriangleIndex, hitPointB1, hitPointB2, &N);
#endif

#if defined(PARAM_HAS_NORMALMAPS)
				// Check it the mesh has a bump map
				unsigned int normalMapIndex = meshNormalMapsBuff[meshIndex];
				if (normalMapIndex != 0xffffffffu) {
					// Apply normal mapping
					__global TexMap *texMap = &texMapDescBuff[normalMapIndex];
					const uint texWidth = texMap->width;
					const uint texHeight = texMap->height;

					Spectrum texColor;
					TexMap_GetColor(&texMapRGBBuff[texMap->rgbOffset], texWidth, texHeight, uv.u, uv.v, &texColor);
					const float x = 2.f * (texColor.r - 0.5f);
					const float y = 2.f * (texColor.g - 0.5f);
					const float z = 2.f * (texColor.b - 0.5f);

					Vector v1, v2;
					CoordinateSystem(&N, &v1, &v2);
					N.x = v1.x * x + v2.x * y + N.x * z;
					N.y = v1.y * x + v2.y * y + N.y * z;
					N.z = v1.z * x + v2.z * y + N.z * z;
					Normalize(&N);
				}
#endif

#if defined(PARAM_HAS_BUMPMAPS)
				// Check it the mesh has a bump map
				unsigned int bumpIndex = meshBumpsBuff[meshIndex];
				if (bumpIndex != 0xffffffffu) {
					// Apply bump mapping
					__global TexMap *texMap = &texMapDescBuff[bumpIndex];
					const uint texWidth = texMap->width;
					const uint texHeight = texMap->height;

					UV dudv;
					dudv.u = 1.f / texWidth;
					dudv.v = 1.f / texHeight;

					Spectrum texColor;
					TexMap_GetColor(&texMapRGBBuff[texMap->rgbOffset], texWidth, texHeight, uv.u, uv.v, &texColor);
					const float b0 = Spectrum_Y(&texColor);

					TexMap_GetColor(&texMapRGBBuff[texMap->rgbOffset], texWidth, texHeight, uv.u + dudv.u, uv.v, &texColor);
					const float bu = Spectrum_Y(&texColor);

					TexMap_GetColor(&texMapRGBBuff[texMap->rgbOffset], texWidth, texHeight, uv.u, uv.v + dudv.v, &texColor);
					const float bv = Spectrum_Y(&texColor);

					const float scale = meshBumpsScaleBuff[meshIndex];
					Vector bump;
					bump.x = scale * (bu - b0);
					bump.y = scale * (bv - b0);
					bump.z = 1.f;

					Vector v1, v2;
					CoordinateSystem(&N, &v1, &v2);
					N.x = v1.x * bump.x + v2.x * bump.y + N.x * bump.z;
					N.y = v1.y * bump.x + v2.y * bump.y + N.y * bump.z;
					N.z = v1.z * bump.x + v2.z * bump.y + N.z * bump.z;
					Normalize(&N);
				}
#endif

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
				Spectrum f;
				f.r = 1.f;
				f.g = 1.f;
				f.b = 1.f;
				float materialPdf;
#if defined(PARAM_DIRECT_LIGHT_SAMPLING) || (PARAM_MAX_DIFFUSE_PATH_VERTEX_COUNT < PARAM_MAX_PATH_DEPTH)
				int specularMaterial;
#endif

				switch (matType) {

#if defined(PARAM_ENABLE_MAT_MATTE)
					case MAT_MATTE:
						Matte_Sample_f(&hitPointMat->param.matte, &wo, &wi, &materialPdf, &f, &shadeN, u0, u1
#if defined(PARAM_DIRECT_LIGHT_SAMPLING) || (PARAM_MAX_DIFFUSE_PATH_VERTEX_COUNT < PARAM_MAX_PATH_DEPTH)
								, &specularMaterial
#endif
								);
						break;
#endif

#if (PARAM_DL_LIGHT_COUNT > 0)
					case MAT_AREALIGHT: {
						Spectrum Le;
						AreaLight_Le(&hitPointMat->param.areaLight, &wo, &N, &Le);

#if defined(PARAM_DIRECT_LIGHT_SAMPLING)
						if (!task->pathState.specularBounce) {
#if defined(PARAM_HAS_SUNLIGHT)
							const uint lightSourceCount = PARAM_DL_LIGHT_COUNT + 1;
#else
							const uint lightSourceCount = PARAM_DL_LIGHT_COUNT;
#endif
#if defined(PARAM_ACCEL_MQBVH)
							const float area = InstanceMesh_Area(meshDesc->trans, iVertices, iTriangles, triangleID);
#else
							const float area = Mesh_Area(vertices, triangles, currentTriangleIndex);
#endif
							const float lpdf = lightSourceCount / area;
							const float ph = PowerHeuristic(1, task->pathState.bouncePdf, 1, lpdf);

							Le.r *= ph;
							Le.g *= ph;
							Le.b *= ph;
						}
#endif

						sample->radiance.r += throughput.r * Le.r;
						sample->radiance.g += throughput.g * Le.g;
						sample->radiance.b += throughput.b * Le.b;

#if defined(PARAM_DIRECT_LIGHT_SAMPLING) || (PARAM_MAX_DIFFUSE_PATH_VERTEX_COUNT < PARAM_MAX_PATH_DEPTH)
						specularMaterial = 0;
#endif
						materialPdf = 0.f;
						break;
					}
#endif

#if defined(PARAM_ENABLE_MAT_MIRROR)
					case MAT_MIRROR:
						Mirror_Sample_f(&hitPointMat->param.mirror, &wo, &wi, &materialPdf, &f, &shadeN
#if defined(PARAM_DIRECT_LIGHT_SAMPLING) || (PARAM_MAX_DIFFUSE_PATH_VERTEX_COUNT < PARAM_MAX_PATH_DEPTH)
								, &specularMaterial
#endif
								);
						break;
#endif

#if defined(PARAM_ENABLE_MAT_GLASS)
					case MAT_GLASS:
						Glass_Sample_f(&hitPointMat->param.glass, &wo, &wi, &materialPdf, &f, &N, &shadeN, u0
#if defined(PARAM_DIRECT_LIGHT_SAMPLING) || (PARAM_MAX_DIFFUSE_PATH_VERTEX_COUNT < PARAM_MAX_PATH_DEPTH)
								, &specularMaterial
#endif
								);
						break;
#endif

#if defined(PARAM_ENABLE_MAT_MATTEMIRROR)
					case MAT_MATTEMIRROR:
						MatteMirror_Sample_f(&hitPointMat->param.matteMirror, &wo, &wi, &materialPdf, &f, &shadeN, u0, u1, u2
#if defined(PARAM_DIRECT_LIGHT_SAMPLING) || (PARAM_MAX_DIFFUSE_PATH_VERTEX_COUNT < PARAM_MAX_PATH_DEPTH)
								, &specularMaterial
#endif
								);
						break;
#endif

#if defined(PARAM_ENABLE_MAT_METAL)
					case MAT_METAL:
						Metal_Sample_f(&hitPointMat->param.metal, &wo, &wi, &materialPdf, &f, &shadeN, u0, u1
#if defined(PARAM_DIRECT_LIGHT_SAMPLING) || (PARAM_MAX_DIFFUSE_PATH_VERTEX_COUNT < PARAM_MAX_PATH_DEPTH)
								, &specularMaterial
#endif
								);
						break;
#endif

#if defined(PARAM_ENABLE_MAT_MATTEMETAL)
					case MAT_MATTEMETAL:
						MatteMetal_Sample_f(&hitPointMat->param.matteMetal, &wo, &wi, &materialPdf, &f, &shadeN, u0, u1, u2
#if defined(PARAM_DIRECT_LIGHT_SAMPLING) || (PARAM_MAX_DIFFUSE_PATH_VERTEX_COUNT < PARAM_MAX_PATH_DEPTH)
								, &specularMaterial
#endif
								);
						break;
#endif

#if defined(PARAM_ENABLE_MAT_ALLOY)
					case MAT_ALLOY:
						Alloy_Sample_f(&hitPointMat->param.alloy, &wo, &wi, &materialPdf, &f, &shadeN, u0, u1, u2
#if defined(PARAM_DIRECT_LIGHT_SAMPLING) || (PARAM_MAX_DIFFUSE_PATH_VERTEX_COUNT < PARAM_MAX_PATH_DEPTH)
								, &specularMaterial
#endif
								);
						break;
#endif

#if defined(PARAM_ENABLE_MAT_ARCHGLASS)
					case MAT_ARCHGLASS:
						ArchGlass_Sample_f(&hitPointMat->param.archGlass, &wo, &wi, &materialPdf, &f, &N, &shadeN, u0
#if defined(PARAM_DIRECT_LIGHT_SAMPLING) || (PARAM_MAX_DIFFUSE_PATH_VERTEX_COUNT < PARAM_MAX_PATH_DEPTH)
								, &specularMaterial
#endif
								);
						break;
#endif

					case MAT_NULL:
						wi = rayDir;
#if defined(PARAM_DIRECT_LIGHT_SAMPLING) || (PARAM_MAX_DIFFUSE_PATH_VERTEX_COUNT < PARAM_MAX_PATH_DEPTH)
						specularMaterial = 1;
#endif
						materialPdf = 1.f;

						// I have also to restore the original throughput
						throughput = prevThroughput;
						break;

					default:
						// Huston, we have a problem...
#if defined(PARAM_DIRECT_LIGHT_SAMPLING) || (PARAM_MAX_DIFFUSE_PATH_VERTEX_COUNT < PARAM_MAX_PATH_DEPTH)
						specularMaterial = 1;
#endif
						materialPdf = 0.f;
						break;
				}

#if defined(PARAM_ENABLE_ALPHA_CHANNEL)
				if (matType != MAT_NULL)
					task->pathState.vertexCount += 1;
#endif

				// Russian roulette

#if (PARAM_SAMPLER_TYPE == 0)
				const float rrSample = RndFloatValue(&seed);
#elif (PARAM_SAMPLER_TYPE == 1) || (PARAM_SAMPLER_TYPE == 2) || (PARAM_SAMPLER_TYPE == 3)
				const float rrSample = sampleData[IDX_RR];
#endif

				const float rrProb = max(max(throughput.r, max(throughput.g, throughput.b)), (float) PARAM_RR_CAP);
				pathDepth += 1;
				float invRRProb = (pathDepth > PARAM_RR_DEPTH) ? ((rrProb < rrSample) ? 0.f : (1.f / rrProb)) : 1.f;
#if (PARAM_MAX_DIFFUSE_PATH_VERTEX_COUNT < PARAM_MAX_PATH_DEPTH)
				const uint diffuseVertexCount = task->pathState.diffuseVertexCount + (specularMaterial ? 0 : 1);
#endif
				invRRProb = ((materialPdf <= 0.f) || (pathDepth >= PARAM_MAX_PATH_DEPTH)
#if (PARAM_MAX_DIFFUSE_PATH_VERTEX_COUNT < PARAM_MAX_PATH_DEPTH)
						|| (diffuseVertexCount > PARAM_MAX_DIFFUSE_PATH_VERTEX_COUNT)
#endif
						) ? 0.f : invRRProb;

				throughput.r *= f.r * invRRProb;
				throughput.g *= f.g * invRRProb;
				throughput.b *= f.b * invRRProb;

				//if (pathDepth > 2)
				//	printf(\"Depth: %d Throughput: (%f, %f, %f)\\n\", pathDepth, throughput.r, throughput.g, throughput.b);

#if defined(PARAM_DIRECT_LIGHT_SAMPLING)
				float directLightPdf;
				switch (matType) {
					case MAT_MATTE:
						directLightPdf = 1.f;
						break;
					case MAT_MATTEMIRROR:
						directLightPdf = hitPointMat->param.matteMirror.mattePdf;
						break;
					case MAT_MATTEMETAL:
						directLightPdf = hitPointMat->param.matteMetal.mattePdf;
						break;
					case MAT_ALLOY: {
						// Schilick's approximation
						const float c = 1.f + Dot(&rayDir, &shadeN);
						const float R0 = hitPointMat->param.alloy.R0;
						const float Re = R0 + (1.f - R0) * c * c * c * c * c;

						const float P = .25f + .5f * Re;

						directLightPdf = 1.f - P;
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

					// Setup the shadow ray
					Spectrum Le;
					uint lightSourceCount;
					float lightPdf;
					float lPdf; // pdf used for MIS
					Ray shadowRay;

#if defined(PARAM_HAS_SUNLIGHT) && (PARAM_DL_LIGHT_COUNT == 0)
					//----------------------------------------------------------
					// This is the case with only the sun light
					//----------------------------------------------------------

					SunLight_Sample_L(sunLight, &hitPoint, &lightPdf, &Le, &shadowRay, ul1, ul2);
					lPdf = lightPdf;
					lightSourceCount = 1;

					//----------------------------------------------------------
#elif defined(PARAM_HAS_SUNLIGHT) && (PARAM_DL_LIGHT_COUNT > 0)
					//----------------------------------------------------------
					// This is the case with sun light and area lights
					//----------------------------------------------------------

					// Select one of the lights
					const uint lightIndex = min((uint)floor((PARAM_DL_LIGHT_COUNT + 1)* ul0), (uint)(PARAM_DL_LIGHT_COUNT));
					lightSourceCount = PARAM_DL_LIGHT_COUNT + 1;

					if (lightIndex == PARAM_DL_LIGHT_COUNT) {
						// The sun light was selected

						SunLight_Sample_L(sunLight, &hitPoint, &lightPdf, &Le, &shadowRay, ul1, ul2);
						lPdf = lightPdf;
					} else {
						// An area light was selected

						__global TriangleLight *l = &triLights[lightIndex];
						TriangleLight_Sample_L(l, &hitPoint, &lightPdf, &Le, &shadowRay, ul1, ul2);
						lPdf = PARAM_DL_LIGHT_COUNT / l->area;
					}

					//----------------------------------------------------------
#elif !defined(PARAM_HAS_SUNLIGHT) && (PARAM_DL_LIGHT_COUNT > 0)
					//----------------------------------------------------------
					// This is the case without sun light and with area lights
					//----------------------------------------------------------

					// Select one of the area lights
					const uint lightIndex = min((uint)floor(PARAM_DL_LIGHT_COUNT * ul0), (uint)(PARAM_DL_LIGHT_COUNT - 1));
					__global TriangleLight *l = &triLights[lightIndex];

					TriangleLight_Sample_L(l, &hitPoint, &lightPdf, &Le, &shadowRay, ul1, ul2);
					lPdf = PARAM_DL_LIGHT_COUNT / l->area;
					lightSourceCount = PARAM_DL_LIGHT_COUNT;

					//----------------------------------------------------------
#else
Error: Huston, we have a problem !
#endif

					const float dp = Dot(&shadeN, &shadowRay.d);
					const float matPdf = M_PI;

					const float mPdf = directLightPdf * dp * INV_PI;
					const float pdf = (dp <= 0.f) ? 0.f :
						(PowerHeuristic(1, lPdf, 1, mPdf) * lightPdf * directLightPdf * matPdf / (dp * lightSourceCount));
					if (pdf > 0.f) {
						Spectrum throughputLightDir = prevThroughput;
#if defined(PARAM_HAS_TEXTUREMAPS)
						throughputLightDir.r *= shadeColor.r;
						throughputLightDir.g *= shadeColor.g;
						throughputLightDir.b *= shadeColor.b;
#endif

						const float k = 1.f / pdf;
						// NOTE: I assume all matte mixed material have a MatteParam as first field
						task->pathState.lightRadiance.r = throughputLightDir.r * hitPointMat->param.matte.r * Le.r * k;
						task->pathState.lightRadiance.g = throughputLightDir.g * hitPointMat->param.matte.g * Le.g * k;
						task->pathState.lightRadiance.b = throughputLightDir.b * hitPointMat->param.matte.b * Le.b * k;

						*ray = shadowRay;

						// Save data for next path vertex
						task->pathState.nextPathRay.o = hitPoint;
						task->pathState.nextPathRay.d = wi;
						task->pathState.nextPathRay.mint = PARAM_RAY_EPSILON;
						task->pathState.nextPathRay.maxt = FLT_MAX;

						task->pathState.bouncePdf = materialPdf;
						task->pathState.specularBounce = specularMaterial;
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

							task->pathState.bouncePdf = materialPdf;
							task->pathState.specularBounce = specularMaterial;
							task->pathState.throughput = throughput;
							task->pathState.depth = pathDepth;
#if (PARAM_MAX_DIFFUSE_PATH_VERTEX_COUNT < PARAM_MAX_PATH_DEPTH)
							task->pathState.diffuseVertexCount = diffuseVertexCount;
#endif

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

						task->pathState.bouncePdf = materialPdf;
						task->pathState.specularBounce = specularMaterial;
						task->pathState.throughput = throughput;
						task->pathState.depth = pathDepth;
#if (PARAM_MAX_DIFFUSE_PATH_VERTEX_COUNT < PARAM_MAX_PATH_DEPTH)
						task->pathState.diffuseVertexCount = diffuseVertexCount;
#endif

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
#if (PARAM_MAX_DIFFUSE_PATH_VERTEX_COUNT < PARAM_MAX_PATH_DEPTH)
					task->pathState.diffuseVertexCount = diffuseVertexCount;
#endif

					pathState = PATH_STATE_NEXT_VERTEX;
				}
#endif

			} else {
				// Nothing was hit

#if defined(PARAM_HAS_INFINITELIGHT)
				Spectrum iLe;
				InfiniteLight_Le(infiniteLight, infiniteLightMap, &iLe, &rayDir);

				sample->radiance.r += throughput.r * iLe.r;
				sample->radiance.g += throughput.g * iLe.g;
				sample->radiance.b += throughput.b * iLe.b;
#endif

#if defined(PARAM_HAS_SUNLIGHT)
				// Make the sun visible only if relsize has been changed (in order
				// to avoid fireflies).
				if (sunLight->relSize > 5.f) {
					Spectrum sLe;
					SunLight_Le(sunLight, &sLe, &rayDir);

					if (!task->pathState.specularBounce) {
						const float lpdf = UniformConePdf(sunLight->cosThetaMax);
						const float ph = PowerHeuristic(1, task->pathState.bouncePdf, 1, lpdf);

						sLe.r *= ph;
						sLe.g *= ph;
						sLe.b *= ph;
					}

					sample->radiance.r += throughput.r * sLe.r;
					sample->radiance.g += throughput.g * sLe.g;
					sample->radiance.b += throughput.b * sLe.b;
				}
#endif

#if defined(PARAM_HAS_SKYLIGHT)
				Spectrum skLe;
				SkyLight_Le(skyLight, &skLe, &rayDir);

				sample->radiance.r += throughput.r * skLe.r;
				sample->radiance.g += throughput.g * skLe.g;
				sample->radiance.b += throughput.b * skLe.b;
#endif

#if defined(PARAM_ENABLE_ALPHA_CHANNEL)
				// TODO: add support for texture alpha channel
				if (task->pathState.vertexCount == 0)
					task->pathState.alpha = 0.f;
#endif

				pathState = PATH_STATE_DONE;
			}
			break;
		}

#if defined(PARAM_DIRECT_LIGHT_SAMPLING)
		case PATH_STATE_SAMPLE_LIGHT: {
			if (currentTriangleIndex != 0xffffffffu) {
				// The shadow ray has hit something

#if (defined(PARAM_HAS_TEXTUREMAPS) && defined(PARAM_HAS_ALPHA_TEXTUREMAPS)) || defined(PARAM_ENABLE_MAT_ARCHGLASS)
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

#if defined(PARAM_HAS_TEXTUREMAPS) && defined(PARAM_HAS_ALPHA_TEXTUREMAPS)
				// Interpolate UV coordinates
				UV uv;
#if defined(PARAM_ACCEL_MQBVH)
				__global Mesh *meshDesc = &meshDescs[meshIndex];
				__global UV *iVertUVs = &vertUVs[meshDesc->vertsOffset];
				__global Triangle *iTriangles = &triangles[meshDesc->trisOffset];
				const uint triangleID = currentTriangleIndex - meshFirstTriangleOffset[meshIndex];
				Mesh_InterpolateUV(iVertUVs, iTriangles, triangleID, hitPointB1, hitPointB2, &uv);
#else
				Mesh_InterpolateUV(vertUVs, triangles, currentTriangleIndex, hitPointB1, hitPointB2, &uv);
#endif

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
#endif
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
#if (PARAM_SAMPLER_TYPE == 2)

		// Read the seed
		Seed seed;
		seed.s1 = task->seed.s1;
		seed.s2 = task->seed.s2;
		seed.s3 = task->seed.s3;

		Sampler_MLT_SplatSample(frameBuffer,
#if defined(PARAM_ENABLE_ALPHA_CHANNEL)
			alphaFrameBuffer,
#endif
			&seed, sample
#if defined(PARAM_ENABLE_ALPHA_CHANNEL)
			, task->pathState.alpha
#endif
			);

		// Save the seed
		task->seed.s1 = seed.s1;
		task->seed.s2 = seed.s2;
		task->seed.s3 = seed.s3;

#else

#if (PARAM_IMAGE_FILTER_TYPE == 0)
		Spectrum radiance = sample->radiance;
		SplatSample(frameBuffer,
#if defined(PARAM_ENABLE_ALPHA_CHANNEL)
			alphaFrameBuffer,
#endif
			sample->pixelIndex, &radiance,
#if defined(PARAM_ENABLE_ALPHA_CHANNEL)
			task->pathState.alpha,
#endif
			1.f);
#else
		__global float *sampleData = &sample->u[0];
		const float sx = sampleData[IDX_SCREEN_X] - .5f;
		const float sy = sampleData[IDX_SCREEN_Y] - .5f;

		Spectrum radiance = sample->radiance;
		SplatSample(frameBuffer,
#if defined(PARAM_ENABLE_ALPHA_CHANNEL)
			alphaFrameBuffer,
#endif
			sample->pixelIndex, sx, sy, &radiance,
#if defined(PARAM_ENABLE_ALPHA_CHANNEL)
			task->pathState.alpha,
#endif
			1.f);
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
