#line 2 "light_types.cl"

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

// This is used to scale the world radius in sun/sky/infinite lights in order to
// avoid problems with objects that are near the borderline of the world bounding sphere
#define LIGHT_WORLD_RADIUS_SCALE 1.05f

typedef enum {
	TYPE_IL, TYPE_SUN, TYPE_TRIANGLE, TYPE_POINT, TYPE_MAPPOINT,
	TYPE_SPOT, TYPE_PROJECTION, TYPE_IL_CONSTANT, TYPE_SHARPDISTANT, TYPE_DISTANT,
	TYPE_IL_SKY2, TYPE_LASER, TYPE_SPHERE, TYPE_MAPSPHERE,
	LIGHT_SOURCE_TYPE_COUNT
} LightSourceType;

typedef struct {
	unsigned int imageMapIndex;
	unsigned int distributionOffset;
	int useVisibilityMapCache;
} InfiniteLightParam;

typedef struct {
	Vector absoluteSunDir, absoluteUpDir;
	Spectrum aTerm, bTerm, cTerm, dTerm, eTerm, fTerm,
		gTerm, hTerm, iTerm, radianceTerm;
	int hasGround, isGroundBlack;
	Spectrum scaledGroundColor;
	unsigned int distributionOffset;
	int useVisibilityMapCache;
} SkyLight2Param;

typedef struct {
	Vector absoluteDir;
	float turbidity, relSize;
	// XY Vectors for cone sampling
	Vector x, y;
	float cosThetaMax, sin2ThetaMax;
	Spectrum color;
} SunLightParam;

typedef struct {
	Vector absolutePos;
	Spectrum emittedFactor;
} PointLightParam;

typedef struct {
	Vector absolutePos, localPos;
	Spectrum emittedFactor;
	float average;
	unsigned int imageMapIndex;
} MapPointLightParam;

typedef struct {
	Vector absolutePos;
	Spectrum emittedFactor;
	float cosTotalWidth, cosFalloffStart;
} SpotLightParam;

typedef struct {
	Vector absolutePos, lightNormal;
	Spectrum emittedFactor;
	Matrix4x4 lightProjection;
	float screenX0, screenX1, screenY0, screenY1;
	unsigned int imageMapIndex;
} ProjectionLightParam;

typedef struct {
	Spectrum color;
	int useVisibilityMapCache;
} ConstantInfiniteLightParam;

typedef struct {
	Spectrum color;
	Vector absoluteLightDir;
} SharpDistantLightParam;

typedef struct {
	Spectrum color;
	Vector absoluteLightDir, x, y;
	float cosThetaMax;
} DistantLightParam;

typedef struct {
	Vector absoluteLightPos, absoluteLightDir;
	Spectrum emittedFactor;
	float radius;
} LaserLightParam;

typedef struct {
	Vector absolutePos;
	Spectrum emittedFactor;
	float radius;
} SphereLightParam;

typedef struct {
	SphereLightParam sphere;
	float average;
	unsigned int imageMapIndex;
} MapSphereLightParam;

typedef struct {
	Transform light2World;
	Spectrum gain, temperatureScale;

	union {
		SunLightParam sun;
		SkyLight2Param sky2;
		InfiniteLightParam infinite;
		PointLightParam point;
		MapPointLightParam mapPoint;
		SpotLightParam spot;
		ProjectionLightParam projection;
		ConstantInfiniteLightParam constantInfinite;
		SharpDistantLightParam sharpDistant;
		DistantLightParam distant;
		LaserLightParam laser;
		SphereLightParam sphere;
		MapSphereLightParam mapSphere;
	};
} NotIntersectableLightSource;

typedef struct {
	float invTriangleArea, invMeshArea;

	unsigned int meshIndex, triangleIndex;

	// Used for image map and/or IES map
	float average;
	unsigned int imageMapIndex;
} TriangleLightParam;

typedef struct {
	LightSourceType type;
	unsigned int lightSceneIndex;
	unsigned int lightID;
	// Type of indirect paths where a light source is visible with a direct hit. It is
	// an OR of DIFFUSE, GLOSSY and SPECULAR.
	BSDFEvent visibility;
	int isDirectLightSamplingEnabled;
	
	union {
		NotIntersectableLightSource notIntersectable;
		TriangleLightParam triangle;
	};
} LightSource;

//------------------------------------------------------------------------------
// Some macro trick in order to have more readable code
//------------------------------------------------------------------------------

#if defined(SLG_OPENCL_KERNEL)

#define LIGHTS_PARAM_DECL , __global const LightSource* restrict lights, \
	__global const uint* restrict envLightIndices, \
	const uint envLightCount, \
	__global const float* restrict envLightDistribution, \
	__global const float* restrict lightsDistribution, \
	__global const float* restrict infiniteLightSourcesDistribution, \
	__global const DLSCacheEntry* restrict dlscAllEntries, \
	__global const float* restrict dlscDistributions, \
	__global const IndexBVHArrayNode* restrict dlscBVHNodes, \
	const float dlscRadius2, const float dlscNormalCosAngle, \
	__global const ELVCacheEntry* restrict elvcAllEntries, \
	__global const float* restrict elvcDistributions, \
	__global const uint* restrict elvcTileDistributionOffsets, \
	__global const IndexBVHArrayNode* restrict elvcBVHNodes, \
	const float elvcRadius2, const float elvcNormalCosAngle, \
	const uint elvcTilesXCount, const uint elvcTilesYCount \
	MATERIALS_PARAM_DECL
#define LIGHTS_PARAM , lights, \
	envLightIndices, \
	envLightCount, \
	envLightDistribution, \
	lightsDistribution, \
	infiniteLightSourcesDistribution, \
	dlscAllEntries, \
	dlscDistributions, \
	dlscBVHNodes, \
	dlscRadius2, \
	dlscNormalCosAngle, \
	elvcAllEntries, \
	elvcDistributions, \
	elvcTileDistributionOffsets, \
	elvcBVHNodes, \
	elvcRadius2, \
	elvcNormalCosAngle, \
	elvcTilesXCount, \
	elvcTilesXCount \
	MATERIALS_PARAM

#endif
