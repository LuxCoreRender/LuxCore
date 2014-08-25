#line 2 "motionsystem_funcs.cl"

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

void InterpolatedTransform_Sample(__global InterpolatedTransform *interpolatedTransform,
		const float time, Matrix4x4 *result) {
	if (!interpolatedTransform->isActive) {
		*result = interpolatedTransform->start.mInv;
		return;
	}

	// Determine interpolation value
	if (time <= interpolatedTransform->startTime) {
		*result = interpolatedTransform->start.mInv;
		return;
	}
	if (time >= interpolatedTransform->endTime) {
		*result = interpolatedTransform->end.mInv;
		return;
	}

	const float w = interpolatedTransform->endTime - interpolatedTransform->startTime;
	const float d = time - interpolatedTransform->startTime;
	const float le = d / w;

	// if translation only, just modify start matrix
	if (interpolatedTransform->hasTranslation &&
			!(interpolatedTransform->hasScale || interpolatedTransform->hasRotation)) {
		*result = interpolatedTransform->start.m;
		if (interpolatedTransform->hasTranslationX)
			result->m[0][3] = mix(interpolatedTransform->startT.Tx, interpolatedTransform->endT.Tx, le);
		if (interpolatedTransform->hasTranslationY)
			result->m[1][3] = mix(interpolatedTransform->startT.Ty, interpolatedTransform->endT.Ty, le);
		if (interpolatedTransform->hasTranslationZ)
			result->m[2][3] = mix(interpolatedTransform->startT.Tz, interpolatedTransform->endT.Tz, le);

		Matrix4x4_Invert(result);
		return;
	}

	if (interpolatedTransform->hasRotation) {
		// Quaternion interpolation of rotation
		const float4 startQ = VLOAD4F(&interpolatedTransform->startQ.w);
		const float4 endQ = VLOAD4F(&interpolatedTransform->endQ.w);
		const float4 interQ = Quaternion_Slerp(le, startQ, endQ);
		Quaternion_ToMatrix(interQ, result);
	} else
		*result = interpolatedTransform->startT.R;

	if (interpolatedTransform->hasScale) {
		const float Sx = mix(interpolatedTransform->startT.Sx, interpolatedTransform->endT.Sx, le);
		const float Sy = mix(interpolatedTransform->startT.Sy, interpolatedTransform->endT.Sy, le); 
		const float Sz = mix(interpolatedTransform->startT.Sz, interpolatedTransform->endT.Sz, le);

		// T * S * R
		for (uint j = 0; j < 3; ++j) {
			result->m[0][j] = Sx * result->m[0][j];
			result->m[1][j] = Sy * result->m[1][j];
			result->m[2][j] = Sz * result->m[2][j];
		}
	} else {
		for (uint j = 0; j < 3; ++j) {
			result->m[0][j] = interpolatedTransform->startT.Sx * result->m[0][j];
			result->m[1][j] = interpolatedTransform->startT.Sy * result->m[1][j];
			result->m[2][j] = interpolatedTransform->startT.Sz * result->m[2][j];
		}
	}

	if (interpolatedTransform->hasTranslationX)
		result->m[0][3] = mix(interpolatedTransform->startT.Tx, interpolatedTransform->endT.Tx, le);
	else
		result->m[0][3] = interpolatedTransform->startT.Tx;

	if (interpolatedTransform->hasTranslationY)
		result->m[1][3] = mix(interpolatedTransform->startT.Ty, interpolatedTransform->endT.Ty, le);
	else
		result->m[1][3] = interpolatedTransform->startT.Ty;

	if (interpolatedTransform->hasTranslationZ)
		result->m[2][3] = mix(interpolatedTransform->startT.Tz, interpolatedTransform->endT.Tz, le);
	else
		result->m[2][3] = interpolatedTransform->startT.Tz;

	Matrix4x4_Invert(result);
}

void MotionSystem_Sample(__global MotionSystem *motionSystem, const float time,
		__global InterpolatedTransform *interpolatedTransforms, Matrix4x4 *result) {
	const uint interpolatedTransformFirstIndex = motionSystem->interpolatedTransformFirstIndex;
	const uint interpolatedTransformLastIndex = motionSystem->interpolatedTransformLastIndex;

	// Pick the right InterpolatedTransform
	uint index = interpolatedTransformLastIndex;
	for (uint i = interpolatedTransformFirstIndex; i <= interpolatedTransformLastIndex; ++i) {
		if (time < interpolatedTransforms[i].endTime) {
			index = i;
			break;
		}
	}

	InterpolatedTransform_Sample(&interpolatedTransforms[index], time, result);
}
