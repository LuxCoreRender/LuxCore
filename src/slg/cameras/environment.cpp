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

#include "slg/cameras/environment.h"
#include "slg/film/film.h"
#include "slg/core/sdl.h"
#include "slg/scene/scene.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// EnvironmentCamera
//------------------------------------------------------------------------------

EnvironmentCamera::EnvironmentCamera(const Point &o, const Point &t, const Vector &u, const float *sw) :
		Camera(ENVIRONMENT), screenOffsetX(0.f), screenOffsetY(0.f), degrees(360.f),
		orig(o), target(t), up(Normalize(u)) {
	if (sw) {
		autoUpdateScreenWindow = false;
		screenWindow[0] = sw[0];
		screenWindow[1] = sw[1];
		screenWindow[2] = sw[2];
		screenWindow[3] = sw[3];
	} else
		autoUpdateScreenWindow = true;
	
	rayOrigin = Point(0.f, 0.f, 0.f);
	
}

void EnvironmentCamera::Update(const u_int width, const u_int height, const u_int *subRegion) {
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
		}
		else {
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
}

void EnvironmentCamera::GenerateRay(const float time,
		const float filmX, const float filmY,
		Ray *ray, PathVolumeInfo *volInfo,
		const float u0, const float u1) const {
	InitRay(ray, filmX, filmY);
	volInfo->AddVolume(volume);

	ray->mint = MachineEpsilon::E(ray->o);
	ray->maxt = (clipYon - clipHither);
	ray->time = time;

	if (motionSystem) {
		*ray = motionSystem->Sample(ray->time) * (camTrans.cameraToWorld * (*ray));
		// I need to normalize the direction vector again because the motion
		// system could include some kind of scale
		ray->d = Normalize(ray->d);
	} else
		*ray = camTrans.cameraToWorld * (*ray);
}

void EnvironmentCamera::ClampRay(Ray *ray) const {
	ray->mint = Max(ray->mint, clipHither);
	ray->maxt = Min(ray->maxt, clipYon);
}

bool EnvironmentCamera::GetSamplePosition(Ray *ray, float *x, float *y) const {	
	if (!isinf(ray->maxt) && (ray->maxt < clipHither || ray->maxt > clipYon))
		return false;

	const Vector w(Inverse(camTrans.cameraToWorld) * ray->d);
	const float cosTheta = w.y;
	const float theta = acosf(Min(1.f, cosTheta));
	*y = filmHeight - 1 - (theta * filmHeight * INV_PI);
	const float sinTheta = sqrtf(Clamp(1.f - cosTheta * cosTheta, 1e-5f, 1.f));

	const float cosPhi = -w.z / sinTheta;
	float phi = acosf(Clamp(cosPhi, -1.f, 1.f));
	if (w.x >= 0.f)
		phi = 2.f * M_PI - phi;
	*x = (phi - Radians((360.f - degrees) * .5f)) * filmWidth / Radians(degrees);
	
	return true;
}

bool EnvironmentCamera::SampleLens(const float time, const float u1, const float u2, Point *lensp) const {
	Point lensPoint(0.f, 0.f, 0.f);

	if (motionSystem)
		*lensp = motionSystem->Sample(time) * (camTrans.cameraToWorld * lensPoint);
	else
		*lensp = camTrans.cameraToWorld * lensPoint;

	return true;
}

void EnvironmentCamera::GetPDF(const Ray &eyeRay, const float eyeDistance,
		const float filmX, const float filmY,
		float *pdfW, float *fluxToRadianceFactor) const {
	const float theta = M_PI * (filmHeight - filmY - 1.f) / filmHeight;
	const float cameraPdfW = 1.f / (2.f * M_PI * M_PI * sinf(theta));

	if (pdfW)
		*pdfW = cameraPdfW;
	if (fluxToRadianceFactor)
		*fluxToRadianceFactor = cameraPdfW / (eyeDistance * eyeDistance);
}

void EnvironmentCamera::InitCameraTransforms(CameraTransforms *trans) {
	// This is a trick I use from LuxCoreRenderer to set cameraToWorld to
	// identity matrix.
	if (orig == target)
		trans->cameraToWorld = Transform();
	else {
		const Transform worldToCamera = LookAt(orig, target, up);
		trans->cameraToWorld = Inverse(worldToCamera);
	}
	
	// Compute environment camera transformations
	trans->screenToCamera = Transform();

	trans->screenToWorld = trans->cameraToWorld * trans->screenToCamera;

	// Compute environment camera screen transformations
	trans->rasterToScreen = luxrays::Translate(Vector(screenWindow[0] + screenOffsetX, screenWindow[3] + screenOffsetY, 0.f)) *
		Scale(screenWindow[1] - screenWindow[0], screenWindow[2] - screenWindow[3], 1.f) *
		Scale(1.f / filmWidth, 1.f / filmHeight, 1.f);
	trans->rasterToCamera = trans->screenToCamera * trans->rasterToScreen;
	trans->rasterToWorld = trans->screenToWorld * trans->rasterToScreen;
}

void EnvironmentCamera::InitPixelArea() {
	const float xPixelWidth = (screenWindow[1] - screenWindow[0]) * .5f;
	const float yPixelHeight = (screenWindow[3] - screenWindow[2]) * .5f;

	pixelArea = xPixelWidth * yPixelHeight;
}

void EnvironmentCamera::InitRay(Ray *ray, const float filmX, const float filmY) const {
	const float theta = M_PI * (filmHeight - filmY - 1.f) / filmHeight;
	const float phi = Radians((360.f - degrees) * .5f + degrees * filmX / filmWidth);

	ray->Update(rayOrigin, Vector(-sinf(theta) * sinf(phi), cosf(theta), -sinf(theta) * cosf(phi)));
}

void EnvironmentCamera::Rotate(const float angle, const luxrays::Vector &axis) {
	luxrays::Vector dir = target - orig;
	luxrays::Transform t = luxrays::Rotate(angle, axis);
	const Vector newDir = t * dir;

	// Check if the up vector is the same of view direction. If they are,
	// skip this operation (it would trigger a Singular matrix in MatrixInvert)
	if (AbsDot(Normalize(newDir), up) < 1.f - DEFAULT_EPSILON_STATIC)
		target = orig + newDir;
}

Properties EnvironmentCamera::ToProperties(const ImageMapCache &imgMapCache, const bool useRealFileName) const {
	Properties props = Camera::ToProperties(imgMapCache, useRealFileName);

	props.Set(Property("scene.camera.type")("environment"));
	props.Set(Property("scene.camera.lookat.orig")(orig));
	props.Set(Property("scene.camera.lookat.target")(target));
	props.Set(Property("scene.camera.up")(up));

	if (!autoUpdateScreenWindow)
		props.Set(Property("scene.camera.screenwindow")(screenWindow[0], screenWindow[1], screenWindow[2], screenWindow[3]));
	
	props.Set(Property("scene.camera.environment.degrees")(degrees));
	
	return props;
}
