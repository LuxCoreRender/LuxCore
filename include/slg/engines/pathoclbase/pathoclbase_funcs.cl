#line 2 "patchoclbase_funcs.cl"

/***************************************************************************
 * Copyright 1998-2015 by authors (see AUTHORS.txt)                        *
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

// List of symbols defined at compile time:
//  PARAM_TASK_COUNT
//  PARAM_RAY_EPSILON_MIN
//  PARAM_RAY_EPSILON_MAX
//  PARAM_HAS_IMAGEMAPS
//  PARAM_HAS_PASSTHROUGH
//  PARAM_USE_PIXEL_ATOMICS
//  PARAM_HAS_BUMPMAPS
//  PARAM_ACCEL_BVH or PARAM_ACCEL_MBVH or PARAM_ACCEL_QBVH or PARAM_ACCEL_MQBVH
//  PARAM_DEVICE_INDEX
//  PARAM_DEVICE_COUNT
//  PARAM_LIGHT_WORLD_RADIUS_SCALE
//  PARAM_TRIANGLE_LIGHT_COUNT
//  PARAM_TRIANGLE_LIGHT_HAS_VERTEX_COLOR
//  PARAM_LIGHT_COUNT
//  PARAM_HAS_VOLUMEs (and SCENE_DEFAULT_VOLUME_INDEX)

// To enable single material support
//  PARAM_ENABLE_MAT_MATTE
//  PARAM_ENABLE_MAT_MIRROR
//  PARAM_ENABLE_MAT_GLASS
//  PARAM_ENABLE_MAT_ARCHGLASS
//  PARAM_ENABLE_MAT_MIX
//  PARAM_ENABLE_MAT_NULL
//  PARAM_ENABLE_MAT_MATTETRANSLUCENT
//  PARAM_ENABLE_MAT_ROUGHMATTETRANSLUCENT
//  PARAM_ENABLE_MAT_GLOSSY2
//  PARAM_ENABLE_MAT_METAL2
//  PARAM_ENABLE_MAT_ROUGHGLASS
//  PARAM_ENABLE_MAT_CLOTH
//  PARAM_ENABLE_MAT_CARPAINT
//  PARAM_ENABLE_MAT_GLOSSYTRANSLUCENT
//  PARAM_ENABLE_MAT_GLOSSYCOATING
//  PARAM_ENABLE_MAT_CLEAR_VOL

// To enable single texture support
//  PARAM_ENABLE_TEX_CONST_FLOAT
//  PARAM_ENABLE_TEX_CONST_FLOAT3
//  PARAM_ENABLE_TEX_CONST_FLOAT4
//  PARAM_ENABLE_TEX_IMAGEMAP
//  PARAM_ENABLE_TEX_SCALE
//  etc.

// Film related parameters:
//  PARAM_FILM_RADIANCE_GROUP_COUNT
//  PARAM_FILM_CHANNELS_HAS_ALPHA
//  PARAM_FILM_CHANNELS_HAS_DEPTH
//  PARAM_FILM_CHANNELS_HAS_POSITION
//  PARAM_FILM_CHANNELS_HAS_GEOMETRY_NORMAL
//  PARAM_FILM_CHANNELS_HAS_SHADING_NORMAL
//  PARAM_FILM_CHANNELS_HAS_MATERIAL_ID
//  PARAM_FILM_CHANNELS_HAS_DIRECT_DIFFUSE
//  PARAM_FILM_CHANNELS_HAS_DIRECT_GLOSSY
//  PARAM_FILM_CHANNELS_HAS_EMISSION
//  PARAM_FILM_CHANNELS_HAS_INDIRECT_DIFFUSE
//  PARAM_FILM_CHANNELS_HAS_INDIRECT_GLOSSY
//  PARAM_FILM_CHANNELS_HAS_INDIRECT_SPECULAR
//  PARAM_FILM_CHANNELS_HAS_MATERIAL_ID_MASK (and PARAM_FILM_MASK_MATERIAL_ID)
//  PARAM_FILM_CHANNELS_HAS_BY_MATERIAL_ID (and PARAM_FILM_BY_MATERIAL_ID)
//  PARAM_FILM_CHANNELS_HAS_DIRECT_SHADOW_MASK
//  PARAM_FILM_CHANNELS_HAS_INDIRECT_SHADOW_MASK
//  PARAM_FILM_CHANNELS_HAS_UV
//  PARAM_FILM_CHANNELS_HAS_RAYCOUNT
//  PARAM_FILM_CHANNELS_HAS_BY_MATERIAL_ID (and PARAM_FILM_BY_MATERIAL_ID)
//  PARAM_FILM_CHANNELS_HAS_IRRADIANCE
//  PARAM_FILM_CHANNELS_HAS_OBJECT_ID
//  PARAM_FILM_CHANNELS_HAS_OBJECT_ID_MASK (and PARAM_FILM_MASK_OBJECT_ID)
//  PARAM_FILM_CHANNELS_HAS_BY_OBJECT_ID (and PARAM_FILM_BY_OBJECT_ID)

// (optional)
//  PARAM_CAMERA_HAS_DOF

// (optional)
//  PARAM_HAS_INFINITELIGHT
//  PARAM_HAS_SUNLIGHT
//  PARAM_HAS_SKYLIGHT
//  PARAM_HAS_SKYLIGHT2
//  PARAM_HAS_POINTLIGHT
//  PARAM_HAS_MAPPOINTLIGHT
//  PARAM_HAS_SPOTLIGHT
//  PARAM_HAS_PROJECTIONLIGHT
//  PARAM_HAS_CONSTANTINFINITELIGHT
//  PARAM_HAS_SHARPDISTANTLIGHT
//  PARAM_HAS_DISTANTLIGHT
//  PARAM_HAS_LASERLIGHT
//  PARAM_HAS_INFINITELIGHTS (if it has any infinite light)
//  PARAM_HAS_ENVLIGHTS (if it has any env. light)

// (optional)
//  PARAM_HAS_NORMALS_BUFFER
//  PARAM_HAS_UVS_BUFFER
//  PARAM_HAS_COLS_BUFFER
//  PARAM_HAS_ALPHAS_BUFFER

void MangleMemory(__global unsigned char *ptr, const size_t size) {
	Seed seed;
	Rnd_Init(7 + get_global_id(0), &seed);

	for (uint i = 0; i < size; ++i)
		*ptr++ = (unsigned char)(Rnd_UintValue(&seed) & 0xff);
}

bool Scene_Intersect(
#if defined(PARAM_HAS_VOLUMES)
		__global PathVolumeInfo *volInfo,
		__global HitPoint *tmpHitPoint,
#endif
#if defined(PARAM_HAS_PASSTHROUGH)
		const float passThrough,
#endif
#if !defined(RENDER_ENGINE_BIASPATHOCL) && !defined(RENDER_ENGINE_RTBIASPATHOCL)
		__global
#endif
		Ray *ray,
#if !defined(RENDER_ENGINE_BIASPATHOCL) && !defined(RENDER_ENGINE_RTBIASPATHOCL)
		__global
#endif
		RayHit *rayHit,
		__global BSDF *bsdf,
		float3 *connectionThroughput,  const float3 pathThroughput,
		__global SampleResult *sampleResult,
		// BSDF_Init parameters
		__global const Mesh* restrict meshDescs,
		__global const SceneObject* restrict sceneObjs,
#if (PARAM_TRIANGLE_LIGHT_COUNT > 0)
		__global const uint *meshTriLightDefsOffset,
#endif
		__global const Point* restrict vertices,
#if defined(PARAM_HAS_NORMALS_BUFFER)
		__global const Vector* restrict vertNormals,
#endif
#if defined(PARAM_HAS_UVS_BUFFER)
		__global const UV* restrict vertUVs,
#endif
#if defined(PARAM_HAS_COLS_BUFFER)
		__global const Spectrum* restrict vertCols,
#endif
#if defined(PARAM_HAS_ALPHAS_BUFFER)
		__global const float* restrict vertAlphas,
#endif
		__global const Triangle* restrict triangles
		MATERIALS_PARAM_DECL
		) {
	*connectionThroughput = WHITE;
	const bool hit = (rayHit->meshIndex != NULL_INDEX);

#if defined(PARAM_HAS_VOLUMES)
	uint rayVolumeIndex = volInfo->currentVolumeIndex;
#endif
	if (hit) {
		// Initialize the BSDF of the hit point
		BSDF_Init(bsdf,
				meshDescs,
				sceneObjs,
#if (PARAM_TRIANGLE_LIGHT_COUNT > 0)
				meshTriLightDefsOffset,
#endif
				vertices,
#if defined(PARAM_HAS_NORMALS_BUFFER)
				vertNormals,
#endif
#if defined(PARAM_HAS_UVS_BUFFER)
				vertUVs,
#endif
#if defined(PARAM_HAS_COLS_BUFFER)
				vertCols,
#endif
#if defined(PARAM_HAS_ALPHAS_BUFFER)
				vertAlphas,
#endif
				triangles, ray, rayHit
#if defined(PARAM_HAS_PASSTHROUGH)
				, passThrough
#endif
#if defined(PARAM_HAS_VOLUMES)
				, volInfo
#endif
				MATERIALS_PARAM
				);

#if defined(PARAM_HAS_VOLUMES)
		rayVolumeIndex = bsdf->hitPoint.intoObject ? bsdf->hitPoint.exteriorVolumeIndex : bsdf->hitPoint.interiorVolumeIndex;
#endif
	}
#if defined(PARAM_HAS_VOLUMES)
	else if (rayVolumeIndex == NULL_INDEX) {
		// No volume information, I use the default volume
		rayVolumeIndex = SCENE_DEFAULT_VOLUME_INDEX;
	}
#endif

#if defined(PARAM_HAS_VOLUMES)
	// Check if there is volume scatter event
	if (rayVolumeIndex != NULL_INDEX) {
		// This applies volume transmittance too
		// Note: by using passThrough here, I introduce subtle correlation
		// between scattering events and pass-through events
		float3 connectionEmission = BLACK;

		const float t = Volume_Scatter(&mats[rayVolumeIndex], ray,
				hit ? rayHit->t : ray->maxt,
				passThrough, volInfo->scatteredStart,
				connectionThroughput, &connectionEmission,
				tmpHitPoint
				TEXTURES_PARAM);

		// Add the volume emitted light to the appropriate light group
		if (!Spectrum_IsBlack(connectionEmission) && sampleResult)
			SampleResult_AddEmission(sampleResult, BSDF_GetLightID(bsdf
				MATERIALS_PARAM),
					pathThroughput, connectionEmission);

		if (t > 0.f) {
			// There was a volume scatter event

			// I have to set RayHit fields even if there wasn't a real
			// ray hit
			rayHit->t = t;
			// This is a trick in order to have RayHit::Miss() return
			// false. I assume 0xfffffffeu will trigger a memory fault if
			// used (and the bug will be noticed)
			rayHit->meshIndex = 0xfffffffeu;

			BSDF_InitVolume(bsdf, mats, ray, rayVolumeIndex, t, passThrough);
			volInfo->scatteredStart = true;

			return false;
		}
	}
#endif

	if (hit) {
		// Check if the volume priority system tells me to continue to trace the ray
		bool continueToTrace =
#if defined(PARAM_HAS_VOLUMES)
			PathVolumeInfo_ContinueToTrace(volInfo, bsdf
				MATERIALS_PARAM);
#else
		false;
#endif

#if defined(PARAM_HAS_PASSTHROUGH)
		// Check if it is a pass through point
		if (!continueToTrace) {
			const float3 passThroughTrans = BSDF_GetPassThroughTransparency(bsdf
				MATERIALS_PARAM);
			if (!Spectrum_IsBlack(passThroughTrans)) {
				*connectionThroughput *= passThroughTrans;

				// It is a pass through point, continue to trace the ray
				continueToTrace = true;
			}
		}
#endif

		if (continueToTrace) {
#if defined(PARAM_HAS_VOLUMES)
			// Update volume information
			const BSDFEvent eventTypes = BSDF_GetEventTypes(bsdf
						MATERIALS_PARAM);
			PathVolumeInfo_Update(volInfo, eventTypes, bsdf
					MATERIALS_PARAM);
#endif

			// It is a transparent material, continue to trace the ray
			ray->mint = rayHit->t + MachineEpsilon_E(rayHit->t);

			// A safety check
			if (ray->mint >= ray->maxt)
				return false;
			else
				return true;
		} else
			return false;
	} else {
		// Nothing was hit, stop tracing the ray
		return false;
	}
}
