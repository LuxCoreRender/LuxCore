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

#ifndef _SLG_STRANDSSHAPE_H
#define	_SLG_STRANDSSHAPE_H

#include <string>
#include <vector>

#include "luxrays/utils/cyhair/cyHairFile.h"

#include "slg/shapes/shape.h"

namespace slg {

class StrendsShape : public Shape {
public:
	typedef enum {
		TESSEL_RIBBON, TESSEL_RIBBON_ADAPTIVE,
		TESSEL_SOLID, TESSEL_SOLID_ADAPTIVE
	} TessellationType;

	StrendsShape(const Scene *scene,
			const luxrays::cyHairFile *hairFile, const TessellationType tesselType,
			const u_int adaptiveMaxDepth, const float adaptiveError, 
			const u_int solidSideCount, const bool solidCapBottom, const bool solidCapTop,
			const bool useCameraPosition);
	virtual ~StrendsShape();

	virtual ShapeType GetType() const { return STRANDS; }

protected:
	virtual luxrays::ExtTriangleMesh *RefineImpl(const Scene *scene);
	
	void TessellateRibbon(const Scene *scene,
		const std::vector<luxrays::Point> &hairPoints,
		const std::vector<float> &hairSizes, const std::vector<luxrays::Spectrum> &hairCols,
		const std::vector<luxrays::UV> &hairUVs, const std::vector<float> &hairTransps,
		std::vector<luxrays::Point> &meshVerts, std::vector<luxrays::Normal> &meshNorms,
		std::vector<luxrays::Triangle> &meshTris, std::vector<luxrays::UV> &meshUVs, std::vector<luxrays::Spectrum> &meshCols,
		std::vector<float> &meshTransps) const;
	void TessellateAdaptive(const Scene *scene,
		const bool solid, const std::vector<luxrays::Point> &hairPoints,
		const std::vector<float> &hairSizes, const std::vector<luxrays::Spectrum> &hairCols,
		const std::vector<luxrays::UV> &hairUVs, const std::vector<float> &hairTransps,
		std::vector<luxrays::Point> &meshVerts, std::vector<luxrays::Normal> &meshNorms,
		std::vector<luxrays::Triangle> &meshTris, std::vector<luxrays::UV> &meshUVs, std::vector<luxrays::Spectrum> &meshCols,
		std::vector<float> &meshTransps) const;
	void TessellateSolid(const Scene *scene,
		const std::vector<luxrays::Point> &hairPoints,
		const std::vector<float> &hairSizes, const std::vector<luxrays::Spectrum> &hairCols,
		const std::vector<luxrays::UV> &hairUVs, const std::vector<float> &hairTransps,
		std::vector<luxrays::Point> &meshVerts, std::vector<luxrays::Normal> &meshNorms,
		std::vector<luxrays::Triangle> &meshTris, std::vector<luxrays::UV> &meshUVs, std::vector<luxrays::Spectrum> &meshCols,
		std::vector<float> &meshTransps) const;

	// Tessellation options
	u_int adaptiveMaxDepth;
	float adaptiveError;
	u_int solidSideCount;
	bool solidCapBottom, solidCapTop;
	bool useCameraPosition;

	luxrays::ExtTriangleMesh *mesh;
};

}

#endif	/* _SLG_STRANDSSHAPE_H */
