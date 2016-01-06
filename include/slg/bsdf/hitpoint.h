/***************************************************************************
 * Copyright 1998-2015 by authors (see AUTHORS.txt)                        *
 *                                                                         *
 *   This file is part of LuxRender.                                       *
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

#ifndef _SLG_HITPOINT_H
#define	_SLG_HITPOINT_H

#include "luxrays/luxrays.h"
#include "luxrays/core/color/color.h"
#include "luxrays/core/geometry/transform.h"
#include "luxrays/core/geometry/frame.h"

namespace slg {

// OpenCL data types
namespace ocl {
#include "slg/bsdf/hitpoint_types.cl"
}

class Volume;

typedef struct {
	// The incoming direction. It is the eyeDir when fromLight = false and
	// lightDir when fromLight = true
	luxrays::Vector fixedDir;
	luxrays::Point p;
	luxrays::UV uv;
	luxrays::Normal geometryN;
	luxrays::Normal shadeN;
	luxrays::Spectrum color;
	// Note: dpdu and dpdv are orthogonal to shading normal (i.e not geometry normal)
	luxrays::Vector dpdu, dpdv;
	luxrays::Normal dndu, dndv;
	float alpha;
	float passThroughEvent;
	// Transformation from local object to world reference frame
	luxrays::Transform local2World;
	// Interior and exterior volume (this includes volume priority system
	// computation and scene default world volume)
	const Volume *interiorVolume, *exteriorVolume;
	bool fromLight, intoObject;

	luxrays::Frame GetFrame() const { return luxrays::Frame(dpdu, dpdv, shadeN); }
} HitPoint;

}

#endif	/* _SLG_HITPOINT_H */
