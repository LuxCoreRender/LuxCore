#line 2 "bsdf_types.cl"

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

typedef enum {
	NO_REFLECT_TRANSMIT, ONLY_REFLECT, ONLY_TRANSMIT, REFLECT_TRANSMIT
} AlbedoSpecularSetting;

typedef enum {
	NONE = 0,
	DIFFUSE = 1,
	GLOSSY = 2,
	SPECULAR = 4,
	REFLECT = 8,
	TRANSMIT = 16,

	ALL_TYPES = DIFFUSE | GLOSSY | SPECULAR,
	ALL_REFLECT = REFLECT | ALL_TYPES,
	ALL_TRANSMIT = TRANSMIT | ALL_TYPES,
	ALL = ALL_REFLECT | ALL_TRANSMIT
} BSDFEventType;

typedef int BSDFEvent;

typedef struct {
	HitPoint hitPoint;

	unsigned int materialIndex, sceneObjectIndex;
	unsigned int triangleLightSourceIndex;

	Frame frame;

	int isVolume;
} BSDF;
