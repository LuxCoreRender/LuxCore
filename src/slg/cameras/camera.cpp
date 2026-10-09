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

#include "luxrays/utils/properties.h"
#include "slg/film/film.h"
#include "slg/core/sdl.h"
#include "slg/bsdf/bsdf.h"
#include "slg/scene/scene.h"
#include "slg/cameras/camera.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// Camera
//------------------------------------------------------------------------------

BBox Camera::ComputeBBox(const Point &orig) const {
	if (motionSystem)
		return motionSystem->Bound(BBox(orig), false);
	else
		return BBox(orig);
}

bool Camera::GetSamplePosition(const luxrays::Point &p,
		float *filmX, float *filmY) const {
	Point lensPoint;
	if (!SampleLens(0.f, 0.f, 0.f, &lensPoint))
		return false;

	Vector eyeDir = p - lensPoint;
	const float eyeDistance = eyeDir.Length();
	eyeDir /= eyeDistance;

	Ray eyeRay(lensPoint, eyeDir, 0.f, eyeDistance);
	// Do not clamp the ray here because of the check inside GetSamplePosition
	return GetSamplePosition(&eyeRay, filmX, filmY);
}

bool Camera::ProjectToImage(luxrays::Ray *ray, float *filmX, float *filmY) const{
	// Intended only for implementation in OrthographicCamera
	return false;
}

void Camera::Update(const u_int width, const u_int height, const u_int *subRegion) {
	filmWidth = width;
	filmHeight = height;

	if (subRegion) {
		filmSubRegion[0] = subRegion[0];
		filmSubRegion[1] = subRegion[1];
		filmSubRegion[2] = subRegion[2];
		filmSubRegion[3] = subRegion[3];
	} else {
		filmSubRegion[0] = 0;
		filmSubRegion[1] = width - 1;
		filmSubRegion[2] = 0;
		filmSubRegion[3] = height - 1;		
	}
}

void Camera::UpdateAuto(SceneConstRef scene) {
	if (autoVolume) {
		// The volume including the camera position (the origin of a ray in
		// the middle of the screen)
		Ray ray;
		PathVolumeInfo volInfo;
		GenerateRay(0.f, filmWidth / 2.f, filmHeight / 2.f, &ray, &volInfo, 0.f, 0.f);

		volume = scene.GetPointVolume(ray);
	}
}

PropertiesUPtr Camera::ToProperties(const ImageMapCache &imgMapCache, const bool useRealFileName) const {
	auto props = std::make_unique<Properties>();

	props->Set(Property("scene.camera.cliphither")(clipHither));
	props->Set(Property("scene.camera.clipyon")(clipYon));
	props->Set(Property("scene.camera.shutteropen")(shutterOpen));
	props->Set(Property("scene.camera.shutterclose")(shutterClose));
	props->Set(Property("scene.camera.autovolume.enable")(autoVolume));
	if (volume)
		props->Set(Property("scene.camera.volume")(volume->GetName()));

	if (motionSystem)
		props->Set(*motionSystem->ToProperties("scene.camera", false));

	return props;
}

void Camera::UpdateVolumeReferences(VolumeConstRef oldVol, VolumeConstRef newVol) {
	if (volume && *volume == oldVol)
		volume = &newVol;
}
// vim: autoindent noexpandtab tabstop=4 shiftwidth=4
