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

#ifndef _SLG_PMJ02_SEQUENCE_H
#define	_SLG_PMJ02_SEQUENCE_H

#include <vector>

#include "luxrays/core/randomgen.h"

#include "slg/slg.h"

namespace slg {

//------------------------------------------------------------------------------
// PMJ02Sequence
//------------------------------------------------------------------------------

class PMJ02Sequence {
public:
	PMJ02Sequence(luxrays::RandomGenerator *rndGen);
	~PMJ02Sequence();
	
	void RequestSamples(const u_int size);
	
	float GetSample(const u_int pixelIndex, const u_int pass);

	u_int rngPass;
	
private:
	// Generates for a single pixel index
	void RequestSamples(const u_int size, const u_int index);
	
	luxrays::RandomGenerator *rndGen;

	struct float2 {
		float2() {}
		float2(float xi, float yi) : x(xi), y(yi) {
		}

		float x;
		float y;
	};

	void generate_2D(float2 points[], u_int size);
	void mark_occupied_strata(float2 points[], u_int N);
	void generate_sample_point(float2 points[], float i, float j, float xhalf, float yhalf, u_int n, u_int N);
	void extend_sequence_even(float2 points[], u_int N);
	void extend_sequence_odd(float2 points[], u_int N);
	void mark_occupied_strata1(float2 pt, u_int NN);
	bool is_occupied(float2 pt, u_int NN);
	void shuffle(float2 points[], u_int size);
	u_int cmj_hash_simple(u_int i, u_int p);

	std::vector<std::vector<bool>> occupiedStrata;
	std::vector<bool> occupied1Dx, occupied1Dy;

	u_int shufflingSeed;

	// How many samples should be generated at once
	u_int num_samples;

	// Current sample being generated
	// Value is only valid during a single generate_2d call
	u_int current_sample = 1;

	// A vector with each pair of dimensions
	//	A vector with the (num_samples) 2D samples for that pixel
	std::vector<std::vector<float2>> samplePoints;

};

}

#endif	/* _SLG_PMJ02_SEQUENCE_H */
