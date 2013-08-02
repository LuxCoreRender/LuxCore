/***************************************************************************
 *   Copyright (C) 1998-2013 by authors (see AUTHORS.txt)                  *
 *                                                                         *
 *   This file is part of LuxRays.                                         *
 *                                                                         *
 *   LuxRays is free software; you can redistribute it and/or modify       *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   LuxRays is distributed in the hope that it will be useful,            *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 *                                                                         *
 *   LuxRays website: http://www.luxrender.net                             *
 ***************************************************************************/

#include <cstddef>

#include "slg/camera/camera.h"
#include "slg/film/film.h"

using namespace luxrays;
using namespace slg;

PerspectiveCamera::PerspectiveCamera(const luxrays::Point &o, const luxrays::Point &t,
		const luxrays::Vector &u, const float *region) :
		orig(o), target(t), up(Normalize(u)), fieldOfView(45.f), clipHither(1e-3f), clipYon(1e30f),
		lensRadius(0.f), focalDistance(10.f) {
	if (region) {
		autoUpdateFilmRegion = false;
		filmRegion[0] = region[0];
		filmRegion[1] = region[1];
		filmRegion[2] = region[2];
		filmRegion[3] = region[3];
	} else
		autoUpdateFilmRegion = true;

	enableHorizStereo = false;
	enableOculusRiftBarrel = false;
	horizStereoEyeDistance = .0626f;
	horizStereoLensDistance = .1;
}

