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
		__global Ray *ray,
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

OPENCL_FORCE_INLINE void PerspectiveCamera_LocalSampleLens(
		__global const PerspectiveCamera* restrict cameraPersp,
		__global const float* restrict cameraBokehDistribution,
		const float u1, const float u2,
		float *lensU, float *lensV) {
	*lensU = 0.f;
	*lensV = 0.f;

	if (cameraPersp->projCamera.lensRadius > 0.f) {
		const float bokehBlades = cameraPersp->bokehBlades;
		const BokehDistributionType bokehDistribution = cameraPersp->bokehDistribution;
		if ((bokehDistribution == DIST_NONE) || ((bokehDistribution != DIST_CUSTOM) && (bokehBlades < 3)))
			ConcentricSampleDisk(u1, u2, lensU, lensV);
		else {
			// Bokeh support
			
			if (bokehDistribution == DIST_CUSTOM) {
				float2 sampleUV;
				float distPdf;
				Distribution2D_SampleContinuous(cameraBokehDistribution, u1, u2, &sampleUV, &distPdf);
				if (distPdf > 0.f) {
					*lensU = sampleUV.x * cameraPersp->bokehScaleX;
					*lensV = sampleUV.y * cameraPersp->bokehScaleY;
				}	
			} else {
				const float halfAngle = M_PI_F / bokehBlades;
				const float honeyRadius = cos(halfAngle);

				const float theta = 2.f * M_PI_F * u2;

				const uint sector = Floor2UInt(theta / halfAngle);
				const float rho = (sector % 2 == 0) ? (theta - sector * halfAngle) :
						((sector + 1) * halfAngle - theta);

				float r = honeyRadius / cos(rho);
				switch (cameraPersp->bokehDistribution) {
					case DIST_UNIFORM:
						r *= sqrt(u1);
						break;
					case DIST_EXPONENTIAL:
						r *= sqrt(ExponentialSampleDisk(u1, cameraPersp->bokehPower));
						break;
					case DIST_INVERSEEXPONENTIAL:
						r *= sqrt(InverseExponentialSampleDisk(u1, cameraPersp->bokehPower));
						break;
					case DIST_GAUSSIAN:
						r *= sqrt(GaussianSampleDisk(u1));
						break;
					case DIST_INVERSEGAUSSIAN:
						r *= sqrt(InverseGaussianSampleDisk(u1));
						break;
					case DIST_TRIANGULAR:
						r *= sqrt(TriangularSampleDisk(u1));
						break;
					default:
						// Something wrong here
						break;
				}

				*lensU = r * cos(theta) * cameraPersp->bokehScaleX;
				*lensV = r * sin(theta) * cameraPersp->bokehScaleY;
			}
		}

		const float lensRadius = cameraPersp->projCamera.lensRadius;
		*lensU *= lensRadius;
		*lensV *= lensRadius;
	}
}

