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
		__global GPUTaskStats *taskStats
		) {
	const size_t gid = get_global_id(0);
	if (gid >= PARAM_TASK_COUNT)
		return;

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

		task->pathState.state = PATH_STATE_GENERATE_EYE_RAY;

		taskStats[gid].sampleCount += 1;

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
		__global GPUTaskStats *taskStats
		) {
	const size_t gid = get_global_id(0);
	if (gid >= PARAM_TASK_COUNT)
		return;

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

		task->pathState.state = PATH_STATE_GENERATE_EYE_RAY;

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

void Sampler_Init(const size_t gid, Seed *seed, __global Sample *sample) {
	sample->totalI = 0.f;
	sample->sampleCount = 0.f;

	sample->current = 0xffffffffu;
	sample->proposed = 1;

	sample->smallMutationCount = 0;
	sample->consecutiveRejects = 0;

	sample->weight = 0.f;
	sample->currentRadiance.r = 0.f;
	sample->currentRadiance.g = 0.f;
	sample->currentRadiance.b = 0.f;

	for (int i = 0; i < TOTAL_U_SIZE; ++i) {
		sample->u[0][i] = RndFloatValue(seed);
		sample->u[1][i] = RndFloatValue(seed);
	}
}

void LargeStep(Seed *seed, __global float *proposedU) {
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

__kernel void Sampler(
		__global GPUTask *tasks,
		__global GPUTaskStats *taskStats
		) {
	const size_t gid = get_global_id(0);
	if (gid >= PARAM_TASK_COUNT)
		return;

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
			LargeStep(&seed, proposedU);
			sample->smallMutationCount = 0;
		} else {
			__global float *currentU = &sample->u[current][0];

			SmallStep(&seed, currentU, proposedU);
			sample->smallMutationCount += 1;
		}

		taskStats[gid].sampleCount += 1;

		task->pathState.state = PATH_STATE_GENERATE_EYE_RAY;

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
		sample->sampleCount = 1;
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
		uint sampleCount = sample->sampleCount;
		uint smallMutationCount = sample->smallMutationCount;
		if (smallMutationCount == 0) {
			// It is a large mutation
			totalI += Spectrum_Y(&proposedL);
			sampleCount += 1;

			sample->totalI = totalI;
			sample->sampleCount = sampleCount;
		}

		const float meanI = (totalI > 0.f) ? (totalI / sampleCount) : 1.f;

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
#if (PARAM_IMAGE_FILTER_TYPE != 0)
		float sx, sy;
#endif
		uint pixelIndex;
		if ((accProb == 1.f) || (rndVal < accProb)) {
			/*if (get_global_id(0) == 0)
				printf(\"\\t\\tACCEPTED !\\n\");*/

			// Add accumulated contribution of previous reference sample
			norm = weight / (currentI / meanI + PARAM_SAMPLER_METROPOLIS_LARGE_STEP_RATE);
			contrib = currentL;

			pixelIndex = PixelIndexFloat(sample->u[current][IDX_PIXEL_INDEX]);
#if (PARAM_IMAGE_FILTER_TYPE != 0)
			sx = sample->u[current][IDX_SCREEN_X];
			sy = sample->u[current][IDX_SCREEN_Y];
#endif

#if defined(PARAM_SAMPLER_METROPOLIS_DEBUG_SHOW_SAMPLE_DENSITY)
			// Debug code: to check sample distribution
			contrib.r = contrib.g = contrib.b = (consecutiveRejects + 1.f)  * .01f;
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

			pixelIndex = PixelIndexFloat(sample->u[proposed][IDX_PIXEL_INDEX]);
#if (PARAM_IMAGE_FILTER_TYPE != 0)
			sx = sample->u[proposed][IDX_SCREEN_X];
			sy = sample->u[proposed][IDX_SCREEN_Y];
#endif

			++consecutiveRejects;

#if defined(PARAM_SAMPLER_METROPOLIS_DEBUG_SHOW_SAMPLE_DENSITY)
			// Debug code: to check sample distribution
			contrib.r = contrib.g = contrib.b = 1.f * .01f;
			SplatSample(frameBuffer, pixelIndex, &contrib, 1.f);
#endif
		}

#if !defined(PARAM_SAMPLER_METROPOLIS_DEBUG_SHOW_SAMPLE_DENSITY)
		if (norm > 0.f) {
			/*if (get_global_id(0) == 0)
				printf(\"\\t\\tPixelIndex: %d Contrib: (%f, %f, %f) [%f] consecutiveRejects: %d\\n\",
						pixelIndex, contrib.r, contrib.g, contrib.b, norm, consecutiveRejects);*/

#if (PARAM_IMAGE_FILTER_TYPE == 0)
			SplatSample(frameBuffer, pixelIndex, &contrib, norm);
#else
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
