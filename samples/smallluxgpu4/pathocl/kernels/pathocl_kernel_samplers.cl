#line 2 "patchocl_kernel_samplers.cl"

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

__global float *GetSampleData(__global float *sampleData) {
	const size_t gid = get_global_id(0);
	return &sampleData[gid * TOTAL_U_SIZE];
}

float Dot(const Vector *v0, const Vector *v1) {
	return v0->x * v1->x + v0->y * v1->y + v0->z * v1->z;
}

void Normalize(Vector *v) {
	const float il = 1.f / sqrt(Dot(v, v));

	v->x *= il;
	v->y *= il;
	v->z *= il;
}

void GenerateCameraRay(
		__global Camera *camera,
		__global Sample *sample,
		__global float *sampleData,
#if (PARAM_SAMPLER_TYPE == 0)
		Seed *seed,
#endif
		__global Ray *ray) {
#if (PARAM_SAMPLER_TYPE == 0)
	const uint pixelIndex = sample->pixelIndex;

	const float scrSampleX = sampleData[IDX_SCREEN_X];
	const float scrSampleY = sampleData[IDX_SCREEN_Y];

	const float screenX = pixelIndex % PARAM_IMAGE_WIDTH + scrSampleX - .5f;
	const float screenY = pixelIndex / PARAM_IMAGE_WIDTH + scrSampleY - .5f;
#elif (PARAM_SAMPLER_TYPE == 2)
	__global float *sampleData = &sample->u[sample->proposed][0];
	const float screenX = min(sampleData[IDX_SCREEN_X] * PARAM_IMAGE_WIDTH, (float)(PARAM_IMAGE_WIDTH - 1));
	const float screenY = min(sampleData[IDX_SCREEN_Y] * PARAM_IMAGE_HEIGHT, (float)(PARAM_IMAGE_HEIGHT - 1));
#endif

	Point Pras;
	Pras.x = screenX;
	Pras.y = PARAM_IMAGE_HEIGHT - screenY - 1.f;
	Pras.z = 0;

	Point orig;
	// RasterToCamera(Pras, &orig);

	const float iw = 1.f / (camera->rasterToCameraMatrix[3][0] * Pras.x + camera->rasterToCameraMatrix[3][1] * Pras.y + camera->rasterToCameraMatrix[3][2] * Pras.z + camera->rasterToCameraMatrix[3][3]);
	orig.x = (camera->rasterToCameraMatrix[0][0] * Pras.x + camera->rasterToCameraMatrix[0][1] * Pras.y + camera->rasterToCameraMatrix[0][2] * Pras.z + camera->rasterToCameraMatrix[0][3]) * iw;
	orig.y = (camera->rasterToCameraMatrix[1][0] * Pras.x + camera->rasterToCameraMatrix[1][1] * Pras.y + camera->rasterToCameraMatrix[1][2] * Pras.z + camera->rasterToCameraMatrix[1][3]) * iw;
	orig.z = (camera->rasterToCameraMatrix[2][0] * Pras.x + camera->rasterToCameraMatrix[2][1] * Pras.y + camera->rasterToCameraMatrix[2][2] * Pras.z + camera->rasterToCameraMatrix[2][3]) * iw;

	Vector dir;
	dir.x = orig.x;
	dir.y = orig.y;
	dir.z = orig.z;

	const float hither = camera->hither;

#if defined(PARAM_CAMERA_HAS_DOF)

#if (PARAM_SAMPLER_TYPE == 0)
	const float dofSampleX = Rnd_FloatValue(seed);
	const float dofSampleY = Rnd_FloatValue(seed);
#elif (PARAM_SAMPLER_TYPE == 1)
	const float dofSampleX = sampleData[IDX_DOF_X];
	const float dofSampleY = sampleData[IDX_DOF_Y];
#endif

	// Sample point on lens
	float lensU, lensV;
	ConcentricSampleDisk(dofSampleX, dofSampleY, &lensU, &lensV);
	const float lensRadius = camera->lensRadius;
	lensU *= lensRadius;
	lensV *= lensRadius;

	// Compute point on plane of focus
	const float focalDistance = camera->focalDistance;
	const float dist = focalDistance - hither;
	const float ft = dist / dir.z;
	Point Pfocus;
	Pfocus.x = orig.x + dir.x * ft;
	Pfocus.y = orig.y + dir.y * ft;
	Pfocus.z = orig.z + dir.z * ft;

	// Update ray for effect of lens
	const float k = dist / focalDistance;
	orig.x += lensU * k;
	orig.y += lensV * k;

	dir.x = Pfocus.x - orig.x;
	dir.y = Pfocus.y - orig.y;
	dir.z = Pfocus.z - orig.z;
#endif

	Normalize(&dir);

	// CameraToWorld(*ray, ray);
	Point torig;
	const float iw2 = 1.f / (camera->cameraToWorldMatrix[3][0] * orig.x + camera->cameraToWorldMatrix[3][1] * orig.y + camera->cameraToWorldMatrix[3][2] * orig.z + camera->cameraToWorldMatrix[3][3]);
	torig.x = (camera->cameraToWorldMatrix[0][0] * orig.x + camera->cameraToWorldMatrix[0][1] * orig.y + camera->cameraToWorldMatrix[0][2] * orig.z + camera->cameraToWorldMatrix[0][3]) * iw2;
	torig.y = (camera->cameraToWorldMatrix[1][0] * orig.x + camera->cameraToWorldMatrix[1][1] * orig.y + camera->cameraToWorldMatrix[1][2] * orig.z + camera->cameraToWorldMatrix[1][3]) * iw2;
	torig.z = (camera->cameraToWorldMatrix[2][0] * orig.x + camera->cameraToWorldMatrix[2][1] * orig.y + camera->cameraToWorldMatrix[2][2] * orig.z + camera->cameraToWorldMatrix[2][3]) * iw2;

	Vector tdir;
	tdir.x = camera->cameraToWorldMatrix[0][0] * dir.x + camera->cameraToWorldMatrix[0][1] * dir.y + camera->cameraToWorldMatrix[0][2] * dir.z;
	tdir.y = camera->cameraToWorldMatrix[1][0] * dir.x + camera->cameraToWorldMatrix[1][1] * dir.y + camera->cameraToWorldMatrix[1][2] * dir.z;
	tdir.z = camera->cameraToWorldMatrix[2][0] * dir.x + camera->cameraToWorldMatrix[2][1] * dir.y + camera->cameraToWorldMatrix[2][2] * dir.z;

	ray->o = torig;
	ray->d = tdir;
	ray->mint = PARAM_RAY_EPSILON;
	ray->maxt = (camera->yon - hither) / dir.z;

	/*printf("(%f, %f, %f) (%f, %f, %f) [%f, %f]\n",
		ray->o.x, ray->o.y, ray->o.z, ray->d.x, ray->d.y, ray->d.z,
		ray->mint, ray->maxt);*/
}

