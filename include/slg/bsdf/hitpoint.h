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

#ifndef _SLG_HITPOINT_H
#define	_SLG_HITPOINT_H

#include "luxrays/luxrays.h"
#include "luxrays/core/epsilon.h"
#include "luxrays/core/color/color.h"
#include "luxrays/core/geometry/uv.h"
#include "luxrays/core/geometry/transform.h"
#include "luxrays/core/geometry/frame.h"
#include "luxrays/core/exttrianglemesh.h"

namespace slg {

// OpenCL data types
namespace ocl {
#include "slg/bsdf/hitpoint_types.cl"
}

class Volume;
class Scene;

typedef struct {
	// The incoming direction. It is the eyeDir when fromLight = false and
	// lightDir when fromLight = true
	luxrays::Vector fixedDir;
	luxrays::Point p;
	luxrays::Normal geometryN;
	luxrays::Normal interpolatedN;
	luxrays::Normal shadeN;

	luxrays::UV uv[EXTMESH_MAX_DATA_COUNT];
	luxrays::Spectrum color[EXTMESH_MAX_DATA_COUNT];
	float alpha[EXTMESH_MAX_DATA_COUNT];

	// Note: dpdu and dpdv are orthogonal to shading normal (i.e not geometry normal)
	luxrays::Vector dpdu, dpdv;
	luxrays::Normal dndu, dndv;

	float passThroughEvent;
	// Transformation from local object to world reference frame
	luxrays::Transform localToWorld;
	// Interior and exterior volume (this includes volume priority system
	// computation and scene default world volume)
	const Volume *interiorVolume, *exteriorVolume;
	u_int objectID;
	bool fromLight, intoObject;
	// If I got here going trough a shadow transparency. It can be used to disable MIS.
	bool throughShadowTransparency;

	// Used when hitting a surface
	//
	// Note: very important, this method assume localToWorld file has been _already_
	// initialized. This is done for performance reasons.
	//
	// Note: this is also _not_ initializing volume related information.
	void Init(const bool fixedFromLight, const bool throughShadowTransparency,
		const Scene &scene, const u_int meshIndex, const u_int triangleIndex,
		const luxrays::Point &p, const luxrays::Vector &d,
		const float b1, const float b2,
		const float passThroughEvent);

	// Initialize all fields (without a constructor)
	void Init();

	luxrays::Frame GetFrame() const { return luxrays::Frame(dpdu, dpdv, shadeN); }
	luxrays::Normal GetLandingGeometryN() const { return (intoObject ? 1.f : -1.f) * geometryN; }
	luxrays::Normal GetLandingInterpolatedN() const { return (intoObject ? 1.f : -1.f) * interpolatedN; }
	luxrays::Normal GetLandingShadeN() const { return (intoObject ? 1.f : -1.f) * shadeN; }
} HitPoint;

}

#endif	/* _SLG_HITPOINT_H */
