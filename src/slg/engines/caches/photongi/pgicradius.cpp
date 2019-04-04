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

#include "slg/engines/caches/photongi/photongicache.h"
#include "slg/utils/film2sceneradius.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// EvaluateBestRadius
//------------------------------------------------------------------------------

namespace slg {

class PGICFilm2SceneRadiusValidator : public Film2SceneRadiusValidator {
public:
	PGICFilm2SceneRadiusValidator(const PhotonGICache &c) : pgic(c) { }
	virtual ~PGICFilm2SceneRadiusValidator() { }
	
	virtual bool IsValid(const BSDF &bsdf) const {
		return pgic.IsPhotonGIEnabled(bsdf);
	}

private:
	const PhotonGICache &pgic;
};

}

float PhotonGICache::EvaluateBestRadius() {
	SLG_LOG("PhotonGI evaluating best radius");

	// The percentage of image plane to cover with the radius
	const float imagePlaneRadius = .02f;

	// The old default radius: 15cm
	const float defaultRadius = .15f;
	
	PGICFilm2SceneRadiusValidator validator(*this);

	return Film2SceneRadius(scene, imagePlaneRadius, defaultRadius,
			params.photon.maxPathDepth,
			params.photon.timeStart, params.photon.timeEnd,
			&validator);
}
