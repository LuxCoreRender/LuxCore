#line 2 "sceneobject_types.cl"

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

typedef struct {
	unsigned int objectID;
	unsigned int materialIndex;
	int cameraInvisible;
} SceneObject;

#if defined(SLG_OPENCL_KERNEL)

#define SCENEOBJECTS_PARAM_DECL , \
		__global const Mesh* restrict meshDescs, \
		__global const SceneObject* restrict sceneObjs, \
		__global const uint* restrict lightIndexOffsetByMeshIndex, \
		__global const uint* restrict lightIndexByTriIndex, \
		__global const Point* restrict vertices, \
		__global const Vector* restrict vertNormals, \
		__global const UV* restrict vertUVs, \
		__global const Spectrum* restrict vertCols, \
		__global const float* restrict vertAlphas, \
		__global const Triangle* restrict triangles
#define SCENEOBJECTS_PARAM , \
		meshDescs, \
		sceneObjs, \
		lightIndexOffsetByMeshIndex, \
		lightIndexByTriIndex, \
		vertices, \
		vertNormals, \
		vertUVs, \
		vertCols, \
		vertAlphas, \
		triangles

#endif
