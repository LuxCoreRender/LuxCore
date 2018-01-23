#line 2 "sampler_metropolis_funcs.cl"

/***************************************************************************
 * Copyright 1998-2018 by authors (see AUTHORS.txt)                        *
 *                                                                         *
 *   This file is part of LuxCoreRender.                                   *
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

//------------------------------------------------------------------------------
// Metropolis Sampler Kernel
//------------------------------------------------------------------------------

#if (PARAM_SAMPLER_TYPE == 1)

#define SOBOL_BITS 32
#define SOBOL_MAX_DIMENSIONS 21201

__global float *Sampler_GetSampleData(__global Sample *sample, __global float *samplesData) {
	const size_t gid = get_global_id(0);
	return &samplesData[gid * (2 * TOTAL_U_SIZE)];
}

__global float *Sampler_GetSampleDataPathBase(__global Sample *sample, __global float *sampleData) {
	return &sampleData[sample->proposed * TOTAL_U_SIZE];
}

__global float *Sampler_GetSampleDataPathVertex(__global Sample *sample,
		__global float *sampleDataPathBase, const uint depth) {
	return &sampleDataPathBase[IDX_BSDF_OFFSET + depth * VERTEX_SAMPLE_SIZE];
}

float Sampler_GetSamplePath(Seed *seed, __global Sample *sample,
		__global float *sampleDataPathBase, const uint index) {
	return sampleDataPathBase[index];
}

float Sampler_GetSamplePathVertex(Seed *seed, __global Sample *sample,
		__global float *sampleDataPathVertexBase,
		const uint depth, const uint index) {
	return sampleDataPathVertexBase[index];
}

void LargeStep(Seed *seed, __global float *proposedU) {
	for (int i = 0; i < TOTAL_U_SIZE; ++i)
		proposedU[i] = Rnd_FloatValue(seed);
}

float Mutate(const float x, const float randomValue) {
	const float s1 = 1.f / 512.f;
	const float s2 = 1.f / 16.f;

	const float dx = s1 / (s1 / s2 + fabs(2.f * randomValue - 1.f)) -
		s1 / (s1 / s2 + 1.f);

	float mutatedX = x;
	if (randomValue < .5f) {
		mutatedX += dx;
		mutatedX = (mutatedX < 1.f) ? mutatedX : (mutatedX - 1.f);
	} else {
		mutatedX -= dx;
		mutatedX = (mutatedX < 0.f) ? (mutatedX + 1.f) : mutatedX;
	}

	return mutatedX;
}

float MutateScaled(const float x, const float range, const float randomValue) {
	const float s1 = 32.f;

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
	proposedU[IDX_SCREEN_X] = MutateScaled(currentU[IDX_SCREEN_X],
			PARAM_SAMPLER_METROPOLIS_IMAGE_MUTATION_RANGE, Rnd_FloatValue(seed));
	proposedU[IDX_SCREEN_Y] = MutateScaled(currentU[IDX_SCREEN_Y],
			PARAM_SAMPLER_METROPOLIS_IMAGE_MUTATION_RANGE, Rnd_FloatValue(seed));

	for (int i = IDX_SCREEN_Y + 1; i < TOTAL_U_SIZE; ++i)
		proposedU[i] = Mutate(currentU[i], Rnd_FloatValue(seed));
}

void Sampler_SplatSample(
		Seed *seed,
		__global SamplerSharedData *samplerSharedData,
		__global Sample *sample, __global float *sampleData
		FILM_PARAM_DECL
		) {
	//--------------------------------------------------------------------------
	// Accept/Reject the sample
	//--------------------------------------------------------------------------

	uint current = sample->current;
	uint proposed = sample->proposed;

	if (current == NULL_INDEX) {
		// It is the very first sample, I have still to initialize the current
		// sample

		Film_AddSample(sample->result.pixelX, sample->result.pixelY,
				&sample->result, 1.f
				FILM_PARAM);

		sample->currentResult = sample->result;
		sample->totalI = SampleResult_Radiance_Y(&sample->result);

		current = proposed;
		proposed ^= 1;
	} else {
		const float currentI = SampleResult_Radiance_Y(&sample->currentResult);

		float proposedI = SampleResult_Radiance_Y(&sample->result);
		proposedI = (isnan(proposedI) || isinf(proposedI)) ? 0.f : proposedI;

		float totalI = sample->totalI;
		uint largeMutationCount = sample->largeMutationCount;
		uint smallMutationCount = sample->smallMutationCount;
		if (smallMutationCount == 0) {
			// It is a large mutation
			totalI += proposedI;
			largeMutationCount += 1;

			sample->totalI = totalI;
			sample->largeMutationCount = largeMutationCount;
		}

		const float invMeanI = (totalI > 0.f) ? (largeMutationCount / totalI) : 1.f;

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

		__global SampleResult *contrib;
		float norm;

		if ((accProb == 1.f) || (rndVal < accProb)) {
			/*if (get_global_id(0) == 0)
				printf("\t\tACCEPTED !\n");*/

			// Add accumulated contribution of previous reference sample
			norm = weight / (currentI * invMeanI + PARAM_SAMPLER_METROPOLIS_LARGE_STEP_RATE);
			contrib = &sample->currentResult;

			current ^= 1;
			proposed ^= 1;
			consecutiveRejects = 0;

			weight = newWeight;
		} else {
			/*if (get_global_id(0) == 0)
				printf("\t\tREJECTED !\n");*/

			// Add contribution of new sample before rejecting it
			norm = newWeight / (proposedI * invMeanI + PARAM_SAMPLER_METROPOLIS_LARGE_STEP_RATE);
			contrib = &sample->result;

			++consecutiveRejects;
		}

		if (norm > 0.f) {
			/*if (get_global_id(0) == 0)
				printf("\t\tContrib: (%f, %f, %f) [%f] consecutiveRejects: %d\n",
						contrib->radiancePerPixelNormalized[0].c[0],
						contrib->radiancePerPixelNormalized[0].c[1],
						contrib->radiancePerPixelNormalized[0].c[2],
						norm, consecutiveRejects);*/

			Film_AddSample(contrib->pixelX, contrib->pixelY,
					contrib, norm
					FILM_PARAM);
		}

		// Check if it is an accepted mutation
		if (consecutiveRejects == 0) {
			// I can now (after Film_SplatSample()) overwrite sample->currentResult and sample->result
			sample->currentResult = sample->result;
		}

		sample->weight = weight;
		sample->consecutiveRejects = consecutiveRejects;
	}

	sample->current = current;
	sample->proposed = proposed;
}

