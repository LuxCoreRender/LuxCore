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

void GenerateCameraPath(
		__global GPUTask *task,
		__global Ray *ray,
		Seed *seed,
		__global float *cameraData
		) {
	__global Sample *sample = &task->sample;

	GenerateCameraRay(sample, ray
#if (PARAM_SAMPLER_TYPE == 0)
			, seed
#endif
			, cameraData);

	sample->radiance.r = 0.f;
	sample->radiance.g = 0.f;
	sample->radiance.b = 0.f;

	// Initialize the path state
	task->pathState.depth = 0;
	task->pathState.throughput.r = 1.f;
	task->pathState.throughput.g = 1.f;
	task->pathState.throughput.b = 1.f;
#if defined(PARAM_DIRECT_LIGHT_SAMPLING)
	task->pathState.bouncePdf = 1.f;
	task->pathState.specularBounce = TRUE;
#endif
	task->pathState.state = PATH_STATE_NEXT_VERTEX;
}

//------------------------------------------------------------------------------
// Inlined Random Sampler Kernel
//------------------------------------------------------------------------------

#if (PARAM_SAMPLER_TYPE == 0)

void Sampler_Init(const size_t gid, Seed *seed, __global Sample *sample) {
	sample->pixelIndex = PixelIndexInt(gid);

	sample->u[IDX_SCREEN_X] = RndFloatValue(seed);
	sample->u[IDX_SCREEN_Y] = RndFloatValue(seed);
}

__kernel void Sampler(
		__global GPUTask *tasks,
		__global GPUTaskStats *taskStats,
		__global Ray *rays,
		__global float *cameraData
		) {
	const size_t gid = get_global_id(0);

	// Initialize the task
	__global GPUTask *task = &tasks[gid];

	if (task->pathState.state == PATH_STATE_DONE) {
		__global Sample *sample = &task->sample;

		// Read the seed
		Seed seed;
		seed.s1 = task->seed.s1;
		seed.s2 = task->seed.s2;
		seed.s3 = task->seed.s3;

		// Move to the next assigned pixel
		sample->pixelIndex = NextPixelIndex(sample->pixelIndex);

		sample->u[IDX_SCREEN_X] = RndFloatValue(&seed);
		sample->u[IDX_SCREEN_Y] = RndFloatValue(&seed);

		taskStats[gid].sampleCount += 1;

		GenerateCameraPath(task, &rays[gid], &seed, cameraData);

		// Save the seed
		task->seed.s1 = seed.s1;
		task->seed.s2 = seed.s2;
		task->seed.s3 = seed.s3;
	}
}

#endif

//------------------------------------------------------------------------------
// Random Sampler Kernel
//------------------------------------------------------------------------------

#if (PARAM_SAMPLER_TYPE == 1)

void Sampler_Init(const size_t gid, Seed *seed, __global Sample *sample) {
	sample->pixelIndex = PixelIndexInt(gid);

	for (int i = 0; i < TOTAL_U_SIZE; ++i)
		sample->u[i] = RndFloatValue(seed);
}

__kernel void Sampler(
		__global GPUTask *tasks,
		__global GPUTaskStats *taskStats,
		__global Ray *rays,
		__global float *cameraData) {
	const size_t gid = get_global_id(0);

	// Initialize the task
	__global GPUTask *task = &tasks[gid];

	if (task->pathState.state == PATH_STATE_DONE) {
		__global Sample *sample = &task->sample;

		// Read the seed
		Seed seed;
		seed.s1 = task->seed.s1;
		seed.s2 = task->seed.s2;
		seed.s3 = task->seed.s3;

		// Move to the next assigned pixel
		sample->pixelIndex = NextPixelIndex(sample->pixelIndex);

		for (int i = 0; i < TOTAL_U_SIZE; ++i)
			sample->u[i] = RndFloatValue(&seed);

		GenerateCameraPath(task, &rays[gid], &seed, cameraData);

		taskStats[gid].sampleCount += 1;

		// Save the seed
		task->seed.s1 = seed.s1;
		task->seed.s2 = seed.s2;
		task->seed.s3 = seed.s3;
	}
}

