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

#include "slg/cameras/perspective.h"
#include "slg/film/film.h"
#include "slg/core/sdl.h"
#include "slg/scene/scene.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// PerspectiveCamera
//------------------------------------------------------------------------------

PerspectiveCamera::PerspectiveCamera(const Point &o, const Point &t,
		const Vector &u, const float *region) :
		ProjectiveCamera(PERSPECTIVE, region, o, t, u),
		screenOffsetX(0.f), screenOffsetY(0.f), fieldOfView(45.f),
		bokehBlades(0), bokehPower(0), bokehDistribution(DIST_EXPONETIAL),
		enableOculusRiftBarrel(false) {
}

PerspectiveCamera::PerspectiveCamera(const CameraType camType,
		const Point &o, const Point &t,
		const Vector &u, const float *region) :
		ProjectiveCamera(camType, region, o, t, u),
		screenOffsetX(0.f), screenOffsetY(0.f), fieldOfView(45.f),
		bokehBlades(0), bokehPower(0), bokehDistribution(DIST_EXPONETIAL),
		enableOculusRiftBarrel(false) {
}

void PerspectiveCamera::InitCameraTransforms(CameraTransforms *trans) {
	// This is a trick I use from LuxCoreRenderer to set cameraToWorld to
	// identity matrix.
	if (orig == target)
		trans->cameraToWorld = Transform();
	else {
		const Transform worldToCamera = LookAt(orig, target, up);
		trans->cameraToWorld = Inverse(worldToCamera);
	}

	// Compute projective camera transformations
	trans->screenToCamera = Inverse(Perspective(fieldOfView, clipHither, clipYon));
	trans->screenToWorld = trans->cameraToWorld * trans->screenToCamera;
	// Compute projective camera screen transformations
	trans->rasterToScreen = luxrays::Translate(Vector(screenWindow[0] + screenOffsetX, screenWindow[3] + screenOffsetY, 0.f)) *
		Scale(screenWindow[1] - screenWindow[0], screenWindow[2] - screenWindow[3], 1.f) *
		Scale(1.f / filmWidth, 1.f / filmHeight, 1.f);
	trans->rasterToCamera = trans->screenToCamera * trans->rasterToScreen;
	trans->rasterToWorld = trans->screenToWorld * trans->rasterToScreen;
}

void PerspectiveCamera::InitCameraData() {
	const float tanAngle = tanf(Radians(fieldOfView) / 2.f) * 2.f;
	const float xPixelWidth = tanAngle * ((screenWindow[1] - screenWindow[0]) / 2.f);
	const float yPixelHeight = tanAngle * ((screenWindow[3] - screenWindow[2]) / 2.f);
	pixelArea = xPixelWidth * yPixelHeight;
}

void PerspectiveCamera::InitRay(Ray *ray, const float filmX, const float filmY) const {
	Point Pras;
	if (enableOculusRiftBarrel) {
		OculusRiftBarrelPostprocess(filmX / filmWidth, (filmHeight - filmY - 1.f) / filmHeight,
				&Pras.x, &Pras.y);

		Pras.x = Min(Pras.x * filmWidth, (float)(filmWidth - 1));
		Pras.y = Min(Pras.y * filmHeight, (float)(filmHeight - 1));
	} else
		Pras = Point(filmX, filmHeight - filmY - 1.f, 0.f);

	const Point Pcamera = Point(camTrans.rasterToCamera * Pras);

	ray->Update(Pcamera, Vector(Pcamera.x, Pcamera.y, Pcamera.z));
}

void PerspectiveCamera::ClampRay(Ray *ray) const {
	Vector globalDir = dir;
	if (motionSystem)
		globalDir *= motionSystem->Sample(ray->time);
	const float cosi = Dot(ray->d, globalDir);

	ray->mint = Max(ray->mint, clipHither / cosi);
	ray->maxt = Min(ray->maxt, clipYon / cosi);
}

