#line 2 "vector_funcs.cl"

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

float SphericalTheta(const float3 v) {
	return acos(clamp(v.z, -1.f, 1.f));
}

float SphericalPhi(const float3 v) {
	const float p = atan2(v.y, v.x);
	return (p < 0.f) ? p + 2.f * M_PI_F : p;
}

void CoordinateSystem(const float3 v1, float3 *v2, float3 *v3) {
	if (fabs(v1.x) > fabs(v1.y)) {
		float invLen = 1.f / sqrt(v1.x * v1.x + v1.z * v1.z);
		(*v2).x = -v1.z * invLen;
		(*v2).y = 0.f;
		(*v2).z = v1.x * invLen;
	} else {
		float invLen = 1.f / sqrt(v1.y * v1.y + v1.z * v1.z);
		(*v2).x = 0.f;
		(*v2).y = v1.z * invLen;
		(*v2).z = -v1.y * invLen;
	}

	*v3 = cross(v1, *v2);
}
