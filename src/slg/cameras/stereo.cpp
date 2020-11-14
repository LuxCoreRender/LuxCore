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
#include "slg/cameras/environment.h"

using namespace std;
using namespace luxrays;
using namespace slg;

StereoCamera::StereoCamera(const StereoCameraType sType,
		const luxrays::Point &o, const luxrays::Point &t,
		const luxrays::Vector &u) : PerspectiveCamera(STEREO, o, t, u),
		stereoType(sType), leftEye(nullptr), rightEye(nullptr) {
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

	switch(stereoType) {
		case STEREO_PERSPECTIVE: {
			// Create left eye camera
			delete leftEye;
			PerspectiveCamera *left = new PerspectiveCamera(orig - .5f * horizStereoEyesDistance * x, target, up);
			left->clipHither = clipHither;
			left->clipYon = clipYon;
			left->shutterOpen = shutterOpen;
			left->shutterClose = shutterClose;
			left->autoVolume = autoVolume;
			left->volume = volume;

			left->clippingPlaneCenter = clippingPlaneCenter;
			left->clippingPlaneNormal = clippingPlaneNormal;
			left->enableClippingPlane = enableClippingPlane;

			left->lensRadius = lensRadius;
			left->focalDistance = focalDistance;
			left->autoFocus = autoFocus;

			left->screenOffsetX = -horizStereoLensDistance * .5f;
			left->fieldOfView = fieldOfView;
			left->enableOculusRiftBarrel = enableOculusRiftBarrel;

			left->Update(filmWidth / 2, filmHeight, nullptr);
			leftEye = left;

			// Create right eye camera
			delete rightEye;
			PerspectiveCamera *right = new PerspectiveCamera(orig + .5f * horizStereoEyesDistance * x, target, up);

			right->clipHither = clipHither;
			right->clipYon = clipYon;
			right->shutterOpen = shutterOpen;
			right->shutterClose = shutterClose;
			right->autoVolume = autoVolume;
			right->volume = volume;

			right->clippingPlaneCenter = clippingPlaneCenter;
			right->clippingPlaneNormal = clippingPlaneNormal;
			right->enableClippingPlane = enableClippingPlane;

			right->lensRadius = lensRadius;
			right->focalDistance = focalDistance;
			right->autoFocus = autoFocus;

			right->screenOffsetX = horizStereoLensDistance * .5f;
			right->fieldOfView = fieldOfView;
			right->enableOculusRiftBarrel = enableOculusRiftBarrel;

			right->Update(filmWidth / 2, filmHeight, nullptr);
			rightEye = right;
			break;
		}
		case STEREO_ENVIRONMENT_180: {
			// Create left eye camera
			delete leftEye;
			EnvironmentCamera *left = new EnvironmentCamera(orig - .5f * horizStereoEyesDistance * x, target, up);
			left->screenOffsetX = -horizStereoLensDistance * .5f;
			left->degrees = 180.f;

			left->Update(filmWidth / 2, filmHeight, nullptr);
			leftEye = left;

			// Create right eye camera
			delete rightEye;
			EnvironmentCamera *right = new EnvironmentCamera(orig + .5f * horizStereoEyesDistance * x, target, up);
			right->screenOffsetX = horizStereoLensDistance * .5f;
			right->degrees = 180.f;

			right->Update(filmWidth / 2, filmHeight, nullptr);
			rightEye = right;
			break;
		}
		case STEREO_ENVIRONMENT_360: {
			// Create left eye camera
			delete leftEye;
			EnvironmentCamera *left = new EnvironmentCamera(orig - .5f * horizStereoEyesDistance * x, target, up);
			left->screenOffsetX = -horizStereoLensDistance * .5f;
			left->degrees = 360.f;

			left->Update(filmWidth, filmHeight / 2, nullptr);
			leftEye = left;

			// Create right eye camera
			delete rightEye;
			EnvironmentCamera *right = new EnvironmentCamera(orig + .5f * horizStereoEyesDistance * x, target, up);
			right->screenOffsetX = horizStereoLensDistance * .5f;
			right->degrees = 360.f;

			right->Update(filmWidth, filmHeight / 2, nullptr);
			rightEye = right;
			break;
		}
		default:
			throw runtime_error("Unknown StereoCamera type in StereoCamera::Update(): " + ToString(stereoType));
	}
}

void StereoCamera::GenerateRay(const float time,
		const float filmX, const float filmY,
		Ray *ray, PathVolumeInfo *volInfo,
		const float u0, const float u1) const {
	switch(stereoType) {
		case STEREO_PERSPECTIVE:
		case STEREO_ENVIRONMENT_180:
			if (filmX < filmWidth / 2)
				leftEye->GenerateRay(time, filmX, filmY, ray, volInfo, u0, u1);
			else
				rightEye->GenerateRay(time, filmX - filmWidth / 2, filmY, ray, volInfo, u0, u1);
			break;
		case STEREO_ENVIRONMENT_360:
			if (filmY < filmHeight / 2)
				leftEye->GenerateRay(time, filmX, filmY, ray, volInfo, u0, u1);
			else
				rightEye->GenerateRay(time, filmX, filmY - filmHeight / 2, ray, volInfo, u0, u1);
			break;
		default:
			throw runtime_error("Unknown StereoCamera type in StereoCamera::GenerateRay(): " + ToString(stereoType));
	}		
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

	switch(stereoType) {
		case STEREO_PERSPECTIVE:
			props.Set(Property("scene.camera.stereo.type")("perspective"));
			break;
		case STEREO_ENVIRONMENT_180:
			props.Set(Property("scene.camera.stereo.type")("environment_180"));
			break;
		case STEREO_ENVIRONMENT_360:
			props.Set(Property("scene.camera.stereo.type")("environment_360"));
			break;
		default:
			throw runtime_error("Unknown StereoCamera type in StereoCamera::ToProperties(): " + ToString(stereoType));
	}
	
	props.Set(Property("scene.camera.eyesdistance")(horizStereoEyesDistance));
	props.Set(Property("scene.camera.lensdistance")(horizStereoLensDistance));

	return props;
}