OPENCL_FORCE_INLINE void PerspectiveCamera_GenerateRayImpl(
		__global const CameraBase* restrict cameraBase,
		__global const PerspectiveCamera* restrict cameraPersp,
		__global const Transform* restrict rasterToCamera,
		__global const Transform* restrict cameraToWorld,
		__global const float* restrict cameraBokehDistribution,
		const uint filmWidth, const uint filmHeight,
		__global Ray *ray,
		__global PathVolumeInfo *volInfo,
		const float filmX, const float filmY, const float timeSample,
		const float dofSampleX, const float dofSampleY) {
	PathVolumeInfo_StartVolume(volInfo, cameraBase->volumeIndex);

	float3 Pras;
	if (cameraPersp->enableOculusRiftBarrel) {
		float ssx, ssy;
		Camera_OculusRiftBarrelPostprocess(filmX / filmWidth, (filmHeight - filmY - 1.f) / filmHeight, &ssx, &ssy);
		Pras = MAKE_FLOAT3(min(ssx * filmWidth, (float) (filmWidth - 1)), min(ssy * filmHeight, (float) (filmHeight - 1)), 0.f);
	} else
		Pras = MAKE_FLOAT3(filmX, filmHeight - filmY - 1.f, 0.f);

	float3 rayOrig = Transform_ApplyPoint(rasterToCamera, Pras);
	float3 rayDir = rayOrig;

	const float hither = cameraBase->hither;

	const float lensRadius = cameraPersp->projCamera.lensRadius;
	const float focalDistance = cameraPersp->projCamera.focalDistance;
	if ((lensRadius > 0.f) && (focalDistance > 0.f)) {
		// Sample point on lens
		float lensU, lensV;
		PerspectiveCamera_LocalSampleLens(cameraPersp, cameraBokehDistribution, dofSampleX, dofSampleY, &lensU, &lensV);

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

	const float maxt = (cameraBase->yon - hither) / rayDir.z;
	const float time = mix(cameraBase->shutterOpen, cameraBase->shutterClose, timeSample);

	// Transform ray in world coordinates
	rayOrig = Transform_ApplyPoint(cameraToWorld, rayOrig);
	rayDir = Transform_ApplyVector(cameraToWorld, rayDir);

	const uint interpolatedTransformFirstIndex = cameraBase->motionSystem.interpolatedTransformFirstIndex;
	if (interpolatedTransformFirstIndex != NULL_INDEX) {
		Matrix4x4 m;
		MotionSystem_Sample(&cameraBase->motionSystem, time, cameraBase->interpolatedTransforms, &m);

		rayOrig = Matrix4x4_ApplyPoint_Private(&m, rayOrig);
		rayDir = Matrix4x4_ApplyVector_Private(&m, rayDir);
	}

	Ray_Init3(ray, rayOrig, rayDir, maxt, time);
	
	if (cameraPersp->projCamera.enableClippingPlane) {
		Camera_ApplyArbitraryClippingPlane(ray,
				VLOAD3F(&cameraPersp->projCamera.clippingPlaneCenter.x),
				VLOAD3F(&cameraPersp->projCamera.clippingPlaneNormal.x));
	}

	/*printf("(%f, %f, %f) (%f, %f, %f) [%f, %f]\n",
		ray->o.x, ray->o.y, ray->o.z, ray->d.x, ray->d.y, ray->d.z,
		ray->mint, ray->maxt);*/
}

OPENCL_FORCE_INLINE void PerspectiveCamera_GenerateRay(
		__global const Camera* restrict camera,
		__global const float* restrict cameraBokehDistribution,
		const uint filmWidth, const uint filmHeight,
		__global Ray *ray,
		__global PathVolumeInfo *volInfo,
		const float filmX, const float filmY, const float timeSample,
		const float dofSampleX, const float dofSampleY) {
	PerspectiveCamera_GenerateRayImpl(&camera->base, &camera->persp,
			&camera->base.rasterToCamera, &camera->base.cameraToWorld,
			cameraBokehDistribution,
			filmWidth, filmHeight, ray, volInfo, filmX, filmY,
			timeSample, dofSampleX, dofSampleY);
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

	const float3 Pras = MAKE_FLOAT3(filmX, filmHeight - filmY - 1.f, 0.f);
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
		Camera_ApplyArbitraryClippingPlane(ray,
				VLOAD3F(&camera->ortho.projCamera.clippingPlaneCenter.x),
				VLOAD3F(&camera->ortho.projCamera.clippingPlaneNormal.x));
	}


	/*printf("(%f, %f, %f) (%f, %f, %f) [%f, %f]\n",
		ray->o.x, ray->o.y, ray->o.z, ray->d.x, ray->d.y, ray->d.z,
		ray->mint, ray->maxt);*/
}

//------------------------------------------------------------------------------
// Environment camera
//------------------------------------------------------------------------------

OPENCL_FORCE_INLINE void EnvironmentCamera_GenerateRayImpl(
		__global const CameraBase* restrict cameraBase,
		__global const Transform* restrict cameraToWorld,
		const uint filmWidth, const uint filmHeight,
		__global Ray *ray,
		__global PathVolumeInfo *volInfo,
		const float filmX, const float filmY, const float timeSample,
		const float dofSampleX, const float dofSampleY) {
	PathVolumeInfo_StartVolume(volInfo, cameraBase->volumeIndex);

	const float theta = M_PI_F * (filmHeight - filmY) / filmHeight;
	const float phi = 2.f * M_PI_F * (filmWidth - filmX) / filmWidth - .5 * M_PI_F;

	float3 rayOrig = MAKE_FLOAT3(0.f, 0.f, 0.f);
	float3 rayDir = MAKE_FLOAT3(sin(theta)*cos(phi), cos(theta), sin(theta)*sin(phi));
	
	const float maxt = (cameraBase->yon - cameraBase->hither);
	const float time = mix(cameraBase->shutterOpen, cameraBase->shutterClose, timeSample);

	// Transform ray in world coordinates
	rayOrig = Transform_ApplyPoint(cameraToWorld, rayOrig);
	rayDir = Transform_ApplyVector(cameraToWorld, rayDir);

	const uint interpolatedTransformFirstIndex = cameraBase->motionSystem.interpolatedTransformFirstIndex;
	if (interpolatedTransformFirstIndex != NULL_INDEX) {
		Matrix4x4 m;
		MotionSystem_Sample(&cameraBase->motionSystem, time, cameraBase->interpolatedTransforms, &m);

		rayOrig = Matrix4x4_ApplyPoint_Private(&m, rayOrig);
		rayDir = Matrix4x4_ApplyVector_Private(&m, rayDir);
	}

	Ray_Init3(ray, rayOrig, rayDir, maxt, time);

	/*printf("(%f, %f, %f) (%f, %f, %f) [%f, %f]\n",
		ray->o.x, ray->o.y, ray->o.z, ray->d.x, ray->d.y, ray->d.z,
		ray->mint, ray->maxt);*/
}

