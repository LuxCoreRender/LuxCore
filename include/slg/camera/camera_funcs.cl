#line 2 "camera_funcs.cl"

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

#if defined(PARAM_CAMERA_ENABLE_OCULUSRIFT_BARREL)
void Camera_OculusRiftBarrelPostprocess(const float x, const float y, float *barrelX, float *barrelY) {
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
	const float distance = sqrt(ex * ex + ey * ey);

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
	ex = clamp(ex, -1.f, 1.f);
	ey = clamp(ey, -1.f, 1.f);

	if (x < .5f) {
		*barrelX = (ex + 1.f) * .25f;
		*barrelY = (ey + 1.f) * .5f;
	} else {
		*barrelX = (ex + 1.f) * .25f + .5f;
		*barrelY = (ey + 1.f) * .5f;
	}
}
#endif

void Camera_GenerateRay(
		__global Camera *camera,
		__global Ray *ray,
		const float scrSampleX, const float scrSampleY
#if defined(PARAM_CAMERA_HAS_DOF)
		, const float dofSampleX, const float dofSampleY
#endif
		) {
#if defined(PARAM_CAMERA_ENABLE_HORIZ_STEREO)
	// Left eye or right eye
	const uint transIndex = (scrSampleX < .5f) ? 0 : 1;
#else
	const uint transIndex = 0;
#endif

	float ssx, ssy;
#if defined(PARAM_CAMERA_ENABLE_HORIZ_STEREO) && defined(PARAM_CAMERA_ENABLE_OCULUSRIFT_BARREL)
	Camera_OculusRiftBarrelPostprocess(scrSampleX, scrSampleY, &ssx, &ssy);
#else
	ssx = scrSampleX;
	ssy = scrSampleY;
#endif

	const float screenX = min(ssx * PARAM_IMAGE_WIDTH, (float)(PARAM_IMAGE_WIDTH - 1));
	const float screenY = min((1.f - ssy) * PARAM_IMAGE_HEIGHT, (float)(PARAM_IMAGE_HEIGHT - 1));
	float3 Pras = (float3)(screenX, screenY, 0.f);

	float3 rayOrig = Transform_ApplyPoint(&camera->rasterToCamera[transIndex], Pras);
	float3 rayDir = rayOrig;

	const float hither = camera->hither;

#if defined(PARAM_CAMERA_HAS_DOF)
	// Sample point on lens
	float lensU, lensV;
	ConcentricSampleDisk(dofSampleX, dofSampleY, &lensU, &lensV);
	const float lensRadius = camera->lensRadius;
	lensU *= lensRadius;
	lensV *= lensRadius;

	// Compute point on plane of focus
	const float focalDistance = camera->focalDistance;
	const float dist = focalDistance - hither;
	const float ft = dist / rayDir.z;
	float3 Pfocus;
	Pfocus = rayOrig + rayDir * ft;

	// Update ray for effect of lens
	const float k = dist / focalDistance;
	rayOrig.x += lensU * k;
	rayOrig.y += lensV * k;

	rayDir = Pfocus - rayOrig;
#endif

	rayDir = normalize(rayDir);
	const float maxt = (camera->yon - hither) / rayDir.z;

	// Transform ray in world coordinates
	rayOrig = Transform_ApplyPoint(&camera->cameraToWorld[transIndex], rayOrig);
	rayDir = Transform_ApplyVector(&camera->cameraToWorld[transIndex], rayDir);

	Ray_Init3(ray, rayOrig, rayDir, maxt);

	/*printf("(%f, %f, %f) (%f, %f, %f) [%f, %f]\n",
		ray->o.x, ray->o.y, ray->o.z, ray->d.x, ray->d.y, ray->d.z,
		ray->mint, ray->maxt);*/
}
