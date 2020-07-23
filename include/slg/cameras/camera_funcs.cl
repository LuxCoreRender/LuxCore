#line 2 "camera_funcs.cl"

/***************************************************************************
 * Copyright 1998-2016 by authors (see AUTHORS.txt)                        *
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

OPENCL_FORCE_INLINE void Camera_OculusRiftBarrelPostprocess(const float x, const float y, float *barrelX, float *barrelY) {
	// Express the sample in coordinates relative to the eye center
	float ex = x * 2.f - 1.f;
	float ey = y * 2.f - 1.f;

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

	*barrelX = (ex + 1.f) * .5f;
	*barrelY = (ey + 1.f) * .5f;
}

OPENCL_FORCE_INLINE void Camera_ApplyArbitraryClippingPlane(
		__global const Camera* restrict camera, __global Ray *ray,
		const float3 clippingPlaneCenter, const float3 clippingPlaneNormal) {
	const float3 rayOrig = MAKE_FLOAT3(ray->o.x, ray->o.y, ray->o.z);
	const float3 rayDir = MAKE_FLOAT3(ray->d.x, ray->d.y, ray->d.z);

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

//------------------------------------------------------------------------------
// Perspective camera
//------------------------------------------------------------------------------

OPENCL_FORCE_INLINE float3 PerspectiveCamera_LocalSampleLens(
		__global const Camera* restrict camera,
		const float u1, const float u2) {
	float3 lensPoint = MAKE_FLOAT3(0.f, 0.f, 0.f);

	if (camera->persp.projCamera.lensRadius > 0.f) {
		const float bokehBlades = camera->persp.bokehBlades;
		if (bokehBlades < 3)
			ConcentricSampleDisk(u1, u2, &lensPoint.x, &lensPoint.y);
		else {
			// Bokeh support
			const float halfAngle = M_PI_F / bokehBlades;
			const float honeyRadius = cosf(halfAngle);

			const float theta = 2.f * M_PI_F * u2;

			const uint sector = Floor2UInt(theta / halfAngle);
			const float rho = (sector % 2 == 0) ? (theta - sector * halfAngle) :
					((sector + 1) * halfAngle - theta);

			float r = honeyRadius / cosf(rho);
			switch (camera->persp.bokehDistribution) {
				case DIST_UNIFORM:
					r *= sqrtf(u1);
					break;
				case DIST_EXPONETIAL:
					r *= sqrtf(ExponentialSampleDisk(u1, camera->persp.bokehPower));
					break;
				case DIST_INVERSEEXPONETIAL:
					r *= sqrtf(InverseExponentialSampleDisk(u1, camera->persp.bokehPower));
					break;
				case DIST_GAUSSIAN:
					r *= sqrtf(GaussianSampleDisk(u1));
					break;
				case DIST_INVERSEGAUSSIAN:
					r *= sqrtf(InverseGaussianSampleDisk(u1));
					break;
				case DIST_TRIANGULAR:
					r *= sqrtf(TriangularSampleDisk(u1));
					break;
				default:
					// Something wrong here
					break;
			}

			lensPoint.x = r * cosf(theta);
			lensPoint.y = r * sinf(theta);
		}

		const float lensRadius = camera->persp.projCamera.lensRadius;
		lensPoint.x *= lensRadius;
		lensPoint.y *= lensRadius;
	}

	return lensPoint;
}

OPENCL_FORCE_INLINE void PerspectiveCamera_GenerateRay(
		__global const Camera* restrict camera,
		const uint filmWidth, const uint filmHeight,
		__global Ray *ray,
		__global PathVolumeInfo *volInfo,
		const float filmX, const float filmY, const float timeSample,
		const float dofSampleX, const float dofSampleY) {
	PathVolumeInfo_StartVolume(volInfo, camera->base.volumeIndex);

	float3 Pras;
	if (camera->persp.enableOculusRiftBarrel) {
		float ssx, ssy;
		Camera_OculusRiftBarrelPostprocess(filmX / filmWidth, (filmHeight - filmY - 1.f) / filmHeight, &ssx, &ssy);
		Pras = MAKE_FLOAT3 (min(ssx * filmWidth, (float) (filmWidth - 1)), min(ssy * filmHeight, (float) (filmHeight - 1)), 0.f);
	} else
		Pras = MAKE_FLOAT3 (filmX, filmHeight - filmY - 1.f, 0.f);

	float3 rayOrig = Transform_ApplyPoint(&camera->base.rasterToCamera, Pras);
	float3 rayDir = rayOrig;

	const float hither = camera->base.hither;

	const float lensRadius = camera->persp.projCamera.lensRadius;
	const float focalDistance = camera->persp.projCamera.focalDistance;
	if ((lensRadius > 0.f) && (focalDistance > 0.f)) {
		// Sample point on lens
		const float3 lensPoint = PerspectiveCamera_LocalSampleLens(camera, dofSampleX, dofSampleY);

		// Compute point on plane of focus
		const float dist = focalDistance - hither;

		const float ft = dist / rayDir.z;
		const float3 Pfocus = rayOrig + rayDir * ft;

		// Update ray for effect of lens
		const float k = dist / focalDistance;
		rayOrig.x += lensPoint.x * k;
		rayOrig.y += lensPoint.y * k;

		rayDir = Pfocus - rayOrig;
	}

	rayDir = normalize(rayDir);

	const float maxt = (camera->base.yon - hither) / rayDir.z;
	const float time = mix(camera->base.shutterOpen, camera->base.shutterClose, timeSample);

	// Transform ray in world coordinates
	rayOrig = Transform_ApplyPoint(&camera->base.cameraToWorld, rayOrig);
	rayDir = Transform_ApplyVector(&camera->base.cameraToWorld, rayDir);

	const uint interpolatedTransformFirstIndex = camera->base.motionSystem.interpolatedTransformFirstIndex;
	if (interpolatedTransformFirstIndex != NULL_INDEX) {
		Matrix4x4 m;
		MotionSystem_Sample(&camera->base.motionSystem, time, camera->base.interpolatedTransforms, &m);

		rayOrig = Matrix4x4_ApplyPoint_Private(&m, rayOrig);
		rayDir = Matrix4x4_ApplyVector_Private(&m, rayDir);
	}

	Ray_Init3(ray, rayOrig, rayDir, maxt, time);
	
	if (camera->persp.projCamera.enableClippingPlane) {
		Camera_ApplyArbitraryClippingPlane(camera, ray,
				VLOAD3F(&camera->persp.projCamera.clippingPlaneCenter.x),
				VLOAD3F(&camera->persp.projCamera.clippingPlaneNormal.x));
	}

	/*printf("(%f, %f, %f) (%f, %f, %f) [%f, %f]\n",
		ray->o.x, ray->o.y, ray->o.z, ray->d.x, ray->d.y, ray->d.z,
		ray->mint, ray->maxt);*/
}

