#line 2 "samplers.cl"

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

#if (PARAM_SAMPLER_TYPE == 0)
#define Sampler_GetSamplePath(index) (Rnd_FloatValue(seed))
#define Sampler_GetSamplePathVertex(index) (Rnd_FloatValue(seed))
#elif (PARAM_SAMPLER_TYPE == 1)
#define Sampler_GetSamplePath(index) (sampleDataPathBase[index])
#define Sampler_GetSamplePathVertex(index) (sampleDataPathVertexBase[index])
#endif

void GenerateCameraRay(
		__global Camera *camera,
		__global Sample *sample,
		__global float *sampleDataPathBase,
		Seed *seed,
		__global Ray *ray) {
#if (PARAM_SAMPLER_TYPE == 0)
	const uint pixelIndex = sample->pixelIndex;

	// Don't use Sampler_GetSample() here
	const float scrSampleX = sampleDataPathBase[IDX_SCREEN_X];
	const float scrSampleY = sampleDataPathBase[IDX_SCREEN_Y];

	const float screenX = pixelIndex % PARAM_IMAGE_WIDTH + scrSampleX - .5f;
	const float screenY = pixelIndex / PARAM_IMAGE_WIDTH + scrSampleY - .5f;
#elif (PARAM_SAMPLER_TYPE == 1)
	const float screenX = min(Sampler_GetSamplePath(IDX_SCREEN_X) * PARAM_IMAGE_WIDTH, (float)(PARAM_IMAGE_WIDTH - 1));
	const float screenY = min(Sampler_GetSamplePath(IDX_SCREEN_Y) * PARAM_IMAGE_HEIGHT, (float)(PARAM_IMAGE_HEIGHT - 1));
#endif

	float3 Pras = (float3)(screenX, PARAM_IMAGE_HEIGHT - screenY - 1.f, 0.f);
	float3 rayOrig = Transform_ApplyPoint(&camera->rasterToCamera, Pras);
	float3 rayDir = rayOrig;

	const float hither = camera->hither;

#if defined(PARAM_CAMERA_HAS_DOF)
	const float dofSampleX = Sampler_GetSamplePath(IDX_DOF_X);
	const float dofSampleY = Sampler_GetSamplePath(IDX_DOF_Y);

	// Sample point on lens
	float lensU, lensV;
	ConcentricSampleDisk(dofSampleX, dofSampleY, &lensU, &lensV);
	const float lensRadius = camera->lensRadius;
	lensU *= lensRadius;
	lensV *= lensRadius;

	// Compute point on plane of focus
	const float focalDistance = camera->focalDistance;
	const float dist = focalDistance - hither;
	const float ft = dist / rayDir.z;
	float3 Pfocus;
	Pfocus = rayOrig + rayDir * ft;

	// Update ray for effect of lens
	const float k = dist / focalDistance;
	rayOrig.x += lensU * k;
	rayOrig.y += lensV * k;

	rayDir = Pfocus - rayOrig;
#endif

	rayDir = normalize(rayDir);
	const float maxt = (camera->yon - hither) / rayDir.z;

	// Transform ray in world coordinates
	rayOrig = Transform_ApplyPoint(&camera->cameraToWorld, rayOrig);
	rayDir = Transform_ApplyVector(&camera->cameraToWorld, rayDir);

	Ray_Init3(ray, rayOrig, rayDir, maxt);

	/*printf("(%f, %f, %f) (%f, %f, %f) [%f, %f]\n",
		ray->o.x, ray->o.y, ray->o.z, ray->d.x, ray->d.y, ray->d.z,
		ray->mint, ray->maxt);*/
}

