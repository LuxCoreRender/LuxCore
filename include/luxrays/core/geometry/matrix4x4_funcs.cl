#line 2 "matrix4x4_funcs.cl"

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

float3 Matrix4x4_ApplyPoint(__global const Matrix4x4* restrict m, const float3 point) {
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

float3 Matrix4x4_ApplyPoint_Align(__global const Matrix4x4* restrict m, const float3 point) {
	const float4 point4 = (float4)(point.x, point.y, point.z, 1.f);

	const float4 row3 = VLOAD4F_Align(&m->m[3][0]);
	const float iw = 1.f / dot(row3, point4);

	const float4 row0 = VLOAD4F_Align(&m->m[0][0]);
	const float4 row1 = VLOAD4F_Align(&m->m[1][0]);
	const float4 row2 = VLOAD4F_Align(&m->m[2][0]);
	return (float3)(
			iw * dot(row0, point4),
			iw * dot(row1, point4),
			iw * dot(row2, point4)
			);
}

float3 Matrix4x4_ApplyPoint_Private(Matrix4x4 *m, const float3 point) {
	const float4 point4 = (float4)(point.x, point.y, point.z, 1.f);

	const float4 row3 = VLOAD4F_Private(&m->m[3][0]);
	const float iw = 1.f / dot(row3, point4);

	const float4 row0 = VLOAD4F_Private(&m->m[0][0]);
	const float4 row1 = VLOAD4F_Private(&m->m[1][0]);
	const float4 row2 = VLOAD4F_Private(&m->m[2][0]);
	return (float3)(
			iw * dot(row0, point4),
			iw * dot(row1, point4),
			iw * dot(row2, point4)
			);
}

float3 Matrix4x4_ApplyVector(__global const Matrix4x4* restrict m, const float3 vector) {
	const float3 row0 = VLOAD3F(&m->m[0][0]);
	const float3 row1 = VLOAD3F(&m->m[1][0]);
	const float3 row2 = VLOAD3F(&m->m[2][0]);
	return (float3)(
			dot(row0, vector),
			dot(row1, vector),
			dot(row2, vector)
			);
}

float3 Matrix4x4_ApplyVector_Private(Matrix4x4 *m, const float3 vector) {
	const float3 row0 = VLOAD3F_Private(&m->m[0][0]);
	const float3 row1 = VLOAD3F_Private(&m->m[1][0]);
	const float3 row2 = VLOAD3F_Private(&m->m[2][0]);
	return (float3)(
			dot(row0, vector),
			dot(row1, vector),
			dot(row2, vector)
			);
}

float3 Matrix4x4_ApplyNormal(__global const Matrix4x4* restrict m, const float3 normal) {
	const float3 row0 = (float3)(m->m[0][0], m->m[1][0], m->m[2][0]);
	const float3 row1 = (float3)(m->m[0][1], m->m[1][1], m->m[2][1]);
	const float3 row2 = (float3)(m->m[0][2], m->m[1][2], m->m[2][2]);
	return (float3)(
			dot(row0, normal),
			dot(row1, normal),
			dot(row2, normal)
			);
}

void Matrix4x4_Identity(Matrix4x4 *m) {
	for (int j = 0; j < 4; ++j)
		for (int i = 0; i < 4; ++i)
			m->m[i][j] = (i == j) ? 1.f : 0.f;
}

void Matrix4x4_Invert(Matrix4x4 *m) {
	int indxc[4], indxr[4];
	int ipiv[4] = {0, 0, 0, 0};

	for (int i = 0; i < 4; ++i) {
		int irow = -1, icol = -1;
		float big = 0.;
		// Choose pivot
		for (int j = 0; j < 4; ++j) {
			if (ipiv[j] != 1) {
				for (int k = 0; k < 4; ++k) {
					if (ipiv[k] == 0) {
						if (fabs(m->m[j][k]) >= big) {
							big = fabs(m->m[j][k]);
							irow = j;
							icol = k;
						}
					} else if (ipiv[k] > 1) {
						//throw std::runtime_error("Singular matrix in MatrixInvert: " + ToString(*this));
						// I can not do very much here
						Matrix4x4_Identity(m);
						return;
					}
				}
			}
		}
		++ipiv[icol];
		// Swap rows _irow_ and _icol_ for pivot
		if (irow != icol) {
			for (int k = 0; k < 4; ++k) {
				const float tmp = m->m[irow][k];
				m->m[irow][k] = m->m[icol][k];
				m->m[icol][k] = tmp;
			}
		}
		indxr[i] = irow;
		indxc[i] = icol;
		if (m->m[icol][icol] == 0.f) {
			//throw std::runtime_error("Singular matrix in MatrixInvert: " + ToString(*this));
			// I can not do very much here
			Matrix4x4_Identity(m);
			return;
		}
		// Set $m[icol][icol]$ to one by scaling row _icol_ appropriately
		float pivinv = 1.f / m->m[icol][icol];
		m->m[icol][icol] = 1.f;
		for (int j = 0; j < 4; ++j)
			m->m[icol][j] *= pivinv;
		// Subtract this row from others to zero out their columns
		for (int j = 0; j < 4; ++j) {
			if (j != icol) {
				float save = m->m[j][icol];
				m->m[j][icol] = 0;
				for (int k = 0; k < 4; ++k)
					m->m[j][k] -= m->m[icol][k] * save;
			}
		}
	}
	// Swap columns to reflect permutation
	for (int j = 3; j >= 0; --j) {
		if (indxr[j] != indxc[j]) {
			for (int k = 0; k < 4; ++k) {
				const float tmp = m->m[k][indxr[j]];
				m->m[k][indxr[j]] = m->m[k][indxc[j]];
				m->m[k][indxc[j]] = tmp;
			}
		}
	}
}
