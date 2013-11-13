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
	TYPE_IL, TYPE_IL_SKY, TYPE_SUN, TYPE_TRIANGLE
} LightSourceType;

typedef struct {
	Transform light2World;
	Spectrum gain;
	TextureMapping2D mapping;
	unsigned int imageMapIndex;
	unsigned int lightSceneIndex, lightID;
} InfiniteLight;

typedef struct {
	Transform light2World;
	Spectrum gain;
	float thetaS;
	float phiS;
	float zenith_Y, zenith_x, zenith_y;
	float perez_Y[6], perez_x[6], perez_y[6];
	unsigned int lightSceneIndex, lightID;
} SkyLight;

typedef struct {
	Vector sunDir;
	Spectrum gain;
	float turbidity;
	float relSize;
	// XY Vectors for cone sampling
	Vector x, y;
	float cosThetaMax;
	Spectrum sunColor;
	unsigned int lightSceneIndex, lightID;
} SunLight;

typedef struct {
	Vector v0, v1, v2;
	UV uv0, uv1, uv2;
	float invArea;

	unsigned int materialIndex;
	unsigned int lightSceneIndex;
} TriangleLight;
