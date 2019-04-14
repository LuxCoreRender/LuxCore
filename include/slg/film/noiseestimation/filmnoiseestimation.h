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

#ifndef _SLG_FILM_NOISE_ESTIMATION_H
#define	_SLG_FILM_NOISE_ESTIMATION_H

#include <vector>

#include "luxrays/utils/properties.h"
#include "luxrays/utils/serializationutils.h"
#include "slg/slg.h"
#include "slg/film/framebuffer.h"

namespace slg {

//------------------------------------------------------------------------------
// FilmNoiseEstimation
//------------------------------------------------------------------------------

class Film;

class FilmNoiseEstimation {
public:
	FilmNoiseEstimation(const Film *film, const u_int warmup,
			const u_int testStep, const u_int filterScale);
	~FilmNoiseEstimation();

	void Reset();
	void Test();

	u_int todoPixelsCount;
	float maxDiff;
	
	friend class boost::serialization::access;

private:
	// Used by serialization
	FilmNoiseEstimation();

	template<class Archive> void serialize(Archive &ar, const u_int version);

	u_int warmup;
	u_int testStep;
	u_int filterScale;

	const Film *film;

	GenericFrameBuffer<3, 0, float> *referenceImage;
	std::vector<float> errorVector;

	double lastSamplesCount;
	bool firstTest;
};

}

BOOST_CLASS_VERSION(slg::FilmNoiseEstimation, 2)

BOOST_CLASS_EXPORT_KEY(slg::FilmNoiseEstimation)

#endif	/* _SLG_FILM_NOISE_ESTIMATION_H */