//------------------------------------------------------------------------------
// Orthographic camera
//------------------------------------------------------------------------------

OPENCL_FORCE_INLINE void OrthographicCamera_GenerateRay(
		__global const Camera* restrict camera,
		const uint filmWidth, const uint filmHeight,
		__global Ray *ray,
		__global PathVolumeInfo *volInfo,
		const float filmX, const float filmY, const float timeSample,
		const float dofSampleX, const float dofSampleY) {
	PathVolumeInfo_StartVolume(volInfo, camera->base.volumeIndex);

	const float3 Pras = MAKE_FLOAT3 (filmX, filmHeight - filmY - 1.f, 0.f);
	float3 rayOrig = Transform_ApplyPoint(&camera->base.rasterToCamera, Pras);
	float3 rayDir = MAKE_FLOAT3(0.f, 0.f, 1.f);

	const float hither = camera->base.hither;

	const float lensRadius = camera->ortho.projCamera.lensRadius;
	const float focalDistance = camera->ortho.projCamera.focalDistance;
	if ((lensRadius > 0.f) && (focalDistance > 0.f)) {
		// Sample point on lens
		float lensU, lensV;
		ConcentricSampleDisk(dofSampleX, dofSampleY, &lensU, &lensV);
		lensU *= lensRadius;
		lensV *= lensRadius;

		// Compute point on plane of focus
		const float dist = focalDistance - hither;

		const float ft = dist;
		const float3 Pfocus = rayOrig + rayDir * ft;

		// Update ray for effect of lens
		const float k = dist / focalDistance;
		rayOrig.x += lensU * k;
		rayOrig.y += lensV * k;

		rayDir = Pfocus - rayOrig;
	}

	rayDir = normalize(rayDir);

	const float maxt = (camera->base.yon - hither);
	const float time = mix(camera->base.shutterOpen, camera->base.shutterClose, timeSample);

	// Transform ray in world coordinates
	rayOrig = Transform_ApplyPoint(&camera->base.cameraToWorld, rayOrig);
	rayDir = Transform_ApplyVector(&camera->base.cameraToWorld, rayDir);

	const uint interpolatedTransformFirstIndex = camera->base.motionSystem.interpolatedTransformFirstIndex;
	if (interpolatedTransformFirstIndex != NULL_INDEX) {
		Matrix4x4 m;
		MotionSystem_Sample(&camera->base.motionSystem, time, camera->base.interpolatedTransforms, &m);

		rayOrig = Matrix4x4_ApplyPoint_Private(&m, rayOrig);
		rayDir = Matrix4x4_ApplyVector_Private(&m, rayDir);
	}

	Ray_Init3(ray, rayOrig, rayDir, maxt, time);

	if (camera->ortho.projCamera.enableClippingPlane) {
		Camera_ApplyArbitraryClippingPlane(camera, ray,
				VLOAD3F(&camera->ortho.projCamera.clippingPlaneCenter.x),
				VLOAD3F(&camera->ortho.projCamera.clippingPlaneNormal.x));
	}


	/*printf("(%f, %f, %f) (%f, %f, %f) [%f, %f]\n",
		ray->o.x, ray->o.y, ray->o.z, ray->d.x, ray->d.y, ray->d.z,
		ray->mint, ray->maxt);*/
}

