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
	PMJ02Sequence();
	~PMJ02Sequence();
	
	void RequestSamples(const u_int size);
	float GetSample(const u_int pass, const u_int index);

private:

	struct float2 {
		float x;
		float y;
	};

	void generate_2D(float2 points[], u_int size, luxrays::RandomGenerator *rndGen);
	void mark_occupied_strata(float2 points[], u_int N);
	void generate_sample_point(float2 points[], float i, float j, float xhalf, float yhalf, u_int n, u_int N);
	void extend_sequence_even(float2 points[], u_int N);
	void extend_sequence_odd(float2 points[], u_int N);
	void mark_occupied_strata1(float2 pt, u_int NN);
	bool is_occupied(float2 pt, u_int NN);
	void shuffle(float2 points[], u_int size);

	std::vector<std::vector<bool>> occupiedStrata;
	std::vector<bool> occupied1Dx, occupied1Dy;

	// Hardcode that for now
	u_int num_samples = 4096;

	// Vector to hold each bidimensional table
	std::vector<std::vector<float2>> samplePoints;
};

}

#endif	/* _SLG_PMJ02_SEQUENCE_H */
