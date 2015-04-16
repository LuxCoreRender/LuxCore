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

#include "slg/cameras/orthographic.h"
#include "slg/film/film.h"
#include "slg/core/sdl.h"
#include "slg/scene/scene.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// OrthographicCamera
//------------------------------------------------------------------------------

OrthographicCamera::OrthographicCamera(const Point &o, const Point &t,
		const Vector &u, const float *region) : Camera(ORTHOGRAPHIC),
		orig(o), target(t), up(Normalize(u)) {
	if (region) {
		autoUpdateFilmRegion = false;
		filmRegion[0] = region[0];
		filmRegion[1] = region[1];
		filmRegion[2] = region[2];
		filmRegion[3] = region[3];
	} else
		autoUpdateFilmRegion = true;

	enableClippingPlane = true;
	clippingPlaneCenter = Point();
	clippingPlaneNormal = Normal(1.f, 0.f, 0.f);
}

void OrthographicCamera::Update(const u_int width, const u_int height, const u_int *subRegion) {
	filmWidth = width;
	filmHeight = height;

	// Used to translate the camera
	dir = target - orig;
	dir = Normalize(dir);

	x = Cross(dir, up);
	x = Normalize(x);

	y = Cross(x, dir);
	y = Normalize(y);

	// Initialize screen information
	const float frame = float(filmWidth) / float(filmHeight);
	float screen[4];

	u_int filmSubRegion[4];
	filmSubRegion[0] = 0;
	filmSubRegion[1] = filmWidth - 1;
	filmSubRegion[2] = 0;
	filmSubRegion[3] = filmHeight - 1;

	if (autoUpdateFilmRegion) {
		if (frame < 1.f) {
			screen[0] = -frame;
			screen[1] = frame;
			screen[2] = -1.f;
			screen[3] = 1.f;
		} else {
			screen[0] = -1.f;
			screen[1] = 1.f;
			screen[2] = -1.f / frame;
			screen[3] = 1.f / frame;
		}
	} else {
		screen[0] = filmRegion[0];
		screen[1] = filmRegion[1];
		screen[2] = filmRegion[2];
		screen[3] = filmRegion[3];
	}

	if (subRegion) {
		// I have to render a sub region of the image
		filmSubRegion[0] = subRegion[0];
		filmSubRegion[1] = subRegion[1];
		filmSubRegion[2] = subRegion[2];
		filmSubRegion[3] = subRegion[3];
		filmWidth = filmSubRegion[1] - filmSubRegion[0] + 1;
		filmHeight = filmSubRegion[3] - filmSubRegion[2] + 1;

		const float halfW = filmWidth * .5f;
		const float halfH = filmHeight * .5f;
		if (frame < 1.f) {
			screen[0] = -frame * (-halfW + filmSubRegion[0]) / (-halfW);
			screen[1] = frame * (filmSubRegion[0] + filmWidth - halfW) / halfW;
			screen[2] = -(-halfH + filmSubRegion[2]) / (-halfH);
			screen[3] = (filmSubRegion[2] + filmHeight - halfH) / halfH;
		} else {
			screen[0] = -(-halfW + filmSubRegion[0]) / (-halfW);
			screen[1] = (filmSubRegion[0] + filmWidth - halfW) / halfW;
			screen[2] = -(1.f / frame) * (-halfH + filmSubRegion[2]) / (-halfH);
			screen[3] = (1.f / frame) * (filmSubRegion[2] + filmHeight - halfH) / halfH;			
		}
	}

	// Initialize camera transformations
	InitCameraTransforms(&camTrans, screen, 0.f, 0.f);

	// Initialize pixel information
	const float xPixelWidth = ((screen[1] - screen[0]) / 2.f);
	const float yPixelHeight = ((screen[3] - screen[2]) / 2.f);
	pixelArea = xPixelWidth * yPixelHeight;
	
	if (enableClippingPlane)
		clippingPlaneNormal = Normalize(clippingPlaneNormal);
}

