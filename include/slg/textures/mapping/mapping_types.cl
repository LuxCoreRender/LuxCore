#line 2 "mapping_types.cl"

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

//------------------------------------------------------------------------------
// TextureMapping2D
//------------------------------------------------------------------------------

typedef enum {
	UVMAPPING2D, UVRANDOMMAPPING2D
} TextureMapping2DType;

typedef struct {
    float sinTheta, cosTheta, uScale, vScale, uDelta, vDelta;
} UVMapping2DParam;

typedef enum {
	OBJECT_ID, TRIANGLE_AOV, OBJECT_ID_OFFSET
} RandomMappingSeedType;

typedef struct {
    RandomMappingSeedType seedType;
	union {
		unsigned int triAOVIndex;
		unsigned int objectIDOffset;
	};
	float uvRotationMin, uvRotationMax, uvRotationStep;
	float uScaleMin, uScaleMax;
	float vScaleMin, vScaleMax;
	float uDeltaMin, uDeltaMax;
	float vDeltaMin, vDeltaMax;
	
	int uniformScale;
} UVRandomMapping2DParam;

typedef struct {
	TextureMapping2DType type;
	unsigned int dataIndex;
	union {
		UVMapping2DParam uvMapping2D;
		UVRandomMapping2DParam uvRandomMapping2D;
	};
} TextureMapping2D;

//------------------------------------------------------------------------------
// TextureMapping3D
//------------------------------------------------------------------------------

typedef enum {
	UVMAPPING3D, GLOBALMAPPING3D, LOCALMAPPING3D, LOCALRANDOMMAPPING3D
} TextureMapping3DType;

typedef struct {
    unsigned int dataIndex;
} UVMapping3DParam;

typedef struct {
	RandomMappingSeedType seedType;
	union {
		unsigned int triAOVIndex;
		unsigned int objectIDOffset;
	};
	float xRotationMin, xRotationMax, xRotationStep;
	float yRotationMin, yRotationMax, yRotationStep;
	float zRotationMin, zRotationMax, zRotationStep;
	float xScaleMin, xScaleMax;
	float yScaleMin, yScaleMax;
	float zScaleMin, zScaleMax;
	float xTranslateMin, xTranslateMax;
	float yTranslateMin, yTranslateMax;
	float zTranslateMin, zTranslateMax;

	int uniformScale;
} LocalRandomMapping3DParam;

typedef struct {
	TextureMapping3DType type;
	Transform worldToLocal;
	union {
		UVMapping3DParam uvMapping3D;
		// GlobalMapping3D has no additional parameters
		// LocalMapping3D has no additional parameters
		LocalRandomMapping3DParam localRandomMapping;
	};
} TextureMapping3D;
