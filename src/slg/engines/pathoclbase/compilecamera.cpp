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

#if !defined(LUXRAYS_DISABLE_OPENCL)

#include <iosfwd>
#include <limits>

#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string.hpp>

#include "slg/engines/pathoclbase/compiledscene.h"
#include "slg/kernels/kernels.h"

#include "slg/cameras/orthographic.h"
#include "slg/cameras/perspective.h"
#include "slg/cameras/stereo.h"
#include "slg/cameras/environment.h"

using namespace std;
using namespace luxrays;
using namespace slg;

void CompiledScene::CompileCamera() {
	wasCameraCompiled = true;

	//SLG_LOG("Compile Camera");

	//--------------------------------------------------------------------------
	// Camera definition
	//--------------------------------------------------------------------------

	const Camera *sceneCamera = scene->camera;

	// Initialize CameraBase

	camera.base.yon = sceneCamera->clipYon;
	camera.base.hither = sceneCamera->clipHither;
	camera.base.shutterOpen = sceneCamera->shutterOpen;
	camera.base.shutterClose = sceneCamera->shutterClose;

	if (sceneCamera->motionSystem) {
		if (sceneCamera->motionSystem->interpolatedTransforms.size() > CAMERA_MAX_INTERPOLATED_TRANSFORM)
			throw runtime_error("Too many interpolated transformations in camera motion system: " +
					ToString(sceneCamera->motionSystem->interpolatedTransforms.size()));

		for (u_int i = 0; i < sceneCamera->motionSystem->interpolatedTransforms.size(); ++i) {
			const InterpolatedTransform &it = sceneCamera->motionSystem->interpolatedTransforms[i];

			// Here, I assume that luxrays::ocl::InterpolatedTransform and
			// luxrays::InterpolatedTransform are the same
			camera.base.interpolatedTransforms[i] = *((const luxrays::ocl::InterpolatedTransform *)&it);
		}

		camera.base.motionSystem.interpolatedTransformFirstIndex = 0;
		camera.base.motionSystem.interpolatedTransformLastIndex = sceneCamera->motionSystem->interpolatedTransforms.size() - 1;
	} else {
		camera.base.motionSystem.interpolatedTransformFirstIndex = NULL_INDEX;
		camera.base.motionSystem.interpolatedTransformLastIndex = NULL_INDEX;
	}

	// Initialize Camera specific data

	switch (sceneCamera->GetType()) {
		case Camera::ORTHOGRAPHIC: {
			const OrthographicCamera *orthoCamera = (OrthographicCamera *)sceneCamera;
			cameraType = slg::ocl::ORTHOGRAPHIC;

			memcpy(camera.base.rasterToCamera.m.m, orthoCamera->GetRasterToCameraMatrix().m, 4 * 4 * sizeof(float));
			memcpy(camera.base.cameraToWorld.m.m, orthoCamera->GetCameraToWorldMatrix().m, 4 * 4 * sizeof(float));

			camera.ortho.projCamera.lensRadius = orthoCamera->lensRadius;
			camera.ortho.projCamera.focalDistance = orthoCamera->focalDistance;

			enableCameraOculusRiftBarrel = false;
			if (orthoCamera->enableClippingPlane) {
				enableCameraClippingPlane = true;
				ASSIGN_VECTOR(camera.ortho.projCamera.clippingPlaneCenter, orthoCamera->clippingPlaneCenter);
				ASSIGN_VECTOR(camera.ortho.projCamera.clippingPlaneNormal, orthoCamera->clippingPlaneNormal);
			} else
				enableCameraClippingPlane = false;
			break;
		}
		case Camera::PERSPECTIVE: {
			const PerspectiveCamera *perspCamera = (PerspectiveCamera *)sceneCamera;
			cameraType = slg::ocl::PERSPECTIVE;

			memcpy(camera.base.rasterToCamera.m.m, perspCamera->GetRasterToCameraMatrix().m, 4 * 4 * sizeof(float));
			memcpy(camera.base.cameraToWorld.m.m, perspCamera->GetCameraToWorldMatrix().m, 4 * 4 * sizeof(float));

			camera.persp.projCamera.lensRadius = perspCamera->lensRadius;
			camera.persp.projCamera.focalDistance = perspCamera->focalDistance;

			camera.persp.screenOffsetX = perspCamera->screenOffsetX;
			camera.persp.screenOffsetY = perspCamera->screenOffsetY;

			enableCameraOculusRiftBarrel = perspCamera->enableOculusRiftBarrel;
			if (perspCamera->enableClippingPlane) {
				enableCameraClippingPlane = true;
				ASSIGN_VECTOR(camera.persp.projCamera.clippingPlaneCenter, perspCamera->clippingPlaneCenter);
				ASSIGN_VECTOR(camera.persp.projCamera.clippingPlaneNormal, perspCamera->clippingPlaneNormal);
			} else
				enableCameraClippingPlane = false;
			break;
		}
		case Camera::STEREO: {
			const StereoCamera *stereoCamera = (StereoCamera *)sceneCamera;
			cameraType = slg::ocl::STEREO;

			camera.stereo.perspCamera.projCamera.lensRadius = stereoCamera->lensRadius;
			camera.stereo.perspCamera.projCamera.focalDistance = stereoCamera->focalDistance;

			memcpy(camera.stereo.leftEyeRasterToCamera.m.m, stereoCamera->GetRasterToCameraMatrix(0).m, 4 * 4 * sizeof(float));
			memcpy(camera.stereo.leftEyeCameraToWorld.m.m, stereoCamera->GetCameraToWorldMatrix(0).m, 4 * 4 * sizeof(float));
			memcpy(camera.stereo.rightEyeRasterToCamera.m.m, stereoCamera->GetRasterToCameraMatrix(1).m, 4 * 4 * sizeof(float));
			memcpy(camera.stereo.rightEyeCameraToWorld.m.m, stereoCamera->GetCameraToWorldMatrix(1).m, 4 * 4 * sizeof(float));

			enableCameraOculusRiftBarrel = stereoCamera->enableOculusRiftBarrel;
			if (stereoCamera->enableClippingPlane) {
				enableCameraClippingPlane = true;
				ASSIGN_VECTOR(camera.stereo.perspCamera.projCamera.clippingPlaneCenter, stereoCamera->clippingPlaneCenter);
				ASSIGN_VECTOR(camera.stereo.perspCamera.projCamera.clippingPlaneNormal, stereoCamera->clippingPlaneNormal);
			} else
				enableCameraClippingPlane = false;
			break;
		}
		case Camera::ENVIRONMENT: {
			const EnvironmentCamera *envCamera = (EnvironmentCamera *)sceneCamera;
			cameraType = slg::ocl::ENVIRONMENT;

			memcpy(camera.base.rasterToCamera.m.m, envCamera->GetRasterToCameraMatrix().m, 4 * 4 * sizeof(float));
			memcpy(camera.base.cameraToWorld.m.m, envCamera->GetCameraToWorldMatrix().m, 4 * 4 * sizeof(float));

			camera.env.projCamera.lensRadius = envCamera->lensRadius;
			camera.env.projCamera.focalDistance = envCamera->focalDistance;

			enableCameraClippingPlane = false;
			break;			
		}		
		default:
			throw runtime_error("Unknown camera type in CompiledScene::CompileCamera(): " + boost::lexical_cast<string>(sceneCamera->GetType()));
	}
}

#endif
