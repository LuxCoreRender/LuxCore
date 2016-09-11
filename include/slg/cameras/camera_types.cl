#line 2 "camera_types.cl"

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

#define CAMERA_MAX_INTERPOLATED_TRANSFORM 8

typedef enum {
	PERSPECTIVE, ORTHOGRAPHIC, STEREO, ENVIRONMENT
} CameraType;

typedef struct {
	// Placed here for Transform memory alignment
	Transform rasterToCamera;
	Transform cameraToWorld;

	float yon, hither;
	float shutterOpen, shutterClose;

	// Used for camera motion blur
	MotionSystem motionSystem;
	InterpolatedTransform interpolatedTransforms[CAMERA_MAX_INTERPOLATED_TRANSFORM];
} CameraBase;

typedef struct {
	// Used for camera clipping plane
	Point clippingPlaneCenter;
	Normal clippingPlaneNormal;
	// Note: preprocessor macro PARAM_CAMERA_ENABLE_CLIPPING_PLANE set if to use
	// the user defined clipping plane

	float lensRadius;
	float focalDistance;
} ProjectiveCamera;

typedef struct {
	ProjectiveCamera projCamera;
	
	float screenOffsetX, screenOffsetY;
	float fieldOfView;
	// Note: preprocessor macro PARAM_CAMERA_ENABLE_OCULUSRIFT_BARREL set if to use
	// Oculus Rift barrel effect
} PerspectiveCamera;

typedef struct {
	ProjectiveCamera projCamera;
} OrthographicCamera;

typedef struct {
	ProjectiveCamera projCamera;
} EnvironmentCamera;

typedef struct {
	PerspectiveCamera perspCamera;

	Transform leftEyeRasterToCamera;
	Transform leftEyeCameraToWorld;

	Transform rightEyeRasterToCamera;
	Transform rightEyeCameraToWorld;
} StereoCamera;

typedef struct {
	// The type of camera in use is defined by preprocessor macro:
	//  PARAM_CAMERA_TYPE (0 = Perspective, 1 = Orthographic, 2 = Stereo)

	CameraBase base;

	union {
		PerspectiveCamera persp;
		OrthographicCamera ortho;
		StereoCamera stereo;
		EnvironmentCamera env;
	};
} Camera;
