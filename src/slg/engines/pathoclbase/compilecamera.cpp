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

	delete[] cameraBokehDistribution;
	cameraBokehDistribution = nullptr;
	
	// Initialize CameraBase

	camera.base.yon = sceneCamera->clipYon;
	camera.base.hither = sceneCamera->clipHither;
	camera.base.shutterOpen = sceneCamera->shutterOpen;
	camera.base.shutterClose = sceneCamera->shutterClose;
	camera.base.volumeIndex = sceneCamera->volume ?
		scene->matDefs.GetMaterialIndex(sceneCamera->volume) : NULL_INDEX;

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
			camera.type = slg::ocl::ORTHOGRAPHIC;

			memcpy(camera.base.rasterToCamera.m.m, orthoCamera->GetRasterToCamera().m.m, 4 * 4 * sizeof(float));
			memcpy(camera.base.cameraToWorld.m.m, orthoCamera->GetCameraToWorld().m.m, 4 * 4 * sizeof(float));

			camera.ortho.projCamera.lensRadius = orthoCamera->lensRadius;
			camera.ortho.projCamera.focalDistance = orthoCamera->focalDistance;

			if (orthoCamera->enableClippingPlane) {
				camera.ortho.projCamera.enableClippingPlane = true;
				ASSIGN_VECTOR(camera.ortho.projCamera.clippingPlaneCenter, orthoCamera->clippingPlaneCenter);
				ASSIGN_VECTOR(camera.ortho.projCamera.clippingPlaneNormal, orthoCamera->clippingPlaneNormal);
			} else
				camera.ortho.projCamera.enableClippingPlane = false;
			break;
		}
		case Camera::PERSPECTIVE: {
			const PerspectiveCamera *perspCamera = (PerspectiveCamera *)sceneCamera;
			camera.type = slg::ocl::PERSPECTIVE;

			memcpy(camera.base.rasterToCamera.m.m, perspCamera->GetRasterToCamera().m.m, 4 * 4 * sizeof(float));
			memcpy(camera.base.cameraToWorld.m.m, perspCamera->GetCameraToWorld().m.m, 4 * 4 * sizeof(float));

			camera.persp.projCamera.lensRadius = perspCamera->lensRadius;
			camera.persp.projCamera.focalDistance = perspCamera->focalDistance;

			camera.persp.screenOffsetX = perspCamera->screenOffsetX;
			camera.persp.screenOffsetY = perspCamera->screenOffsetY;

			camera.persp.enableOculusRiftBarrel = perspCamera->enableOculusRiftBarrel;
			if (perspCamera->enableClippingPlane) {
				camera.persp.projCamera.enableClippingPlane = true;
				ASSIGN_VECTOR(camera.persp.projCamera.clippingPlaneCenter, perspCamera->clippingPlaneCenter);
				ASSIGN_VECTOR(camera.persp.projCamera.clippingPlaneNormal, perspCamera->clippingPlaneNormal);
			} else
				camera.persp.projCamera.enableClippingPlane = false;

			camera.persp.bokehBlades = perspCamera->bokehBlades;
			camera.persp.bokehPower = perspCamera->bokehPower;
			switch (perspCamera->bokehDistribution) {
				case PerspectiveCamera::DIST_NONE:
					camera.persp.bokehDistribution = slg::ocl::DIST_NONE;
				case PerspectiveCamera::DIST_UNIFORM:
					camera.persp.bokehDistribution = slg::ocl::DIST_UNIFORM;
					break;
				case PerspectiveCamera::DIST_EXPONENTIAL:
					camera.persp.bokehDistribution = slg::ocl::DIST_EXPONENTIAL;
					break;
				case PerspectiveCamera::DIST_INVERSEEXPONENTIAL:
					camera.persp.bokehDistribution = slg::ocl::DIST_INVERSEEXPONENTIAL;
					break;
				case PerspectiveCamera::DIST_GAUSSIAN:
					camera.persp.bokehDistribution = slg::ocl::DIST_GAUSSIAN;
					break;
				case PerspectiveCamera::DIST_INVERSEGAUSSIAN:
					camera.persp.bokehDistribution = slg::ocl::DIST_INVERSEGAUSSIAN;
					break;
				case PerspectiveCamera::DIST_TRIANGULAR:
					camera.persp.bokehDistribution = slg::ocl::DIST_TRIANGULAR;
					break;
				case PerspectiveCamera::DIST_CUSTOM: {
					camera.persp.bokehDistribution = slg::ocl::DIST_CUSTOM;

					cameraBokehDistribution = CompileDistribution2D(perspCamera->bokehDistributionMap, &cameraBokehDistributionSize);
					break;
				}
				default:
					throw runtime_error("Unknown bokeh distribution type in CompiledScene::CompileCamera(): " + ToString(perspCamera->bokehDistribution));
			}
			camera.persp.bokehScaleX = perspCamera->bokehScaleX;
			camera.persp.bokehScaleY = perspCamera->bokehScaleY;
			break;
		}
		case Camera::ENVIRONMENT: {
			const EnvironmentCamera *envCamera = (EnvironmentCamera *)sceneCamera;
			camera.type = slg::ocl::ENVIRONMENT;

			memcpy(camera.base.rasterToCamera.m.m, envCamera->GetRasterToCamera().m.m, 4 * 4 * sizeof(float));
			memcpy(camera.base.cameraToWorld.m.m, envCamera->GetCameraToWorld().m.m, 4 * 4 * sizeof(float));
			
			camera.env.degrees = envCamera->degrees;	
			break;			
		}		
		case Camera::STEREO: {
			const StereoCamera *stereoCamera = (StereoCamera *)sceneCamera;

			camera.type = slg::ocl::STEREO;
			
			switch(stereoCamera->GetStereoType()) {
				case StereoCamera::STEREO_PERSPECTIVE:
					camera.stereo.stereoCameraType = slg::ocl::STEREO_PERSPECTIVE;
					break;
				case StereoCamera::STEREO_ENVIRONMENT:
					camera.stereo.stereoCameraType = slg::ocl::STEREO_ENVIRONMENT;
					break;
				default:
					throw runtime_error("Unknown StereoCamera type in CompiledScene::CompileCamera(): " + ToString(stereoCamera->GetStereoType()));
			}

			camera.stereo.perspCamera.projCamera.lensRadius = stereoCamera->lensRadius;
			camera.stereo.perspCamera.projCamera.focalDistance = stereoCamera->focalDistance;

			memcpy(camera.stereo.leftEyeRasterToCamera.m.m, stereoCamera->GetRasterToCamera(0).m.m, 4 * 4 * sizeof(float));
			memcpy(camera.stereo.leftEyeCameraToWorld.m.m, stereoCamera->GetCameraToWorld(0).m.m, 4 * 4 * sizeof(float));
			memcpy(camera.stereo.rightEyeRasterToCamera.m.m, stereoCamera->GetRasterToCamera(1).m.m, 4 * 4 * sizeof(float));
			memcpy(camera.stereo.rightEyeCameraToWorld.m.m, stereoCamera->GetCameraToWorld(1).m.m, 4 * 4 * sizeof(float));

			camera.stereo.perspCamera.enableOculusRiftBarrel = stereoCamera->enableOculusRiftBarrel;
			if (stereoCamera->enableClippingPlane) {
				camera.stereo.perspCamera.projCamera.enableClippingPlane = true;
				ASSIGN_VECTOR(camera.stereo.perspCamera.projCamera.clippingPlaneCenter, stereoCamera->clippingPlaneCenter);
				ASSIGN_VECTOR(camera.stereo.perspCamera.projCamera.clippingPlaneNormal, stereoCamera->clippingPlaneNormal);
			} else
				camera.stereo.perspCamera.projCamera.enableClippingPlane = false;
			break;
		}
		default:
			throw runtime_error("Unknown camera type in CompiledScene::CompileCamera(): " + ToString(sceneCamera->GetType()));
	}
}

#endif
