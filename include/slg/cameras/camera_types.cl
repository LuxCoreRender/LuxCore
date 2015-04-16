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
		ORTHOGRAPHIC, PERSPECTIVE
	} CameraType;

typedef struct {
	Transform rasterToCamera[2]; // 2 used for stereo rendering
	Transform cameraToWorld[2]; // 2 used for stereo rendering

	// Used for camera clipping plane
	Point clippingPlaneCenter;
	Normal clippingPlaneNormal;

	// Placed here for Transform memory alignement
	float lensRadius;
	float focalDistance;
	float yon, hither;
	float shutterOpen, shutterClose;
	CameraType type;

	// Used for camera motion blur
	MotionSystem motionSystem;
	InterpolatedTransform interpolatedTransforms[CAMERA_MAX_INTERPOLATED_TRANSFORM];
} Camera;