void OrthographicCamera::UpdateFocus(const Scene *scene) {
	if (autoFocus) {
		// Trace a ray in the middle of the screen
		const Point Pras(filmWidth / 2, filmHeight / 2, 0.f);

		const Point Pcamera(camTrans.rasterToCamera * Pras);
		Ray ray;
		ray.o = Pcamera;
		ray.d = Vector(0.f, 0.f, 1.f);
		ray.d = Normalize(ray.d);
		
		ray.mint = 0.f;
		ray.maxt = (clipYon - clipHither);

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

void OrthographicCamera::InitCameraTransforms(CameraTransforms *trans, const float screen[4],
		const float screenOffsetX, const float screenOffsetY) {
	// This is a trick I use from LuxCoreRenderer to set cameraToWorld to
	// identity matrix.
	if (orig == target)
		trans->cameraToWorld = Transform();
	else {
		const Transform worldToCamera = LookAt(orig, target, up);
		trans->cameraToWorld = Inverse(worldToCamera);
	}

	// Compute orthographic camera transformations
	trans->screenToCamera = Inverse(Orthographic(clipHither, clipYon));
	trans->screenToWorld = trans->cameraToWorld * trans->screenToCamera;
	// Compute orthographic camera screen transformations
	trans->rasterToScreen = luxrays::Translate(Vector(screen[0] + screenOffsetX, screen[3] + screenOffsetY, 0.f)) *
		Scale(screen[1] - screen[0], screen[2] - screen[3], 1.f) *
		Scale(1.f / filmWidth, 1.f / filmHeight, 1.f);
	trans->rasterToCamera = trans->screenToCamera * trans->rasterToScreen;
	trans->rasterToWorld = trans->screenToWorld * trans->rasterToScreen;
}

void OrthographicCamera::GenerateRay(
	const float filmX, const float filmY,
	Ray *ray, const float u1, const float u2, const float u3) const {
	Point Pras, Pcamera;

	Pras = Point(filmX, filmHeight - filmY - 1.f, 0.f);
	Pcamera = Point(camTrans.rasterToCamera * Pras);

	ray->o = Pcamera;
	ray->d = Vector(0.f, 0.f, 1.f);

	// Modify ray for depth of field
	if (lensRadius > 0.f) {
		// Sample point on lens
		float lensU, lensV;
		ConcentricSampleDisk(u1, u2, &lensU, &lensV);
		lensU *= lensRadius;
		lensV *= lensRadius;

		// Compute point on plane of focus
		const float ft = (focalDistance - clipHither);
		Point Pfocus = (*ray)(ft);
		// Update ray for effect of lens
		ray->o.x += lensU * (focalDistance - clipHither) / focalDistance;
		ray->o.y += lensV * (focalDistance - clipHither) / focalDistance;
		ray->d = Pfocus - ray->o;
	}

	ray->d = Normalize(ray->d);
	ray->mint = MachineEpsilon::E(ray->o);
	ray->maxt = clipYon - clipHither;
	ray->time = Lerp(u3, shutterOpen, shutterClose);

	if (motionSystem)
		*ray = motionSystem->Sample(ray->time) * (camTrans.cameraToWorld * (*ray));
	else
		*ray = camTrans.cameraToWorld * (*ray);

	// World arbitrary clipping plane support
	if (enableClippingPlane)
		ApplyArbitraryClippingPlane(ray);
}

void OrthographicCamera::ApplyArbitraryClippingPlane(Ray *ray) const {
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

bool OrthographicCamera::GetSamplePosition(Ray *ray, float *x, float *y) const {
	const float cosi = Dot(ray->d, dir);
	if ((cosi <= 0.f) || (!isinf(ray->maxt) && (ray->maxt * cosi < clipHither ||
		ray->maxt * cosi > clipYon)))
		return false;

	Point pO(Inverse(camTrans.rasterToWorld) * ray->o);

	if (motionSystem)
		pO *= motionSystem->Sample(ray->time);

	*x = pO.x;
	*y = filmHeight - 1 - pO.y;

	// Check if we are inside the image plane
	if ((*x < 0.f) || (*x >= filmWidth) ||
			(*y < 0.f) || (*y >= filmHeight))
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

bool OrthographicCamera::SampleLens(const float time,
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

Properties OrthographicCamera::ToProperties() const {
	Properties props;

	props.Set(Camera::ToProperties());

	props.Set(Property("scene.camera.type")("orthographic"));
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