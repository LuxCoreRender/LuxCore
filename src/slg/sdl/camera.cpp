/***************************************************************************
 * Copyright 1998-2013 by authors (see AUTHORS.txt)                        *
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

#include "slg/camera/camera.h"
#include "slg/film/film.h"
#include "slg/sdl/sdl.h"
#include "slg/sdl/scene.h"
#include "luxcore/luxcore.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// Camera
//------------------------------------------------------------------------------

Properties Camera::ToProperties() const {
	Properties props;

	props.Set(Property("scene.camera.cliphither")(clipHither));
	props.Set(Property("scene.camera.clipyon")(clipYon));
	props.Set(Property("scene.camera.lensradius")(lensRadius));
	props.Set(Property("scene.camera.focaldistance")(focalDistance));
	props.Set(Property("scene.camera.shutteropen")(shutterOpen));
	props.Set(Property("scene.camera.shutterclose")(shutterClose));
	props.Set(Property("scene.camera.autofocus.enable")(autoFocus));

	if (motionSystem)
		props.Set(motionSystem->ToProperties("scene.camera."));
		
	return props;
}

//------------------------------------------------------------------------------
// PerspectiveCamera
//------------------------------------------------------------------------------

PerspectiveCamera::PerspectiveCamera(const luxrays::Point &o, const luxrays::Point &t,
		const luxrays::Vector &u, const float *region) : Camera(PERSPECTIVE),
		orig(o), target(t), up(Normalize(u)), fieldOfView(45.f) {
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
	horizStereoEyesDistance = .0626f;
	horizStereoLensDistance = .2779f;

	enableClippingPlane = true;
	clippingPlaneCenter = Point();
	clippingPlaneNormal = Normal(1.f, 0.f, 0.f);
}

void PerspectiveCamera::Update(const u_int width, const u_int height, const u_int *subRegion) {
	filmWidth = width;
	filmHeight = height;

	// Used to translate the camera
	dir = target - orig;
	dir = Normalize(dir);

	x = Cross(dir, up);
	x = Normalize(x);

	y = Cross(x, dir);
	y = Normalize(y);

	// Initialize screen information
	const float frame = float(filmWidth) / float(filmHeight);
	float screen[4];

	u_int filmSubRegion[4];
	filmSubRegion[0] = 0;
	filmSubRegion[1] = filmWidth - 1;
	filmSubRegion[2] = 0;
	filmSubRegion[3] = filmHeight - 1;

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
			throw runtime_error("Can not enable horizontal stereo support with subregion rendering");

		// I have to render a sub region of the image
		filmSubRegion[0] = subRegion[0];
		filmSubRegion[1] = subRegion[1];
		filmSubRegion[2] = subRegion[2];
		filmSubRegion[3] = subRegion[3];
		filmWidth = filmSubRegion[1] - filmSubRegion[0] + 1;
		filmHeight = filmSubRegion[3] - filmSubRegion[2] + 1;

		const float halfW = filmWidth * .5f;
		const float halfH = filmHeight * .5f;
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
		InitCameraTransforms(&camTrans[0], screen, -horizStereoEyesDistance * .5f, offset, 0.f);
		// Right eye
		InitCameraTransforms(&camTrans[1], screen, horizStereoEyesDistance * .5f, -offset, 0.f);
	} else {
		camTrans.resize(1);
		InitCameraTransforms(&camTrans[0], screen, 0.f, 0.f, 0.f);
	}

	// Initialize pixel information
	const float tanAngle = tanf(Radians(fieldOfView) / 2.f) * 2.f;
	const float xPixelWidth = tanAngle * ((screen[1] - screen[0]) / 2.f);
	const float yPixelHeight = tanAngle * ((screen[3] - screen[2]) / 2.f);
	pixelArea = xPixelWidth * yPixelHeight;
	
	if (enableClippingPlane)
		clippingPlaneNormal = Normalize(clippingPlaneNormal);
}

void PerspectiveCamera::UpdateFocus(const Scene *scene) {
	if (autoFocus) {
		// Trace a ray in the middle of the screen
		// Note: for stereo, I'm just using the left eyes as main camera
		const Point Pras(filmWidth / 2, filmHeight / 2, 0.f);

		const Point Pcamera(camTrans[0].rasterToCamera * Pras);
		Ray ray;
		ray.o = Pcamera;
		ray.d = Vector(Pcamera.x, Pcamera.y, Pcamera.z);
		ray.d = Normalize(ray.d);
		
		ray.mint = 0.f;
		ray.maxt = (clipYon - clipHither) / ray.d.z;

		if (motionSystem)
			ray = motionSystem->Sample(0.f) * (camTrans[0].cameraToWorld * ray);
		else
			ray = camTrans[0].cameraToWorld * ray;
		
		// Trace the ray. If there isn't an intersection just use the current
		// focal distance
		RayHit rayHit;
		if (scene->dataSet->GetAccelerator()->Intersect(&ray, &rayHit))
			focalDistance = rayHit.t;
	}
}

void PerspectiveCamera::InitCameraTransforms(CameraTransforms *trans, const float screen[4],
		const float eyeOffset,
		const float screenOffsetX, const float screenOffsetY) {
	// This is a trick I use from LuxCoreRenderer to set cameraToWorld to
	// identity matrix.
	if (orig == target)
		trans->cameraToWorld = Transform();
	else {
		// Shift from camera to eye position
		const Point eyeOrig = orig + eyeOffset * luxrays::Normalize(x);
		const Point eyeTarget = target + eyeOffset * luxrays::Normalize(x);
		const Transform worldToCamera = LookAt(eyeOrig, eyeTarget, up);
		trans->cameraToWorld = Inverse(worldToCamera);
	}

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
	Ray *ray, const float u1, const float u2, const float u3) const {
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
	ray->time = Lerp(u3, shutterOpen, shutterClose);

	if (motionSystem)
		*ray = motionSystem->Sample(ray->time) * (camTrans[transIndex].cameraToWorld * (*ray));
	else
		*ray = camTrans[transIndex].cameraToWorld * (*ray);

	// World arbitrary clipping plane support
	if (enableClippingPlane)
		ApplyArbitraryClippingPlane(ray);
}

void PerspectiveCamera::ApplyArbitraryClippingPlane(Ray *ray) const {
	// Intersect the ray with clipping plane
	const float denom = Dot(clippingPlaneNormal, ray->d);
	const Vector pr = clippingPlaneCenter - ray->o;
	float d = Dot(pr, clippingPlaneNormal);

	if (fabsf(denom) > DEFAULT_COS_EPSILON_STATIC) {
		// There is a valid intersection
		d /= denom; 

		if (d > 0.f) {
			// The plane is in front of the camera
			if (denom < 0.f) {
				// The plane points toward the camera
				ray->maxt = Clamp(d, ray->mint, ray->maxt);
			} else {
				// The plane points away from the camera
				ray->mint = Clamp(d, ray->mint, ray->maxt);
			}
		} else {
			if ((denom < 0.f) && (d < 0.f)) {
				// No intersection possible, I use a trick here to avoid any
				// intersection by setting mint=maxt
				ray->mint = ray->maxt;
			} else {
				// Nothing to do
			}
		}
	} else {
		// The plane is parallel to the view directions. Check if I'm on the
		// visible side of the plane or not
		if (d >= 0.f) {
			// No intersection possible, I use a trick here to avoid any
			// intersection by setting mint=maxt
			ray->mint = ray->maxt;
		} else {
			// Nothing to do
		}
	}
}

bool PerspectiveCamera::GetSamplePosition(Ray *ray, float *x, float *y) const {
	const float cosi = Dot(ray->d, dir);
	if ((cosi <= 0.f) || (!isinf(ray->maxt) && (ray->maxt * cosi < clipHither ||
		ray->maxt * cosi > clipYon)))
		return false;

	Point pO(Inverse(camTrans[0].rasterToWorld) * (ray->o + ((lensRadius > 0.f) ?	(ray->d * (focalDistance / cosi)) : ray->d)));
	if (motionSystem)
		pO *= motionSystem->Sample(ray->time);

	*x = pO.x;
	*y = filmHeight - 1 - pO.y;

	// Check if we are inside the image plane
	if ((*x < 0.f) || (*x >= filmWidth) ||
			(*y < 0.f) || (*y >= filmHeight))
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

bool PerspectiveCamera::SampleLens(const float time,
		const float u1, const float u2,
		Point *lensp) const {
	Point lensPoint(0.f, 0.f, 0.f);
	if (lensRadius > 0.f) {
		ConcentricSampleDisk(u1, u2, &lensPoint.x, &lensPoint.y);
		lensPoint.x *= lensRadius;
		lensPoint.y *= lensRadius;
	}

	if (motionSystem)
		*lensp = motionSystem->Sample(time) * (camTrans[0].cameraToWorld * lensPoint);
	else
		*lensp = camTrans[0].cameraToWorld * lensPoint;

	return true;
}

Properties PerspectiveCamera::ToProperties() const {
	Properties props;

	props.Set(Camera::ToProperties());

	props.Set(Property("scene.camera.lookat.orig")(orig));
	props.Set(Property("scene.camera.lookat.target")(target));
	props.Set(Property("scene.camera.up")(up));

	if (!autoUpdateFilmRegion)
		props.Set(Property("scene.camera.screenwindow")(filmRegion[0], filmRegion[1], filmRegion[2], filmRegion[3]));

	props.Set(Property("scene.camera.fieldofview")(fieldOfView));
	props.Set(Property("scene.camera.horizontalstereo.enable")(enableHorizStereo));
	props.Set(Property("scene.camera.horizontalstereo.oculusrift.barrelpostpro.enable")(enableOculusRiftBarrel));

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

	if (x < .5f) {
		*barrelX = (ex + 1.f) * .25f;
		*barrelY = (ey + 1.f) * .5f;
	} else {
		*barrelX = (ex + 1.f) * .25f + .5f;
		*barrelY = (ey + 1.f) * .5f;
	}
}

//------------------------------------------------------------------------------
// Allocate a Camera
//------------------------------------------------------------------------------

Camera *Camera::AllocCamera(const luxrays::Properties &props) {
	Point orig, target;
	Vector up;
	if (props.IsDefined("scene.camera.lookat")) {
		SDL_LOG("WARNING: deprecated property scene.camera.lookat");

		const Property &prop = props.Get("scene.camera.lookat");
		orig.x = prop.Get<float>(0);
		orig.y = prop.Get<float>(1);
		orig.z = prop.Get<float>(2);
		target.x = prop.Get<float>(3);
		target.y = prop.Get<float>(4);
		target.z = prop.Get<float>(5);
		up = Vector(0.f, 0.f, 1.f);
	} else {
		if (!props.IsDefined("scene.camera.lookat.orig")) {
			// This is a trick I use from LuxCoreRenderer to set cameraToWorld to
			// identity matrix.
			//
			// I leave orig, target and up all set to (0.f, 0.f, 0.f)
		} else {
			orig = props.Get(Property("scene.camera.lookat.orig")(0.f, 10.f, 0.f)).Get<Point>();
			target = props.Get(Property("scene.camera.lookat.target")(0.f, 0.f, 0.f)).Get<Point>();
			up = props.Get(Property("scene.camera.up")(0.f, 0.f, 1.f)).Get<Vector>();
		}
	}

	SDL_LOG("Camera position: " << orig);
	SDL_LOG("Camera target: " << target);

	auto_ptr<PerspectiveCamera> camera;
	if (props.IsDefined("scene.camera.screenwindow")) {
		float screenWindow[4];

		const Property &prop = props.Get(Property("scene.camera.screenwindow")(0.f, 1.f, 0.f, 1.f));
		screenWindow[0] = prop.Get<float>(0);
		screenWindow[1] = prop.Get<float>(1);
		screenWindow[2] = prop.Get<float>(2);
		screenWindow[3] = prop.Get<float>(3);

		camera.reset(new PerspectiveCamera(orig, target, up, &screenWindow[0]));
	} else
		camera.reset(new PerspectiveCamera(orig, target, up));

	camera->clipHither = props.Get(Property("scene.camera.cliphither")(1e-3f)).Get<float>();
	camera->clipYon = props.Get(Property("scene.camera.clipyon")(1e30f)).Get<float>();
	camera->lensRadius = props.Get(Property("scene.camera.lensradius")(0.f)).Get<float>();
	camera->focalDistance = props.Get(Property("scene.camera.focaldistance")(10.f)).Get<float>();
	camera->shutterOpen = props.Get(Property("scene.camera.shutteropen")(0.f)).Get<float>();
	camera->shutterClose = props.Get(Property("scene.camera.shutterclose")(1.f)).Get<float>();
	camera->fieldOfView = props.Get(Property("scene.camera.fieldofview")(45.f)).Get<float>();
	camera->autoFocus = props.Get(Property("scene.camera.autofocus.enable")(false)).Get<bool>();

	if (props.Get(Property("scene.camera.horizontalstereo.enable")(false)).Get<bool>()) {
		SDL_LOG("Camera horizontal stereo enabled");
		camera->SetHorizontalStereo(true);

		const float eyesDistance = props.Get(Property("scene.camera.horizontalstereo.eyesdistance")(.0626f)).Get<float>();
		SDL_LOG("Camera horizontal stereo eyes distance: " << eyesDistance);
		camera->SetHorizontalStereoEyesDistance(eyesDistance);
		const float lesnDistance = props.Get(Property("scene.camera.horizontalstereo.lensdistance")(.1f)).Get<float>();
		SDL_LOG("Camera horizontal stereo lens distance: " << lesnDistance);
		camera->SetHorizontalStereoLensDistance(lesnDistance);

		// Check if I have to enable Oculus Rift Barrel post-processing
		if (props.Get(Property("scene.camera.horizontalstereo.oculusrift.barrelpostpro.enable")(false)).Get<bool>()) {
			SDL_LOG("Camera Oculus Rift Barrel post-processing enabled");
			camera->SetOculusRiftBarrel(true);
		} else {
			SDL_LOG("Camera Oculus Rift Barrel post-processing disabled");
			camera->SetOculusRiftBarrel(false);
		}
	} else {
		SDL_LOG("Camera horizontal stereo disabled");
		camera->SetHorizontalStereo(false);
	}
	
	if (props.Get(Property("scene.camera.clippingplane.enable")(false)).Get<bool>()) {
		camera->clippingPlaneCenter = props.Get(Property("scene.camera.clippingplane.center")(Point())).Get<Point>();
		camera->clippingPlaneNormal = props.Get(Property("scene.camera.clippingplane.normal")(Normal())).Get<Normal>();

		SDL_LOG("Camera clipping plane enabled");
		camera->SetClippingPlane(true);
	} else {
		SDL_LOG("Camera clipping plane disabled");
		camera->SetClippingPlane(false);
	}

	// Check if I have to use a motion system
	if (props.IsDefined("scene.camera.motion.0.time")) {
		// Build the motion system
		vector<float> times;
		vector<Transform> transforms;
		for (u_int i =0;; ++i) {
			const string prefix = "scene.camera.motion." + ToString(i);
			if (!props.IsDefined(prefix +".time"))
				break;

			const float t = props.Get(prefix +".time").Get<float>();
			if (i > 0 && t <= times.back())
				throw runtime_error("Motion camera time must be monotonic");
			times.push_back(t);

			const Matrix4x4 mat = props.Get(Property(prefix +
				".transformation")(Matrix4x4::MAT_IDENTITY)).Get<Matrix4x4>();
			transforms.push_back(Transform(mat));
		}

		camera->motionSystem = new MotionSystem(times, transforms);
	}

	return camera.release();
}