void PerspectiveCamera::Update(const u_int width, const u_int height, const u_int *subRegion) {
	// Used to move translate the camera
	dir = target - orig;
	dir = Normalize(dir);

	x = Cross(dir, up);
	x = Normalize(x);

	y = Cross(x, dir);
	y = Normalize(y);

	// Initialize screen information
	const float frame = float(width) / float(height);
	float screen[4];

	filmWidth = width;
	filmHeight = height;
	u_int filmSubRegion[4];
	filmSubRegion[0] = 0;
	filmSubRegion[1] = width - 1;
	filmSubRegion[2] = 0;
	filmSubRegion[3] = height - 1;

	if (autoUpdateFilmRegion) {
		if (enableHorizStereo) {
			if (frame < 2.f) {
				screen[0] = -frame;
				screen[1] = frame;
				screen[2] = -1.f;
				screen[3] = 1.f;
			} else {
				screen[0] = -2.f;
				screen[1] = 2.f;
				screen[2] = -2.f / frame;
				screen[3] = 2.f / frame;
			}
		} else {
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
		}
	} else {
		screen[0] = filmRegion[0];
		screen[1] = filmRegion[1];
		screen[2] = filmRegion[2];
		screen[3] = filmRegion[3];
	}

	if (subRegion) {
		if (enableHorizStereo)
			throw std::runtime_error("Can not enable horizontal stereo support with subregion rendering");

		// I have to render a sub region of the image
		filmSubRegion[0] = subRegion[0];
		filmSubRegion[1] = subRegion[1];
		filmSubRegion[2] = subRegion[2];
		filmSubRegion[3] = subRegion[3];
		filmWidth = filmSubRegion[1] - filmSubRegion[0] + 1;
		filmHeight = filmSubRegion[3] - filmSubRegion[2] + 1;

		const float halfW = width * .5f;
		const float halfH = height * .5f;
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
	if (enableHorizStereo) {
		camTrans.resize(2);

		const float offset = (screen[1] - screen[0]) * .25f  - horizStereoLensDistance * .5f;

		// Left eye
		InitCameraTransforms(&camTrans[0], screen, -horizStereoEyeDistance * .5f, offset, 0.f);
		// Right eye
		InitCameraTransforms(&camTrans[1], screen, horizStereoEyeDistance * .5f, -offset, 0.f);
	} else {
		camTrans.resize(1);
		InitCameraTransforms(&camTrans[0], screen, 0.f, 0.f, 0.f);
	}

	// Initialize pixel information
	const float tanAngle = tanf(Radians(fieldOfView) / 2.f) * 2.f;
	const float xPixelWidth = tanAngle * ((screen[1] - screen[0]) / 2.f);
	const float yPixelHeight = tanAngle * ((screen[3] - screen[2]) / 2.f);
	pixelArea = xPixelWidth * yPixelHeight;
}

void PerspectiveCamera::InitCameraTransforms(CameraTransforms *trans, const float screen[4],
		const float eyeOffset,
		const float screenOffsetX, const float screenOffsetY) {
	// Shift from camera to eye position
	const Point eyeOrig = orig + eyeOffset * luxrays::Normalize(x);
	const Point eyeTarget = target + eyeOffset * luxrays::Normalize(x);
	const Transform worldToCamera = LookAt(eyeOrig, eyeTarget, up);
	trans->cameraToWorld = Inverse(worldToCamera);

	// Compute projective camera transformations
	trans->screenToCamera = Inverse(Perspective(fieldOfView, clipHither, clipYon));
	trans->screenToWorld = trans->cameraToWorld * trans->screenToCamera;
	// Compute projective camera screen transformations
	trans->rasterToScreen = luxrays::Translate(Vector(screen[0] + screenOffsetX, screen[3] + screenOffsetY, 0.f)) *
		Scale(screen[1] - screen[0], screen[2] - screen[3], 1.f) *
		Scale(1.f / filmWidth, 1.f / filmHeight, 1.f);
	trans->rasterToCamera = trans->screenToCamera * trans->rasterToScreen;
	trans->rasterToWorld = trans->screenToWorld * trans->rasterToScreen;
}

void PerspectiveCamera::GenerateRay(
	const float filmX, const float filmY,
	Ray *ray, const float u1, const float u2) const {
	u_int transIndex;
	Point Pras, Pcamera;
	if (enableHorizStereo) {
		if (enableOculusRiftBarrel) {
			OculusRiftBarrelPostprocess(filmX / filmWidth, (filmHeight - filmY - 1.f) / filmHeight,
					&Pras.x, &Pras.y);
			Pras.x = Min(Pras.x * filmWidth, (float)(filmWidth - 1));
			Pras.y = Min(Pras.y * filmHeight, (float)(filmHeight - 1));
		} else
			Pras = Point(filmX, filmHeight - filmY - 1.f, 0.f);

		// Left eye or right eye
		transIndex = (filmX < filmWidth * .5f) ? 0 : 1;
	} else {
		transIndex = 0;
		Pras = Point(filmX, filmHeight - filmY - 1.f, 0.f);
	}

	Pcamera = Point(camTrans[transIndex].rasterToCamera * Pras);

	ray->o = Pcamera;
	ray->d = Vector(Pcamera.x, Pcamera.y, Pcamera.z);

	// Modify ray for depth of field
	if (lensRadius > 0.f) {
		// Sample point on lens
		float lensU, lensV;
		ConcentricSampleDisk(u1, u2, &lensU, &lensV);
		lensU *= lensRadius;
		lensV *= lensRadius;

		// Compute point on plane of focus
		const float ft = (focalDistance - clipHither) / ray->d.z;
		Point Pfocus = (*ray)(ft);
		// Update ray for effect of lens
		ray->o.x += lensU * (focalDistance - clipHither) / focalDistance;
		ray->o.y += lensV * (focalDistance - clipHither) / focalDistance;
		ray->d = Pfocus - ray->o;
	}

	ray->d = Normalize(ray->d);
	ray->mint = MachineEpsilon::E(ray->o);
	ray->maxt = (clipYon - clipHither) / ray->d.z;

	*ray = camTrans[transIndex].cameraToWorld * (*ray);
}

bool PerspectiveCamera::GetSamplePosition(const Point &p, const Vector &wi,
	float distance, float *x, float *y) const {
	const float cosi = Dot(wi, dir);
	if ((cosi <= 0.f) || (!isinf(distance) && (distance * cosi < clipHither ||
		distance * cosi > clipYon)))
		return false;

	const Point pO(Inverse(camTrans[0].rasterToWorld) * (p + ((lensRadius > 0.f) ?
		(wi * (focalDistance / cosi)) : wi)));

	*x = pO.x;
	*y = filmHeight - 1 - pO.y;

	// Check if we are inside the image plane
	if ((*x < 0.f) || (*x >= filmWidth) ||
			(*y < 0.f) || (*y >= filmHeight))
		return false;
	else
		return true;
}

bool PerspectiveCamera::SampleLens(const float u1, const float u2,
		Point *lensp) const {
	Point lensPoint(0.f, 0.f, 0.f);
	if (lensRadius > 0.f) {
		ConcentricSampleDisk(u1, u2, &lensPoint.x, &lensPoint.y);
		lensPoint.x *= lensRadius;
		lensPoint.y *= lensRadius;
	}

	*lensp = camTrans[0].cameraToWorld * lensPoint;

	return true;
}

Properties PerspectiveCamera::ToProperties() const {
	Properties props;

	props.SetString("scene.camera.lookat",
			ToString(orig.x) + " " + ToString(orig.y) + " " + ToString(orig.z) + " " +
			ToString(target.x) + " " + ToString(target.y) + " " + ToString(target.z));

	props.SetString("scene.camera.up", ToString(up.x) + " " + ToString(up.y) + " " + ToString(up.z));

	if (!autoUpdateFilmRegion)
		props.SetString("scene.camera.screenwindow",
			ToString(filmRegion[0]) + " " + ToString(filmRegion[1]) + " " + 
			ToString(filmRegion[2]) + " " + ToString(filmRegion[3]));

	props.SetString("scene.camera.cliphither", ToString(clipHither));
	props.SetString("scene.camera.clipyon", ToString(clipYon));
	props.SetString("scene.camera.lensradius", ToString(lensRadius));
	props.SetString("scene.camera.focaldistance", ToString(focalDistance));
	props.SetString("scene.camera.fieldofview", ToString(fieldOfView));
	props.SetString("scene.camera.horizontalstereo.enable", ToString(enableHorizStereo));
	props.SetString("scene.camera.horizontalstereo.oculusrift.barrelpostpro.enable", ToString(enableOculusRiftBarrel));

	return props;
}

//------------------------------------------------------------------------------
// Oculus Rift post-processing pixel shader
//------------------------------------------------------------------------------

void PerspectiveCamera::OculusRiftBarrelPostprocess(const float x, const float y, float *barrelX, float *barrelY) {
	// Express the sample in coordinates relative to the eye center
	float ex, ey;
	if (x < .5f) {
		// A left eye sample
		ex = x * 4.f - 1.f;
		ey = y * 2.f - 1.f;
	} else {
		// A right eye sample
		ex = (x - .5f) * 4.f - 1.f;
		ey = y * 2.f - 1.f;
	}

	if ((ex == 0.f) && (ey == 0.f)) {
		*barrelX = 0.f;
		*barrelY = 0.f;
		return;
	}

	// Distance from the eye center
	const float distance = sqrtf(ex * ex + ey * ey);

	// "Push" the sample away base on the distance from the center
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

	if (x < .5f) {
		*barrelX = (ex + 1.f) * .25f;
		*barrelY = (ey + 1.f) * .5f;
	} else {
		*barrelX = (ex + 1.f) * .25f + .5f;
		*barrelY = (ey + 1.f) * .5f;
	}
}
