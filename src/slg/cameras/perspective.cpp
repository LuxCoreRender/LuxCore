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
		ProjectiveCamera(PERSPECTIVE, region, o, t, u), fieldOfView(45.f) {
}

void PerspectiveCamera::InitCameraTransforms(CameraTransforms *trans, const float screen[4]) {
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
	trans->rasterToScreen = luxrays::Translate(Vector(screen[0], screen[3], 0.f)) *
		Scale(screen[1] - screen[0], screen[2] - screen[3], 1.f) *
		Scale(1.f / filmWidth, 1.f / filmHeight, 1.f);
	trans->rasterToCamera = trans->screenToCamera * trans->rasterToScreen;
	trans->rasterToWorld = trans->screenToWorld * trans->rasterToScreen;
}

void PerspectiveCamera::InitPixelArea() {
	const float tanAngle = tanf(Radians(fieldOfView) / 2.f) * 2.f;
	const float xPixelWidth = tanAngle * ((screen[1] - screen[0]) / 2.f);
	const float yPixelHeight = tanAngle * ((screen[3] - screen[2]) / 2.f);
	pixelArea = xPixelWidth * yPixelHeight;
}

void PerspectiveCamera::InitRay(Ray *ray, const float filmX, const float filmY) const {
	const Point Pras = Point(filmX, filmHeight - filmY - 1.f, 0.f);
	const Point Pcamera = Point(camTrans.rasterToCamera * Pras);

	ray->o = Pcamera;
	ray->d = Vector(Pcamera.x, Pcamera.y, Pcamera.z);
}

Properties PerspectiveCamera::ToProperties() const {
	Properties props = ProjectiveCamera::ToProperties();

	props.Set(Property("scene.camera.type")("perspective"));
	props.Set(Property("scene.camera.fieldofview")(fieldOfView));

	return props;
}
