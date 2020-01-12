#line 2 "sampleresult_types.cl"

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

#define FILM_MAX_RADIANCE_GROUP_COUNT 8

//------------------------------------------------------------------------------
// SampleResult
//------------------------------------------------------------------------------

#if defined(SLG_OPENCL_KERNEL)

typedef struct {
	uint pixelX, pixelY;
	float filmX, filmY;

	Spectrum radiancePerPixelNormalized[FILM_MAX_RADIANCE_GROUP_COUNT];
#if defined(PARAM_FILM_CHANNELS_HAS_ALPHA)
	float alpha;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_DEPTH)
	float depth;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_POSITION)
	Point position;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_GEOMETRY_NORMAL)
	Normal geometryNormal;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_SHADING_NORMAL) || defined(PARAM_FILM_CHANNELS_HAS_AVG_SHADING_NORMAL)
	Normal shadingNormal;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_MATERIAL_ID) || defined(PARAM_FILM_CHANNELS_HAS_BY_MATERIAL_ID) || defined(PARAM_FILM_CHANNELS_HAS_MATERIAL_ID_MASK) || defined(PARAM_FILM_CHANNELS_HAS_MATERIAL_ID_COLOR)
	// Note: MATERIAL_ID_COLOR, MATERIAL_ID_MASK and BY_MATERIAL_ID are
	// calculated starting from materialID field
	uint materialID;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_OBJECT_ID)
	// Note: OBJECT_ID_MASK and BY_OBJECT_ID are calculated starting from objectID field
	uint objectID;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_DIRECT_DIFFUSE)
	Spectrum directDiffuse;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_DIRECT_GLOSSY)
	Spectrum directGlossy;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_EMISSION)
	Spectrum emission;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_INDIRECT_DIFFUSE)
	Spectrum indirectDiffuse;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_INDIRECT_GLOSSY)
	Spectrum indirectGlossy;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_INDIRECT_SPECULAR)
	Spectrum indirectSpecular;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_DIRECT_SHADOW_MASK)
	float directShadowMask;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_INDIRECT_SHADOW_MASK)
	float indirectShadowMask;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_UV)
	UV uv;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_RAYCOUNT)
	float rayCount;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_IRRADIANCE)
	Spectrum irradiance, irradiancePathThroughput;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_ALBEDO)
	Spectrum albedo;
#endif

	BSDFEvent firstPathVertexEvent;
	int firstPathVertex, lastPathVertex;
} SampleResult;

#endif