//------------------------------------------------------------------------------
// Stereo camera
//------------------------------------------------------------------------------

OPENCL_FORCE_INLINE void StereoCamera_GenerateRay(
		__global const Camera* restrict camera,
		const uint origFilmWidth, const uint filmHeight,
		__global Ray *ray,
		__global PathVolumeInfo *volInfo,
		const float origFilmX, const float filmY, const float timeSample,
		const float dofSampleX, const float dofSampleY) {
	PathVolumeInfo_StartVolume(volInfo, camera->base.volumeIndex);

	__global const Transform* restrict rasterToCamera;
	__global const Transform* restrict cameraToWorld;
	float filmX;
	const float filmWidth = origFilmWidth /2 ;
	if (origFilmX < filmWidth) {
		rasterToCamera = &camera->stereo.leftEyeRasterToCamera;
		cameraToWorld = &camera->stereo.leftEyeCameraToWorld;
		filmX = origFilmX;
	} else {
		rasterToCamera = &camera->stereo.rightEyeRasterToCamera;
		cameraToWorld = &camera->stereo.rightEyeCameraToWorld;
		filmX = origFilmX - filmWidth;
	}

	float3 Pras;
	if (camera->stereo.perspCamera.enableOculusRiftBarrel) {
		float ssx, ssy;
		Camera_OculusRiftBarrelPostprocess(filmX / filmWidth, (filmHeight - filmY - 1.f) / filmHeight, &ssx, &ssy);
		Pras = MAKE_FLOAT3 (min(ssx * filmWidth, (float) (filmWidth - 1)), min(ssy * filmHeight, (float) (filmHeight - 1)), 0.f);
	} else
		Pras = MAKE_FLOAT3 (filmX, filmHeight - filmY - 1.f, 0.f);

	float3 rayOrig = Transform_ApplyPoint(rasterToCamera, Pras);
	float3 rayDir = rayOrig;

	const float hither = camera->base.hither;

	const float lensRadius = camera->persp.projCamera.lensRadius;
	const float focalDistance = camera->persp.projCamera.focalDistance;
	if ((lensRadius > 0.f) && (focalDistance > 0.f)) {
		// Sample point on lens
		float lensU, lensV;
		ConcentricSampleDisk(dofSampleX, dofSampleY, &lensU, &lensV);
		lensU *= lensRadius;
		lensV *= lensRadius;

		// Compute point on plane of focus
		const float dist = focalDistance - hither;

		const float ft = dist / rayDir.z;
		const float3 Pfocus = rayOrig + rayDir * ft;

		// Update ray for effect of lens
		const float k = dist / focalDistance;
		rayOrig.x += lensU * k;
		rayOrig.y += lensV * k;

		rayDir = Pfocus - rayOrig;
	}

	rayDir = normalize(rayDir);

	const float maxt = (camera->base.yon - hither) / rayDir.z;
	const float time = mix(camera->base.shutterOpen, camera->base.shutterClose, timeSample);

	// Transform ray in world coordinates
	rayOrig = Transform_ApplyPoint(cameraToWorld, rayOrig);
	rayDir = Transform_ApplyVector(cameraToWorld, rayDir);

	const uint interpolatedTransformFirstIndex = camera->base.motionSystem.interpolatedTransformFirstIndex;
	if (interpolatedTransformFirstIndex != NULL_INDEX) {
		Matrix4x4 m;
		MotionSystem_Sample(&camera->base.motionSystem, time, camera->base.interpolatedTransforms, &m);

		rayOrig = Matrix4x4_ApplyPoint_Private(&m, rayOrig);
		rayDir = Matrix4x4_ApplyVector_Private(&m, rayDir);
	}

	Ray_Init3(ray, rayOrig, rayDir, maxt, time);

	if (camera->stereo.perspCamera.projCamera.enableClippingPlane) {
		Camera_ApplyArbitraryClippingPlane(camera, ray,
				VLOAD3F(&camera->stereo.perspCamera.projCamera.clippingPlaneCenter.x),
				VLOAD3F(&camera->stereo.perspCamera.projCamera.clippingPlaneNormal.x));
	}

	/*printf("(%f, %f, %f) (%f, %f, %f) [%f, %f]\n",
		ray->o.x, ray->o.y, ray->o.z, ray->d.x, ray->d.y, ray->d.z,
		ray->mint, ray->maxt);*/
}