void GenerateCameraPath(
		__global GPUTask *task,
		__global float *sampleData,
#if (PARAM_SAMPLER_TYPE == 0)
		Seed *seed,
#endif
		__global Camera *camera,
		__global Ray *ray) {
	__global Sample *sample = &task->sample;

	GenerateCameraRay(camera, sample, sampleData,
#if (PARAM_SAMPLER_TYPE == 0)
			seed,
#endif
			ray);

	sample->radiance.r = 0.f;
	sample->radiance.g = 0.f;
	sample->radiance.b = 0.f;

	// Initialize the path state
	task->pathStateBase.state = RT_NEXT_VERTEX;
	task->pathStateBase.depth = 0;
	task->pathStateBase.throughput.r = 1.f;
	task->pathStateBase.throughput.g = 1.f;
	task->pathStateBase.throughput.b = 1.f;
#if defined(PARAM_DIRECT_LIGHT_SAMPLING)
	task->pathStateDirectLight.bouncePdf = 1.f;
	task->pathStateDirectLight.specularBounce = TRUE;
#endif
#if defined(PARAM_ENABLE_ALPHA_CHANNEL)
	task->pathStateAlphaChannel.vertexCount = 0;
	task->pathStateAlphaChannel.alpha = 1.f;
#endif
}

//------------------------------------------------------------------------------
// Random Sampler Kernel
//------------------------------------------------------------------------------

#if (PARAM_SAMPLER_TYPE == 0)

void Sampler_Init(Seed *seed, __global Sample *sample, __global float *sampleData) {
	const size_t gid = get_global_id(0);

	sample->radiance.r = 0.f;
	sample->radiance.g = 0.f;
	sample->radiance.b = 0.f;
	sample->pixelIndex = InitialPixelIndex(gid);

	sampleData[IDX_SCREEN_X] = Rnd_FloatValue(seed);
	sampleData[IDX_SCREEN_Y] = Rnd_FloatValue(seed);
}

