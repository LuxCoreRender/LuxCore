/***************************************************************************
 * Copyright 1998-2020 by authors (see AUTHORS.txt)                        *
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

#ifndef _SLG_SUBDIVSHAPE_H
#define	_SLG_SUBDIVSHAPE_H

#include <string>
#include <vector>

#include "slg/usings.h"
#include "slg/shapes/shape.h"
#include "slg/shapes/subdiv_corner_data.h"

namespace slg {

// How the creases (sharp edges) of a subdiv shape are determined
enum class SubdivCreaseMode {
	NONE,       // no creases at all
	COMPUTED,   // computed from the dihedral angles (sharpnessthreshold) with a uniform creaseweight
	IMPORTED    // read from the mesh crease data (PLY edge element / DefineMeshExt crease_pairs);
	            // falls back to NONE when the mesh carries no crease data
};

class SubdivShape : public Shape {
public:
	SubdivShape(
		CameraConstPtr camera,
		luxrays::ExtTriangleMeshRef srcMesh,
		const u_int maxLevel,
		const float maxEdgeScreenSize,
		const float sharpnessThresholdRadians,
		const float creaseWeight,
		const SubdivCreaseMode creaseMode = SubdivCreaseMode::COMPUTED,
		const int matSlot = -1
	);
	virtual ~SubdivShape();

	virtual ShapeType GetType() const override { return SUBDIV; }

	static float MaxEdgeScreenSize(CameraConstRef camera, luxrays::ExtTriangleMeshRef srcMesh);
	static luxrays::ExtTriangleMeshUPtr ApplySubdiv(
		luxrays::ExtTriangleMeshRef srcMesh,
		const u_int maxLevel,
		const float sharpnessThresholdRadians,
		const float creaseWeight,
		const SubdivCreaseMode creaseMode = SubdivCreaseMode::COMPUTED,
		std::vector<u_int> *triBaseFace = nullptr,
		const SubdivCornerData *cornerData = nullptr,
		SubdivCornerData *outCorner = nullptr
	);

protected:
	virtual luxrays::ExtTriangleMeshUPtr RefineImpl(SceneConstRef scene) override;

	CameraConstPtr camera;
	luxrays::ExtTriangleMeshRef srcMesh;
	u_int maxLevel;
	float maxEdgeScreenSize;
	float sharpnessThresholdRadians;
	float creaseWeight;
	SubdivCreaseMode creaseMode;
	int matSlot;

};

}

#endif	/* _SLG_SUBDIVSHAPE_H */
// vim: autoindent noexpandtab tabstop=4 shiftwidth=4
