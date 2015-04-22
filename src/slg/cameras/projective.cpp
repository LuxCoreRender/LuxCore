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

ProjectiveCamera::ProjectiveCamera(const CameraType type, const float *region,
		const luxrays::Point &o, const luxrays::Point &t, const luxrays::Vector &u) :
		Camera(type), orig(o), target(t), up(Normalize(u)),
		lensRadius(0.f), focalDistance(10.f) {
	if (region) {
		autoUpdateFilmRegion = false;
		filmRegion[0] = region[0];
		filmRegion[1] = region[1];
		filmRegion[2] = region[2];
		filmRegion[3] = region[3];
	} else
		autoUpdateFilmRegion = true;

	enableClippingPlane = false;
	clippingPlaneCenter = Point();
	clippingPlaneNormal = Normal(1.f, 0.f, 0.f);
}

void ProjectiveCamera::UpdateFocus(const Scene *scene) {
	if (autoFocus) {
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

Properties ProjectiveCamera::ToProperties() const {
	Properties props = Camera::ToProperties();

	props.Set(Property("scene.camera.lookat.orig")(orig));
	props.Set(Property("scene.camera.lookat.target")(target));
	props.Set(Property("scene.camera.up")(up));

	if (!autoUpdateFilmRegion)
		props.Set(Property("scene.camera.screenwindow")(filmRegion[0], filmRegion[1], filmRegion[2], filmRegion[3]));

	if (enableClippingPlane) {
		props.Set(Property("scene.camera.clippingplane.enable")(enableClippingPlane));
		props.Set(Property("scene.camera.clippingplane.center")(clippingPlaneCenter));
		props.Set(Property("scene.camera.clippingplane.normal")(clippingPlaneNormal));
	}

	return props;
}
