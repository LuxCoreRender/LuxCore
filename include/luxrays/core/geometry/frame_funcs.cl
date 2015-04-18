#line 2 "frame_funcs.cl"

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

void Frame_SetFromZ(__global Frame *frame, const float3 Z) {
	float3 X, Y;
	CoordinateSystem(Z, &X, &Y);

	VSTORE3F(X, &frame->X.x);
	VSTORE3F(Y, &frame->Y.x);
	VSTORE3F(Z, &frame->Z.x);
}

float3 ToWorld(const float3 X, const float3 Y, const float3 Z, const float3 v) {
	return X * v.x + Y * v.y + Z * v.z;
}

float3 ToLocal(const float3 X, const float3 Y, const float3 Z, const float3 a) {
	return (float3)(dot(a, X), dot(a, Y), dot(a, Z));
}

float3 Frame_ToWorld(__global const Frame *frame, const float3 v) {
	return ToWorld(VLOAD3F(&frame->X.x), VLOAD3F(&frame->Y.x), VLOAD3F(&frame->Z.x), v);
}

float3 Frame_ToWorld_Private(const Frame *frame, const float3 v) {
	return ToWorld(
			(float3)(frame->X.x, frame->X.y, frame->X.z),
			(float3)(frame->Y.x, frame->Y.y, frame->Y.z),
			(float3)(frame->Z.x, frame->Z.y, frame->Z.z), v);
}

float3 Frame_ToLocal(__global const Frame *frame, const float3 v) {
	return ToLocal(VLOAD3F(&frame->X.x), VLOAD3F(&frame->Y.x), VLOAD3F(&frame->Z.x), v);
}

float3 Frame_ToLocal_Private(const Frame *frame, const float3 v) {
	return ToLocal(
			(float3)(frame->X.x, frame->X.y, frame->X.z),
			(float3)(frame->Y.x, frame->Y.y, frame->Y.z),
			(float3)(frame->Z.x, frame->Z.y, frame->Z.z), v);
}
