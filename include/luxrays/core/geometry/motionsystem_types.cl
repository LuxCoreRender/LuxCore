#line 2 "motionsystem_types.cl"

/***************************************************************************
 * Copyright 1998-2018 by authors (see AUTHORS.txt)                        *
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

typedef struct {
	// Scaling
	float Sx, Sy, Sz;
	// Shearing
	float Sxy, Sxz, Syz;
	// Rotation
	Matrix4x4 R;
	// Translation
	float Tx, Ty, Tz;
	// Perspective
	float Px, Py, Pz, Pw;
	// Represents a valid series of transformations
	bool Valid;
} DecomposedTransform;

typedef struct {
	float startTime, endTime;
	Transform start, end;
	DecomposedTransform startT, endT;
	Quaternion startQ, endQ;
	int hasRotation, hasTranslation, hasScale;
	int hasTranslationX, hasTranslationY, hasTranslationZ;
	int hasScaleX, hasScaleY, hasScaleZ;
	// false if start and end transformations are identical
	int isActive;
} InterpolatedTransform;

typedef struct {
	unsigned int interpolatedTransformFirstIndex;
	unsigned int interpolatedTransformLastIndex;

	unsigned int interpolatedInverseTransformFirstIndex;
	unsigned int interpolatedInverseTransformLastIndex;
} MotionSystem;
