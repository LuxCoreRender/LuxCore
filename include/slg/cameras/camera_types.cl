#line 2 "camera_types.cl"

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

	unsigned int volumeIndex;

	// Used for camera motion blur
	MotionSystem motionSystem;
	InterpolatedTransform interpolatedTransforms[CAMERA_MAX_INTERPOLATED_TRANSFORM];
} CameraBase;

typedef struct {
	// Used for camera clipping plane
	Point clippingPlaneCenter;
	Normal clippingPlaneNormal;
	int enableClippingPlane;

	float lensRadius;
	float focalDistance;
} ProjectiveCamera;

typedef enum {
	DIST_NONE,
	DIST_UNIFORM,
	DIST_EXPONENTIAL,
	DIST_INVERSEEXPONENTIAL,
	DIST_GAUSSIAN,
	DIST_INVERSEGAUSSIAN,
	DIST_TRIANGULAR
} BokehDistributionType;

typedef struct {
	ProjectiveCamera projCamera;
	
	float screenOffsetX, screenOffsetY;
	float fieldOfView;

	unsigned int bokehBlades, bokehPower;
	BokehDistributionType bokehDistribution;
	float bokehScaleX, bokehScaleY;

	int enableOculusRiftBarrel;
} PerspectiveCamera;

typedef struct {
	ProjectiveCamera projCamera;
} OrthographicCamera;

typedef struct {
	PerspectiveCamera perspCamera;

	Transform leftEyeRasterToCamera;
	Transform leftEyeCameraToWorld;

	Transform rightEyeRasterToCamera;
	Transform rightEyeCameraToWorld;
} StereoCamera;

typedef struct {
	CameraType type;

	CameraBase base;

	union {
		PerspectiveCamera persp;
		OrthographicCamera ortho;
		StereoCamera stereo;
		// Nothing for EnvironmentCamera;
	};
} Camera;
