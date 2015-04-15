#line 2 "camera_funcs.cl"

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

#if defined(PARAM_CAMERA_ENABLE_OCULUSRIFT_BARREL)
void Camera_OculusRiftBarrelPostprocess(const float x, const float y, float *barrelX, float *barrelY) {
	// Express the sample in coordinates relative to the eye center
	float ex, ey;
	if (x < .5f) {
		// A left eye sample
		ex = x * 4.f - 1.f;
		ey = y * 2.f - 1.f;
	} else {
		// A right eye sample
		ex = (x - .5f) * 4.f - 1.f;
		ey = y * 2.f - 1.f;
	}

	if ((ex == 0.f) && (ey == 0.f)) {
		*barrelX = 0.f;
		*barrelY = 0.f;
		return;
	}

	// Distance from the eye center
	const float distance = sqrt(ex * ex + ey * ey);

	// "Push" the sample away base on the distance from the center
	const float scale = 1.f / 1.4f;
	const float k0 = 1.f;
	const float k1 = .22f;
	const float k2 = .23f;
	const float k3 = 0.f;
	const float distance2 = distance * distance;
	const float distance4 = distance2 * distance2;
	const float distance6 = distance2 * distance4;
	const float fr = scale * (k0 + k1 * distance2 + k2 * distance4 + k3 * distance6);

	ex *= fr;
	ey *= fr;

	// Clamp the coordinates
	ex = clamp(ex, -1.f, 1.f);
	ey = clamp(ey, -1.f, 1.f);

	if (x < .5f) {
		*barrelX = (ex + 1.f) * .25f;
		*barrelY = (ey + 1.f) * .5f;
	} else {
		*barrelX = (ex + 1.f) * .25f + .5f;
		*barrelY = (ey + 1.f) * .5f;
	}
}
#endif

#if defined(PARAM_CAMERA_ENABLE_CLIPPING_PLANE)
void Camera_ApplyArbitraryClippingPlane(
		__global const Camera *camera,
#if !defined(CAMERA_GENERATERAY_PARAM_MEM_SPACE_PRIVATE)
		__global
#endif
		Ray *ray) {
	const float3 rayOrig = (float3)(ray->o.x, ray->o.y, ray->o.z);
	const float3 rayDir = (float3)(ray->d.x, ray->d.y, ray->d.z);

	const float3 clippingPlaneCenter = (float3)(camera->clippingPlaneCenter.x, camera->clippingPlaneCenter.y, camera->clippingPlaneCenter.z);
	const float3 clippingPlaneNormal = (float3)(camera->clippingPlaneNormal.x, camera->clippingPlaneNormal.y, camera->clippingPlaneNormal.z);

	// Intersect the ray with clipping plane
	const float denom = dot(clippingPlaneNormal, rayDir);
	const float3 pr = clippingPlaneCenter - rayOrig;
	float d = dot(pr, clippingPlaneNormal);

	if (fabs(denom) > DEFAULT_COS_EPSILON_STATIC) {
		// There is a valid intersection
		d /= denom; 

		if (d > 0.f) {
			// The plane is in front of the camera
			if (denom < 0.f) {
				// The plane points toward the camera
				ray->maxt = clamp(d, ray->mint, ray->maxt);
			} else {
				// The plane points away from the camera
				ray->mint = clamp(d, ray->mint, ray->maxt);
			}
		} else {
			if ((denom < 0.f) && (d < 0.f)) {
				// No intersection possible, I use a trick here to avoid any
				// intersection by setting mint=maxt
				ray->mint = ray->maxt;
			} else {
				// Nothing to do
			}
		}
	} else {
		// The plane is parallel to the view directions. Check if I'm on the
		// visible side of the plane or not
		if (d >= 0.f) {
			// No intersection possible, I use a trick here to avoid any
			// intersection by setting mint=maxt
			ray->mint = ray->maxt;
		} else {
			// Nothing to do
		}
	}
}
#endif

