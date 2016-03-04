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
		screenOffsetX(0.f), screenOffsetY(0.f),
		fieldOfView(45.f), enableOculusRiftBarrel(false) {
}

PerspectiveCamera::PerspectiveCamera(const CameraType camType,
		const Point &o, const Point &t,
		const Vector &u, const float *region) :
		ProjectiveCamera(camType, region, o, t, u),
		screenOffsetX(0.f), screenOffsetY(0.f),
		fieldOfView(45.f), enableOculusRiftBarrel(false) {
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

void PerspectiveCamera::InitPixelArea() {
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

Properties PerspectiveCamera::ToProperties() const {
	Properties props = ProjectiveCamera::ToProperties();

	props.Set(Property("scene.camera.type")("perspective"));
	props.Set(Property("scene.camera.oculusrift.barrelpostpro.enable")(enableOculusRiftBarrel));
	props.Set(Property("scene.camera.fieldofview")(fieldOfView));

	return props;
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