bool PerspectiveCamera::GetSamplePosition(Ray *ray, float *x, float *y) const {
	Vector globalDir = dir;
	if (motionSystem)
		globalDir *= motionSystem->Sample(ray->time);
	const float cosi = Dot(ray->d, globalDir);

	if ((cosi <= 0.f) || (!isinf(ray->maxt) && (ray->maxt * cosi < clipHither ||
		ray->maxt * cosi > clipYon)))
		return false;

	Point pO = (ray->o + ((lensRadius > 0.f) ? (ray->d * (focalDistance / cosi)) : ray->d));
	if (motionSystem)
		pO *= motionSystem->SampleInverse(ray->time);
	pO *= Inverse(camTrans.rasterToWorld);

	*x = pO.x;
	*y = filmHeight - 1 - pO.y;

	// Check if we are inside the image plane
	if ((*x < filmSubRegion[0]) || (*x >= filmSubRegion[1] + 1) ||
			(*y < filmSubRegion[2]) || (*y >= filmSubRegion[3] + 1))
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

bool PerspectiveCamera::LocalSampleLens(const float time,
		const float u1, const float u2,
		Point *lensp) const {
	Point lensPoint(0.f, 0.f, 0.f);
	if (lensRadius > 0.f) {
		if (bokehBlades < 3)
			ConcentricSampleDisk(u1, u2, &lensPoint.x, &lensPoint.y);
		else {
			// Bokeh support
			const float halfAngle = M_PI / bokehBlades;
			const float honeyRadius = cosf(halfAngle);

			const float theta = 2.f * M_PI * u2;

			const u_int sector = Floor2UInt(theta / halfAngle);
			const float rho = (sector % 2 == 0) ? (theta - sector * halfAngle) :
					((sector + 1) * halfAngle - theta);

			float r = honeyRadius / cosf(rho);
			switch (bokehDistribution) {
				case DIST_UNIFORM:
					r *= sqrtf(u1);
					break;
				case DIST_EXPONETIAL:
					r *= sqrtf(ExponentialSampleDisk(u1, bokehPower));
					break;
				case DIST_INVERSEEXPONETIAL:
					r *= sqrtf(InverseExponentialSampleDisk(u1, bokehPower));
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
					throw runtime_error("Unknown bokeh distribution in PerspectiveCamera::LocalSampleLens(): " + ToString(bokehDistribution));
			}

			lensPoint.x = r * cosf(theta);
			lensPoint.y = r * sinf(theta);
		}

		lensPoint.x *= lensRadius;
		lensPoint.y *= lensRadius;
	}

	*lensp = lensPoint;

	return true;
}

bool PerspectiveCamera::SampleLens(const float time,
		const float u1, const float u2,
		Point *lensp) const {
	Point lensPoint(0.f, 0.f, 0.f);
	LocalSampleLens(time, u1, u2, &lensPoint);

	if (motionSystem)
		*lensp = motionSystem->Sample(time) * (camTrans.cameraToWorld * lensPoint);
	else
		*lensp = camTrans.cameraToWorld * lensPoint;

	return true;
}

void PerspectiveCamera::GetPDF(const Ray &eyeRay, const float eyeDistance,
		const float filmX, const float filmY,
		float *pdfW, float *fluxToRadianceFactor) const {
	Vector globalDir = dir;
	if (motionSystem)
		globalDir *= motionSystem->Sample(eyeRay.time);

	const float cosAtCamera = Dot(eyeRay.d, globalDir);
	if (cosAtCamera <= 0.f) {
		if (pdfW)
			*pdfW = 0.f;
		if (fluxToRadianceFactor)
			*fluxToRadianceFactor = 0.f;
	} else {
		const float cameraPdfW = 1.f / (cosAtCamera * cosAtCamera * cosAtCamera * pixelArea);

		if (pdfW)
			*pdfW = cameraPdfW;
		if (fluxToRadianceFactor)
			*fluxToRadianceFactor = cameraPdfW / (eyeDistance * eyeDistance);
	}
}

Properties PerspectiveCamera::ToProperties() const {
	Properties props = ProjectiveCamera::ToProperties();

	props.Set(Property("scene.camera.type")("perspective"));
	props.Set(Property("scene.camera.oculusrift.barrelpostpro.enable")(enableOculusRiftBarrel));
	props.Set(Property("scene.camera.fieldofview")(fieldOfView));
	props.Set(Property("scene.camera.bokeh.blades")(bokehBlades));
	props.Set(Property("scene.camera.bokeh.power")(bokehPower));
	props.Set(Property("scene.camera.bokeh.distribution.type")(BokehDistributionType2String(bokehDistribution)));

	return props;
}

PerspectiveCamera::BokehDistributionType PerspectiveCamera::String2BokehDistributionType(string type) {
	if (type == "UNIFORM")
		return DIST_UNIFORM;
	else if (type == "EXPONETIAL")
		return DIST_EXPONETIAL;
	else if (type == "INVERSEEXPONETIAL")
		return DIST_INVERSEEXPONETIAL;
	else if (type == "GAUSSIAN")
		return DIST_GAUSSIAN;
	else if (type == "INVERSEGAUSSIAN")
		return DIST_INVERSEGAUSSIAN;
	else if (type == "TRIANGULAR")
		return DIST_TRIANGULAR;
	else
		throw runtime_error("Unknown bokeh distribution type: " + type);
}

string PerspectiveCamera::BokehDistributionType2String(const BokehDistributionType type) {
	switch (type) {
		case DIST_UNIFORM:
			return "UNIFORM";
		case DIST_EXPONETIAL:
			return "EXPONETIAL";
		case DIST_INVERSEEXPONETIAL:
			return "INVERSEEXPONETIAL";
		case DIST_GAUSSIAN:
			return "GAUSSIAN";
		case DIST_INVERSEGAUSSIAN:
			return "INVERSEGAUSSIAN";
		case DIST_TRIANGULAR:
			return "TRIANGULAR";
		default:
			throw runtime_error("Unsupported bokeh distribution type in PerspectiveCamera::String2BokehDistributionType(): " + ToString(type));
	}
}
	
//------------------------------------------------------------------------------
// Oculus Rift post-processing pixel shader
//------------------------------------------------------------------------------

void PerspectiveCamera::OculusRiftBarrelPostprocess(const float x, const float y, float *barrelX, float *barrelY) {
	// Express the sample in coordinates relative to the eye center
	float ex = x * 2.f - 1.f;
	float ey = y * 2.f - 1.f;

	if ((ex == 0.f) && (ey == 0.f)) {
		*barrelX = 0.f;
		*barrelY = 0.f;
		return;
	}

	// Distance from the eye center
	const float distance = sqrtf(ex * ex + ey * ey);

	// "Push" the sample away based on the distance from the center
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
	ex = Clamp(ex, -1.f, 1.f);
	ey = Clamp(ey, -1.f, 1.f);

	*barrelX = (ex + 1.f) * .5f;
	*barrelY = (ey + 1.f) * .5f;
}
