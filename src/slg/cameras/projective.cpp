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

#include <cstddef>

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
		lensRadius(0.f), focalDistance(10.f) {
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

void ProjectiveCamera::UpdateFocus(const Scene *scene) {
	// scene->dataSet->GetAccelerator() is there because
	// FILESAVER engine doesn't initialize any accelerator
	if (autoFocus && scene->dataSet->GetAccelerator()) {
		// Trace a ray in the middle of the screen
		const Point Pras(filmWidth / 2, filmHeight / 2, 0.f);

		const Point Pcamera(camTrans.rasterToCamera * Pras);
		Ray ray;
		ray.o = Pcamera;
		ray.d = Vector(Pcamera.x, Pcamera.y, Pcamera.z);
		ray.d = Normalize(ray.d);
		
		ray.mint = 0.f;
		ray.maxt = (clipYon - clipHither) / ray.d.z;

		if (motionSystem)
			ray = motionSystem->Sample(0.f) * (camTrans.cameraToWorld * ray);
		else
			ray = camTrans.cameraToWorld * ray;
		
		// Trace the ray. If there isn't an intersection just use the current
		// focal distance
		RayHit rayHit;
		if (scene->dataSet->GetAccelerator()->Intersect(&ray, &rayHit))
			focalDistance = rayHit.t;
	}
}

void ProjectiveCamera::Update(const u_int width, const u_int height, const u_int *subRegion) {
	filmWidth = width;
	filmHeight = height;

	if (subRegion) {
		filmSubRegion[0] = subRegion[0];
		filmSubRegion[1] = subRegion[1];
		filmSubRegion[2] = subRegion[2];
		filmSubRegion[3] = subRegion[3];
	} else {
		filmSubRegion[0] = 0;
		filmSubRegion[1] = width - 1;
		filmSubRegion[2] = 0;
		filmSubRegion[3] = height - 1;		
	}

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

	// Initialize pixel information
	InitPixelArea();
	
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

void ProjectiveCamera::GenerateRay(const float filmX, const float filmY,
		Ray *ray, const float u1, const float u2, const float u3) const {
	InitRay(ray, filmX, filmY);

	// Modify ray for depth of field
	if (lensRadius > 0.f) {
		// Sample point on lens
		float lensU, lensV;
		ConcentricSampleDisk(u1, u2, &lensU, &lensV);
		lensU *= lensRadius;
		lensV *= lensRadius;

		// Compute point on plane of focus
		float ft = (focalDistance - clipHither);
		if (type != ORTHOGRAPHIC)
			ft /= ray->d.z;
		Point Pfocus = (*ray)(ft);
		// Update ray for effect of lens
		ray->o.x += lensU * (focalDistance - clipHither) / focalDistance;
		ray->o.y += lensV * (focalDistance - clipHither) / focalDistance;
		ray->d = Pfocus - ray->o;
	}

	ray->d = Normalize(ray->d);
	ray->mint = MachineEpsilon::E(ray->o);
	ray->maxt = (clipYon - clipHither);
	if (type != ORTHOGRAPHIC)
		ray->maxt /= ray->d.z;
	ray->time = Lerp(u3, shutterOpen, shutterClose);

	if (motionSystem)
		*ray = motionSystem->Sample(ray->time) * (camTrans.cameraToWorld * (*ray));
	else
		*ray = camTrans.cameraToWorld * (*ray);

	// World arbitrary clipping plane support
	if (enableClippingPlane)
		ApplyArbitraryClippingPlane(ray);
}

bool ProjectiveCamera::GetSamplePosition(Ray *ray, float *x, float *y) const {
	const float cosi = Dot(ray->d, dir);
	if ((cosi <= 0.f) || (!isinf(ray->maxt) && (ray->maxt * cosi < clipHither ||
		ray->maxt * cosi > clipYon)))
		return false;

	Point pO(Inverse(camTrans.rasterToWorld) * (ray->o + ((lensRadius > 0.f) ?	(ray->d * (focalDistance / cosi)) : ray->d)));
	if (motionSystem)
		pO *= motionSystem->Sample(ray->time);

	*x = pO.x;
	*y = filmHeight - 1 - pO.y;

	// Check if we are inside the image plane
	if ((*x < filmSubRegion[0]) || (*x >= filmSubRegion[1]) ||
			(*y < filmSubRegion[2]) || (*y >= filmSubRegion[3]))
		return false;
	else {
		// World arbitrary clipping plane support
		if (enableClippingPlane) {
			// Check if the ray end point is on the not visible side of the plane
			const Point endPoint = (*ray)(ray->maxt);
			if (Dot(clippingPlaneNormal, endPoint - clippingPlaneCenter) <= 0.f)
				return false;
			// Update ray mint/maxt
			ApplyArbitraryClippingPlane(ray);
		}

		return true;
	}
}

bool ProjectiveCamera::SampleLens(const float time,
		const float u1, const float u2,
		Point *lensp) const {
	Point lensPoint(0.f, 0.f, 0.f);
	if (lensRadius > 0.f) {
		ConcentricSampleDisk(u1, u2, &lensPoint.x, &lensPoint.y);
		lensPoint.x *= lensRadius;
		lensPoint.y *= lensRadius;
	}

	if (motionSystem)
		*lensp = motionSystem->Sample(time) * (camTrans.cameraToWorld * lensPoint);
	else
		*lensp = camTrans.cameraToWorld * lensPoint;

	return true;
}

Properties ProjectiveCamera::ToProperties() const {
	Properties props = Camera::ToProperties();

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

	return props;
}
