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
		const Vector &u, const float *screenWindow) :
		ProjectiveCamera(ORTHOGRAPHIC, screenWindow, o, t, u) {
}

void OrthographicCamera::InitCameraTransforms(CameraTransforms *trans) {
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
	trans->rasterToScreen = luxrays::Translate(Vector(screenWindow[0], screenWindow[3], 0.f)) *
		Scale(screenWindow[1] - screenWindow[0], screenWindow[2] - screenWindow[3], 1.f) *
		Scale(1.f / filmWidth, 1.f / filmHeight, 1.f);
	trans->rasterToCamera = trans->screenToCamera * trans->rasterToScreen;
	trans->rasterToWorld = trans->screenToWorld * trans->rasterToScreen;
}

void OrthographicCamera::InitCameraData() {
	const float xPixelWidth = screenWindow[1] - screenWindow[0];
	const float yPixelHeight = screenWindow[3] - screenWindow[2];
	cameraPdf = 1.f / (xPixelWidth * yPixelHeight);
}

void OrthographicCamera::InitRay(Ray *ray, const float filmX, const float filmY) const {
	const Point Pras = Point(filmX, filmHeight - filmY - 1.f, 0.f);
	const Point Pcamera = Point(camTrans.rasterToCamera * Pras);

	ray->Update(Pcamera, Vector(0.f, 0.f, 1.f));
}

void OrthographicCamera::ClampRay(Ray *ray) const {
	ray->mint = Max(ray->mint, clipHither);
	ray->maxt = Min(ray->maxt, clipYon);
}

bool OrthographicCamera::GetSamplePosition(Ray *ray, float *x, float *y) const {
	const float cosi = Dot(ray->d, dir);

	if ((cosi <= 0.f) || (!isinf(ray->maxt) && (ray->maxt < clipHither ||
		ray->maxt > clipYon)))
		return false;

	const Point endPoint = (*ray)(ray->maxt);
	Point pO = Inverse(camTrans.rasterToWorld) * endPoint;
	if (motionSystem)
		pO *= motionSystem->Sample(ray->time);

	*x = pO.x;
	*y = filmHeight - 1 - pO.y;

	// Update the ray origin
	pO.z = 0.f;
	ray->o = camTrans.rasterToWorld * pO;

	// Update the ray direction
	Vector eyeDir = endPoint - ray->o;
	const float eyeDistance = eyeDir.Length();
	eyeDir /= eyeDistance;
	ray->d = eyeDir;

	// Update ray mint and maxt
	ray->mint = 0.f;
	ray->maxt = eyeDistance;
	ray->UpdateMinMaxWithEpsilon();
	
	// Check if we are inside the image plane
	if ((*x < filmSubRegion[0]) || (*x >= filmSubRegion[1] + 1) ||
			(*y < filmSubRegion[2]) || (*y >= filmSubRegion[3] + 1))
		return false;
	else {
		// World arbitrary clipping plane support
		if (enableClippingPlane) {
			// Check if the ray end point is on the not visible side of the plane
			if (Dot(clippingPlaneNormal, endPoint - clippingPlaneCenter) <= 0.f)
				return false;
			// Update ray mint/maxt
			ApplyArbitraryClippingPlane(ray);
		}

		return true;
	}
}

bool OrthographicCamera::LocalSampleLens(const float time,
		const float u1, const float u2,
		Point *lensp) const {
	*lensp = Point(0.f, 0.f, 0.f);

	return true;
}

bool OrthographicCamera::SampleLens(const float time,
		const float u1, const float u2,
		Point *lensp) const {
	Point lensPoint(0.f, 0.f, 0.f);

	if (motionSystem)
		*lensp = motionSystem->Sample(time) * (camTrans.cameraToWorld * lensPoint);
	else
		*lensp = camTrans.cameraToWorld * lensPoint;

	return true;
}

void OrthographicCamera::GetPDF(const Ray &eyeRay, const float eyeDistance,
		const float filmX, const float filmY,
		float *pdfW, float *fluxToRadianceFactor) const {
	if (pdfW)
		*pdfW = cameraPdf;
	if (fluxToRadianceFactor)
		*fluxToRadianceFactor = cameraPdf;
}

Properties OrthographicCamera::ToProperties(const ImageMapCache &imgMapCache, const bool useRealFileName) const {
	Properties props = ProjectiveCamera::ToProperties(imgMapCache, useRealFileName);

	props.Set(Property("scene.camera.type")("orthographic"));

	return props;
}
