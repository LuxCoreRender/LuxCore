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

#include <cstddef>

#include "luxrays/core/epsilon.h"
#include "slg/cameras/projective.h"
#include "slg/film/film.h"
#include "slg/core/sdl.h"
#include "slg/scene/scene.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// PerspectiveCamera
//------------------------------------------------------------------------------

ProjectiveCamera::ProjectiveCamera(const CameraType type, const float *sw,
		const luxrays::Point &o, const luxrays::Point &t, const luxrays::Vector &u) :
		Camera(type), orig(o), target(t), up(Normalize(u)),
		lensRadius(0.f), focalDistance(10.f), autoFocus(false) {
	if (sw) {
		autoUpdateScreenWindow = false;
		screenWindow[0] = sw[0];
		screenWindow[1] = sw[1];
		screenWindow[2] = sw[2];
		screenWindow[3] = sw[3];
	} else
		autoUpdateScreenWindow = true;

	enableClippingPlane = false;
	clippingPlaneCenter = Point();
	clippingPlaneNormal = Normal(1.f, 0.f, 0.f);
}

void ProjectiveCamera::UpdateAuto(const Scene *scene) {
	if (autoFocus) {
		// Save lens radius
		const float lensR = lensRadius;

		// Trace a ray in the middle of the screen
		Ray ray;
		PathVolumeInfo volInfo;
		GenerateRay(0.f, filmWidth / 2.f, filmHeight / 2.f, &ray, &volInfo, 0.f, 0.f);

		// Restore lens radius
		lensRadius = lensR;
	
		// Trace the ray. If there isn't an intersection just use the current
		// focal distance
		RayHit rayHit;
		if (scene->dataSet->GetAccelerator(ACCEL_EMBREE)->Intersect(&ray, &rayHit))
			focalDistance = rayHit.t;
	}

	Camera::UpdateAuto(scene);
}

void ProjectiveCamera::Update(const u_int width, const u_int height, const u_int *subRegion) {
	Camera::Update(width, height, subRegion);

	// Used to translate the camera
	dir = target - orig;
	dir = Normalize(dir);

	x = Cross(dir, up);
	x = Normalize(x);

	y = Cross(x, dir);
	y = Normalize(y);

	// Initialize screen information
	const float frame = float(filmWidth) / float(filmHeight);

	// Check if I have to update screenWindow
	if (autoUpdateScreenWindow) {
		if (frame < 1.f) {
			screenWindow[0] = -frame;
			screenWindow[1] = frame;
			screenWindow[2] = -1.f;
			screenWindow[3] = 1.f;
		} else {
			screenWindow[0] = -1.f;
			screenWindow[1] = 1.f;
			screenWindow[2] = -1.f / frame;
			screenWindow[3] = 1.f / frame;
		}
	}

	// Initialize camera transformations
	InitCameraTransforms(&camTrans);

	// Initialize other camera data
	InitCameraData();
	
	if (enableClippingPlane)
		clippingPlaneNormal = Normalize(clippingPlaneNormal);
}

void ProjectiveCamera::ApplyArbitraryClippingPlane(Ray *ray) const {
	// Intersect the ray with clipping plane
	const float denom = Dot(clippingPlaneNormal, ray->d);
	const Vector pr = clippingPlaneCenter - ray->o;
	float d = Dot(pr, clippingPlaneNormal);

	if (fabsf(denom) > DEFAULT_COS_EPSILON_STATIC) {
		// There is a valid intersection
		d /= denom; 

		if (d > 0.f) {
			// The plane is in front of the camera
			if (denom < 0.f) {
				// The plane points toward the camera
				ray->maxt = Clamp(d, ray->mint, ray->maxt);
			} else {
				// The plane points away from the camera
				ray->mint = Clamp(d, ray->mint, ray->maxt);
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

void ProjectiveCamera::GenerateRay(const float  time,
		const float filmX, const float filmY,
		Ray *ray, PathVolumeInfo *volInfo,
		const float u0, const float u1) const {
	InitRay(ray, filmX, filmY);
	volInfo->AddVolume(volume);

	// Modify ray for depth of field
	if ((lensRadius > 0.f) && (focalDistance > 0.f)) {
		// Sample point on lens
		Point lensPoint;
		LocalSampleLens(time, u0, u1, &lensPoint);

		// Compute point on plane of focus
		const float dist = focalDistance - clipHither;
		float ft = dist;
		if (type != ORTHOGRAPHIC)
			ft /= ray->d.z;
		Point Pfocus = (*ray)(ft);
		// Update ray for effect of lens
		const float k = dist / focalDistance;
		ray->o.x += lensPoint.x * k;
		ray->o.y += lensPoint.y * k;
		ray->d = Pfocus - ray->o;
	}

	ray->d = Normalize(ray->d);
	ray->mint = MachineEpsilon::E(ray->o);
	ray->maxt = (clipYon - clipHither);
	if (type != ORTHOGRAPHIC)
		ray->maxt /= ray->d.z;
	ray->time = time;

	if (motionSystem) {
		*ray = motionSystem->Sample(ray->time) * (camTrans.cameraToWorld * (*ray));
		// I need to normalize the direction vector again because the motion
		// system could include some kind of scale
		ray->d = Normalize(ray->d);
	} else
		*ray = camTrans.cameraToWorld * (*ray);

	// World arbitrary clipping plane support
	if (enableClippingPlane)
		ApplyArbitraryClippingPlane(ray);
}

void ProjectiveCamera::Rotate(const float angle, const luxrays::Vector &axis) {
	luxrays::Vector dir = target - orig;
	luxrays::Transform t = luxrays::Rotate(angle, axis);
	const Vector newDir = t * dir;

	// Check if the up vector is the same of view direction. If they are,
	// skip this operation (it would trigger a Singular matrix in MatrixInvert)
	if (AbsDot(Normalize(newDir), up) < 1.f - DEFAULT_EPSILON_STATIC)
		target = orig + newDir;
}

Properties ProjectiveCamera::ToProperties(const ImageMapCache &imgMapCache, const bool useRealFileName) const {
	Properties props = Camera::ToProperties(imgMapCache, useRealFileName);

	props.Set(Property("scene.camera.lookat.orig")(orig));
	props.Set(Property("scene.camera.lookat.target")(target));
	props.Set(Property("scene.camera.up")(up));

	if (!autoUpdateScreenWindow)
		props.Set(Property("scene.camera.screenwindow")(screenWindow[0], screenWindow[1], screenWindow[2], screenWindow[3]));

	if (enableClippingPlane) {
		props.Set(Property("scene.camera.clippingplane.enable")(enableClippingPlane));
		props.Set(Property("scene.camera.clippingplane.center")(clippingPlaneCenter));
		props.Set(Property("scene.camera.clippingplane.normal")(clippingPlaneNormal));
	}

	props.Set(Property("scene.camera.lensradius")(lensRadius));
	props.Set(Property("scene.camera.focaldistance")(focalDistance));
	props.Set(Property("scene.camera.autofocus.enable")(autoFocus));

	return props;
}
