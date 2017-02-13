#line 2 "mapping_types.cl"

/***************************************************************************
 * Copyright 1998-2017 by authors (see AUTHORS.txt)                        *
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

//------------------------------------------------------------------------------
// TextureMapping2D
//------------------------------------------------------------------------------

typedef enum {
	UVMAPPING2D
} TextureMapping2DType;

typedef struct {
    float uScale, vScale, uDelta, vDelta;
} UVMappingParam;


typedef struct {
	TextureMapping2DType type;
	union {
		UVMappingParam uvMapping2D;
	};
} TextureMapping2D;

//------------------------------------------------------------------------------
// TextureMapping3D
//------------------------------------------------------------------------------

typedef enum {
	UVMAPPING3D, GLOBALMAPPING3D, LOCALMAPPING3D
} TextureMapping3DType;

typedef struct {
	TextureMapping3DType type;
	Transform worldToLocal;
	//union {
		// UVMapping3D has no additional parameters
		// GlobalMapping3D has no additional parameters
		// LocalMapping3D has no additional parameters
	//};
} TextureMapping3D;