void Sampler_NextSample(
		Seed *seed,
		__global SamplerSharedData *samplerSharedData,
		__global Sample *sample, __global float *sampleData,
		const uint filmWidth, const uint filmHeight,
		const uint filmSubRegion0, const uint filmSubRegion1,
		const uint filmSubRegion2, const uint filmSubRegion3) {
	//--------------------------------------------------------------------------
	// Mutate the sample
	//--------------------------------------------------------------------------

	__global float *proposedU = &sampleData[sample->proposed * TOTAL_U_SIZE];
	if (Rnd_FloatValue(seed) < PARAM_SAMPLER_METROPOLIS_LARGE_STEP_RATE) {
		LargeStep(seed, proposedU);
		sample->smallMutationCount = 0;
	} else {
		__global float *currentU = &sampleData[sample->current * TOTAL_U_SIZE];

		SmallStep(seed, currentU, proposedU);
		sample->smallMutationCount += 1;
	}
}

void Sampler_Init(Seed *seed, __global SamplerSharedData *samplerSharedData,
		__global Sample *sample, __global float *sampleData,
		const uint filmWidth, const uint filmHeight,
		const uint filmSubRegion0, const uint filmSubRegion1,
		const uint filmSubRegion2, const uint filmSubRegion3) {
	sample->totalI = 0.f;
	sample->largeMutationCount = 1.f;

	sample->current = NULL_INDEX;
	sample->proposed = 1;

	sample->smallMutationCount = 0;
	sample->consecutiveRejects = 0;

	sample->weight = 0.f;

	__global float *sampleDataPathBase = Sampler_GetSampleDataPathBase(sample, sampleData);
	LargeStep(seed, sampleDataPathBase);
}

#endif