#endif

//------------------------------------------------------------------------------
// Metropolis Sampler Kernel
//------------------------------------------------------------------------------

#if (PARAM_SAMPLER_TYPE == 2)

void LargeStep(Seed *seed, const uint largeStepCount, __global float *proposedU) {
	for (int i = 0; i < TOTAL_U_SIZE; ++i)
		proposedU[i] = RndFloatValue(seed);
}

float Mutate(Seed *seed, const float x) {
	const float s1 = 1.f / 512.f;
	const float s2 = 1.f / 16.f;

	const float randomValue = RndFloatValue(seed);

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

void SmallStep(Seed *seed, __global float *currentU, __global float *proposedU) {
	for (int i = 0; i < TOTAL_U_SIZE; ++i)
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

	LargeStep(seed, 0, &sample->u[1][0]);
}

__kernel void Sampler(
		__global GPUTask *tasks,
		__global GPUTaskStats *taskStats,
		__global Ray *rays,
		__global float *cameraData) {
	const size_t gid = get_global_id(0);

	// Initialize the task
	__global GPUTask *task = &tasks[gid];

	__global Sample *sample = &task->sample;
	const uint current = sample->current;

	// Check if it is a complete path and not the very first sample
	if ((current != 0xffffffffu) && (task->pathState.state == PATH_STATE_DONE)) {
		// Read the seed
		Seed seed;
		seed.s1 = task->seed.s1;
		seed.s2 = task->seed.s2;
		seed.s3 = task->seed.s3;

		const uint proposed = sample->proposed;
		__global float *proposedU = &sample->u[proposed][0];

		if (RndFloatValue(&seed) < PARAM_SAMPLER_METROPOLIS_LARGE_STEP_RATE) {
			LargeStep(&seed, sample->largeMutationCount, proposedU);
			sample->smallMutationCount = 0;
		} else {
			__global float *currentU = &sample->u[current][0];

			SmallStep(&seed, currentU, proposedU);
			sample->smallMutationCount += 1;
		}

		taskStats[gid].sampleCount += 1;

		GenerateCameraPath(task, &rays[gid], &seed, cameraData);

		// Save the seed
		task->seed.s1 = seed.s1;
		task->seed.s2 = seed.s2;
		task->seed.s3 = seed.s3;
	}
}

void Sampler_MLT_SplatSample(__global Pixel *frameBuffer, Seed *seed, __global Sample *sample) {
	uint current = sample->current;
	uint proposed = sample->proposed;

	Spectrum radiance = sample->radiance;

	if (current == 0xffffffffu) {
		// It is the very first sample, I have still to initialize the current
		// sample

		sample->currentRadiance = radiance;
		sample->totalI = Spectrum_Y(&radiance);

		// The following 2 lines could be moved in the initialization code
		sample->largeMutationCount = 1;
		sample->weight = 0.f;

		current = proposed;
		proposed ^= 1;
	} else {
		const Spectrum currentL = sample->currentRadiance;
		const float currentI = Spectrum_Y(&currentL);

		const Spectrum proposedL = radiance;
		float proposedI = Spectrum_Y(&proposedL);
		proposedI = isinf(proposedI) ? 0.f : proposedI;

		float totalI = sample->totalI;
		uint largeMutationCount = sample->largeMutationCount;
		uint smallMutationCount = sample->smallMutationCount;
		if (smallMutationCount == 0) {
			// It is a large mutation
			totalI += Spectrum_Y(&proposedL);
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

		const float rndVal = RndFloatValue(seed);

		/*if (get_global_id(0) == 0)
			printf(\"[%d] Current: (%f, %f, %f) [%f] Proposed: (%f, %f, %f) [%f] accProb: %f <%f>\\n\",
					smallMutationCount,
					currentL.r, currentL.g, currentL.b, weight,
					proposedL.r, proposedL.g, proposedL.b, newWeight,
					accProb, rndVal);*/

		Spectrum contrib;
		float norm;
		float scrX, scrY;

		if ((accProb == 1.f) || (rndVal < accProb)) {
			/*if (get_global_id(0) == 0)
				printf(\"\\t\\tACCEPTED !\\n\");*/

			// Add accumulated contribution of previous reference sample
			norm = weight / (currentI / meanI + PARAM_SAMPLER_METROPOLIS_LARGE_STEP_RATE);
			contrib = currentL;

			scrX = sample->u[current][IDX_SCREEN_X];
			scrY = sample->u[current][IDX_SCREEN_Y];

#if defined(PARAM_SAMPLER_METROPOLIS_DEBUG_SHOW_SAMPLE_DENSITY)
			// Debug code: to check sample distribution
			contrib.r = contrib.g = contrib.b = (consecutiveRejects + 1.f)  * .01f;
			const uint pixelIndex = PPixelIndexFloat2D(scrX, scrY);
			SplatSample(frameBuffer, pixelIndex, &contrib, 1.f);
#endif

			current ^= 1;
			proposed ^= 1;
			consecutiveRejects = 0;

			weight = newWeight;

			sample->currentRadiance = proposedL;
		} else {
			/*if (get_global_id(0) == 0)
				printf(\"\\t\\tREJECTED !\\n\");*/

			// Add contribution of new sample before rejecting it
			norm = newWeight / (proposedI / meanI + PARAM_SAMPLER_METROPOLIS_LARGE_STEP_RATE);
			contrib = proposedL;

			scrX = sample->u[proposed][IDX_SCREEN_X];
			scrY = sample->u[proposed][IDX_SCREEN_Y];

			++consecutiveRejects;

#if defined(PARAM_SAMPLER_METROPOLIS_DEBUG_SHOW_SAMPLE_DENSITY)
			// Debug code: to check sample distribution
			contrib.r = contrib.g = contrib.b = 1.f * .01f;
			const uint pixelIndex = PixelIndexFloat2D(scrX, scrY);
			SplatSample(frameBuffer, pixelIndex, &contrib, 1.f);
#endif
		}

#if !defined(PARAM_SAMPLER_METROPOLIS_DEBUG_SHOW_SAMPLE_DENSITY)
		if (norm > 0.f) {
			/*if (get_global_id(0) == 0)
				printf(\"\\t\\tPixelIndex: %d Contrib: (%f, %f, %f) [%f] consecutiveRejects: %d\\n\",
						pixelIndex, contrib.r, contrib.g, contrib.b, norm, consecutiveRejects);*/

#if (PARAM_IMAGE_FILTER_TYPE == 0)
			const uint pixelIndex = PixelIndexFloat2D(scrX, scrY);
			SplatSample(frameBuffer, pixelIndex, &contrib, norm);
#else
			float sx, sy;
			const uint pixelIndex = PixelIndexFloat2DWithOffset(scrX, scrY, &sx, &sy);
			SplatSample(frameBuffer, pixelIndex, sx, sy, &contrib, norm);
#endif
		}
#endif

		sample->weight = weight;
		sample->consecutiveRejects = consecutiveRejects;
	}

	sample->current = current;
	sample->proposed = proposed;
}

#endif

//------------------------------------------------------------------------------
// Stratified Sampler Kernel
//------------------------------------------------------------------------------

#if (PARAM_SAMPLER_TYPE == 3)

void StratifiedSample1D(__local float *buff, Seed *seed) {
	const float dx = 1.f / PARAM_SAMPLER_STRATIFIED_X_SAMPLES;

	for (uint x = 0; x < PARAM_SAMPLER_STRATIFIED_X_SAMPLES; ++x) {
		*buff++ = (x + RndFloatValue(seed)) * dx;
	}
}

void Shuffle1D(__local float *buff, Seed *seed) {
	const uint count = PARAM_SAMPLER_STRATIFIED_X_SAMPLES;

	for (uint i = 0; i < count; ++i) {
		const uint other = RndUintValue(seed) % (count - i);

		const float u0 = buff[other];
		buff[other] = buff[i];
		buff[i] = u0;
	}
}

void StratifiedSample2D(__local float *buff, Seed *seed) {
	const float dx = 1.f / PARAM_SAMPLER_STRATIFIED_X_SAMPLES;
	const float dy = 1.f / PARAM_SAMPLER_STRATIFIED_Y_SAMPLES;

	for (uint y = 0; y < PARAM_SAMPLER_STRATIFIED_Y_SAMPLES; ++y) {
		for (uint x = 0; x < PARAM_SAMPLER_STRATIFIED_X_SAMPLES; ++x) {
			*buff++ = (x + RndFloatValue(seed)) * dx;
			*buff++ = (y + RndFloatValue(seed)) * dy;
		}
	}
}

void Shuffle2D(__local float *buff, Seed *seed) {
	const uint count = PARAM_SAMPLER_STRATIFIED_X_SAMPLES *  PARAM_SAMPLER_STRATIFIED_Y_SAMPLES;

	for (uint i = 0; i < count; ++i) {
		const uint other = RndUintValue(seed) % (count - i);

		uint otherIdx = 2 * other;
		uint iIdx = 2 * i;

		const float u0 = buff[otherIdx];
		buff[otherIdx] = buff[iIdx];
		buff[iIdx] = u0;

		++otherIdx;
		++iIdx;
		const float u1 = buff[otherIdx];
		buff[otherIdx] = buff[iIdx];
		buff[iIdx] = u1;
	}
}

void Copy2D(__local float *src, __global float *dest) {
	for (uint i = 0; i < PARAM_SAMPLER_STRATIFIED_X_SAMPLES *  PARAM_SAMPLER_STRATIFIED_Y_SAMPLES; ++i) {
		*dest++ = *src++;
		*dest++ = *src++;
	}
}

void Copy1D(__local float *src, __global float *dest) {
	for (uint i = 0; i < PARAM_SAMPLER_STRATIFIED_X_SAMPLES *  PARAM_SAMPLER_STRATIFIED_Y_SAMPLES; ++i)
		*dest++ = *src++;
}

void Sampler_StratifiedBufferInit(__local float *localMemTempBuff,
		Seed *seed, __global Sample *sample) {
	__local float *tempBuff = &localMemTempBuff[get_local_id(0) * PARAM_SAMPLER_STRATIFIED_X_SAMPLES * PARAM_SAMPLER_STRATIFIED_Y_SAMPLES * 2];

	StratifiedSample2D(tempBuff, seed);
	Copy2D(tempBuff, &sample->stratifiedScreen2D[0]);

#if defined(PARAM_CAMERA_HAS_DOF)
	StratifiedSample2D(tempBuff, seed);
	Shuffle2D(tempBuff, seed);
	Copy2D(tempBuff, &sample->stratifiedDof2D[0]);
#endif

#if defined(PARAM_HAS_ALPHA_TEXTUREMAPS)
	StratifiedSample1D(tempBuff, seed);
	Shuffle1D(tempBuff, seed);
	Copy1D(tempBuff, &sample->stratifiedAlpha1D[0]);
#endif

	StratifiedSample2D(tempBuff, seed);
	Shuffle2D(tempBuff, seed);
	Copy2D(tempBuff, &sample->stratifiedBSDF2D[0]);

	StratifiedSample1D(tempBuff, seed);
	Shuffle1D(tempBuff, seed);
	Copy1D(tempBuff, &sample->stratifiedBSDF1D[0]);

#if defined(PARAM_DIRECT_LIGHT_SAMPLING)
	StratifiedSample2D(tempBuff, seed);
	Shuffle2D(tempBuff, seed);
	Copy2D(tempBuff, &sample->stratifiedLight2D[0]);

	StratifiedSample1D(tempBuff, seed);
	Shuffle1D(tempBuff, seed);
	Copy1D(tempBuff, &sample->stratifiedLight1D[0]);
#endif
}

void Sampler_CopyFromStratifiedBuffer(Seed *seed, __global Sample *sample, const uint index) {
	const uint i0 = index * 2;
	const uint i1 = i0 + 1;

	sample->u[IDX_SCREEN_X] = sample->stratifiedScreen2D[i0];
	sample->u[IDX_SCREEN_Y] = sample->stratifiedScreen2D[i1];

#if defined(PARAM_CAMERA_HAS_DOF)
	sample->u[IDX_DOF_X] = sample->stratifiedDof2D[i0];
	sample->u[IDX_DOF_Y] = sample->stratifiedDof2D[i1];
#endif

#if defined(PARAM_HAS_ALPHA_TEXTUREMAPS)
	sample->u[IDX_BSDF_OFFSET + IDX_TEX_ALPHA] = sample->stratifiedAlpha1D[index];
#endif

	sample->u[IDX_BSDF_OFFSET + IDX_BSDF_X] = sample->stratifiedBSDF2D[i0];
	sample->u[IDX_BSDF_OFFSET + IDX_BSDF_Y] = sample->stratifiedBSDF2D[i1];
	sample->u[IDX_BSDF_OFFSET + IDX_BSDF_Z] = sample->stratifiedBSDF1D[index];

#if defined(PARAM_DIRECT_LIGHT_SAMPLING)
	sample->u[IDX_BSDF_OFFSET + IDX_DIRECTLIGHT_X] = sample->stratifiedLight2D[i0];
	sample->u[IDX_BSDF_OFFSET + IDX_DIRECTLIGHT_Y] = sample->stratifiedLight2D[i1];
	sample->u[IDX_BSDF_OFFSET + IDX_DIRECTLIGHT_Z] = sample->stratifiedLight1D[index];
#endif

	for (int i = IDX_BSDF_OFFSET + IDX_RR; i < TOTAL_U_SIZE; ++i)
		sample->u[i] = RndFloatValue(seed);
}

void Sampler_Init(const size_t gid, __local float *localMemTempBuff,
		Seed *seed, __global Sample *sample) {
	sample->pixelIndex = PixelIndexInt(gid);

	Sampler_StratifiedBufferInit(localMemTempBuff, seed, sample);

	Sampler_CopyFromStratifiedBuffer(seed, sample, 0);
}

__kernel void Sampler(
		__global GPUTask *tasks,
		__global GPUTaskStats *taskStats,
		__global Ray *rays,
		__global float *cameraData,
		__local float *localMemTempBuff
		) {
	const size_t gid = get_global_id(0);

	// Initialize the task
	__global GPUTask *task = &tasks[gid];

	if (task->pathState.state == PATH_STATE_DONE) {
		__global Sample *sample = &task->sample;

		// Read the seed
		Seed seed;
		seed.s1 = task->seed.s1;
		seed.s2 = task->seed.s2;
		seed.s3 = task->seed.s3;

		// Check if I have used all the stratified samples
		const uint sampleNewCount = taskStats[gid].sampleCount + 1;
		const uint sampleNewIndex = sampleNewCount % (PARAM_SAMPLER_STRATIFIED_X_SAMPLES * PARAM_SAMPLER_STRATIFIED_Y_SAMPLES);

		if (sampleNewIndex == 0) {
			// Move to the next assigned pixel
			sample->pixelIndex = NextPixelIndex(sample->pixelIndex);

			// Initialize the stratified buffer
			Sampler_StratifiedBufferInit(localMemTempBuff, &seed, sample);
		}

		Sampler_CopyFromStratifiedBuffer(&seed, sample, sampleNewIndex);

		GenerateCameraPath(task, &rays[gid], &seed, cameraData);

		taskStats[gid].sampleCount = sampleNewCount;

		// Save the seed
		task->seed.s1 = seed.s1;
		task->seed.s2 = seed.s2;
		task->seed.s3 = seed.s3;
	}
}

#endif
