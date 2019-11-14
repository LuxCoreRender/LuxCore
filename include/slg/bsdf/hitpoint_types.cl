#line 2 "hitpoint_types.cl"

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

// This is defined only under OpenCL because of variable size structures
#if defined(SLG_OPENCL_KERNEL)

typedef struct {
	// The incoming direction. It is the eyeDir when fromLight = false and
	// lightDir when fromLight = true
	Vector fixedDir;
	Point p;
	Normal geometryN;
	Normal interpolatedN;
	Normal shadeN;

	UV uv;
#if defined(PARAM_ENABLE_TEX_HITPOINTCOLOR) || defined(PARAM_ENABLE_TEX_HITPOINTGREY) || defined(PARAM_TRIANGLE_LIGHT_HAS_VERTEX_COLOR)
	Spectrum color;
#endif
#if defined(PARAM_ENABLE_TEX_HITPOINTALPHA)
	float alpha;
#endif

	// Note: dpdu and dpdv are orthogonal to shading normal (i.e not geometry normal)
	Vector dpdu, dpdv;
	Normal dndu, dndv;

	// passThroughEvent can be stored here in a path state even before of
	// BSDF initialization (while tracing the next path vertex ray)
	float passThroughEvent;

	// Transformation from local object to world reference frame
	Transform localToWorld;

#if defined(PARAM_HAS_VOLUMES)
	// Interior and exterior volume (this includes volume priority system
	// computation and scene default world volume)
	unsigned int interiorVolumeIndex, exteriorVolumeIndex;
	// Material code (i.e. glass, etc.) doesn't have access to materials list
	// so I use HitPoint to carry texture index information
	unsigned int interiorIorTexIndex, exteriorIorTexIndex;
#endif

#if defined(PARAM_ENABLE_TEX_OBJECTID) || defined(PARAM_ENABLE_TEX_OBJECTID_COLOR) || defined(PARAM_ENABLE_TEX_OBJECTID_NORMALIZED)
	unsigned int objectID;
#endif

	int intoObject, throughShadowTransparency;
} HitPoint;

#endif