//------------------------------------------------------------------------------
// Environment camera
//------------------------------------------------------------------------------

OPENCL_FORCE_INLINE void EnvironmentCamera_GenerateRay(
		__global const Camera* restrict camera,
		const uint filmWidth, const uint filmHeight,
		__global Ray *ray,
		__global PathVolumeInfo *volInfo,
		const float filmX, const float filmY, const float timeSample,
		const float dofSampleX, const float dofSampleY) {
	PathVolumeInfo_StartVolume(volInfo, camera->base.volumeIndex);

	const float theta = M_PI_F * (filmHeight - filmY) / filmHeight;
	const float phi = 2.f * M_PI_F * (filmWidth - filmX) / filmWidth - 0.5 * M_PI_F;

	float3 rayOrig = MAKE_FLOAT3 (0.f, 0.f, 0.f);
	float3 rayDir = MAKE_FLOAT3(sin(theta)*cos(phi), cos(theta), sin(theta)*sin(phi));
	
	const float maxt = (camera->base.yon - camera->base.hither);
	const float time = mix(camera->base.shutterOpen, camera->base.shutterClose, timeSample);

	// Transform ray in world coordinates
	rayOrig = Transform_ApplyPoint(&camera->base.cameraToWorld, rayOrig);
	rayDir = Transform_ApplyVector(&camera->base.cameraToWorld, rayDir);

	const uint interpolatedTransformFirstIndex = camera->base.motionSystem.interpolatedTransformFirstIndex;
	if (interpolatedTransformFirstIndex != NULL_INDEX) {
		Matrix4x4 m;
		MotionSystem_Sample(&camera->base.motionSystem, time, camera->base.interpolatedTransforms, &m);

		rayOrig = Matrix4x4_ApplyPoint_Private(&m, rayOrig);
		rayDir = Matrix4x4_ApplyVector_Private(&m, rayDir);
	}

	Ray_Init3(ray, rayOrig, rayDir, maxt, time);

	/*printf("(%f, %f, %f) (%f, %f, %f) [%f, %f]\n",
		ray->o.x, ray->o.y, ray->o.z, ray->d.x, ray->d.y, ray->d.z,
		ray->mint, ray->maxt);*/
}

//------------------------------------------------------------------------------
// Generic function
//------------------------------------------------------------------------------

OPENCL_FORCE_NOT_INLINE void Camera_GenerateRay(
		__global const Camera* restrict camera,
		const uint filmWidth, const uint filmHeight,
		__global Ray *ray,
		__global PathVolumeInfo *volInfo,
		const float filmX, const float filmY, const float timeSample,
		const float dofSampleX, const float dofSampleY) {
	switch (camera->type) {
		case PERSPECTIVE:
			PerspectiveCamera_GenerateRay(camera, filmWidth, filmHeight, ray, volInfo, filmX, filmY, timeSample, dofSampleX, dofSampleY);
			break;
		case ORTHOGRAPHIC:
			OrthographicCamera_GenerateRay(camera, filmWidth, filmHeight, ray, volInfo, filmX, filmY, timeSample, dofSampleX, dofSampleY);
			break;
		case STEREO:
			StereoCamera_GenerateRay(camera, filmWidth, filmHeight, ray, volInfo, filmX, filmY, timeSample, dofSampleX, dofSampleY);
			break;
		case ENVIRONMENT:
			EnvironmentCamera_GenerateRay(camera, filmWidth, filmHeight, ray, volInfo, filmX, filmY, timeSample, dofSampleX, dofSampleY);
			break;
		default:
			// Something really wrong here
			break;
	}
}
