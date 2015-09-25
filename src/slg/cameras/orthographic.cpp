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
		const Vector &u, const float *region) :
		ProjectiveCamera(ORTHOGRAPHIC, region, o, t, u) {
}

void OrthographicCamera::InitCameraTransforms(CameraTransforms *trans, const float screen[4]) {
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
	trans->rasterToScreen = luxrays::Translate(Vector(screen[0], screen[3], 0.f)) *
		Scale(screen[1] - screen[0], screen[2] - screen[3], 1.f) *
		Scale(1.f / filmWidth, 1.f / filmHeight, 1.f);
	trans->rasterToCamera = trans->screenToCamera * trans->rasterToScreen;
	trans->rasterToWorld = trans->screenToWorld * trans->rasterToScreen;
}

void OrthographicCamera::InitPixelArea(const float screen[4]) {
	const float xPixelWidth = ((screen[1] - screen[0]) / 2.f);
	const float yPixelHeight = ((screen[3] - screen[2]) / 2.f);
	pixelArea = xPixelWidth * yPixelHeight;
}

void OrthographicCamera::InitRay(Ray *ray, const float filmX, const float filmY) const {
	const Point Pras = Point(filmX, filmHeight - filmY - 1.f, 0.f);
	const Point Pcamera = Point(camTrans.rasterToCamera * Pras);

	ray->o = Pcamera;
	ray->d = Vector(0.f, 0.f, 1.f);
}

Properties OrthographicCamera::ToProperties() const {
	Properties props = ProjectiveCamera::ToProperties();

	props.Set(Property("scene.camera.type")("orthographic"));

	return props;
}