void GenerateCameraPath(
		__global GPUTask *task,
		__global float *sampleDataPathBase,
		Seed *seed,
		__global Camera *camera,
		__global Ray *ray) {
	__global Sample *sample = &task->sample;

	GenerateCameraRay(camera, sample, sampleDataPathBase, seed, ray);

	vstore3(BLACK, 0, &sample->radiance.r);

	// Initialize the path state
	task->pathStateBase.state = RT_NEXT_VERTEX;
	task->pathStateBase.depth = 1;
	vstore3(WHITE, 0, &task->pathStateBase.throughput.r);
#if defined(PARAM_DIRECT_LIGHT_SAMPLING)
	task->directLightState.lastPdfW = 1.f;
	task->directLightState.lastSpecular = TRUE;
#endif
#if defined(PARAM_ENABLE_ALPHA_CHANNEL)
	task->alphaChannelState.vertexCount = 0;
	task->alphaChannelState.alpha = 1.f;
#endif
}

//------------------------------------------------------------------------------
// Random Sampler Kernel
//------------------------------------------------------------------------------

#if (PARAM_SAMPLER_TYPE == 0)

__global float *Sampler_GetSampleData(__global Sample *sample, __global float *samplesData) {
	const size_t gid = get_global_id(0);
	return &samplesData[gid * TOTAL_U_SIZE];
}

__global float *Sampler_GetSampleDataPathBase(__global Sample *sample, __global float *sampleData) {
	return sampleData;
}

__global float *Sampler_GetSampleDataPathVertex(__global Sample *sample,
		__global float *sampleDataPathBase, const uint depth) {
	return &sampleDataPathBase[IDX_BSDF_OFFSET + depth * SAMPLE_SIZE];
}

void Sampler_Init(Seed *seed, __global Sample *sample, __global float *sampleData) {
	const size_t gid = get_global_id(0);

	vstore3(BLACK, 0, &sample->radiance.r);
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
#if defined(PARAM_ENABLE_ALPHA_CHANNEL)
		__global AlphaPixel *alphaFrameBuffer,
#endif
		__global Camera *camera,
		__global Ray *ray
		) {
	const uint pixelIndex = sample->pixelIndex;
	SplatSample(frameBuffer,
#if defined(PARAM_ENABLE_ALPHA_CHANNEL)
				alphaFrameBuffer,
#endif
				pixelIndex, vload3(0, &sample->radiance.r),
#if defined(PARAM_ENABLE_ALPHA_CHANNEL)
				contribAlpha,
#endif
				1.f);

	// Move to the next assigned pixel
	sample->pixelIndex = NextPixelIndex(pixelIndex);

	sampleData[IDX_SCREEN_X] = Rnd_FloatValue(seed);
	sampleData[IDX_SCREEN_Y] = Rnd_FloatValue(seed);

	__global float *sampleDataPathBase = Sampler_GetSampleDataPathBase(sample, sampleData);
	GenerateCameraPath(task, sampleDataPathBase, seed, camera, ray);
}

#endif

//------------------------------------------------------------------------------
// Metropolis Sampler Kernel
//------------------------------------------------------------------------------

#if (PARAM_SAMPLER_TYPE == 1)

__global float *Sampler_GetSampleData(__global Sample *sample, __global float *samplesData) {
	const size_t gid = get_global_id(0);
	return &samplesData[gid * (2 *TOTAL_U_SIZE)];
}

__global float *Sampler_GetSampleDataPathBase(__global Sample *sample, __global float *sampleData) {
	const size_t gid = get_global_id(0);
	return &sampleData[sample->proposed * TOTAL_U_SIZE];
}

__global float *Sampler_GetSampleDataPathVertex(__global Sample *sample,
		__global float *sampleDataPathBase, const uint depth) {
	return &sampleDataPathBase[IDX_BSDF_OFFSET + depth * SAMPLE_SIZE];
}

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