void Sampler_NextSample(
		__global GPUTask *task,
		__global Sample *sample,
		__global float *sampleData,
		Seed *seed,
		__global Pixel *frameBuffer,
		__global Ray *ray,
		__global Camera *camera
		) {
	const uint pixelIndex = sample->pixelIndex;
	Spectrum radiance = sample->radiance;
	SplatSample(frameBuffer,
#if defined(PARAM_ENABLE_ALPHA_CHANNEL)
				alphaFrameBuffer,
#endif
				pixelIndex, &radiance,
#if defined(PARAM_ENABLE_ALPHA_CHANNEL)
				contribAlpha,
#endif
				1.f);

	// Move to the next assigned pixel
	sample->pixelIndex = NextPixelIndex(pixelIndex);

	sample->u[IDX_SCREEN_X] = Rnd_FloatValue(seed);
	sample->u[IDX_SCREEN_Y] = Rnd_FloatValue(seed);

	GenerateCameraPath(task, sampleData,
#if (PARAM_SAMPLER_TYPE == 0)
			seed,
#endif
			camera, ray);
}

#endif

//------------------------------------------------------------------------------
// Metropolis Sampler Kernel
//------------------------------------------------------------------------------

#if (PARAM_SAMPLER_TYPE == 1)

void LargeStep(Seed *seed, const uint largeStepCount, __global float *proposedU) {
	for (int i = 0; i < TOTAL_U_SIZE; ++i)
		proposedU[i] = Rnd_FloatValue(seed);
}

float Mutate(Seed *seed, const float x) {
	const float s1 = 1.f / 512.f;
	const float s2 = 1.f / 16.f;

	const float randomValue = Rnd_FloatValue(seed);

	const float dx = s1 / (s1 / s2 + fabs(2.f * randomValue - 1.f)) -
		s1 / (s1 / s2 + 1.f);

	float mutatedX = x;
	if (randomValue < 0.5f) {
		mutatedX += dx;
		mutatedX = (mutatedX < 1.f) ? mutatedX : (mutatedX - 1.f);
	} else {
		mutatedX -= dx;
		mutatedX = (mutatedX < 0.f) ? (mutatedX + 1.f) : mutatedX;
	}

	return mutatedX;
}

float MutateScaled(Seed *seed, const float x, const float range) {
	const float s1 = 32.f;

	const float randomValue = Rnd_FloatValue(seed);

	const float dx = range / (s1 / (1.f + s1) + (s1 * s1) / (1.f + s1) *
		fabs(2.f * randomValue - 1.f)) - range / s1;

	float mutatedX = x;
	if (randomValue < 0.5f) {
		mutatedX += dx;
		mutatedX = (mutatedX < 1.f) ? mutatedX : (mutatedX - 1.f);
	} else {
		mutatedX -= dx;
		mutatedX = (mutatedX < 0.f) ? (mutatedX + 1.f) : mutatedX;
	}

	return mutatedX;
}

void SmallStep(Seed *seed, __global float *currentU, __global float *proposedU) {
	proposedU[IDX_SCREEN_X] = MutateScaled(seed, currentU[IDX_SCREEN_X],
			PARAM_SAMPLER_METROPOLIS_IMAGE_MUTATION_RANGE);
	proposedU[IDX_SCREEN_Y] = MutateScaled(seed, currentU[IDX_SCREEN_Y],
			PARAM_SAMPLER_METROPOLIS_IMAGE_MUTATION_RANGE);

	for (int i = IDX_SCREEN_Y + 1; i < TOTAL_U_SIZE; ++i)
		proposedU[i] = Mutate(seed, currentU[i]);
}

void Sampler_Init(const size_t gid, Seed *seed, __global Sample *sample) {
	sample->totalI = 0.f;
	sample->largeMutationCount = 0.f;

	sample->current = 0xffffffffu;
	sample->proposed = 1;

	sample->smallMutationCount = 0;
	sample->consecutiveRejects = 0;

	sample->weight = 0.f;
	sample->currentRadiance.r = 0.f;
	sample->currentRadiance.g = 0.f;
	sample->currentRadiance.b = 0.f;
#if defined(PARAM_ENABLE_ALPHA_CHANNEL)
	sample->currentAlpha = 0.f;
#endif

	LargeStep(seed, 0, &sample->u[1][0]);
}