void Camera_GenerateRay(
		__global const Camera *camera,
		const uint filmWidth, const uint filmHeight,
#if !defined(CAMERA_GENERATERAY_PARAM_MEM_SPACE_PRIVATE)
		__global
#endif
		Ray *ray,
		const float filmX, const float filmY, const float timeSample
#if defined(PARAM_CAMERA_HAS_DOF)
		, const float dofSampleX, const float dofSampleY
#endif
		) {
#if defined(PARAM_CAMERA_ENABLE_HORIZ_STEREO)
	// Left eye or right eye
	const uint transIndex = (filmX < filmWidth * .5f) ? 0 : 1;
#else
	const uint transIndex = 0;
#endif

#if defined(PARAM_CAMERA_ENABLE_HORIZ_STEREO) && defined(PARAM_CAMERA_ENABLE_OCULUSRIFT_BARREL)
	float ssx, ssy;
	Camera_OculusRiftBarrelPostprocess(filmX / filmWidth, (filmHeight - filmY - 1.f) / filmHeight, &ssx, &ssy);
	float3 Pras = (float3)(min(ssx * filmWidth, (float)(filmWidth - 1)), min(ssy * filmHeight, (float)(filmHeight - 1)), 0.f);
#else
	float3 Pras = (float3)(filmX, filmHeight - filmY - 1.f, 0.f);
#endif

	float3 rayOrig = Transform_ApplyPoint(&camera->rasterToCamera[transIndex], Pras);
	float3 rayDir = rayOrig;

	const float hither = camera->hither;

#if defined(PARAM_CAMERA_HAS_DOF)
	// Sample point on lens
	float lensU, lensV;
	ConcentricSampleDisk(dofSampleX, dofSampleY, &lensU, &lensV);
	const float lensRadius = camera->lensRadius;
	lensU *= lensRadius;
	lensV *= lensRadius;

	// Compute point on plane of focus
	const float focalDistance = camera->focalDistance;
	const float dist = focalDistance - hither;
	const float ft = dist / rayDir.z;
	float3 Pfocus;
	Pfocus = rayOrig + rayDir * ft;

	// Update ray for effect of lens
	const float k = dist / focalDistance;
	rayOrig.x += lensU * k;
	rayOrig.y += lensV * k;

	rayDir = Pfocus - rayOrig;
#endif

	rayDir = normalize(rayDir);
	const float maxt = (camera->yon - hither) / rayDir.z;
	const float time = mix(camera->shutterOpen, camera->shutterClose, timeSample);

	// Transform ray in world coordinates
	rayOrig = Transform_ApplyPoint(&camera->cameraToWorld[transIndex], rayOrig);
	rayDir = Transform_ApplyVector(&camera->cameraToWorld[transIndex], rayDir);

	const uint interpolatedTransformFirstIndex = camera->motionSystem.interpolatedTransformFirstIndex;
	if (interpolatedTransformFirstIndex != NULL_INDEX) {
		Matrix4x4 m;
		MotionSystem_Sample(&camera->motionSystem, time, camera->interpolatedTransforms, &m);

		rayOrig = Matrix4x4_ApplyPoint_Private(&m, rayOrig);
		rayDir = Matrix4x4_ApplyVector_Private(&m, rayDir);		
	}

#if defined(CAMERA_GENERATERAY_PARAM_MEM_SPACE_PRIVATE)
	Ray_Init3_Private(ray, rayOrig, rayDir, maxt, time);
#else
	Ray_Init3(ray, rayOrig, rayDir, maxt, time);
#endif

#if defined(PARAM_CAMERA_ENABLE_CLIPPING_PLANE)
	Camera_ApplyArbitraryClippingPlane(camera, ray);
#endif

	/*printf("(%f, %f, %f) (%f, %f, %f) [%f, %f]\n",
		ray->o.x, ray->o.y, ray->o.z, ray->d.x, ray->d.y, ray->d.z,
		ray->mint, ray->maxt);*/
}