void Sampler_Init(Seed *seed, __global Sample *sample, __global float *sampleData) {
	sample->totalI = 0.f;
	sample->largeMutationCount = 1.f;

	sample->current = NULL_INDEX;
	sample->proposed = 1;

	sample->smallMutationCount = 0;
	sample->consecutiveRejects = 0;

	sample->weight = 0.f;
	vstore3(WHITE, 0, &sample->currentRadiance.r);
#if defined(PARAM_ENABLE_ALPHA_CHANNEL)
	sample->currentAlpha = 0.f;
#endif

	__global float *sampleDataPathBase = Sampler_GetSampleDataPathBase(sample, sampleData);
	LargeStep(seed, 0, sampleDataPathBase);
}

void Sampler_NextSample(
		__global GPUTask *task,
		__global Sample *sample,
		__global float *sampleData,
		Seed *seed,
		__global Pixel *frameBuffer,
#if defined(PARAM_ENABLE_ALPHA_CHANNEL)
		__global AlphaPixel *alphaFrameBuffer,
#endif
		__global Camera *camera,
		__global Ray *ray
		) {
	//--------------------------------------------------------------------------
	// Accept/Reject the sample
	//--------------------------------------------------------------------------

	uint current = sample->current;
	uint proposed = sample->proposed;

	const float3 radiance = vload3(0, &sample->radiance.r);

	if (current == NULL_INDEX) {
		// It is the very first sample, I have still to initialize the current
		// sample

		vstore3(radiance, 0, &sample->currentRadiance.r);
#if defined(PARAM_ENABLE_ALPHA_CHANNEL)
		sample->currentAlpha = alpha;
#endif
		sample->totalI = Spectrum_Y(radiance);

		current = proposed;
		proposed ^= 1;
	} else {
		const float3 currentL = vload3(0, &sample->currentRadiance.r);
#if defined(PARAM_ENABLE_ALPHA_CHANNEL)
		const float currentAlpha = sample->currentAlpha;
#endif
		const float currentI = Spectrum_Y(currentL);

		const float3 proposedL = radiance;
#if defined(PARAM_ENABLE_ALPHA_CHANNEL)
		const float proposedAlpha = alpha;
#endif
		float proposedI = Spectrum_Y(proposedL);
		proposedI = isinf(proposedI) ? 0.f : proposedI;

		float totalI = sample->totalI;
		uint largeMutationCount = sample->largeMutationCount;
		uint smallMutationCount = sample->smallMutationCount;
		if (smallMutationCount == 0) {
			// It is a large mutation
			totalI += Spectrum_Y(proposedL);
			largeMutationCount += 1;

			sample->totalI = totalI;
			sample->largeMutationCount = largeMutationCount;
		}

		const float meanI = (totalI > 0.f) ? (totalI / largeMutationCount) : 1.f;

		// Calculate accept probability from old and new image sample
		uint consecutiveRejects = sample->consecutiveRejects;

		float accProb;
		if ((currentI > 0.f) && (consecutiveRejects < PARAM_SAMPLER_METROPOLIS_MAX_CONSECUTIVE_REJECT))
			accProb = min(1.f, proposedI / currentI);
		else
			accProb = 1.f;

		const float newWeight = accProb + ((smallMutationCount == 0) ? 1.f : 0.f);
		float weight = sample->weight;
		weight += 1.f - accProb;

		const float rndVal = Rnd_FloatValue(seed);

		/*if (get_global_id(0) == 0)
			printf(\"[%d] Current: (%f, %f, %f) [%f] Proposed: (%f, %f, %f) [%f] accProb: %f <%f>\\n\",
					consecutiveRejects,
					currentL.r, currentL.g, currentL.b, weight,
					proposedL.r, proposedL.g, proposedL.b, newWeight,
					accProb, rndVal);*/

		float3 contrib;
#if defined(PARAM_ENABLE_ALPHA_CHANNEL)
		float contribAlpha;
#endif
		float norm;
		float scrX, scrY;

		if ((accProb == 1.f) || (rndVal < accProb)) {
			/*if (get_global_id(0) == 0)
				printf(\"\\t\\tACCEPTED !\\n\");*/

			// Add accumulated contribution of previous reference sample
			norm = weight / (currentI / meanI + PARAM_SAMPLER_METROPOLIS_LARGE_STEP_RATE);
			contrib = currentL;
#if defined(PARAM_ENABLE_ALPHA_CHANNEL)
			contribAlpha = currentAlpha;
#endif
			scrX = sampleData[current * TOTAL_U_SIZE + IDX_SCREEN_X];
			scrY = sampleData[current * TOTAL_U_SIZE + IDX_SCREEN_Y];

			current ^= 1;
			proposed ^= 1;
			consecutiveRejects = 0;

			weight = newWeight;

			vstore3(proposedL, 0, &sample->currentRadiance.r);
#if defined(PARAM_ENABLE_ALPHA_CHANNEL)
			sample->currentAlpha = proposedAlpha;
#endif
		} else {
			/*if (get_global_id(0) == 0)
				printf(\"\\t\\tREJECTED !\\n\");*/

			// Add contribution of new sample before rejecting it
			norm = newWeight / (proposedI / meanI + PARAM_SAMPLER_METROPOLIS_LARGE_STEP_RATE);
			contrib = proposedL;
#if defined(PARAM_ENABLE_ALPHA_CHANNEL)
			contribAlpha = proposedAlpha;
#endif

			scrX = sampleData[proposed * TOTAL_U_SIZE + IDX_SCREEN_X];
			scrY = sampleData[proposed * TOTAL_U_SIZE + IDX_SCREEN_Y];

			++consecutiveRejects;
		}

		if (norm > 0.f) {
			/*if (get_global_id(0) == 0)
				printf(\"\\t\\tContrib: (%f, %f, %f) [%f] consecutiveRejects: %d\\n\",
						contrib.r, contrib.g, contrib.b, norm, consecutiveRejects);*/

#if (PARAM_IMAGE_FILTER_TYPE == 0)
			const uint pixelIndex = PixelIndexFloat2D(scrX, scrY);
			SplatSample(frameBuffer,
#if defined(PARAM_ENABLE_ALPHA_CHANNEL)
				alphaFrameBuffer,
#endif
				pixelIndex, contrib,
#if defined(PARAM_ENABLE_ALPHA_CHANNEL)
				contribAlpha,
#endif
				norm);
#else
			float sx, sy;
			const uint pixelIndex = PixelIndexFloat2DWithOffset(scrX, scrY, &sx, &sy);
			SplatSample(frameBuffer,
#if defined(PARAM_ENABLE_ALPHA_CHANNEL)
				alphaFrameBuffer,
#endif
				pixelIndex, sx, sy, contrib,
#if defined(PARAM_ENABLE_ALPHA_CHANNEL)
				contribAlpha,
#endif
				norm);
#endif
		}

		sample->weight = weight;
		sample->consecutiveRejects = consecutiveRejects;
	}

	sample->current = current;
	sample->proposed = proposed;

	//--------------------------------------------------------------------------
	// Mutate the sample
	//--------------------------------------------------------------------------

	__global float *proposedU = &sampleData[proposed * TOTAL_U_SIZE];
	if (Rnd_FloatValue(seed) < PARAM_SAMPLER_METROPOLIS_LARGE_STEP_RATE) {
		LargeStep(seed, sample->largeMutationCount, proposedU);
		sample->smallMutationCount = 0;
	} else {
		__global float *currentU = &sampleData[current * TOTAL_U_SIZE];

		SmallStep(seed, currentU, proposedU);
		sample->smallMutationCount += 1;
	}

	//--------------------------------------------------------------------------
	// Generate a new camera path
	//--------------------------------------------------------------------------

	__global float *sampleDataPathBase = Sampler_GetSampleDataPathBase(sample, sampleData);
	GenerateCameraPath(task, sampleDataPathBase, seed, camera, ray);
}

#endif