//__kernel void Sampler(
//		__global GPUTask *tasks,
//		__global GPUTaskStats *taskStats,
//		__global Ray *rays,
//		__global Camera *camera) {
//	const size_t gid = get_global_id(0);
//
//	// Initialize the task
//	__global GPUTask *task = &tasks[gid];
//
//	__global Sample *sample = &task->sample;
//	const uint current = sample->current;
//
//	// Check if it is a complete path and not the very first sample
//	if ((current != 0xffffffffu) && (task->pathState.state == PATH_STATE_DONE)) {
//		// Read the seed
//		Seed seed;
//		seed.s1 = task->seed.s1;
//		seed.s2 = task->seed.s2;
//		seed.s3 = task->seed.s3;
//
//		const uint proposed = sample->proposed;
//		__global float *proposedU = &sample->u[proposed][0];
//
//		if (Rnd_FloatValue(&seed) < PARAM_SAMPLER_METROPOLIS_LARGE_STEP_RATE) {
//			LargeStep(&seed, sample->largeMutationCount, proposedU);
//			sample->smallMutationCount = 0;
//		} else {
//			__global float *currentU = &sample->u[current][0];
//
//			SmallStep(&seed, currentU, proposedU);
//			sample->smallMutationCount += 1;
//		}
//
//		taskStats[gid].sampleCount += 1;
//
//		GenerateCameraPath(task, &rays[gid], &seed, camera);
//
//		// Save the seed
//		task->seed.s1 = seed.s1;
//		task->seed.s2 = seed.s2;
//		task->seed.s3 = seed.s3;
//	}
//}

