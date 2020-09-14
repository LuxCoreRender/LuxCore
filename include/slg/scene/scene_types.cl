#line 2 "scene_types.cl"

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

// Note: keep aligned with the copy in scene.h
typedef enum {
	// Mandatory setting: one or the other must be used
	EYE_RAY = 1,
	LIGHT_RAY = 2,

	// This is used to disable any type of ray switch
	GENERIC_RAY = 4,
	// For the very first eye ray
	CAMERA_RAY = 8,
	// For rays used for direct light sampling
	SHADOW_RAY = 16,
	// For rays used for indirect light sampling
	INDIRECT_RAY = 32
} SceneRayTypeType;

// Note: keep aligned with the copy in scene.h
typedef int SceneRayType;

typedef struct {
	unsigned int defaultVolumeIndex;
} Scene;

#if defined(SLG_OPENCL_KERNEL)

#define SCENE_PARAM_DECL , \
		__constant const Scene* restrict scene, \
		__global const SceneObject* restrict sceneObjs, \
		__global const uint* restrict lightIndexOffsetByMeshIndex, \
		__global const uint* restrict lightIndexByTriIndex \
		EXTMESH_PARAM_DECL
#define SCENE_PARAM , \
		scene, \
		sceneObjs, \
		lightIndexOffsetByMeshIndex, \
		lightIndexByTriIndex \
		EXTMESH_PARAM

#endif
