#line 2 "matrix4x4_funcs.cl"

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

float3 Matrix4x4_ApplyPoint(__global Matrix4x4 *m, const float3 point) {
	const float4 point4 = (float4)(point.x, point.y, point.z, 1.f);

	const float4 row3 = VLOAD4F(&m->m[3][0]);
	const float iw = 1.f / dot(row3, point4);

	const float4 row0 = VLOAD4F(&m->m[0][0]);
	const float4 row1 = VLOAD4F(&m->m[1][0]);
	const float4 row2 = VLOAD4F(&m->m[2][0]);
	return (float3)(
			iw * dot(row0, point4),
			iw * dot(row1, point4),
			iw * dot(row2, point4)
			);
}

float3 Matrix4x4_ApplyVector(__global Matrix4x4 *m, const float3 vector) {
	const float3 row0 = VLOAD3F(&m->m[0][0]);
	const float3 row1 = VLOAD3F(&m->m[1][0]);
	const float3 row2 = VLOAD3F(&m->m[2][0]);
	return (float3)(
			dot(row0, vector),
			dot(row1, vector),
			dot(row2, vector)
			);
}

float3 Matrix4x4_ApplyNormal(__global Matrix4x4 *m, const float3 normal) {
	const float3 row0 = (float3)(m->m[0][0], m->m[1][0], m->m[2][0]);
	const float3 row1 = (float3)(m->m[0][1], m->m[1][1], m->m[2][1]);
	const float3 row2 = (float3)(m->m[0][2], m->m[1][2], m->m[2][2]);
	return (float3)(
			dot(row0, normal),
			dot(row1, normal),
			dot(row2, normal)
			);
}