//void Sampler_MLT_SplatSample(__global Pixel *frameBuffer,
//#if defined(PARAM_ENABLE_ALPHA_CHANNEL)
//		__global AlphaPixel *alphaFrameBuffer,
//#endif
//		Seed *seed, __global Sample *sample
//#if defined(PARAM_ENABLE_ALPHA_CHANNEL)
//		, const float alpha
//#endif
//		) {
//	uint current = sample->current;
//	uint proposed = sample->proposed;
//
//	Spectrum radiance = sample->radiance;
//
//	if (current == 0xffffffffu) {
//		// It is the very first sample, I have still to initialize the current
//		// sample
//
//		sample->currentRadiance = radiance;
//#if defined(PARAM_ENABLE_ALPHA_CHANNEL)
//		sample->currentAlpha = alpha;
//#endif
//		sample->totalI = Spectrum_Y(&radiance);
//
//		// The following 2 lines could be moved in the initialization code
//		sample->largeMutationCount = 1;
//		sample->weight = 0.f;
//
//		current = proposed;
//		proposed ^= 1;
//	} else {
//		const Spectrum currentL = sample->currentRadiance;
//#if defined(PARAM_ENABLE_ALPHA_CHANNEL)
//		const float currentAlpha = sample->currentAlpha;
//#endif
//		const float currentI = Spectrum_Y(&currentL);
//
//		const Spectrum proposedL = radiance;
//#if defined(PARAM_ENABLE_ALPHA_CHANNEL)
//		const float proposedAlpha = alpha;
//#endif
//		float proposedI = Spectrum_Y(&proposedL);
//		proposedI = isinf(proposedI) ? 0.f : proposedI;
//
//		float totalI = sample->totalI;
//		uint largeMutationCount = sample->largeMutationCount;
//		uint smallMutationCount = sample->smallMutationCount;
//		if (smallMutationCount == 0) {
//			// It is a large mutation
//			totalI += Spectrum_Y(&proposedL);
//			largeMutationCount += 1;
//
//			sample->totalI = totalI;
//			sample->largeMutationCount = largeMutationCount;
//		}
//
//		const float meanI = (totalI > 0.f) ? (totalI / largeMutationCount) : 1.f;
//
//		// Calculate accept probability from old and new image sample
//		uint consecutiveRejects = sample->consecutiveRejects;
//
//		float accProb;
//		if ((currentI > 0.f) && (consecutiveRejects < PARAM_SAMPLER_METROPOLIS_MAX_CONSECUTIVE_REJECT))
//			accProb = min(1.f, proposedI / currentI);
//		else
//			accProb = 1.f;
//
//		const float newWeight = accProb + ((smallMutationCount == 0) ? 1.f : 0.f);
//		float weight = sample->weight;
//		weight += 1.f - accProb;
//
//		const float rndVal = Rnd_FloatValue(seed);
//
//		/*if (get_global_id(0) == 0)
//			printf(\"[%d] Current: (%f, %f, %f) [%f] Proposed: (%f, %f, %f) [%f] accProb: %f <%f>\\n\",
//					consecutiveRejects,
//					currentL.r, currentL.g, currentL.b, weight,
//					proposedL.r, proposedL.g, proposedL.b, newWeight,
//					accProb, rndVal);*/
//
//		Spectrum contrib;
//#if defined(PARAM_ENABLE_ALPHA_CHANNEL)
//		float contribAlpha;
//#endif
//		float norm;
//		float scrX, scrY;
//
//		if ((accProb == 1.f) || (rndVal < accProb)) {
//			/*if (get_global_id(0) == 0)
//				printf(\"\\t\\tACCEPTED !\\n\");*/
//
//			// Add accumulated contribution of previous reference sample
//			norm = weight / (currentI / meanI + PARAM_SAMPLER_METROPOLIS_LARGE_STEP_RATE);
//			contrib = currentL;
//#if defined(PARAM_ENABLE_ALPHA_CHANNEL)
//			contribAlpha = currentAlpha;
//#endif
//			scrX = sample->u[current][IDX_SCREEN_X];
//			scrY = sample->u[current][IDX_SCREEN_Y];
//
//#if defined(PARAM_SAMPLER_METROPOLIS_DEBUG_SHOW_SAMPLE_DENSITY)
//			// Debug code: to check sample distribution
//			contrib.r = contrib.g = contrib.b = (consecutiveRejects + 1.f)  * .01f;
//			const uint pixelIndex = PPixelIndexFloat2D(scrX, scrY);
//			SplatSample(frameBuffer,
//#if defined(PARAM_ENABLE_ALPHA_CHANNEL)
//				alphaFrameBuffer,
//#endif
//				pixelIndex, &contrib,
//#if defined(PARAM_ENABLE_ALPHA_CHANNEL)
//				contribAlpha,
//#endif
//				1.f);
//#endif
//
//			current ^= 1;
//			proposed ^= 1;
//			consecutiveRejects = 0;
//
//			weight = newWeight;
//
//			sample->currentRadiance = proposedL;
//#if defined(PARAM_ENABLE_ALPHA_CHANNEL)
//			sample->currentAlpha = proposedAlpha;
//#endif
//		} else {
//			/*if (get_global_id(0) == 0)
//				printf(\"\\t\\tREJECTED !\\n\");*/
//
//			// Add contribution of new sample before rejecting it
//			norm = newWeight / (proposedI / meanI + PARAM_SAMPLER_METROPOLIS_LARGE_STEP_RATE);
//			contrib = proposedL;
//#if defined(PARAM_ENABLE_ALPHA_CHANNEL)
//			contribAlpha = proposedAlpha;
//#endif
//
//			scrX = sample->u[proposed][IDX_SCREEN_X];
//			scrY = sample->u[proposed][IDX_SCREEN_Y];
//
//			++consecutiveRejects;
//
//#if defined(PARAM_SAMPLER_METROPOLIS_DEBUG_SHOW_SAMPLE_DENSITY)
//			// Debug code: to check sample distribution
//			contrib.r = contrib.g = contrib.b = 1.f * .01f;
//			const uint pixelIndex = PixelIndexFloat2D(scrX, scrY);
//			SplatSample(frameBuffer,
//#if defined(PARAM_ENABLE_ALPHA_CHANNEL)
//				alphaFrameBuffer,
//#endif
//				pixelIndex, &contrib,
//#if defined(PARAM_ENABLE_ALPHA_CHANNEL)
//				contribAlpha,
//#endif
//				1.f);
//#endif
//		}
//
//#if !defined(PARAM_SAMPLER_METROPOLIS_DEBUG_SHOW_SAMPLE_DENSITY)
//		if (norm > 0.f) {
//			/*if (get_global_id(0) == 0)
//				printf(\"\\t\\tContrib: (%f, %f, %f) [%f] consecutiveRejects: %d\\n\",
//						contrib.r, contrib.g, contrib.b, norm, consecutiveRejects);*/
//
//#if (PARAM_IMAGE_FILTER_TYPE == 0)
//			const uint pixelIndex = PixelIndexFloat2D(scrX, scrY);
//			SplatSample(frameBuffer,
//#if defined(PARAM_ENABLE_ALPHA_CHANNEL)
//				alphaFrameBuffer,
//#endif
//				pixelIndex, &contrib,
//#if defined(PARAM_ENABLE_ALPHA_CHANNEL)
//				contribAlpha,
//#endif
//				norm);
//#else
//			float sx, sy;
//			const uint pixelIndex = PixelIndexFloat2DWithOffset(scrX, scrY, &sx, &sy);
//			SplatSample(frameBuffer,
//#if defined(PARAM_ENABLE_ALPHA_CHANNEL)
//				alphaFrameBuffer,
//#endif
//				pixelIndex, sx, sy, &contrib,
//#if defined(PARAM_ENABLE_ALPHA_CHANNEL)
//				contribAlpha,
//#endif
//				norm);
//#endif
//		}
//#endif
//
//		sample->weight = weight;
//		sample->consecutiveRejects = consecutiveRejects;
//	}
//
//	sample->current = current;
//	sample->proposed = proposed;
//}

#endif
