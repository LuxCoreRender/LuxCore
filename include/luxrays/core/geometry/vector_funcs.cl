#line 2 "vector_funcs.cl"

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

OPENCL_FORCE_INLINE float SphericalTheta(const float3 v) {
	return acos(clamp(v.z, -1.f, 1.f));
}

OPENCL_FORCE_INLINE float SphericalPhi(const float3 v) {
	const float p = atan2(v.y, v.x);
	return (p < 0.f) ? p + 2.f * M_PI_F : p;
}

OPENCL_FORCE_INLINE void CoordinateSystem(const float3 v1, float3 *v2, float3 *v3) {
	float len = sqrt(v1.x * v1.x + v1.y * v1.y);
	if (len < 1.e-5) { // it's pretty-much along z-axis
		(*v3).x = 1.f;
		(*v3).y = 0.f;
		(*v3).z = 0.f;
	} else {
		(*v3).x = -v1.y / len;
		(*v3).y = v1.x / len;
		(*v3).z = 0.f;
	}
	*v2 = cross(v1, *v3);
}
