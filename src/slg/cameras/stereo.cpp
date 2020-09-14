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

#include "slg/cameras/stereo.h"

using namespace std;
using namespace luxrays;
using namespace slg;

StereoCamera::StereoCamera(const luxrays::Point &o, const luxrays::Point &t,
			const luxrays::Vector &u) : PerspectiveCamera(STEREO, o, t, u),
			leftEye(NULL), rightEye(NULL) {
	horizStereoEyesDistance = .0626f;
	horizStereoLensDistance = .2779f;
}

StereoCamera::~StereoCamera() {
	delete leftEye;
	delete rightEye;
}

const Transform &StereoCamera::GetRasterToCamera(const u_int index) const {
	if (index == 0)
		return leftEye->GetRasterToCamera();
	else if (index == 1)
		return rightEye->GetRasterToCamera();
	else
		throw runtime_error("Unknown index in GetRasterToCamera(): " + ToString(index));
}

const Transform &StereoCamera::GetCameraToWorld(const u_int index) const {
	if (index == 0)
		return leftEye->GetCameraToWorld();
	else if (index == 1)
		return rightEye->GetCameraToWorld();
	else
		throw runtime_error("Unknown index in GetCameraToWorld(): " + ToString(index));
}

const Transform &StereoCamera::GetScreenToWorld(const u_int index) const {
	if (index == 0)
		return leftEye->GetScreenToWorld();
	else if (index == 1)
		return rightEye->GetScreenToWorld();
	else
		throw runtime_error("Unknown index in GetScreenToWorld(): " + ToString(index));
}

void StereoCamera::Update(const u_int width, const u_int height,
		const u_int *subRegion) {
	/*if (filmSubRegion)
		throw runtime_error("Stereo camera doesn't support subregion rendering");*/

	Camera::Update(width, height, subRegion);

	// Used to translate the camera
	dir = target - orig;
	dir = Normalize(dir);

	x = Cross(dir, up);
	x = Normalize(x);

	y = Cross(x, dir);
	y = Normalize(y);

	// Create left eye camera
	delete leftEye;
	leftEye = new PerspectiveCamera(orig - .5f * horizStereoEyesDistance * x, target, up);
	leftEye->clipHither = clipHither;
	leftEye->clipYon = clipYon;
	leftEye->shutterOpen = shutterOpen;
	leftEye->shutterClose = shutterClose;

	leftEye->clippingPlaneCenter = clippingPlaneCenter;
	leftEye->clippingPlaneNormal = clippingPlaneNormal;
	leftEye->enableClippingPlane = enableClippingPlane;

	leftEye->lensRadius = lensRadius;
	leftEye->focalDistance = focalDistance;
	leftEye->autoFocus = autoFocus;

	leftEye->screenOffsetX = -horizStereoLensDistance * .5f;
	leftEye->fieldOfView = fieldOfView;
	leftEye->enableOculusRiftBarrel = enableOculusRiftBarrel;
	
	leftEye->Update(filmWidth / 2, filmHeight, NULL);

	// Create right eye camera
	delete rightEye;
	rightEye = new PerspectiveCamera(orig + .5f * horizStereoEyesDistance * x, target, up);

	rightEye->clipHither = clipHither;
	rightEye->clipYon = clipYon;
	rightEye->shutterOpen = shutterOpen;
	rightEye->shutterClose = shutterClose;

	rightEye->clippingPlaneCenter = clippingPlaneCenter;
	rightEye->clippingPlaneNormal = clippingPlaneNormal;
	rightEye->enableClippingPlane = enableClippingPlane;

	rightEye->lensRadius = lensRadius;
	rightEye->focalDistance = focalDistance;
	rightEye->autoFocus = autoFocus;

	rightEye->screenOffsetX = horizStereoLensDistance * .5f;
	rightEye->fieldOfView = fieldOfView;
	rightEye->enableOculusRiftBarrel = enableOculusRiftBarrel;

	rightEye->Update(filmWidth / 2, filmHeight, NULL);
}

void StereoCamera::GenerateRay(const float time,
		const float filmX, const float filmY,
		Ray *ray, PathVolumeInfo *volInfo,
		const float u0, const float u1) const {
	if (filmX < filmWidth / 2)
		leftEye->GenerateRay(time, filmX, filmY, ray, volInfo, u0, u1);
	else
		rightEye->GenerateRay(time, filmX - filmWidth / 2, filmY, ray, volInfo, u0, u1);
}

bool StereoCamera::GetSamplePosition(Ray *eyeRay, float *filmX, float *filmY) const {
	// BIDIRCPU/LIGHTCPU don't support stereo rendering
	return leftEye->GetSamplePosition(eyeRay, filmX, filmY);
}

bool StereoCamera::SampleLens(const float time, const float u1, const float u2,
	luxrays::Point *lensPoint) const {
	// BIDIRCPU/LIGHTCPU don't support stereo rendering
	return leftEye->SampleLens(time, u1, u2, lensPoint);
}

void StereoCamera::GetPDF(const Ray &eyeRay, const float eyeDistance,
		const float filmX, const float filmY,
		float *pdfW, float *fluxToRadianceFactor) const {
	// BIDIRCPU/LIGHTCPU don't support stereo rendering
	leftEye->GetPDF(eyeRay, eyeDistance, filmX, filmY, pdfW, fluxToRadianceFactor);
}

Properties StereoCamera::ToProperties(const ImageMapCache &imgMapCache, const bool useRealFileName) const {
	Properties props = PerspectiveCamera::ToProperties(imgMapCache, useRealFileName);

	props.Set(Property("scene.camera.type")("stereo"));
	props.Set(Property("scene.camera.eyesdistance")(horizStereoEyesDistance));
	props.Set(Property("scene.camera.lensdistance")(horizStereoLensDistance));

	return props;
}
