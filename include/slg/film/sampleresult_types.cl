#line 2 "sampleresult_types.cl"

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

#define FILM_MAX_RADIANCE_GROUP_COUNT 8

//------------------------------------------------------------------------------
// SampleResult
//------------------------------------------------------------------------------

typedef struct {
	unsigned int pixelX, pixelY;
	float filmX, filmY;

	Spectrum radiancePerPixelNormalized[FILM_MAX_RADIANCE_GROUP_COUNT];
	float alpha;
	float depth;
	Point position;
	Normal geometryNormal;
	Normal shadingNormal;
	// Note: MATERIAL_ID_COLOR, MATERIAL_ID_MASK and BY_MATERIAL_ID are
	// calculated starting from materialID field
	unsigned int materialID;
	// Note: OBJECT_ID_MASK and BY_OBJECT_ID are calculated starting from objectID field
	unsigned int objectID;
	Spectrum directDiffuse;
	Spectrum directGlossy;
	Spectrum emission;
	Spectrum indirectDiffuse;
	Spectrum indirectGlossy;
	Spectrum indirectSpecular;
	float directShadowMask;
	float indirectShadowMask;
	UV uv;
	float rayCount;
	Spectrum irradiance, irradiancePathThroughput;
	Spectrum albedo;

	BSDFEvent firstPathVertexEvent;
	int isHoldout;

	int firstPathVertex, lastPathVertex;
} SampleResult;
