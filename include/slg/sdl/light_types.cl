#line 2 "light_types.cl"

/***************************************************************************
 * Copyright 1998-2013 by authors (see AUTHORS.txt)                        *
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

typedef enum {
	TYPE_IL, TYPE_IL_SKY, TYPE_SUN, TYPE_TRIANGLE, TYPE_POINT, TYPE_MAPPOINT,
	LIGHT_SOURCE_TYPE_COUNT
} LightSourceType;

typedef struct {
	TextureMapping2D mapping;
	unsigned int imageMapIndex;
	unsigned int distributionOffset;
} InfiniteLightParam;

typedef struct {
	float thetaS;
	float phiS;
	float zenith_Y, zenith_x, zenith_y;
	float perez_Y[6], perez_x[6], perez_y[6];
} SkyLightParam;

typedef struct {
	Vector sunDir;
	float turbidity, relSize;
	// XY Vectors for cone sampling
	Vector x, y;
	float cosThetaMax;
	Spectrum sunColor;
} SunLightParam;

typedef struct {
	Vector absolutePos;
	Spectrum emittedFactor;
} PointLightParam;

typedef struct {
	Vector absolutePos, localPos;
	Spectrum emittedFactor;
	float avarage;
	unsigned int imageMapIndex;
} MapPointLightParam;

typedef struct {
	Transform light2World;
	Spectrum gain;

	union {
		SunLightParam sun;
		SkyLightParam sky;
		InfiniteLightParam infinite;
		PointLightParam point;
		MapPointLightParam mapPoint;
	};
} NotIntersecableLightSource;

typedef struct {
	Vector v0, v1, v2;
	UV uv0, uv1, uv2;
	float invArea;

	unsigned int materialIndex;
	unsigned int lightSceneIndex;
} TriangleLightParam;

typedef struct {
	LightSourceType type;
	unsigned int lightSceneIndex;
	unsigned int lightID;
	int samples;
	// Type of indirect paths where a light source is visible with a direct hit. It is
	// an OR of DIFFUSE, GLOSSY and SPECULAR.
	BSDFEvent visibility;
	
	union {
		NotIntersecableLightSource notIntersecable;
		TriangleLightParam triangle;
	};
} LightSource;


//------------------------------------------------------------------------------
// Some macro trick in order to have more readable code
//------------------------------------------------------------------------------

#if defined(SLG_OPENCL_KERNEL)

#if defined(PARAM_HAS_INFINITELIGHT)
#define LIGHTS_PARAM_DECL , __global LightSource *lights, __global uint *envLightIndices, const uint envLightCount, __global uint *meshTriLightDefsOffset, __global float *infiniteLightDistribution, __global float *lightsDistribution MATERIALS_PARAM_DECL
#define LIGHTS_PARAM , lights, envLightIndices, envLightCount, meshTriLightDefsOffset, infiniteLightDistribution, lightsDistribution MATERIALS_PARAM
#elif defined(PARAM_HAS_SUNLIGHT) || defined(PARAM_HAS_SKYLIGHT)
#define LIGHTS_PARAM_DECL , __global LightSource *lights, __global uint *envLightIndices, const uint envLightCount, __global uint *meshTriLightDefsOffset, __global float *lightsDistribution MATERIALS_PARAM_DECL
#define LIGHTS_PARAM , lights, envLightIndices, envLightCount, meshTriLightDefsOffset, lightsDistribution MATERIALS_PARAM
#else
#define LIGHTS_PARAM_DECL , __global LightSource *lights, __global uint *meshTriLightDefsOffset, __global float *lightsDistribution MATERIALS_PARAM_DECL
#define LIGHTS_PARAM , lights, meshTriLightDefsOffset, lightsDistribution MATERIALS_PARAM
#endif

#endif