OPENCL_FORCE_INLINE void EnvironmentCamera_GenerateRay(
		__global const Camera* restrict camera,
		const uint filmWidth, const uint filmHeight,
		__global Ray *ray,
		__global PathVolumeInfo *volInfo,
		const float filmX, const float filmY, const float timeSample,
		const float dofSampleX, const float dofSampleY) {
	EnvironmentCamera_GenerateRayImpl(&camera->base, &camera->base.cameraToWorld,
			filmWidth, filmHeight, ray, volInfo,
			filmX, filmY, timeSample, dofSampleX, dofSampleY);
}

//------------------------------------------------------------------------------
// Stereo camera
//------------------------------------------------------------------------------

OPENCL_FORCE_INLINE void StereoCamera_GenerateRay(
		__global const Camera* restrict camera,
		__global const float* restrict cameraBokehDistribution,
		const uint origFilmWidth, const uint filmHeight,
		__global Ray *ray,
		__global PathVolumeInfo *volInfo,
		const float origFilmX, const float filmY, const float timeSample,
		const float dofSampleX, const float dofSampleY) {
	PathVolumeInfo_StartVolume(volInfo, camera->base.volumeIndex);

	__global const Transform* restrict rasterToCamera;
	__global const Transform* restrict cameraToWorld;
	float filmX;
	const float filmWidth = origFilmWidth / 2;
	if (origFilmX < filmWidth) {
		rasterToCamera = &camera->stereo.leftEyeRasterToCamera;
		cameraToWorld = &camera->stereo.leftEyeCameraToWorld;
		filmX = origFilmX;
	} else {
		rasterToCamera = &camera->stereo.rightEyeRasterToCamera;
		cameraToWorld = &camera->stereo.rightEyeCameraToWorld;
		filmX = origFilmX - filmWidth;
	}

	switch(camera->stereo.stereoCameraType) {
		case STEREO_PERSPECTIVE:
			PerspectiveCamera_GenerateRayImpl(&camera->base, &camera->stereo.perspCamera,
					rasterToCamera, cameraToWorld, cameraBokehDistribution,
					filmWidth, filmHeight, ray, volInfo, filmX, filmY, timeSample, dofSampleX, dofSampleY);
			break;
		case STEREO_ENVIRONMENT:
			EnvironmentCamera_GenerateRayImpl(&camera->base, cameraToWorld,
					filmWidth, filmHeight, ray, volInfo,
					filmX, filmY, timeSample, dofSampleX, dofSampleY);
			break;
		default:
			// Something very wrong here...
			break;
	}

	/*printf("(%f, %f, %f) (%f, %f, %f) [%f, %f]\n",
		ray->o.x, ray->o.y, ray->o.z, ray->d.x, ray->d.y, ray->d.z,
		ray->mint, ray->maxt);*/
}

//------------------------------------------------------------------------------
// Generic function
//------------------------------------------------------------------------------

OPENCL_FORCE_NOT_INLINE void Camera_GenerateRay(
		__global const Camera* restrict camera,
		__global const float* restrict cameraBokehDistribution,
		const uint filmWidth, const uint filmHeight,
		__global Ray *ray,
		__global PathVolumeInfo *volInfo,
		const float filmX, const float filmY, const float timeSample,
		const float dofSampleX, const float dofSampleY) {
	switch (camera->type) {
		case PERSPECTIVE:
			PerspectiveCamera_GenerateRay(camera, cameraBokehDistribution, filmWidth, filmHeight, ray, volInfo, filmX, filmY, timeSample, dofSampleX, dofSampleY);
			break;
		case ORTHOGRAPHIC:
			OrthographicCamera_GenerateRay(camera, filmWidth, filmHeight, ray, volInfo, filmX, filmY, timeSample, dofSampleX, dofSampleY);
			break;
		case STEREO:
			StereoCamera_GenerateRay(camera, cameraBokehDistribution, filmWidth, filmHeight, ray, volInfo, filmX, filmY, timeSample, dofSampleX, dofSampleY);
			break;
		case ENVIRONMENT:
			EnvironmentCamera_GenerateRay(camera, filmWidth, filmHeight, ray, volInfo, filmX, filmY, timeSample, dofSampleX, dofSampleY);
			break;
		default:
			// Something really wrong here
			break;
	}
}
