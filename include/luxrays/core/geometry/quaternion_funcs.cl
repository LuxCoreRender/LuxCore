#line 2 "quaternion_funcs.cl"

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

// Get the rotation matrix from quaternion
void Quaternion_ToMatrix(const float4 q, Matrix4x4 *m) {
	const float xx = q.s1 * q.s1;
	const float yy = q.s2 * q.s2;
	const float zz = q.s3 * q.s3;
	const float xy = q.s1 * q.s2;
	const float xz = q.s1 * q.s3;
	const float yz = q.s2 * q.s3;
	const float xw = q.s1 * q.s0;
	const float yw = q.s2 * q.s0;
	const float zw = q.s3 * q.s0;

	m->m[0][0] = 1.f - 2.f * (yy + zz);
	m->m[1][0] = 2.f * (xy - zw);
	m->m[2][0] = 2.f * (xz + yw);
	m->m[0][1] = 2.f * (xy + zw);
	m->m[1][1] = 1.f - 2.f * (xx + zz);
	m->m[2][1] = 2.f * (yz - xw);
	m->m[0][2] = 2.f * (xz - yw);
	m->m[1][2] = 2.f * (yz + xw);
	m->m[2][2] = 1.f - 2.f * (xx + yy);

	// Complete matrix
	m->m[0][3] = m->m[1][3] = m->m[2][3] = 0.f;
	m->m[3][0] = m->m[3][1] = m->m[3][2] = 0.f;
	m->m[3][3] = 1.f;
}

float4 Quaternion_Slerp(float t, const float4 q1, const float4 q2) {

	float cos_phi = dot(q1, q2);
	const float sign = (cos_phi > 0.f) ? 1.f : -1.f;
	
	cos_phi *= sign;

	float f1, f2;
	if (1.f - cos_phi > 1e-6f) {	
		float phi = acos(cos_phi);
		float sin_phi = sin(phi);	
		f1 = sin((1.f - t) * phi) / sin_phi;
		f2 = sin(t * phi) / sin_phi;
	} else {
		// start and end are very close
		// perform linear interpolation
		f1 = 1.f - t;
		f2 = t;
	}

	return f1 * q1 + (sign * f2) * q2;
}
