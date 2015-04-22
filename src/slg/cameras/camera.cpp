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
#include "slg/cameras/perspective.h"
#include "slg/film/film.h"
#include "slg/core/sdl.h"
#include "slg/scene/scene.h"

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
	props.Set(Property("scene.camera.shutteropen")(shutterOpen));
	props.Set(Property("scene.camera.shutterclose")(shutterClose));

	if (motionSystem)
		props.Set(motionSystem->ToProperties("scene.camera."));
		
	return props;
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

	const std::string type = props.Get(Property("scene.camera.type")("perspective")).Get<std::string>();		

	SDL_LOG("Camera type: " << type);
	SDL_LOG("Camera position: " << orig);
	SDL_LOG("Camera target: " << target);

	auto_ptr<Camera> camera;
	if ((type == "orthographic") || (type == "perspective")) {
		if (type == "orthographic") {
			OrthographicCamera *orthoCamera;

			if (props.IsDefined("scene.camera.screenwindow")) {
				float screenWindow[4];

				const Property &prop = props.Get(Property("scene.camera.screenwindow")(0.f, 1.f, 0.f, 1.f));
				screenWindow[0] = prop.Get<float>(0);
				screenWindow[1] = prop.Get<float>(1);
				screenWindow[2] = prop.Get<float>(2);
				screenWindow[3] = prop.Get<float>(3);

				orthoCamera = new OrthographicCamera(orig, target, up, &screenWindow[0]);
			} else
				orthoCamera = new OrthographicCamera(orig, target, up);

			camera.reset(orthoCamera);
		} else {
			PerspectiveCamera *perspCamera;

			if (props.IsDefined("scene.camera.screenwindow")) {
				float screenWindow[4];

				const Property &prop = props.Get(Property("scene.camera.screenwindow")(0.f, 1.f, 0.f, 1.f));
				screenWindow[0] = prop.Get<float>(0);
				screenWindow[1] = prop.Get<float>(1);
				screenWindow[2] = prop.Get<float>(2);
				screenWindow[3] = prop.Get<float>(3);

				perspCamera = new PerspectiveCamera(orig, target, up, &screenWindow[0]);
			} else 
				perspCamera = new PerspectiveCamera(orig, target, up);

			camera.reset(perspCamera);
		}

		ProjectiveCamera *projCamera = (ProjectiveCamera *)camera.get();

		projCamera->lensRadius = props.Get(Property("scene.camera.lensradius")(0.f)).Get<float>();
		projCamera->focalDistance = props.Get(Property("scene.camera.focaldistance")(10.f)).Get<float>();
		projCamera->autoFocus = props.Get(Property("scene.camera.autofocus.enable")(false)).Get<bool>();
		
		// Check if I have to arbitrary clipping plane
		if (props.Get(Property("scene.camera.clippingplane.enable")(false)).Get<bool>()) {
			projCamera->clippingPlaneCenter = props.Get(Property("scene.camera.clippingplane.center")(Point())).Get<Point>();
			projCamera->clippingPlaneNormal = props.Get(Property("scene.camera.clippingplane.normal")(Normal())).Get<Normal>();
			projCamera->enableClippingPlane = true;
			SDL_LOG("Camera clipping plane enabled");
		} else {
			projCamera->enableClippingPlane = false;
			SDL_LOG("Camera clipping plane disabled");
		}
	} else
		throw runtime_error("Unknown camera type: " + type);
	
	camera->clipHither = props.Get(Property("scene.camera.cliphither")(1e-3f)).Get<float>();
	camera->clipYon = props.Get(Property("scene.camera.clipyon")(1e30f)).Get<float>();
	camera->shutterOpen = props.Get(Property("scene.camera.shutteropen")(0.f)).Get<float>();
	camera->shutterClose = props.Get(Property("scene.camera.shutterclose")(1.f)).Get<float>();

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
